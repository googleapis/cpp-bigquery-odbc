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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_foreign_keys.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {
std::string const kNamedCatalogParam = "catalog_name";
std::string const kNamedSchemaParam = "schema_name";
std::string const kNamedPKTableParam = "pk_table_name";
std::string const kNamedFKTableParam = "fk_table_name";

std::string const kBasicForeignKeysQueryPrefix =
    "WITH pk_constraint AS ( "
    "SELECT key_column_usage.constraint_catalog as pk_catalog,"
    "key_column_usage.constraint_schema as pk_dataset, "
    "key_column_usage.table_name as pk_table, "
    "key_column_usage.column_name as pk_column, "
    "key_column_usage.constraint_name as pk_name, "
    "key_column_usage.ordinal_position as pk_column_ordinal_position "
    "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE key_column_usage "
    "INNER JOIN INFORMATION_SCHEMA.TABLE_CONSTRAINTS table_constraints "
    "ON  table_constraints.table_name = key_column_usage.table_name "
    "AND table_constraints.constraint_name = key_column_usage.constraint_name "
    "AND table_constraints.constraint_schema = "
    "key_column_usage.constraint_schema "
    "WHERE table_constraints.CONSTRAINT_TYPE = 'PRIMARY KEY' "
    "), "
    "pk_references AS ( "
    "SELECT pk_constraint.*, "
    "constraints_column_usage.constraint_schema as fk_constraint_schema, "
    "constraints_column_usage.constraint_name as fk_constraint_name "
    "FROM pk_constraint "
    "JOIN INFORMATION_SCHEMA.CONSTRAINT_COLUMN_USAGE constraints_column_usage "
    "ON true "
    "AND pk_constraint.pk_table = constraints_column_usage.table_name "
    "AND pk_constraint.pk_column = constraints_column_usage.column_name "
    "AND pk_constraint.pk_dataset = constraints_column_usage.TABLE_SCHEMA "
    ") "
    "SELECT pk_references.pk_catalog, "
    "pk_references.pk_dataset, "
    "pk_references.pk_table, "
    "pk_references.pk_column, "
    "key_column_usage.table_catalog as fk_catalog, "
    "key_column_usage.table_schema as fk_dataset, "
    "key_column_usage.table_name as fk_table, "
    "key_column_usage.column_name as fk_column, "
    "key_column_usage.ordinal_position as fk_column_ordinal_position, "
    "key_column_usage.constraint_name as fk_name, "
    "pk_references.pk_name "
    "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE key_column_usage "
    "JOIN pk_references "
    "ON pk_references.fk_constraint_name = key_column_usage.constraint_name "
    "AND pk_references.fk_constraint_schema = "
    "key_column_usage.constraint_schema "
    "AND pk_references.pk_column_ordinal_position = "
    "key_column_usage.POSITION_IN_UNIQUE_CONSTRAINT ";

std::string const kBasicForeignKeysQuerySuffix =
    "ORDER BY pk_table, pk_column_ordinal_position, pk_column";

}  // namespace

odbc_internal::StatusRecordOr<DSResults> FetchForeignKeysFromDataSource(
    StatementHandle& stmt_handle, std::string const& pk_catalog_name,
    int pk_catalog_name_len, std::string const& pk_schema_name,
    int pk_schema_name_len, std::string const& pk_table_name,
    int pk_table_name_len, std::string const& fk_catalog_name,
    int fk_catalog_name_len, std::string const& fk_schema_name,
    int fk_schema_name_len, std::string const& fk_table_name,
    int fk_table_name_len) {
  // Parameter validation.
  std::string catalog_name =
      (!pk_catalog_name.empty()) ? pk_catalog_name : fk_catalog_name;
  if (catalog_name.empty() ||
      (pk_catalog_name_len == 0 && fk_catalog_name_len == 0)) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource:: Catalog name for both "
                  "primary and foreign keys cannot be empty.";
    auto status_record =
        StatusRecord{SQLStates::k_HY090(),
                     "Catalog name for both primary and foreign keys "
                     "cannot be empty. One of them needs to be provided"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (!pk_catalog_name.empty() && !fk_catalog_name.empty() &&
      pk_catalog_name != fk_catalog_name) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource:: PK and FK catalog names "
                  "need to be the same.";
    auto status_record =
        StatusRecord{SQLStates::k_HYC00(),
                     "Optional feature not supported by the data source: PK "
                     "and FK catalog needs to be the same"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  std::string schema_name =
      (!pk_schema_name.empty()) ? pk_schema_name : fk_schema_name;
  if (schema_name.empty() ||
      (pk_schema_name_len == 0 && fk_schema_name_len == 0)) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource:: Schema name for both "
                  "primary and foreign keys cannot be empty.";
    auto status_record =
        StatusRecord{SQLStates::k_HY090(),
                     "Schema name for both primary and foreign keys "
                     "cannot be empty. One of them needs to be provided"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (!pk_schema_name.empty() && !fk_schema_name.empty() &&
      pk_schema_name != fk_schema_name) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource:: PK and FK schema names "
                  "need to be the same.";
    auto status_record =
        StatusRecord{SQLStates::k_HYC00(),
                     "Optional feature not supported by the data source: PK "
                     "and FK schema needs to be the same"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if ((pk_table_name.empty() && fk_table_name.empty()) ||
      (pk_table_name_len == 0 && fk_table_name_len == 0)) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource:: Both Primary and Foreign "
                  "key table names cannot be empty.";
    auto status_record = StatusRecord{
        SQLStates::k_HY009(),
        "Both Primary and Foreign key table names cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (stmt_handle.GetConnectionHandle() == nullptr) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource:: Connection handle is null.";
    auto status_record = StatusRecord{SQLStates::k_HY013(),
                                      "Internal connection handle is null"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  // Construct named query for foreign keys.
  std::string foreign_keys_query(kBasicForeignKeysQueryPrefix);
  foreign_keys_query
      .append(" AND pk_catalog = @")  // PrimaryKey catalog
      .append(kNamedCatalogParam)
      .append(" AND pk_dataset = @")  // PrimaryKey dataset
      .append(kNamedSchemaParam)
      .append(" AND key_column_usage.table_catalog = @")  // ForeignKey catalog
      .append(kNamedCatalogParam)
      .append(" AND key_column_usage.table_schema = @")  // ForeignKey dataset
      .append(kNamedSchemaParam);
  if (!pk_table_name.empty()) {
    foreign_keys_query.append(" AND pk_references.pk_table LIKE @");
    foreign_keys_query.append(kNamedPKTableParam);
  }
  if (!fk_table_name.empty()) {
    foreign_keys_query.append(" AND key_column_usage.table_name LIKE @");
    foreign_keys_query.append(kNamedFKTableParam);
  }
  foreign_keys_query.append(" ").append(kBasicForeignKeysQuerySuffix);
  // Construct named query params
  std::map<std::string, std::string> named_query_params;
  named_query_params.insert({kNamedCatalogParam, catalog_name});
  named_query_params.insert({kNamedSchemaParam, schema_name});
  if (!pk_table_name.empty()) {
    named_query_params.insert({kNamedPKTableParam, pk_table_name});
  }
  if (!fk_table_name.empty()) {
    named_query_params.insert({kNamedFKTableParam, fk_table_name});
  }
  auto query_param_status = ConstructStringQueryParameters(named_query_params);
  if (!query_param_status) {
    LOG(ERROR)
        << "FetchForeignKeysFromDataSource::ConstructStringQueryParameters:: "
        << query_param_status.GetStatusRecord().message;
    auto status_record = query_param_status.GetStatusRecord();
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  // Construct post query request.
  auto post_query_request_status = ConstructNamedParametersPostQueryRequest(
      catalog_name, schema_name, foreign_keys_query, *query_param_status);
  if (!post_query_request_status) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource::"
                  "ConstructNamedParametersPostQueryRequest:: "
               << post_query_request_status.GetStatusRecord().message;
    auto status_record = post_query_request_status.GetStatusRecord();
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  // Fetch BQ Data using the post query request above.
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  auto status_record_or = FetchBQData(conn_handle, *post_query_request_status);
  if (!status_record_or) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource::FetchBQData:: "
               << status_record_or.GetStatusRecord().message;
    stmt_handle.GetDiagnostics().AddStatusRecord(
        status_record_or.GetStatusRecord());
  }
  return status_record_or;
}

}  // namespace google::cloud::odbc_bq_driver_internal
