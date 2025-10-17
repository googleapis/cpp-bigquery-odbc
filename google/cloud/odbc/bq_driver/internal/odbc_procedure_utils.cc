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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using ::google::cloud::odbc_bq_driver_internal::GetFixedColumnMetadata;
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
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Invalid catalog length.";
    return StatusRecord{SQLStates::k_HY090(), "Invalid catalog length"};
  }
  if (schema_name_len < 0 && schema_name_len != SQL_NTS) {
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Invalid schema length.";
    return StatusRecord{SQLStates::k_HY090(), "Invalid schema length"};
  }
  if (procedure_name_len < 0 && procedure_name_len != SQL_NTS) {
    LOG(ERROR)
        << "ValidateProcedureColumnParameters:: Invalid procedure name length.";
    return StatusRecord{SQLStates::k_HY090(), "Invalid procedure name length"};
  }

  if (metadata_id == SQL_TRUE) {
    if (!catalog_name) {
      LOG(ERROR)
          << "ValidateProcedureColumnParameters:: Catalog name cannot be NULL.";
      return StatusRecord{SQLStates::k_HY009(), "Catalog name cannot be NULL"};
    }
    if (!schema_name) {
      LOG(ERROR)
          << "ValidateProcedureColumnParameters:: Schema name cannot be NULL.";
      return StatusRecord{SQLStates::k_HY009(), "Schema name cannot be NULL"};
    }
    if (!procedure_name) {
      LOG(ERROR) << "ValidateProcedureColumnParameters:: Procedure name cannot "
                    "be NULL.";
      return StatusRecord{SQLStates::k_HY009(),
                          "Procedure name cannot be NULL"};
    }
  }

  if (IsSearchPatternArgument(reinterpret_cast<char const*>(catalog_name))) {
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Catalog name cannot be "
                  "a search pattern.";
    return StatusRecord{SQLStates::k_HY090(),
                        "Catalog name cannot be a search pattern"};
  }

  std::string catalog =
      (catalog_name_len == SQL_NTS)
          ? std::string(reinterpret_cast<char const*>(catalog_name))
          : std::string(reinterpret_cast<char const*>(catalog_name),
                        catalog_name_len);

  std::string dataset =
      (schema_name_len == SQL_NTS)
          ? std::string(reinterpret_cast<char const*>(schema_name))
          : std::string(reinterpret_cast<char const*>(schema_name),
                        schema_name_len);

  std::string proc_name =
      (procedure_name_len == SQL_NTS)
          ? std::string(reinterpret_cast<char const*>(procedure_name))
          : std::string(reinterpret_cast<char const*>(procedure_name),
                        procedure_name_len);

  if (catalog.empty()) {
    LOG(ERROR)
        << "ValidateProcedureColumnParameters:: Catalog cannot be empty.";
    return StatusRecord{SQLStates::k_HY000(), "Catalog cannot be empty"};
  }
  if (dataset.empty()) {
    LOG(ERROR)
        << "ValidateProcedureColumnParameters:: Dataset cannot be empty.";
    return StatusRecord{SQLStates::k_HY000(), "Dataset cannot be empty"};
  }
  if (proc_name.empty()) {
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Procedure name cannot "
                  "be empty.";
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
    LOG(ERROR)
        << "FetchBQProcedureData:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR) << "FetchBQProcedureData:: Invalid or null BQ Client within the "
                  "connection handle.";
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
    LOG(ERROR)
        << "FetchBQProcedureData::Query:: Failed to fetch procedure data: ";
    return StatusRecord{SQLStates::k_HY000(), "Failed to fetch procedure data"};
  }

  auto response = query_result.GetValue();

  for (auto const& row : response.rows) {
    auto const& columns = row.columns;

    if (columns.size() < 8) {
      LOG(ERROR)
          << "FetchBQProcedureData:: Unexpected column count in the response.";
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
    StatementHandle& stmt_handle, std::string const& project_id,
    std::string const& dataset_id, std::string const& procedures_filter) {
  std::vector<QueryParameter> named_query_params;
  QueryParameter param;
  param.name = "procedure_name";
  param.parameter_type.type = "STRING";
  param.parameter_value.value = procedures_filter;
  named_query_params.push_back(param);

  std::string query = R"(
  SELECT routine_name, routine_schema, routine_type
  FROM `)" + project_id +
                      "." + dataset_id + R"(.INFORMATION_SCHEMA.ROUTINES`
  WHERE routine_name LIKE @procedure_name
  AND routine_type IN ('PROCEDURE', 'FUNCTION' , 'TABLE FUNCTION')
  )";

  // Construct Post Query Request
  auto post_query_request_status = ConstructNamedParametersPostQueryRequest(
      project_id, dataset_id, query, named_query_params);

  if (!post_query_request_status) {
    LOG(ERROR)
        << "GetFilteredProcedures::ConstructNamedParametersPostQueryRequest:: "
        << post_query_request_status.GetStatusRecord().message;
    return post_query_request_status.GetStatusRecord();
  }

  // Fetch Data
  auto fetch_status_record_or =
      FetchBQData(stmt_handle, *post_query_request_status);
  if (!fetch_status_record_or) {
    LOG(ERROR) << "GetFilteredProcedures::FetchBQData:: "
               << fetch_status_record_or.GetStatusRecord().message;
    return fetch_status_record_or.GetStatusRecord();
  }

  StatusRecordOr<std::vector<RowData>> rows =
      GetRowsResults(*fetch_status_record_or);
  if (!rows) {
    LOG(ERROR) << "GetFilteredProcedures::GetRowsResults:: "
               << rows.GetStatusRecord().message;
    return rows.GetStatusRecord();
  }
  std::vector<FilteredProcedureResponse> procedure_response;
  for (auto const& row : *rows) {
    procedure_response.push_back({row.columns[0].value, row.columns[1].value});
  }

  return procedure_response;
}

StatusRecordOr<DSRow> CreateSQLProceduresResultSetDSRow(
    SQLProcedures const& procedure) {
  DSRow ds_row;

  // PROCEDURE_CAT
  DSValue ds_procedure_cat = kNullValue;
  if (!procedure.procedure_catalog.empty()) {
    StringToDSValue(procedure.procedure_catalog, ds_procedure_cat);
  }
  ds_row.emplace_back(ds_procedure_cat);

  // PROCEDURE_SCHEMA
  DSValue ds_procedure_schema = kNullValue;
  if (!procedure.procedure_schema.empty()) {
    StringToDSValue(procedure.procedure_schema, ds_procedure_schema);
  }
  ds_row.emplace_back(ds_procedure_schema);

  // PROCEDURE_NAME
  DSValue ds_procedure_name = kNullValue;
  if (!procedure.procedure_name.empty()) {
    StringToDSValue(procedure.procedure_name, ds_procedure_name);
  }
  ds_row.emplace_back(ds_procedure_name);

  DSValue ds_num_input_params = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(procedure.num_input_params,
                                 ds_num_input_params);
  ds_row.emplace_back(ds_num_input_params);

  // NUM_OUTPUT_PARAMS
  DSValue ds_num_output_params = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(procedure.num_output_params,
                                 ds_num_output_params);
  ds_row.emplace_back(ds_num_output_params);

  // NUM_RESULT_SETS
  DSValue ds_num_result_set = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(-1, ds_num_result_set);
  ds_row.emplace_back(ds_num_result_set);

  // REMARKS
  DSValue ds_remarks = kNullValue;
  if (!procedure.remarks.empty()) {
    StringToDSValue(procedure.remarks, ds_remarks);
  }
  ds_row.emplace_back(ds_remarks);

  // PROCEDURE_TYPE (Assumed to be SQL_PT_PROCEDURE as default)
  DSValue ds_procedure_type = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(procedure.procedure_type, ds_procedure_type);
  ds_row.emplace_back(ds_procedure_type);

  return ds_row;
}

StatusRecordOr<SQLProcedures> FetchBQSQLProcedureData(
    ConnectionHandle& conn_handle, std::string const& catalog,
    std::string const& dataset, std::string const& proc_name) {
  if (!conn_handle.IsConnected()) {
    LOG(ERROR)
        << "FetchBQSQLProcedureData:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR) << "FetchBQSQLProcedureData:: Invalid or null BQ Client.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  // Query to fetch procedure metadata
  std::string query =
      "SELECT * "
      "FROM `" +
      catalog + "." + dataset +
      ".INFORMATION_SCHEMA.ROUTINES` "
      "WHERE routine_name = '" +
      proc_name + "' ";

  QueryRequest query_request;
  query_request.set_query(query);

  auto query_result = bq_client->Query(catalog, query_request, {});
  if (!query_result.Ok()) {
    LOG(ERROR) << "FetchBQSQLProcedureData:: Failed to fetch procedure data";

    return StatusRecord{SQLStates::k_HY000(), "Failed to fetch procedure data"};
  }
  SQLProcedures procedure;

  auto response = query_result.GetValue();
  if (response.rows.empty()) {
    return procedure;
  }

  // Query to fetch input and output parameter counts
  query =
      "SELECT "
      "    SUM(CASE WHEN parameter_mode = 'IN' OR parameter_mode = 'INOUT' OR "
      "parameter_mode IS NULL THEN 1 ELSE 0 END) AS num_input_params, "
      "    SUM(CASE WHEN parameter_mode = 'OUT' OR parameter_mode = 'INOUT' "
      "THEN 1 ELSE 0 END) AS num_output_params "
      "FROM `" +
      catalog + "." + dataset +
      ".INFORMATION_SCHEMA.PARAMETERS` "
      "WHERE specific_name = '" +
      proc_name + "';";

  query_request.set_query(query);

  auto query_result2 = bq_client->Query(catalog, query_request, {});
  if (!query_result2.Ok()) {
    LOG(ERROR) << "FetchBQSQLProcedureData:: Failed to fetch procedure "
                  "parameter data.";
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to fetch procedure parameter data"};
  }

  auto response_val = query_result2.GetValue();
  if (response_val.rows.empty()) {
    LOG(ERROR) << "FetchBQSQLProcedureData:: No parameter data found.";
    return StatusRecord{SQLStates::k_HY000(), "No parameter data found"};
  }

  auto& row = response.rows[0];
  auto& columns = row.columns;

  if (columns.size() < 4) {
    LOG(ERROR) << "FetchBQSQLProcedureData:: Unexpected column count in "
                  "procedure response.";
    return StatusRecord{SQLStates::k_HY000(),
                        "Unexpected column count in procedure response"};
  }

  procedure.procedure_catalog = columns[0].value;  // catalog_name
  procedure.procedure_schema = columns[1].value;   // schema_name
  procedure.procedure_name = columns[2].value;     // routine_name
  procedure.remarks =
      columns[8].value;  // routine_type (or other remark column)
  procedure.procedure_type = SQL_PT_UNKNOWN;  // Default to UNKNOWN

  if (columns[6].value == "PROCEDURE") {
    procedure.procedure_type = SQL_PT_PROCEDURE;
  } else if (columns[6].value == "FUNCTION") {
    procedure.procedure_type = SQL_PT_FUNCTION;
  }

  // Extract input/output parameter counts
  auto& param_row = response_val.rows[0];
  auto& param_columns = param_row.columns;

  if (param_columns.size() < 2) {
    LOG(ERROR) << "FetchBQSQLProcedureData:: Unexpected column count in "
                  "parameter response.";
    return StatusRecord{SQLStates::k_HY000(),
                        "Unexpected column count in parameter response"};
  }

  if (procedure.procedure_type == SQL_PT_FUNCTION) {
    procedure.num_input_params = std::stoi(param_columns[0].value) - 1;
  } else {
    procedure.num_input_params = std::stoi(param_columns[0].value);
  }

  procedure.num_output_params = std::stoi(param_columns[1].value);

  return procedure;
}

static std::map<std::string, ColumnSchema> const kCommonProcedureFields = {
    {"PROCEDURE_CAT", ColumnSchema{0, BQDataType::kString}},
    {"PROCEDURE_SCHEMA", ColumnSchema{1, BQDataType::kString}},
    {"PROCEDURE_NAME", ColumnSchema{2, BQDataType::kString}},
};

static std::map<std::string, ColumnSchema> const kODBCProceduresColumnsMap =
    [] {
      std::map<std::string, ColumnSchema> map = kCommonProcedureFields;
      map.insert({
          {"NUM_INPUT_PARAMS", ColumnSchema{3, BQDataType::kInt64}},
          {"NUM_OUTPUT_PARAMS", ColumnSchema{4, BQDataType::kInt64}},
          {"NUM_RESULT_COLS", ColumnSchema{5, BQDataType::kInt64}},
          {"REMARKS", ColumnSchema{6, BQDataType::kString}},
          {"PROCEDURE_TYPE", ColumnSchema{7, BQDataType::kInt64}},
      });
      return map;
    }();

StatusRecordOr<ColumnSchema> GetProcedureSchema(std::string const& proc_name) {
  auto map_item = kODBCProceduresColumnsMap.find(proc_name);
  if (map_item != kODBCProceduresColumnsMap.end()) {
    return map_item->second;
  }
  LOG(ERROR) << "GetProcedureSchema:: Invalid column name: " << proc_name;
  return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                     "Invalid column name: " + proc_name};
}

StatusRecord CreateSQLProcedureResultSetRowSchema(ResultSet& result_set) {
  for (auto const& entry : kODBCProceduresColumnsMap) {
    auto col_schema_status = GetProcedureSchema(entry.first);
    if (!col_schema_status) {
      return col_schema_status.GetStatusRecord();
    }
    result_set.row_schema.emplace_back(*col_schema_status);
  }
  return StatusRecord::Ok();
}

StatusRecordOr<ResultSet> ProcessProcedures(
    std::vector<SQLProcedures> const& bq_procedure) {
  ResultSet result_set;

  // Create schema for the result set
  auto row_schema_status = CreateSQLProcedureResultSetRowSchema(result_set);
  if (!row_schema_status.ok()) {
    return row_schema_status;
  }

  if (bq_procedure.empty()) {
    return result_set;
  }

  int ord_pos = 1;
  auto ds_row_status = CreateSQLProceduresResultSetDSRow(bq_procedure.front());
  if (!ds_row_status) {
    return ds_row_status.GetStatusRecord();
  }

  result_set.rows.emplace_back(*ds_row_status);

  return result_set;
}

template <typename ProcedureType>
StatusRecordOr<std::vector<ProcedureType>> FetchProceduresData(
    StatementHandle& stmt_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id,
    std::function<
        StatusRecordOr<ProcedureType>(ConnectionHandle&, std::string const&,
                                      std::string const&, std::string const&)>
        fetch_procedure_fn) {
  std::vector<ProcedureType> result;
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    LOG(ERROR)
        << "FetchProceduresData:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR) << "FetchProceduresData:: Invalid or null BQ Client within the "
                  "connection handle.";
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
        GetFilteredProcedures(stmt_handle, catalog, dataset, procedure_pattern);
    if (!procedure_status) {
      return procedure_status.GetStatusRecord();
    }

    for (auto const& filtered_proc : *procedure_status) {
      StatusRecordOr<ProcedureType> procedure = fetch_procedure_fn(
          conn_handle, catalog, dataset, filtered_proc.proc_name);
      if (!procedure) {
        return procedure.GetStatusRecord();
      }
      result.emplace_back(*procedure);
    }
  }

  return result;
}

StatusRecordOr<std::vector<SQLProcedures>> FetchBQSQLProceduresData(
    StatementHandle& stmt_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id) {
  return FetchProceduresData<SQLProcedures>(
      stmt_handle, catalog, dataset_pattern, procedure_pattern, metadata_id,
      [](ConnectionHandle& handle, std::string const& cat,
         std::string const& ds, std::string const& proc) {
        return FetchBQSQLProcedureData(handle, cat, ds, proc);
      });
}

StatusRecordOr<std::vector<Procedure>> FetchBQProceduresData(
    StatementHandle& stmt_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id) {
  return FetchProceduresData<Procedure>(
      stmt_handle, catalog, dataset_pattern, procedure_pattern, metadata_id,
      [&](ConnectionHandle& handle, std::string const& cat,
          std::string const& ds, std::string const& proc) {
        StatusRecordOr<Procedure> validated_proc =
            ValidateProcedureColumnParameters(
                reinterpret_cast<const SQLCHAR*>(cat.c_str()),
                static_cast<SQLSMALLINT>(cat.length()),
                reinterpret_cast<const SQLCHAR*>(ds.c_str()),
                static_cast<SQLSMALLINT>(ds.length()),
                reinterpret_cast<const SQLCHAR*>(proc.c_str()),
                static_cast<SQLSMALLINT>(proc.length()), metadata_id);

        if (!validated_proc) {
          return validated_proc;
        }

        return FetchBQProcedureData(handle, *validated_proc);
      });
}

StatusRecordOr<DSRow> CreateProcedureColumnResultSetDSRow(
    ProcedureFieldSchema const& proc_column) {
  DSRow ds_row;

  // PROCEDURE_CAT
  DSValue ds_procedure_cat = kNullValue;
  if (!proc_column.catalog.empty()) {
    StringToDSValue(proc_column.catalog, ds_procedure_cat);
  }
  ds_row.emplace_back(ds_procedure_cat);

  // PROCEDURE_SCHEMA
  DSValue ds_procedure_schema = kNullValue;
  if (!proc_column.dataset.empty()) {
    StringToDSValue(proc_column.dataset, ds_procedure_schema);
  }
  ds_row.emplace_back(ds_procedure_schema);

  // PROCEDURE_NAME
  DSValue ds_procedure_name = kNullValue;
  if (!proc_column.procedure.empty()) {
    StringToDSValue(proc_column.procedure, ds_procedure_name);
  }
  ds_row.emplace_back(ds_procedure_name);

  // COLUMN_NAME
  DSValue ds_column_name = kNullValue;
  if (!proc_column.name.empty()) {
    StringToDSValue(proc_column.name, ds_column_name);
  }
  ds_row.emplace_back(ds_column_name);

  // COLUMN_TYPE
  DSValue ds_column_type;
  std::string value;
  if (proc_column.column_type == "OUT") {
    value = "4";
  } else if (proc_column.column_type == "INOUT") {
    value = "2";
  } else {
    value = "1";
  }
  SQLBIGINT bigint_value = std::stoll(value);
  ArithmeticToDSValue<SQLBIGINT>(bigint_value, ds_column_type);
  ds_row.emplace_back(ds_column_type);

  // DATA_TYPE
  DSValue ds_data_type = kNullValue;
  auto data_type_status = GetSQLDataType(proc_column.type_name, false);
  if (!data_type_status) {
    return data_type_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> data_type = data_type_status.GetValue();
  if (data_type.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*data_type),
                                   ds_data_type);
  }
  ds_row.emplace_back(ds_data_type);

  // TYPE_NAME
  DSValue ds_type_name = kNullValue;
  auto type_status = GetTypeDescription(proc_column.type_name);
  if (!type_status) {
    return type_status.GetStatusRecord();
  }
  std::string type_name = *type_status;
  if (!type_name.empty()) {
    StringToDSValue(type_name, ds_type_name);
  }
  ds_row.emplace_back(ds_type_name);

  auto fixed_col_status = GetFixedColumnMetadata(proc_column.type_name);
  if (!fixed_col_status.Ok()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to retrieve fixed column metadata"};
  }
  FixedColumnMetadata fixed_column_metadata = *fixed_col_status;

  // COLUMN_SIZE
  DSValue ds_col_size = kNullValue;
  if (fixed_column_metadata.precision.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.precision.value_or(0)),
        ds_col_size);
  }
  ds_row.emplace_back(ds_col_size);

  // BUFFER_LENGTH
  DSValue ds_buf_len = kNullValue;
  if (fixed_column_metadata.buf_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.buf_len.value_or(0)),
        ds_buf_len);
  }
  ds_row.emplace_back(ds_buf_len);

  // DECIMAL_DIGITS
  DSValue ds_dec_digits = kNullValue;
  if (fixed_column_metadata.scale.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.scale.value_or(0)),
        ds_dec_digits);
  }
  ds_row.emplace_back(ds_dec_digits);

  // NUM_PREC_RADIX
  DSValue ds_radix = kNullValue;
  if (fixed_column_metadata.radix.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.radix.value_or(0)),
        ds_radix);
  }
  ds_row.emplace_back(ds_radix);

  // NULLABLE
  DSValue ds_nullable;
  if (proc_column.nullable == "YES") {
    ArithmeticToDSValue<SQLBIGINT>(1, ds_nullable);
  } else {
    ArithmeticToDSValue<SQLBIGINT>(0, ds_nullable);
  }
  ds_row.emplace_back(ds_nullable);

  // REMARKS
  DSValue ds_remarks = kNullValue;
  ds_row.emplace_back(ds_remarks);

  // COLUMN_DEF
  DSValue column_def = kNullValue;
  ds_row.emplace_back(column_def);

  // SQL_DATA_TYPE
  DSValue ds_sql_data_type = kNullValue;
  optional<SQLSMALLINT> sql_data_type;
  if (data_type.has_value()) {
    auto sql_data_type_status = GetSQLDataType(*data_type);
    if (!sql_data_type_status) {
      return sql_data_type_status.GetStatusRecord();
    }
    sql_data_type = *sql_data_type_status;
    if (sql_data_type.has_value()) {
      ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_data_type),
                                     ds_sql_data_type);
    }
  }
  ds_row.emplace_back(ds_sql_data_type);

  // SQL_DATETIME_SUB
  DSValue ds_sql_datetime_sub = kNullValue;
  if (sql_data_type.has_value() && data_type.has_value()) {
    auto sql_data_time_sub_status =
        GetSQLDateTimeSub(*sql_data_type, *data_type);
    if (!sql_data_time_sub_status) {
      return sql_data_time_sub_status.GetStatusRecord();
    }
    optional<SQLSMALLINT> sql_date_time_sub = *sql_data_time_sub_status;
    if (sql_date_time_sub.has_value()) {
      ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_date_time_sub),
                                     ds_sql_datetime_sub);
    }
  }
  ds_row.emplace_back(ds_sql_datetime_sub);

  // CHAR_OCTET_LENGTH
  DSValue ds_char_octet_len = kNullValue;
  if (proc_column.column_type == "OUT") {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(16384),
                                   ds_char_octet_len);
  } else if (fixed_column_metadata.char_octet_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(
            fixed_column_metadata.char_octet_len.value_or(0)),
        ds_char_octet_len);
  }
  ds_row.emplace_back(ds_char_octet_len);

  // ORDINAL_POSITION
  DSValue ds_ord_pos = kNullValue;
  if (std::stoll(proc_column.ordinal_number) < 0) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid ordinal position"};
  }
  ArithmeticToDSValue<SQLBIGINT>(std::stoll(proc_column.ordinal_number),
                                 ds_ord_pos);
  ds_row.emplace_back(ds_ord_pos);

  // IS_NULLABLE
  DSValue ds_is_nullable;
  std::string is_nullable = proc_column.nullable;
  StringToDSValue(is_nullable, ds_is_nullable);
  ds_row.emplace_back(ds_is_nullable);

  return ds_row;
}

static std::map<std::string, ColumnSchema> const kODBCProcedureColumnsMap = {
    {"PROCEDURE_CAT",
     ColumnSchema{0, BQDataType::kString}},  // Procedure catalog
    {"PROCEDURE_SCHEMA",
     ColumnSchema{1, BQDataType::kString}},  // Procedure schema
    {"PROCEDURE_NAME", ColumnSchema{2, BQDataType::kString}},  // Procedure name
    {"COLUMN_NAME", ColumnSchema{3, BQDataType::kString}},     // Column name
    {"COLUMN_TYPE",
     ColumnSchema{4, BQDataType::kInt64}},  // Column type (input, output, etc.)
    {"DATA_TYPE", ColumnSchema{5, BQDataType::kInt64}},   // SQL data type
    {"TYPE_NAME", ColumnSchema{6, BQDataType::kString}},  // Type name
    {"COLUMN_SIZE",
     ColumnSchema{7, BQDataType::kInt64}},  // Column size (precision)
    {"BUFFER_LENGTH", ColumnSchema{8, BQDataType::kInt64}},   // Buffer length
    {"DECIMAL_DIGITS", ColumnSchema{9, BQDataType::kInt64}},  // Decimal digits
    {"NUM_PREC_RADIX",
     ColumnSchema{10, BQDataType::kInt64}},  // Numeric precision radix
    {"NULLABLE", ColumnSchema{11, BQDataType::kInt64}},  // Nullable flag
    {"REMARKS", ColumnSchema{12, BQDataType::kString}},  // Remarks/comments
    {"COLUMN_DEF",
     ColumnSchema{13, BQDataType::kString}},  // Column default value
    {"SQL_DATA_TYPE",
     ColumnSchema{14,
                  BQDataType::kInt64}},  // SQL data type (same as DATA_TYPE)
    {"SQL_DATETIME_SUB",
     ColumnSchema{15, BQDataType::kInt64}},  // Date/time subtype
    {"CHAR_OCTET_LENGTH",
     ColumnSchema{16, BQDataType::kInt64}},  // Character octet length
    {"ORDINAL_POSITION",
     ColumnSchema{17, BQDataType::kInt64}},  // Ordinal position
    {"IS_NULLABLE",
     ColumnSchema{18, BQDataType::kString}}  // "YES", "NO", or "UNKNOWN"
};

StatusRecordOr<ColumnSchema> GetProcedureColumnSchema(
    std::string const& col_name) {
  auto map_item = kODBCProcedureColumnsMap.find(col_name);
  if (map_item != kODBCProcedureColumnsMap.end()) {
    return map_item->second;
  }
  LOG(ERROR) << "GetProcedureColumnSchema:: Invalid column name: " << col_name;
  return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                     "Invalid column name: " + col_name};
}

StatusRecord CreateProcedureColumnResultSetRowSchema(ResultSet& result_set) {
  for (auto const& entry : kODBCProcedureColumnsMap) {
    auto col_schema_status = GetProcedureColumnSchema(entry.first);
    if (!col_schema_status) {
      return col_schema_status.GetStatusRecord();
    }
    result_set.row_schema.emplace_back(*col_schema_status);
  }
  return StatusRecord::Ok();
}

StatusRecordOr<ResultSet> ProcessProcedureColumnResults(
    Procedure const& bq_procedure, std::string const& bq_procedure_column,
    SQLULEN metadata_id) {
  ResultSet result_set;

  auto row_schema_status = CreateProcedureColumnResultSetRowSchema(result_set);
  if (!row_schema_status.ok()) {
    return row_schema_status;
  }

  if (!metadata_id &&
      (bq_procedure_column.empty() || bq_procedure_column == "%")) {
    int ord_pos = 1;
    for (auto const& procedure_field : bq_procedure.schema.fields) {
      auto ds_row_status = CreateProcedureColumnResultSetDSRow(procedure_field);
      if (!ds_row_status) {
        return ds_row_status.GetStatusRecord();
      }
      result_set.rows.emplace_back(*ds_row_status);
    }
  } else {
    int ord_pos = 1;
    for (auto const& procedure_field : bq_procedure.schema.fields) {
      std::regex column_pattern = BuildRegex(bq_procedure_column, metadata_id);
      if (std::regex_match(procedure_field.name, column_pattern)) {
        auto ds_row_status =
            CreateProcedureColumnResultSetDSRow(procedure_field);
        if (!ds_row_status) {
          return ds_row_status.GetStatusRecord();
        }
        result_set.rows.emplace_back(*ds_row_status);
        break;
      }
      ord_pos++;
    }
  }
  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
