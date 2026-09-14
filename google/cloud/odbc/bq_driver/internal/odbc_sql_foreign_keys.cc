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
#include "google/cloud/odbc/bq_client_interface/utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_tables.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "absl/strings/match.h"
#include <algorithm>
#include <string>
#include <thread>
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::ColumnReference;
using ::google::cloud::bigquery_v2_minimal_internal::ForeignKey;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {

// BigQuery exposes constraint names in the form "<table_id>.<constraint_name>".
// Primary keys cannot be named explicitly in BigQuery, so they always use
// "pk$". Unnamed foreign keys receive synthetic names "fk$<ordinal>" based on
// declaration order.
std::string const kPrimaryKeyNameSuffix = ".pk$";
std::string const kForeignKeyNameSuffix = ".fk$";

// Converts BigQuery ForeignKey constraints into standard ODBC SQLForeignKeys
// result set rows.
//
// Arguments:
//   result_set:    Target result set receiving formatted row data.
//   foreign_key:   BigQuery foreign key definition containing referenced table
//   and columns. catalog_name:  Current project/catalog ID. schema_name:
//   Current dataset/schema ID. fk_table_name: Table ID containing the foreign
//   key constraint. fk_ordinal:    Ordinal index of the foreign key in the
//   table definition.
void AppendForeignKeyRows(ResultSet& result_set, ForeignKey const& foreign_key,
                          std::string const& catalog_name,
                          std::string const& schema_name,
                          std::string const& fk_table_name, int fk_ordinal) {
  // Format the foreign key constraint name to match standard BigQuery naming
  // conventions.
  DSValue fk_name_value;
  if (foreign_key.key_name.empty()) {
    StringToDSValue(
        fk_table_name + kForeignKeyNameSuffix + std::to_string(fk_ordinal),
        fk_name_value);
  } else {
    StringToDSValue(fk_table_name + "." + foreign_key.key_name, fk_name_value);
  }

  // Format the primary key constraint name.
  DSValue pk_name_value;
  StringToDSValue(foreign_key.referenced_table.table_id + kPrimaryKeyNameSuffix,
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

// Sorts SQLForeignKeys result rows to ensure deterministic result ordering.
// Matches the legacy INFORMATION_SCHEMA path ordering: PKTABLE_NAME ascending,
// then KEY_SEQ ascending.
void SortForeignKeyRows(ResultSet& result_set) {
  std::stable_sort(result_set.rows.begin(), result_set.rows.end(),
                   [](DSRow const& a, DSRow const& b) {
                     if (a[2] != b[2]) return a[2] < b[2];
                     DSValue seq_a = a[8];
                     DSValue seq_b = b[8];
                     return DSValueToInt(seq_a) < DSValueToInt(seq_b);
                   });
}

// FAST PATH: Used when an explicit foreign key table name (without wildcards
// '%') is provided. Fetches table metadata directly via `tables.get` without
// listing all tables in the dataset.
StatusRecordOr<DSResults> FetchForeignKeysFromTableMetadata(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    std::string const& schema_name, std::string const& pk_table_name,
    std::string const& fk_table_name) {
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());

  // Directly retrieve table metadata using driver helper `FetchBQTableData`.
  auto bq_table_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, fk_table_name);

  ResultSet result_set;
  result_set.row_schema.resize(kForeignKeysMap.size());
  for (auto const& [_, schema] : kForeignKeysMap) {
    result_set.row_schema[schema.col_index] = schema;
  }

  if (!bq_table_status) {
    auto const& status = bq_table_status.GetStatusRecord();
    // Non-existent tables return an empty result set per ODBC specification.
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

  int fk_ordinal = 0;
  for (ForeignKey const& foreign_key :
       bq_table_status->table_constraints.foreign_keys) {
    ++fk_ordinal;
    // Filter out foreign keys that do not match the requested primary key
    // table.
    if (!pk_table_name.empty() &&
        foreign_key.referenced_table.table_id != pk_table_name) {
      continue;
    }
    AppendForeignKeyRows(result_set, foreign_key, catalog_name, schema_name,
                         fk_table_name, fk_ordinal);
  }
  SortForeignKeyRows(result_set);

  DSResults ds_results;
  ds_results.data_source_results = std::move(result_set);
  return ds_results;
}

// MULTI-THREADED FALLBACK PATH: Used when `fk_table_name` is empty or contains
// wildcard characters. Lists matching tables in the dataset via `tables.list`,
// then fetches metadata in parallel across thread workers.
StatusRecordOr<DSResults> FetchForeignKeysByListingTables(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    std::string const& schema_name, std::string const& pk_table_name,
    std::string const& fk_table_name) {
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());

  if (!conn_handle.IsConnected()) {
    LOG(ERROR)
        << "FetchForeignKeysByListingTables:: Connection to the data source "
           "is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR) << "FetchForeignKeysByListingTables:: Invalid or null BQ Client "
                  "within the connection handle.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  ResultSet result_set;
  result_set.row_schema.resize(kForeignKeysMap.size());
  for (auto const& [_, schema] : kForeignKeysMap) {
    result_set.row_schema[schema.col_index] = schema;
  }

  int const max_retries = conn_handle.GetDsn().max_retries;
  std::string const tables_filter =
      fk_table_name.empty() ? kMatchAll : fk_table_name;

  // Enumerate candidate tables in the target dataset.
  auto tables_status =
      GetFilteredTables(*bq_client, catalog_name, schema_name, tables_filter,
                        "TABLE", SQL_FALSE, max_retries);

  if (!tables_status) {
    return tables_status.GetStatusRecord();
  }

  // Determine thread pool concurrency limit based on DSN trace settings or
  // hardware concurrency.
  std::uint32_t max_threads = 1U;
  auto trace_option = TraceOptions::GetTraceOption();
  if (trace_option != nullptr && trace_option->max_threads > 0) {
    max_threads = static_cast<std::uint32_t>(trace_option->max_threads);
  } else {
    max_threads = std::max(1U, std::thread::hardware_concurrency());
  }

  struct ForeignKeyTableResult {
    std::string table_name;
    Table table;
  };

  // Task lambda passed to `ExecuteParallelTasks`. Wraps `FetchBQTableData` to:
  // 1. Capture required catalog and schema parameters from outer scope.
  // 2. Handle HTTP 404 gracefully (if a table is deleted mid-execution).
  auto fetch_table_task = [&conn_handle, &catalog_name,
                           &schema_name](FilteredTableResponse const& table)
      -> StatusRecordOr<std::optional<ForeignKeyTableResult>> {
    auto bq_table_status = FetchBQTableData(conn_handle, catalog_name,
                                            schema_name, table.table_name);

    if (!bq_table_status) {
      auto const& status = bq_table_status.GetStatusRecord();
      // If a table is concurrently dropped after `tables.list`, skip it without
      // failing the task batch.
      if (status.native_error_code == 404) {
        return std::optional<ForeignKeyTableResult>{};
      }
      LOG(ERROR) << "FetchForeignKeysByListingTables::FetchBQTableData:: "
                 << status.message;
      return status;
    }

    return std::optional<ForeignKeyTableResult>(
        ForeignKeyTableResult{table.table_name, std::move(*bq_table_status)});
  };

  // Execute table metadata fetches concurrently using the shared thread
  // manager.
  auto table_results_or =
      ExecuteParallelTasks<FilteredTableResponse,
                           std::optional<ForeignKeyTableResult>>(
          max_threads, *tables_status, fetch_table_task);

  if (!table_results_or) {
    auto const& status = table_results_or.GetStatusRecord();
    stmt_handle.GetDiagnostics().AddStatusRecord(status);
    return status;
  }

  std::unique_ptr<re2::RE2> pk_table_pattern;
  if (!pk_table_name.empty()) {
    pk_table_pattern = BuildRegex(pk_table_name, SQL_FALSE);
  }
  // Process parallel task results sequentially and build final result set rows.
  for (auto& maybe_table : *table_results_or) {
    if (!maybe_table.has_value()) {
      continue;
    }

    auto& table_result = *maybe_table;
    int fk_ordinal = 0;

    for (ForeignKey const& foreign_key :
         table_result.table.table_constraints.foreign_keys) {
      ++fk_ordinal;

      // Apply primary key table filtering if `pk_table_name` was specified.
      if (!pk_table_name.empty() &&
          !re2::RE2::FullMatch(foreign_key.referenced_table.table_id,
                               *pk_table_pattern)) {
        continue;
      }

      // Verify catalog and schema scope match the foreign key definition.
      if (foreign_key.referenced_table.project_id != catalog_name ||
          foreign_key.referenced_table.dataset_id != schema_name) {
        continue;
      }

      AppendForeignKeyRows(result_set, foreign_key, catalog_name, schema_name,
                           table_result.table_name, fk_ordinal);
    }
  }

  SortForeignKeyRows(result_set);

  DSResults ds_results;
  ds_results.data_source_results = std::move(result_set);
  return ds_results;
}

}  // namespace

// Main entry point for `SQLForeignKeys`.
odbc_internal::StatusRecordOr<DSResults> FetchForeignKeysFromDataSource(
    StatementHandle& stmt_handle, std::string const& pk_catalog_name,
    int pk_catalog_name_len, std::string const& pk_schema_name,
    int pk_schema_name_len, std::string const& pk_table_name,
    int pk_table_name_len, std::string const& fk_catalog_name,
    int fk_catalog_name_len, std::string const& fk_schema_name,
    int fk_schema_name_len, std::string const& fk_table_name,
    int fk_table_name_len) {
  // 1. Parameter Validation & Fallbacks.
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

  // 2. Query Routing:
  // If `fk_table_name` is non-empty and contains no wildcard '%' characters,
  // use the single-table fast path. Otherwise, fall back to parallel dataset
  // table enumeration.
  if (!fk_table_name.empty() && !absl::StrContains(fk_table_name, '%') &&
      !absl::StrContains(pk_table_name, '%')) {
    return FetchForeignKeysFromTableMetadata(
        stmt_handle, catalog_name, schema_name, pk_table_name, fk_table_name);
  }
  return FetchForeignKeysByListingTables(stmt_handle, catalog_name, schema_name,
                                         pk_table_name, fk_table_name);
}

}  // namespace google::cloud::odbc_bq_driver_internal
