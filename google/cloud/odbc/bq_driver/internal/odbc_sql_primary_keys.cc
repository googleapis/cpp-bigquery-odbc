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
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
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

StatusRecordOr<DSRow> CreateResultSetForPrimaryKeys(
    std::string const& catalog, std::string const& dataset,
    std::string const& table, TableFieldSchema const& field_schema,
    SQLSMALLINT field_pos) {
  DSRow ds_row;

  // TABLE_CAT
  DSValue ds_table_cat = kNullValue;
  if (!catalog.empty()) {
    StringToDSValue(catalog, ds_table_cat);
  }
  ds_row.emplace_back(ds_table_cat);

  // TABLE_SCHEMA
  DSValue ds_table_schema = kNullValue;
  if (!dataset.empty()) {
    StringToDSValue(dataset, ds_table_schema);
  }
  ds_row.emplace_back(ds_table_schema);

  // TABLE_NAME
  DSValue ds_table_name = kNullValue;
  if (!table.empty()) {
    StringToDSValue(table, ds_table_name);
  }
  ds_row.emplace_back(ds_table_name);

  // COLUMN_NAME
  DSValue ds_column_name = kNullValue;
  if (!field_schema.name.empty()) {
    StringToDSValue(field_schema.name, ds_column_name);
  }
  ds_row.emplace_back(ds_column_name);

  // ORDINAL_POSITION
  DSValue ds_ord_pos = kNullValue;
  // field_pos is always >= 0 any other value is error.
  if (field_pos < 0) {
    LOG(ERROR) << "CreateResultSetDSRow:: Invalid ordinal position: "
               << field_pos;
    return StatusRecord{SQLStates::k_HY000(), "Invalid ordinal position"};
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(field_pos), ds_ord_pos);
  ds_row.emplace_back(ds_ord_pos);

  // PK_NAME
  DSValue ds_pk_name = kNullValue;
  if (!table.empty()) {
    auto pk_name = table + ".pk$";
    StringToDSValue(pk_name, ds_pk_name);
  }
  ds_row.emplace_back(ds_pk_name);

  return ds_row;
}

StatusRecordOr<ResultSet> FetchPKResultSetFromTableMetaData(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    int catalog_name_len, std::string const& schema_name, int schema_name_len,
    std::string const& table_name, int table_name_len) {
  // Input validation of required parameters.
  if (catalog_name.empty() ||
      (catalog_name_len <= 0 && catalog_name_len != SQL_NTS)) {
    LOG(ERROR) << "FetchPKResultSetFromTableMetaData:: Parameter catalog_name "
                  "cannot be empty.";
    auto status_record = StatusRecord{SQLStates::k_HY090(),
                                      "Parameter catalog_name cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (schema_name.empty() ||
      (schema_name_len <= 0 && schema_name_len != SQL_NTS)) {
    LOG(ERROR) << "FetchPKResultSetFromTableMetaData:: Parameter schema_name "
                  "cannot be empty.";
    auto status_record = StatusRecord{SQLStates::k_HY090(),
                                      "Parameter schema_name cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (table_name.empty() ||
      (table_name_len <= 0 && table_name_len != SQL_NTS)) {
    LOG(ERROR) << "FetchPKResultSetFromTableMetaData:: Parameter table_name "
                  "cannot be empty.";
    auto status_record = StatusRecord{SQLStates::k_HY090(),
                                      "Parameter table_name cannot be empty"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  if (stmt_handle.GetConnectionHandle() == nullptr) {
    LOG(ERROR)
        << "FetchPKResultSetFromTableMetaData:: Connection handle is null.";
    auto status_record = StatusRecord{SQLStates::k_HY013(),
                                      "Internal connection handle is null"};
    stmt_handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }

  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  auto bq_table_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, table_name);

  if (!bq_table_status) {
    return bq_table_status.GetStatusRecord();
  }

  ResultSet result_set;
  result_set.row_schema.resize(kPrimaryKeysMap.size());
  for (auto const& [_, schema] : kPrimaryKeysMap) {
    result_set.row_schema[schema.col_index] = schema;
  }

  auto table_metadata = *bq_table_status;
  auto primary_keys = table_metadata.table_constraints.primary_key;
  int ord_pos = 1;
  for (auto const& pk_column : primary_keys.columns) {
    auto column_name = pk_column;
    if (column_name.empty()) {
      return result_set;
    }
    for (auto const& table_field_schema : table_metadata.schema.fields) {
      if (table_field_schema.name != column_name) continue;

      auto ds_row_status = CreateResultSetForPrimaryKeys(
          table_metadata.table_reference.project_id,
          table_metadata.table_reference.dataset_id,
          table_metadata.table_reference.table_id, table_field_schema, ord_pos);
      if (!ds_row_status) {
        LOG(ERROR) << "FetchPKResultSetFromTableMetaData::"
                      "CreateResultSetForPrimaryKeys:: "
                   << ds_row_status.GetStatusRecord().message;
        return ds_row_status.GetStatusRecord();
      }
      result_set.rows.emplace_back(*ds_row_status);
      break;
    }
    ord_pos++;
  }
  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
