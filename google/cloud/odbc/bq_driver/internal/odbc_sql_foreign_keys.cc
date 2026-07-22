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
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

StatusRecordOr<DSRow> CreateResultSetForForeignKeys(
    std::string const& pk_catalog_name, std::string const& pk_schema_name,
    std::string const& pk_table_name, std::string const& pk_column_name,
    std::string const& fk_catalog_name, std::string const& fk_schema_name,
    std::string const& fk_table_name, std::string const& fk_column_name,
    SQLSMALLINT field_pos) {
  DSRow ds_row;
  // PK_TABLE_CAT
  DSValue ds_pk_table_cat = kNullValue;
  if (!pk_catalog_name.empty()) {
    StringToDSValue(pk_catalog_name, ds_pk_table_cat);
  }
  ds_row.emplace_back(ds_pk_table_cat);

  // PKTABLE_SCHEM
  DSValue ds_pk_table_schema = kNullValue;
  if (!pk_schema_name.empty()) {
    StringToDSValue(pk_schema_name, ds_pk_table_schema);
  }
  ds_row.emplace_back(ds_pk_table_schema);

  // PK_TABLE_NAME
  DSValue ds_pk_table_name = kNullValue;
  if (!pk_table_name.empty()) {
    StringToDSValue(pk_table_name, ds_pk_table_name);
  }
  ds_row.emplace_back(ds_pk_table_name);

  // PK_COLUMN_NAME
  DSValue ds_pk_column_name = kNullValue;
  if (!pk_column_name.empty()) {
    StringToDSValue(pk_column_name, ds_pk_column_name);
  }
  ds_row.emplace_back(ds_pk_column_name);

  // FK_TABLE_CAT
  DSValue ds_fk_table_cat = kNullValue;
  if (!fk_catalog_name.empty()) {
    StringToDSValue(fk_catalog_name, ds_fk_table_cat);
  }
  ds_row.emplace_back(ds_fk_table_cat);

  // FKTABLE_SCHEM
  DSValue ds_fk_table_schema = kNullValue;
  if (!fk_schema_name.empty()) {
    StringToDSValue(fk_schema_name, ds_fk_table_schema);
  }
  ds_row.emplace_back(ds_fk_table_schema);

  // FK_TABLE_NAME
  DSValue ds_fk_table_name = kNullValue;
  if (!fk_table_name.empty()) {
    StringToDSValue(fk_table_name, ds_fk_table_name);
  }
  ds_row.emplace_back(ds_fk_table_name);

  // FK_COLUMN_NAME
  DSValue ds_fk_column_name = kNullValue;
  if (!fk_column_name.empty()) {
    StringToDSValue(fk_column_name, ds_fk_column_name);
  }
  ds_row.emplace_back(ds_fk_column_name);

  // FK_COLUMN_ORDINAL_POSITION
  DSValue ds_fk_column_ordinal_pos = kNullValue;
  // field_pos is always >= 0 any other value is error.
  if (field_pos < 0) {
    LOG(ERROR) << "CreateResultSetDSRow:: Invalid ordinal position: "
               << field_pos;
    return StatusRecord{SQLStates::k_HY000(), "Invalid ordinal position"};
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(field_pos),
                                 ds_fk_column_ordinal_pos);
  ds_row.emplace_back(ds_fk_column_ordinal_pos);

  // UPDATE_RULE
  DSValue ds_update_rule = kNullValue;
  ds_row.emplace_back(ds_update_rule);

  // DELETE_RULE
  DSValue ds_delete_rule = kNullValue;
  ds_row.emplace_back(ds_delete_rule);

  // FK_NAME
  DSValue ds_fk_name = kNullValue;
  if (!fk_table_name.empty()) {
    std::string fk_name = fk_table_name + ".fk$" + std::to_string(field_pos);
    StringToDSValue(fk_name, ds_fk_name);
  }
  ds_row.emplace_back(ds_fk_name);

  // PK_NAME
  DSValue ds_pk_name = kNullValue;
  if (!pk_table_name.empty()) {
    std::string pk_name = pk_table_name + ".pk$";
    StringToDSValue(pk_name, ds_pk_name);
  }
  ds_row.emplace_back(ds_pk_name);

  // DEFERRABILITY
  DSValue ds_deferrability = kNullValue;
  IntToDSValue(SQL_SET_NULL, ds_deferrability);
  ds_row.emplace_back(ds_deferrability);

  return ds_row;
}

StatusRecordOr<ResultSetRows> CreateFKResultRows(
    ConnectionHandle& conn_handle, std::string const& catalog_name,
    std::string const& schema_name, std::string const& table_name,
    std::string const& pk_catalog_name, std::string const& pk_schema_name,
    std::string const& pk_table_name, std::vector<std::string> pk_key_columns,
    bool const& has_pk_table_only) {
  ResultSetRows result_rows;
  if (table_name == pk_table_name && catalog_name == pk_catalog_name &&
      schema_name == pk_schema_name) {
    return result_rows;
  }

  auto table_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, table_name);
  if (!table_status) {
    return table_status.GetStatusRecord();
  }
  auto const& metadata = *table_status;
  auto const& foreign_keys = metadata.table_constraints.foreign_keys;

  for (auto const& fk_key : foreign_keys) {
    bool matches_pk_table = false;
    if (has_pk_table_only) {
      matches_pk_table = (fk_key.referenced_table.table_id == pk_table_name);
    } else {
      auto const& referenced_table = fk_key.referenced_table;

      matches_pk_table = (referenced_table.table_id == pk_table_name);
    }
    if (!matches_pk_table) {
      continue;
    }

    int ord_pos = 1;
    for (auto const& column_reference : fk_key.column_references) {
      auto pk_column_it =
          std::find(pk_key_columns.begin(), pk_key_columns.end(),
                    column_reference.referenced_column);

      if (pk_column_it == pk_key_columns.end()) {
        ++ord_pos;
        continue;
      }

      auto row_status = CreateResultSetForForeignKeys(
          pk_catalog_name, pk_schema_name, pk_table_name,
          column_reference.referenced_column, catalog_name, schema_name,
          table_name, column_reference.referencing_column, ord_pos);

      if (!row_status) {
        return row_status.GetStatusRecord();
      }
      result_rows.emplace_back(std::move(*row_status));
      ++ord_pos;
    }
  }
  return result_rows;
}

StatusRecordOr<ResultSet> FetchFKResultSetFromTableMetaData(
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
    LOG(ERROR) << "FetchFKResultSetFromTableMetaData:: Catalog name for both "
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
    LOG(ERROR) << "FetchFKResultSetFromTableMetaData:: PK and FK catalog names "
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
    LOG(ERROR) << "FetchFKResultSetFromTableMetaData:: Schema name for both "
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
    LOG(ERROR) << "FetchFKResultSetFromTableMetaData:: PK and FK schema names "
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
    LOG(ERROR)
        << "FetchFKResultSetFromTableMetaData:: Both Primary and Foreign "
           "key table names cannot be empty.";
    auto status_record = StatusRecord{
        SQLStates::k_HY009(),
        "Both Primary and Foreign key table names cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (stmt_handle.GetConnectionHandle() == nullptr) {
    LOG(ERROR)
        << "FetchFKResultSetFromTableMetaData:: Connection handle is null.";
    auto status_record = StatusRecord{SQLStates::k_HY013(),
                                      "Internal connection handle is null"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }

  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  ResultSet result_set;
  result_set.row_schema.resize(kForeignKeysMap.size());
  for (auto const& [_, schema] : kForeignKeysMap) {
    result_set.row_schema[schema.col_index] = schema;
  }

  std::string lookup_table =
      !pk_table_name.empty() ? pk_table_name : fk_table_name;
  auto table_metadata_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, lookup_table);
  if (!table_metadata_status) {
    return table_metadata_status.GetStatusRecord();
  }

  auto const& pk_columns =
      table_metadata_status->table_constraints.primary_key.columns;
  // case : When  both pk & fk table provided
  if (!pk_table_name.empty() && !fk_table_name.empty()) {
    auto row_status = CreateFKResultRows(
        conn_handle, catalog_name, schema_name, fk_table_name, pk_catalog_name,
        pk_schema_name, pk_table_name, pk_columns, false);
    if (!row_status) {
      return row_status.GetStatusRecord();
    }

    result_set.rows.insert(result_set.rows.end(),
                           std::make_move_iterator(row_status->begin()),
                           std::make_move_iterator(row_status->end()));
    return result_set;
  }

  // case: either pk or fk table provided
  Options opts;
  auto table_status =
      conn_handle.GetClient()->ListAllTables(catalog_name, schema_name, opts);
  if (!table_status) {
    return table_status.GetStatusRecord();
  }

  bool has_pk_table_only = !pk_table_name.empty();
  std::vector<std::future<StatusRecordOr<ResultSetRows>>> futures;
  for (auto const& table : *table_status) {
    futures.emplace_back(std::async(
        std::launch::async,
        [&conn_handle, catalog_name, schema_name, table, pk_catalog_name,
         pk_schema_name, pk_table_name, pk_columns, has_pk_table_only]() {
          return CreateFKResultRows(
              conn_handle, catalog_name, schema_name,
              table.table_reference.table_id, pk_catalog_name, pk_schema_name,
              pk_table_name, pk_columns, has_pk_table_only);
        }));
  }

  for (auto& future : futures) {
    auto row_status = future.get();
    if (!row_status) {
      continue;
    }
    result_set.rows.insert(result_set.rows.end(),
                           std::make_move_iterator(row_status->begin()),
                           std::make_move_iterator(row_status->end()));
  }
  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
