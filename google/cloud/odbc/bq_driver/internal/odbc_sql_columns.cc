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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView;
using ::google::cloud::odbc_bigquery_client_interface::TableFilter;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

StatusRecord CreateResultSetRowSchema(ResultSet& result_set) {
  for (auto const& entry : kODBCColumnsMap) {
    auto col_schema_status = GetColumnSchema(entry.first);
    if (!col_schema_status) {
      return col_schema_status.GetStatusRecord();
    }
    result_set.row_schema.emplace_back(*col_schema_status);
  }
  return StatusRecord::Ok();
}

StatusRecordOr<DSRow> CreateResultSetDSRow(std::string const& catalog,
                                           std::string const& dataset,
                                           std::string const& table,
                                           TableFieldSchema const& field_schema,
                                           SQLSMALLINT field_pos) {
  DSRow ds_row;

  // TABLE_CAT
  DSValue ds_table_cat;
  StringToDSValue(catalog, ds_table_cat);
  ds_row.emplace_back(ds_table_cat);

  // TABLE_SCHEMA
  DSValue ds_table_schema;
  StringToDSValue(dataset, ds_table_schema);
  ds_row.emplace_back(ds_table_schema);

  // TABLE_NAME
  DSValue ds_table_name;
  StringToDSValue(table, ds_table_name);
  ds_row.emplace_back(ds_table_name);

  // COLUMN_NAME
  DSValue ds_column_name;
  StringToDSValue(field_schema.name, ds_column_name);
  ds_row.emplace_back(ds_column_name);

  // DATA_TYPE
  DSValue ds_data_type;
  auto data_type_status = GetSQLDataType(field_schema.type, false);
  if (!data_type_status) {
    return data_type_status.GetStatusRecord();
  }
  SQLSMALLINT data_type = *data_type_status;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(data_type),
                                 ds_data_type);
  ds_row.emplace_back(ds_data_type);

  // TYPE_NAME
  DSValue ds_type_name;
  StringToDSValue(field_schema.type, ds_type_name);
  ds_row.emplace_back(ds_type_name);

  // COLUMN_SIZE
  DSValue ds_col_size;
  auto col_size_status = GetColSize(field_schema);
  if (!col_size_status) {
    return col_size_status.GetStatusRecord();
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*col_size_status),
                                 ds_col_size);
  ds_row.emplace_back(ds_col_size);

  // BUFFER_LENGTH
  DSValue ds_buf_len;
  auto buf_len_status = GetBufferLen(field_schema);
  if (!buf_len_status) {
    return buf_len_status.GetStatusRecord();
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*buf_len_status),
                                 ds_buf_len);
  ds_row.emplace_back(ds_buf_len);

  // DECIMAL_DIGITS
  DSValue ds_dec_digits;
  auto dec_digits_status = GetDecimalDigits(field_schema);
  if (!dec_digits_status) {
    return dec_digits_status.GetStatusRecord();
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*dec_digits_status),
                                 ds_dec_digits);
  ds_row.emplace_back(ds_dec_digits);

  // NUM_PREC_RADIX
  DSValue ds_radix;
  auto radix_status = GetRadix(field_schema);
  if (!radix_status) {
    return radix_status.GetStatusRecord();
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*radix_status),
                                 ds_radix);
  ds_row.emplace_back(ds_radix);

  // NULLABLE
  DSValue ds_nullable;
  SQLSMALLINT nullable =
      (field_schema.mode == "REQUIRED") ? SQL_NO_NULLS : SQL_NULLABLE;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(nullable), ds_nullable);
  ds_row.emplace_back(ds_nullable);

  // REMARKS
  DSValue ds_description;
  StringToDSValue(field_schema.description, ds_description);
  ds_row.emplace_back(ds_description);

  // COLUMN_DEF
  DSValue column_def;
  StringToDSValue(field_schema.default_value_expression, column_def);
  ds_row.emplace_back(column_def);

  // SQL_DATA_TYPE
  DSValue ds_sql_data_type;
  auto sql_data_type_status = GetSQLDataType(data_type);
  if (!sql_data_type_status) {
    return sql_data_type_status.GetStatusRecord();
  }
  SQLSMALLINT sql_data_type = *sql_data_type_status;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(sql_data_type),
                                 ds_sql_data_type);
  ds_row.emplace_back(ds_sql_data_type);

  // SQL_DATETIME_SUB
  DSValue ds_sql_datetime_sub;
  auto sql_data_time_sub_status = GetSQLDateTimeSub(sql_data_type, data_type);
  if (!sql_data_time_sub_status) {
    return sql_data_time_sub_status.GetStatusRecord();
  }
  ArithmeticToDSValue<SQLBIGINT>(
      static_cast<SQLBIGINT>(*sql_data_time_sub_status), ds_sql_datetime_sub);
  ds_row.emplace_back(ds_sql_datetime_sub);

  // CHAR_OCTET_LENGTH
  DSValue ds_char_octet_len;
  auto char_octet_len_status = GetCharOctetLen(field_schema);
  if (!char_octet_len_status) {
    return char_octet_len_status.GetStatusRecord();
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*char_octet_len_status),
                                 ds_char_octet_len);
  ds_row.emplace_back(ds_char_octet_len);

  // ORDINAL_POSITION
  DSValue ds_ord_pos;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(field_pos), ds_ord_pos);
  ds_row.emplace_back(ds_ord_pos);

  // IS_NULLABLE
  DSValue ds_is_nullable;
  std::string is_nullable = (nullable) ? "YES" : "NO";
  StringToDSValue(is_nullable, ds_is_nullable);
  ds_row.emplace_back(ds_is_nullable);

  return ds_row;
}

StatusRecordOr<Table> FetchBQTableData(ConnectionHandle& conn_handle,
                                       std::string const& catalog,
                                       std::string const& dataset,
                                       std::string const& table) {
  StatusRecordOr<Table> result;
  // Validate the data source parameters for the BQ call.
  if (catalog.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Catalog cannot be empty for BQ Data source"};
  }
  if (dataset.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Dataset cannot be empty for BQ Data source"};
  }
  if (table.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Table cannot be empty for BQ Data source"};
  }
  // Validate the  connection handle.
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }
  Options options;
  TableFilter filter{{}, TableMetadataView::Full()};
  return bq_client->GetTable(catalog, dataset, table, filter, options);
}

StatusRecordOr<ResultSet> ProcessTableResults(
    Table const& bq_table, std::string const& bq_table_column) {
  ResultSet result_set;
  // Populate Row Schema for the ResultSet.
  auto row_schema_status = CreateResultSetRowSchema(result_set);
  if (!row_schema_status.ok()) {
    return row_schema_status;
  }
  // Now populate data for the resultset from the BQ Table.
  if (bq_table_column.empty() || bq_table_column == "%") {
    // Puts all columns in the result set.
    // Each table_field_schema entry below represents
    // a ResultSetRow that has all the ODBC fields as mentioned in
    // CreateResultSetRowSchema. In this usecase, number of resultset rows =
    // number of table_field_schema entries.
    int ord_pos = 1;
    for (TableFieldSchema const& table_field_schema : bq_table.schema.fields) {
      auto ds_row_status = CreateResultSetDSRow(
          bq_table.table_reference.project_id,
          bq_table.table_reference.dataset_id,
          bq_table.table_reference.table_id, table_field_schema, ord_pos++);
      if (!ds_row_status) {
        return ds_row_status.GetStatusRecord();
      }
      result_set.rows.emplace_back(*ds_row_status);
    }
  } else {
    // Put only the specific column metadata in the resultset. In this
    // usecase, number of rows in the resultset = 1.
    int ord_pos = 1;
    for (TableFieldSchema const& table_field_schema : bq_table.schema.fields) {
      if (bq_table_column == table_field_schema.name) {
        auto ds_row_status = CreateResultSetDSRow(
            bq_table.table_reference.project_id,
            bq_table.table_reference.dataset_id,
            bq_table.table_reference.table_id, table_field_schema, ord_pos);
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
