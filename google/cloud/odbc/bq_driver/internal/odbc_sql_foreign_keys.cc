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
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "absl/strings/match.h"
#include <algorithm>
#include <string>
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::ColumnReference;
using ::google::cloud::bigquery_v2_minimal_internal::ForeignKey;
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
    "CAST(NULL AS INT64) AS update_rule, "
    "CAST(NULL AS INT64) AS delete_rule, "
    "key_column_usage.constraint_name as fk_name, "
    "pk_references.pk_name, "
    "CAST(2 AS INT64) AS deferrability "
    "FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE key_column_usage "
    "JOIN pk_references "
    "ON pk_references.fk_constraint_name = key_column_usage.constraint_name "
    "AND pk_references.fk_constraint_schema = "
    "key_column_usage.constraint_schema "
    "AND pk_references.pk_column_ordinal_position = "
    "key_column_usage.POSITION_IN_UNIQUE_CONSTRAINT ";

std::string const kBasicForeignKeysQuerySuffix =
    "ORDER BY pk_table, pk_column_ordinal_position, pk_column";

// BigQuery INFORMATION_SCHEMA reports constraint names as
// "<table_id>.<constraint_name>", where the primary key constraint name is
// always "pk$" (BigQuery primary keys cannot be named) and an unnamed foreign
// key constraint is named "fk$<n>" by order of declaration, which is the order
// tables.get lists them in. These suffixes let the tables.get based path below
// produce the same constraint names as the INFORMATION_SCHEMA query path.
std::string const kPrimaryKeyNameSuffix = ".pk$";
std::string const kForeignKeyNameSuffix = ".fk$";

// Fetches the foreign keys of a single, exactly named table by reading the
// table's constraints via the tables.get REST API, the same way
// FetchPKResultSetFromTableMetaData does for SQLPrimaryKeys. This avoids the
// several seconds of fixed latency an INFORMATION_SCHEMA query job carries
// (table names are ordinary identifiers, not search patterns, per the ODBC
// spec). When pk_table_name is non-empty, only foreign keys referencing that
// table are returned. The rows are returned as a ready ResultSet inside
// DSResults, which ProcessQueryResults passes through unchanged.
StatusRecordOr<DSResults> FetchForeignKeysFromTableMetadata(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    std::string const& schema_name, std::string const& pk_table_name,
    std::string const& fk_table_name) {
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  auto bq_table_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, fk_table_name);
  ResultSet result_set;
  result_set.row_schema.resize(kForeignKeysMap.size());
  for (auto const& [_, schema] : kForeignKeysMap) {
    result_set.row_schema[schema.col_index] = schema;
  }
  if (!bq_table_status) {
    auto const& status = bq_table_status.GetStatusRecord();
    // An unknown table produces an empty result set, matching the
    // INFORMATION_SCHEMA query path.
    if (status.native_error_code == 404) {
      LOG(INFO) << "FetchForeignKeysFromTableMetadata:: Table not found: '"
                << catalog_name << "." << schema_name << "." << fk_table_name
                << "'";
      DSResults ds_results;
      ds_results.data_source_results = std::move(result_set);
      return ds_results;
    }
    LOG(ERROR) << "FetchForeignKeysFromTableMetadata::FetchBQTableData:: "
               << status.message;
    stmt_handle.GetDiagnostics().AddStatusRecord(status);
    return status;
  }
  // Ordinal of the foreign key within the table's constraints, which is what
  // BigQuery numbers the unnamed ones by. Counted over all of them, not just
  // the ones kept by the pk_table_name filter below.
  int fk_ordinal = 0;
  for (ForeignKey const& foreign_key :
       bq_table_status->table_constraints.foreign_keys) {
    ++fk_ordinal;
    if (!pk_table_name.empty() &&
        foreign_key.referenced_table.table_id != pk_table_name) {
      continue;
    }
    // INFORMATION_SCHEMA reports constraint names prefixed with the table id,
    // e.g. "my_table.fk$1" / "my_table.my_named_fk"; reproduce that. The name
    // of an explicitly named constraint is not available here: tables.get
    // reports it in "name", but google-cloud-cpp parses it from "keyName", so
    // ForeignKey::key_name always arrives empty and such a constraint is
    // reported as "fk$<n>" instead of its declared name. key_name is still
    // preferred when present, so this corrects itself if the dependency does.
    DSValue fk_name_value;
    if (foreign_key.key_name.empty()) {
      StringToDSValue(
          fk_table_name + kForeignKeyNameSuffix + std::to_string(fk_ordinal),
          fk_name_value);
    } else {
      StringToDSValue(fk_table_name + "." + foreign_key.key_name,
                      fk_name_value);
    }
    DSValue pk_name_value;
    StringToDSValue(
        foreign_key.referenced_table.table_id + kPrimaryKeyNameSuffix,
        pk_name_value);
    SQLBIGINT key_seq = 0;
    for (ColumnReference const& column_ref : foreign_key.column_references) {
      ++key_seq;
      DSRow row(kForeignKeysMap.size());
      StringToDSValue(foreign_key.referenced_table.project_id,
                      row[0]);  // PKTABLE_CAT
      StringToDSValue(foreign_key.referenced_table.dataset_id,
                      row[1]);  // PKTABLE_SCHEM
      StringToDSValue(foreign_key.referenced_table.table_id,
                      row[2]);                                 // PKTABLE_NAME
      StringToDSValue(column_ref.referenced_column, row[3]);   // PKCOLUMN_NAME
      StringToDSValue(catalog_name, row[4]);                   // FKTABLE_CAT
      StringToDSValue(schema_name, row[5]);                    // FKTABLE_SCHEM
      StringToDSValue(fk_table_name, row[6]);                  // FKTABLE_NAME
      StringToDSValue(column_ref.referencing_column, row[7]);  // FKCOLUMN_NAME
      ArithmeticToDSValue<SQLBIGINT>(key_seq, row[8]);         // KEY_SEQ
      row[9] = kNullValue;                                     // UPDATE_RULE
      row[10] = kNullValue;                                    // DELETE_RULE
      row[11] = fk_name_value;                                 // FK_NAME
      row[12] = pk_name_value;                                 // PK_NAME
      ArithmeticToDSValue<SQLBIGINT>(2, row[13]);              // DEFERRABILITY
      result_set.rows.push_back(std::move(row));
    }
  }
  // Match the query path ordering: PKTABLE_NAME, then key sequence.
  std::stable_sort(result_set.rows.begin(), result_set.rows.end(),
                   [](DSRow const& a, DSRow const& b) {
                     if (a[2] != b[2]) return a[2] < b[2];
                     DSValue seq_a = a[8];
                     DSValue seq_b = b[8];
                     return DSValueToInt(seq_a) < DSValueToInt(seq_b);
                   });
  DSResults ds_results;
  ds_results.data_source_results = std::move(result_set);
  return ds_results;
}

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
  // Fast path: the foreign key table is specified with an exact (non-pattern)
  // name — read that table's constraints directly via the tables.get REST API
  // instead of running an INFORMATION_SCHEMA query job. This covers both the
  // FK-only and the PK+FK cases; the PK-only case cannot use it because the
  // referencing tables are not known upfront. Names containing '%' fall back
  // to the query below, which preserves the historical LIKE matching.
  if (!fk_table_name.empty() && !absl::StrContains(fk_table_name, '%') &&
      !absl::StrContains(pk_table_name, '%')) {
    return FetchForeignKeysFromTableMetadata(
        stmt_handle, catalog_name, schema_name, pk_table_name, fk_table_name);
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
  auto status_record_or = FetchBQData(stmt_handle, *post_query_request_status);
  if (!status_record_or) {
    LOG(ERROR) << "FetchForeignKeysFromDataSource::FetchBQData:: "
               << status_record_or.GetStatusRecord().message;
    stmt_handle.GetDiagnostics().AddStatusRecord(
        status_record_or.GetStatusRecord());
  }
  return status_record_or;
}

}  // namespace google::cloud::odbc_bq_driver_internal
