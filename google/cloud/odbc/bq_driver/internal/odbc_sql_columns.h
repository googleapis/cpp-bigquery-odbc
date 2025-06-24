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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_COLUMNS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_COLUMNS_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <map>
#include <regex>
#include <string>

namespace google::cloud::odbc_bq_driver_internal {
// Map of ODBC data source columns names and number as per the design:
// https://docs.google.com/document/d/1THL56A-lfcsW0XlZcrk1aMzl8sb56Oa0gszN7V_koXE/edit?pli=1&tab=t.0#bookmark=id.hzhd12b54a5r
static std::map<std::string, ColumnSchema> const kODBCColumnsMap = {
    {"TABLE_CAT", ColumnSchema{0, BQDataType::kString}},
    {"TABLE_SCHEM", ColumnSchema{1, BQDataType::kString}},
    {"TABLE_NAME", ColumnSchema{2, BQDataType::kString}},
    {"COLUMN_NAME", ColumnSchema{3, BQDataType::kString}},
    {"DATA_TYPE", ColumnSchema{4, BQDataType::kInt64}},
    {"TYPE_NAME", ColumnSchema{5, BQDataType::kString}},
    {"COLUMN_SIZE", ColumnSchema{6, BQDataType::kInt64}},
    {"BUFFER_LENGTH", ColumnSchema{7, BQDataType::kInt64}},
    {"DECIMAL_DIGITS", ColumnSchema{8, BQDataType::kInt64}},
    {"NUM_PREC_RADIX", ColumnSchema{9, BQDataType::kInt64}},
    {"NULLABLE", ColumnSchema{10, BQDataType::kInt64}},
    {"REMARKS", ColumnSchema{11, BQDataType::kString}},
    {"COLUMN_DEF", ColumnSchema{12, BQDataType::kString}},
    {"SQL_DATA_TYPE", ColumnSchema{13, BQDataType::kInt64}},
    {"SQL_DATETIME_SUB", ColumnSchema{14, BQDataType::kInt64}},
    {"CHAR_OCTET_LENGTH", ColumnSchema{15, BQDataType::kInt64}},
    {"ORDINAL_POSITION", ColumnSchema{16, BQDataType::kInt64}},
    {"IS_NULLABLE", ColumnSchema{17, BQDataType::kString}}};

inline odbc_internal::StatusRecordOr<ColumnSchema> GetColumnSchema(
    std::string const& col_name) {
  auto map_item = kODBCColumnsMap.find(col_name);
  if (map_item != kODBCColumnsMap.end()) {
    return map_item->second;
  }
  return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                     "Invalid column name: " + col_name};
}

// Fetches detailed Table information from the BQ datasource based on the
// parameters supplied.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Table>
FetchBQTableData(ConnectionHandle& conn_handle, std::string const& catalog,
                 std::string const& dataset, std::string const& table);

// Similar to the above except handles cases where dataset name and table names
// can have search pattern. In this case multiple tables matching the search
// pattern would be returned.
odbc_internal::StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::Table>>
FetchBQTablesData(ConnectionHandle& conn_handle, std::string const& catalog,
                  std::string const& dataset_pattern,
                  std::string const& table_pattern, SQLULEN metadata_id);

// Filters out the table column metadata information based on the column
// supplied.
// 1) If a bq_table_column is supplied, then only the specified column metadata
// is returned in the ResultSet.
// 2) If bq_table_column is empty, then the
// resultset would include metadata information for all columns in the table.
odbc_internal::StatusRecordOr<ResultSet> ProcessTableResults(
    ConnectionHandle& conn_handle,
    ::google::cloud::bigquery_v2_minimal_internal::Table const& bq_table,
    std::string const& bq_table_column, SQLULEN metadata_id = SQL_FALSE);

// Helper functions
odbc_internal::StatusRecord CreateResultSetRowSchema(ResultSet& result_set);
odbc_internal::StatusRecordOr<DSRow> CreateResultSetDSRow(
    ConnectionHandle& conn_handle, std::string const& catalog,
    std::string const& dataset, std::string const& table,
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema const&
        field_schema,
    SQLSMALLINT field_pos);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_COLUMNS_H
