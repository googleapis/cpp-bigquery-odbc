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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bigquery_client_interface::MaxRetriesOption;
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
    std::string const& schema_name, std::string const& pk_table_name,
    std::string const& fk_table_name, std::vector<std::string> const& pk_cols) {
  ResultSetRows result_rows;
  if (pk_table_name == fk_table_name) {
    return result_rows;
  }

  auto lookup_table_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, fk_table_name);
  if (!lookup_table_status) {
    return lookup_table_status.GetStatusRecord();
  }
  auto const& foreign_keys_obj =
      lookup_table_status->table_constraints.foreign_keys;
  if (foreign_keys_obj.empty()) {
    return result_rows;
  }
  int ord_pos = 1;
  for (auto const& fk_keys : foreign_keys_obj) {
    if (fk_keys.referenced_table.table_id != pk_table_name) {
      continue;
    }
    for (auto const& col_ref : fk_keys.column_references) {
      std::string col_to_find = col_ref.referenced_column;
      auto is_in = std::find(pk_cols.begin(), pk_cols.end(), col_to_find) !=
                   pk_cols.end();
      if (is_in) {
        auto row_status = CreateResultSetForForeignKeys(
            catalog_name, schema_name, pk_table_name, col_ref.referenced_column,
            catalog_name, schema_name, fk_table_name,
            col_ref.referencing_column, ord_pos);

        if (!row_status) {
          return row_status.GetStatusRecord();
        }
        result_rows.emplace_back(std::move(*row_status));
        ++ord_pos;
      }
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

  bool has_pk_table_only = !pk_table_name.empty() && fk_table_name.empty();
  // When only pk_table_name provided
  if (has_pk_table_only) {
    auto pk_table_status =
        FetchBQTableData(conn_handle, catalog_name, schema_name, pk_table_name);
    if (!pk_table_status) {
      return pk_table_status.GetStatusRecord();
    }
    auto pk_cols = pk_table_status->table_constraints.primary_key.columns;
    if (pk_cols.empty()) {
      return result_set;
    }

    Options opts;
    opts.set<MaxRetriesOption>(conn_handle.GetDsn().max_retries);
    auto table_status =
        conn_handle.GetClient()->ListAllTables(catalog_name, schema_name, opts);
    if (!table_status) {
      return table_status.GetStatusRecord();
    }
    std::vector<std::future<StatusRecordOr<ResultSetRows>>> futures;
    for (auto const& lookup_table : *table_status) {
      std::string lookup_table_id = lookup_table.table_reference.table_id;
      if (lookup_table_id == pk_table_name) {
        continue;
      }
      futures.emplace_back(std::async(
          std::launch::async, [&conn_handle, catalog_name, schema_name,
                               pk_table_name, lookup_table_id, pk_cols]() {
            return CreateFKResultRows(conn_handle, catalog_name, schema_name,
                                      pk_table_name, lookup_table_id, pk_cols);
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
  std::string table_name = fk_table_name;
  bool const has_both_table = !pk_table_name.empty() && !fk_table_name.empty();
  auto table_metadata_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, table_name);
  if (!table_metadata_status) {
    return table_metadata_status.GetStatusRecord();
  }

  auto foreign_keys = table_metadata_status->table_constraints.foreign_keys;
  for (auto const& fk_keys : foreign_keys) {
    auto ref_table = fk_keys.referenced_table;
    auto col_refs = fk_keys.column_references;

    if (catalog_name == ref_table.project_id &&
        schema_name == ref_table.dataset_id) {
      if (has_both_table && ref_table.table_id != pk_table_name) {
        continue;
      }
      auto lookup_table = FetchBQTableData(conn_handle, catalog_name,
                                           schema_name, ref_table.table_id);
      if (!lookup_table) {
        return lookup_table.GetStatusRecord();
      }
      auto lookup_pk_keys = lookup_table->table_constraints.primary_key.columns;

      int ord_pos = 1;
      for (auto const& col_ref : col_refs) {
        std::string col_to_find = col_ref.referenced_column;
        auto is_in = std::find(lookup_pk_keys.begin(), lookup_pk_keys.end(),
                               col_to_find) != lookup_pk_keys.end();
        if (is_in) {
          auto row_status = CreateResultSetForForeignKeys(
              ref_table.project_id, ref_table.dataset_id, ref_table.table_id,
              col_ref.referenced_column, catalog_name, schema_name, table_name,
              col_ref.referencing_column, ord_pos);

          if (!row_status) {
            return row_status.GetStatusRecord();
          }
          result_set.rows.emplace_back(std::move(*row_status));
        }
        ++ord_pos;
      }
    }
  }
  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
