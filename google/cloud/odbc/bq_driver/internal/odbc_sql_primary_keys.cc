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
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Struct;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using ::google::cloud::bigquery_v2_minimal_internal::Value;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

std::string const kBasicPrimaryKeysQuery =
    "SELECT kc.table_catelog,"
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

namespace {}  // namespace

StatusRecordOr<DSResults> FetchPrimaryKeysFromDataSource(
    StatementHandle* stmt_handle, std::string const& catalog_name,
    int catalogNameLen, std::string const& schema_name, int schema_name_len,
    std::string const& table_name, int table_name_len) {
  // Input validation of required parameters.
  if (!stmt_handle) {
    return StatusRecord{SQLStates::k_HY013(),
                        "Parameter statement_handle cannot be null"};
  }
  if (catalog_name.empty() || catalogNameLen <= 0) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Parameter catelog_name cannot be empty"};
  }
  if (schema_name.empty() || schema_name_len <= 0) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Parameter schema_name cannot be empty"};
  }
  if (table_name.empty() || table_name_len <= 0) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Parameter table_name cannot be empty"};
  }
  // Construct post query request.
  std::string kPrimaryKeysQuery(kBasicPrimaryKeysQuery);
  kPrimaryKeysQuery.append(" AND kc.table_catelog = '")
      .append(catalog_name)
      .append("'")
      .append(" AND kc.table_schema = ")
      .append(schema_name)
      .append("'")
      .append(" AND kc.table_name = ")
      .append(table_name)
      .append("'");
  PostQueryRequest post_request;
  QueryRequest query_request;
  DatasetReference ds_ref;
  ds_ref.project_id = catalog_name;
  ds_ref.dataset_id = schema_name;
  query_request.set_dry_run(false);
  query_request.set_default_dataset(ds_ref);
  query_request.set_query(kPrimaryKeysQuery);
  query_request.set_use_legacy_sql(
      false);  // This is required for the query to execute successfully.
  post_request.set_project_id(catalog_name);
  post_request.set_query_request(query_request);
  // Fetch BQ Data using the query request above.
  return FetchBQData(stmt_handle->GetConnectionHandle(), post_request);
}

}  // namespace google::cloud::odbc_bq_driver_internal
