// Copyright 2024 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_primary_keys.h"
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::DatasetReference;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {
std::string const kNamedCatalogParam = "catalog_name";
std::string const kNamedSchemaParam = "schema_name";
std::string const kNamedTableParam = "table_name";

std::string const kBasicPrimaryKeysQuery =
    "SELECT kc.table_catalog,"
    " kc.table_schema,"
    " kc.table_name,"
    " kc.column_name,"
    " kc.ordinal_position,"
    " kc.constraint_name"
    " FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE as kc"
    " INNER JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS as tc"
    " ON kc.constraint_name = tc.constraint_name AND"
    " kc.table_catalog = tc.table_catalog AND"
    " kc.table_schema = tc.table_schema AND"
    " kc.table_name = tc.table_name "
    " WHERE tc.constraint_type = 'PRIMARY KEY'";
}  // namespace

StatusRecordOr<DSResults> FetchPrimaryKeysFromDataSource(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    int catalog_name_len, std::string const& schema_name, int schema_name_len,
    std::string const& table_name, int table_name_len) {
  // Input validation of required parameters.
  if (catalog_name.empty() || catalog_name_len <= 0) {
    auto status_record = StatusRecord{SQLStates::k_HY090(),
                                      "Parameter catalog_name cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (schema_name.empty() || schema_name_len <= 0) {
    auto status_record = StatusRecord{SQLStates::k_HY090(),
                                      "Parameter schema_name cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (table_name.empty() || table_name_len <= 0) {
    auto status_record = StatusRecord{SQLStates::k_HY090(),
                                      "Parameter table_name cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (stmt_handle.GetConnectionHandle() == nullptr) {
    auto status_record = StatusRecord{SQLStates::k_HY013(),
                                      "Internal connection handle is null"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  // Construct post query request.
  // TODO(b/339618125): Cleanse this query to avoid
  // potential SQL injection issues.
  std::string primary_keys_query(kBasicPrimaryKeysQuery);
  primary_keys_query.append(" AND kc.table_catalog = @")
      .append(kNamedCatalogParam)
      .append(" AND kc.table_schema = @")
      .append(kNamedSchemaParam)
      .append(" AND kc.table_name = @")
      .append(kNamedTableParam);
  PostQueryRequest post_request;
  QueryRequest query_request;
  DatasetReference ds_ref;
  std::map<std::string, std::string> named_query_params;
  named_query_params.insert({kNamedCatalogParam, catalog_name});
  named_query_params.insert({kNamedSchemaParam, schema_name});
  named_query_params.insert({kNamedTableParam, table_name});
  auto query_param_status = ConstructStringQueryParameters(named_query_params);
  if (!query_param_status) {
    return query_param_status.GetStatusRecord();
  }
  // Set dataset info.
  ds_ref.project_id = catalog_name;
  ds_ref.dataset_id = schema_name;
  // Construct query request.
  query_request.set_dry_run(false);
  query_request.set_default_dataset(ds_ref);
  query_request.set_query(primary_keys_query);
  // Following are specific to parametrized queries.
  query_request.set_parameter_mode("NAMED");
  query_request.set_query_parameters(*query_param_status);
  query_request.set_use_legacy_sql(false);
  // Set billing info and query request.
  post_request.set_project_id(catalog_name);
  post_request.set_query_request(query_request);

  // Fetch BQ Data using the query request above.
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  auto status_record_or = FetchBQData(conn_handle, post_request);
  if (!status_record_or) {
    stmt_handle.GetDiagnostics().AddStatusRecord(
        status_record_or.GetStatusRecord());
  }
  return status_record_or;
}

}  // namespace google::cloud::odbc_bq_driver_internal
