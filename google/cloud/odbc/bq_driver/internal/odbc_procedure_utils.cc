// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/odbc/bq_driver/internal/odbc_procedure_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

/**
 * Validates the parameters for retrieving procedure column metadata.
 *
 * @param metadata_id - Indicates whether to use standard metadata retrieval.
 * @return StatusRecord indicating validation SUCCESS or FAILURE.
 */

StatusRecordOr<Procedure> ValidateProcedureColumnParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* procedure_name, SQLSMALLINT procedure_name_len,
    SQLULEN metadata_id) {
  if (catalog_name_len < 0 && catalog_name_len != SQL_NTS) {
    return StatusRecord{SQLStates::k_HY090(), "Invalid catalog length"};
  }
  if (schema_name_len < 0 && schema_name_len != SQL_NTS) {
    return StatusRecord{SQLStates::k_HY090(), "Invalid schema length"};
  }
  if (procedure_name_len < 0 && procedure_name_len != SQL_NTS) {
    return StatusRecord{SQLStates::k_HY090(), "Invalid procedure name length"};
  }

  if (metadata_id == SQL_TRUE) {
    if (!catalog_name) {
      return StatusRecord{SQLStates::k_HY009(), "Catalog name cannot be NULL"};
    }
    if (!schema_name) {
      return StatusRecord{SQLStates::k_HY009(), "Schema name cannot be NULL"};
    }
    if (!procedure_name) {
      return StatusRecord{SQLStates::k_HY009(),
                          "Procedure name cannot be NULL"};
    }
  }

  if (IsSearchPatternArgument(reinterpret_cast<char const*>(catalog_name))) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Catalog name cannot be a search pattern"};
  }

  std::string catalog(reinterpret_cast<char const*>(catalog_name),
                      catalog_name_len);
  std::string dataset(reinterpret_cast<char const*>(schema_name),
                      schema_name_len);
  std::string proc_name(reinterpret_cast<char const*>(procedure_name),
                        procedure_name_len);

  if (catalog.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "Catalog cannot be empty"};
  }
  if (dataset.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "Dataset cannot be empty"};
  }
  if (proc_name.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "Procedure name cannot be empty"};
  }

  Procedure procedure;
  procedure.catalog = catalog;
  procedure.dataset = dataset;
  procedure.procedure_name = proc_name;

  return procedure;
}

StatusRecordOr<Procedure> FetchBQProcedureData(ConnectionHandle& conn_handle,
                                               Procedure& in_proc) {
  // Validate connection
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  // Construct the query
  std::string query =
      "SELECT * FROM `" + in_proc.catalog + "." + in_proc.dataset +
      ".INFORMATION_SCHEMA.PARAMETERS` WHERE specific_name = '" +
      in_proc.procedure_name + "'";

  QueryRequest query_request;
  query_request.set_query(query);

  auto query_result = bq_client->Query(in_proc.catalog, query_request, {});
  if (!query_result.Ok()) {
    return StatusRecord{SQLStates::k_HY000(), "Failed to fetch procedure data"};
  }

  auto response = query_result.GetValue();

  for (auto const& row : response.rows) {
    auto const& columns = row.columns;

    if (columns.size() < 8) {
      return StatusRecord{SQLStates::k_HY000(),
                          "Unexpected column count in the response"};
    }

    in_proc.schema.fields.emplace_back(ProcedureFieldSchema{
        columns[0].value,                   // catalog
        columns[1].value,                   // dataset
        columns[2].value,                   // procedure
        columns[3].value,                   // ordinal_number
        columns[4].value,                   // column_type
        columns[5].is_null ? "NO" : "YES",  // nullable
        columns[6].value,                   // name
        columns[7].value                    // type_name
    });
  }

  return in_proc;
}

StatusRecordOr<std::vector<FilteredProcedureResponse>> GetFilteredProcedures(
    ConnectionHandle& conn_handle, std::string const& project_id,
    std::string const& dataset_id, std::string const& procedures_filter) {
  // Ensure Connection Handle is Valid
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(), "Connection lost"};
  }

  std::vector<QueryParameter> named_query_params;
  QueryParameter param;
  param.name = "procedure_name";
  param.parameter_type.type = "STRING";
  param.parameter_value.value = procedures_filter;
  named_query_params.push_back(param);

  std::string query = R"(
SELECT routine_name, routine_schema
FROM `)" + project_id +
                      "." + dataset_id + R"(.INFORMATION_SCHEMA.ROUTINES`
WHERE routine_name LIKE @procedure_name
AND routine_type = 'PROCEDURE'
)";

  // Construct Post Query Request
  auto post_query_request_status = ConstructNamedParametersPostQueryRequest(
      project_id, dataset_id, query, named_query_params);

  if (!post_query_request_status) {
    return post_query_request_status.GetStatusRecord();
  }

  // Fetch Data
  auto fetch_status_record_or =
      FetchBQData(conn_handle, *post_query_request_status);
  if (!fetch_status_record_or) {
    return fetch_status_record_or.GetStatusRecord();
  }

  StatusRecordOr<std::vector<RowData>> rows =
      GetRowsResults(*fetch_status_record_or);
  if (!rows) {
    return rows.GetStatusRecord();
  }
  std::vector<FilteredProcedureResponse> procedure_response;
  for (auto const& row : *rows) {
    procedure_response.push_back({row.columns[0].value, row.columns[1].value});
  }

  return procedure_response;
}

StatusRecordOr<std::vector<Procedure>> FetchBQProceduresData(
    ConnectionHandle& conn_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id) {
  std::vector<Procedure> result;

  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  StatusRecordOr<std::vector<std::string>> datasets_status =
      GetFilteredDatasetIds(*bq_client, catalog, dataset_pattern, metadata_id);
  if (!datasets_status) {
    return datasets_status.GetStatusRecord();
  }

  for (auto const& dataset : *datasets_status) {
    StatusRecordOr<std::vector<FilteredProcedureResponse>> procedure_status =
        GetFilteredProcedures(conn_handle, catalog, dataset, procedure_pattern);
    if (!procedure_status) {
      return procedure_status.GetStatusRecord();
    }

    for (auto const& filtered_proc : *procedure_status) {
      StatusRecordOr<Procedure> validated_proc =
          ValidateProcedureColumnParameters(
              reinterpret_cast<const SQLCHAR*>(catalog.c_str()),
              static_cast<SQLSMALLINT>(catalog.length()),
              reinterpret_cast<const SQLCHAR*>(dataset.c_str()),
              static_cast<SQLSMALLINT>(dataset.length()),
              reinterpret_cast<const SQLCHAR*>(filtered_proc.proc_name.c_str()),
              static_cast<SQLSMALLINT>(filtered_proc.proc_name.length()),
              metadata_id);

      if (!validated_proc) {
        return validated_proc.GetStatusRecord();
      }

      StatusRecordOr<Procedure> bq_procedure_status =
          FetchBQProcedureData(conn_handle, *validated_proc);
      if (!bq_procedure_status) {
        return bq_procedure_status.GetStatusRecord();
      }
      result.push_back(*bq_procedure_status);
    }
  }

  return result;
}

}  // namespace google::cloud::odbc_bq_driver_internal
