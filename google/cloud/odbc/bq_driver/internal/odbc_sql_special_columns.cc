// Copyright 2026 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_special_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_primary_keys.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {
StatusRecordOr<DSRow> CreateResultSetForSpecialColumns(
    ConnectionHandle const& conn_handle, TableFieldSchema const& field_schema) {
  DSRow ds_row;
  bool is_repeated = (field_schema.mode == "REPEATED");
  std::uint32_t default_column_length =
      conn_handle.GetDsn().default_string_column_length;

  // SCOPE
  DSValue ds_scope = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(SQL_SCOPE_SESSION),
                                 ds_scope);
  ds_row.emplace_back(ds_scope);

  // COLUMN_NAME
  DSValue ds_column_name = kNullValue;
  if (!field_schema.name.empty()) {
    StringToDSValue(field_schema.name, ds_column_name);
  }
  ds_row.emplace_back(ds_column_name);

  // DATA_TYPE
  DSValue ds_data_type = kNullValue;
  auto data_type_status = GetSQLDataType(field_schema.type, is_repeated);
  if (!data_type_status) {
    return data_type_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> data_type = *data_type_status;
  if (data_type.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*data_type),
                                   ds_data_type);
  }
  ds_row.emplace_back(ds_data_type);

  // TYPE_NAME
  DSValue ds_type_name = kNullValue;
  auto type = is_repeated ? "ARRAY" : field_schema.type;
  auto type_status = GetTypeDescription(type);
  if (!type_status) {
    return type_status.GetStatusRecord();
  }
  std::string type_name = *type_status;
  if (!type_name.empty()) {
    StringToDSValue(type_name, ds_type_name);
  }
  ds_row.emplace_back(ds_type_name);

  // COLUMN_SIZE
  DSValue ds_col_size = kNullValue;
  auto col_size_status =
      GetColSize(field_schema, default_column_length, is_repeated);
  if (!col_size_status) {
    return col_size_status.GetStatusRecord();
  }
  optional<SQLINTEGER> col_size = *col_size_status;
  if (col_size.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*col_size),
                                   ds_col_size);
  }
  ds_row.emplace_back(ds_col_size);

  // BUFFER_LENGTH
  DSValue ds_buf_len = kNullValue;
  auto buf_len_status =
      GetBufferLen(field_schema, default_column_length, is_repeated);
  if (!buf_len_status) {
    return buf_len_status.GetStatusRecord();
  }
  optional<SQLINTEGER> buf_len = *buf_len_status;
  if (buf_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*buf_len),
                                   ds_buf_len);
  }
  ds_row.emplace_back(ds_buf_len);

  // DECIMAL_DIGITS
  DSValue ds_dec_digits = kNullValue;
  auto dec_digits_status =
      GetDecimalDigits(field_schema, default_column_length, is_repeated);
  if (!dec_digits_status) {
    return dec_digits_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> dec_digits = *dec_digits_status;
  if (dec_digits.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*dec_digits),
                                   ds_dec_digits);
  }
  ds_row.emplace_back(ds_dec_digits);

  // PSEUDO_COLUMN
  DSValue ds_pseudo_column = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(SQL_PC_NOT_PSEUDO),
                                 ds_pseudo_column);
  ds_row.emplace_back(ds_pseudo_column);

  return ds_row;
}
}  // namespace

StatusRecordOr<ResultSet> FetchSpecialColumnsResultSetFromTableMetaData(
    StatementHandle& stmt_handle, SQLUSMALLINT identifier_type,
    std::string const& catalog_name, int /*catalog_name_len*/,
    std::string const& schema_name, int /*schema_name_len*/,
    std::string const& table_name, int table_name_len,
    SQLUSMALLINT /*min_row_id_scope*/, SQLUSMALLINT /*col_nullable*/) {
  LOG(INFO) << "FetchSpecialColumnsResultSetFromTableMetaData:: Start";
  ResultSet result_set;

  for (auto const& [_, schema] : kSpecialColumnsMap) {
    result_set.row_schema.emplace_back(schema);
  }

  if (identifier_type == SQL_ROWVER || identifier_type == SQL_BEST_ROWID) {
    return result_set;
  }

  if (table_name.empty() ||
      (table_name_len <= 0 && table_name_len != SQL_NTS)) {
    LOG(ERROR) << "FetchSpecialColumnsResultSetFromTableMetaData:: Parameter "
                  "table_name "
                  "cannot be empty.";
    auto status_record = StatusRecord{SQLStates::k_HY009(),
                                      "Parameter table_name cannot be empty"};
    return status_record;
  }

  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());

  auto bq_table_status =
      FetchBQTableData(conn_handle, catalog_name, schema_name, table_name);

  if (!bq_table_status) {
    LOG(ERROR) << "FetchSpecialColumnsResultSetFromTableMetaData::"
                  "FetchBQTableData:: "
               << bq_table_status.GetStatusRecord().message;
    return bq_table_status.GetStatusRecord();
  }

  auto table_metadata = *bq_table_status;
  auto primary_keys = table_metadata.table_constraints.primary_key;
  for (auto const& pk_column : primary_keys.columns) {
    auto column_name = pk_column;
    if (column_name.empty()) {
      return result_set;
    }
    for (auto const& table_field_schema : table_metadata.schema.fields) {
      if (table_field_schema.name != column_name) continue;

      auto ds_row_status =
          CreateResultSetForSpecialColumns(conn_handle, table_field_schema);
      if (!ds_row_status) {
        LOG(ERROR) << "FetchSpecialColumnsResultSetFromTableMetaData::"
                      "CreateResultSetForSpecialColumns:: "
                   << ds_row_status.GetStatusRecord().message;
        return ds_row_status.GetStatusRecord();
      }
      result_set.rows.emplace_back(*ds_row_status);
      break;
    }
  }
  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
