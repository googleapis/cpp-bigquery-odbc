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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

std::string const kTestCatalog = "test-catalog";
std::string const kTestDataset = "test-dataset";
std::string const kTestTable = "test-table";

namespace {
// Helper functions and structures for  SQLColumns unit tests.

struct TestSqlColumnsData {
  TableFieldSchema field_schema;
  SQLINTEGER col_size;
  SQLINTEGER buf_len;
  SQLINTEGER octet_len;
  SQLSMALLINT dec_digits;
  SQLSMALLINT radix;
  SQLSMALLINT data_type;
  SQLSMALLINT nullable;
  SQLSMALLINT sql_data_type;
  SQLSMALLINT sql_datetime_sub;
  SQLSMALLINT ord_pos;
  std::string is_nullable;
};

void AssertVectorEquals(std::vector<ColumnSchema>& expected,
                        std::vector<ColumnSchema>& actual) {
  ASSERT_EQ(expected.size(), actual.size());

  std::sort(actual.begin(), actual.end());
  std::sort(expected.begin(), expected.end());

  std::equal(actual.begin(), actual.end(), expected.begin(), expected.end());
}

void VerifyDSRow(DSRow& ds_row,
                 TestSqlColumnsData const& exp_sql_columns_data) {
  int i = 0;
  DSValue ds_val = ds_row[i++];
  // TABLE_CAT
  std::string table_cat;
  DSValueToString(ds_val, table_cat);
  ASSERT_EQ(table_cat, kTestCatalog);
  // TABLE_SCHEMA
  ds_val = ds_row[i++];
  std::string table_schema;
  DSValueToString(ds_val, table_schema);
  ASSERT_EQ(table_schema, kTestDataset);
  // TABLE_NAME
  ds_val = ds_row[i++];
  std::string table_name;
  DSValueToString(ds_val, table_name);
  ASSERT_EQ(table_name, kTestTable);
  // COLUMN_NAME
  ds_val = ds_row[i++];
  std::string col_name;
  DSValueToString(ds_val, col_name);
  ASSERT_EQ(col_name, exp_sql_columns_data.field_schema.name);
  // DATA_TYPE
  ds_val = ds_row[i++];
  SQLSMALLINT actual_data_type = DSValueToArithmetic<SQLSMALLINT>(ds_val);
  ASSERT_EQ(exp_sql_columns_data.data_type, actual_data_type);
  // TYPE_NAME
  ds_val = ds_row[i++];
  std::string type_name;
  DSValueToString(ds_val, type_name);
  ASSERT_EQ(type_name, exp_sql_columns_data.field_schema.type);
  // COL_SIZE
  ds_val = ds_row[i++];
  SQLINTEGER actual_col_size = DSValueToArithmetic<SQLINTEGER>(ds_val);
  ASSERT_EQ(actual_col_size, exp_sql_columns_data.col_size);
  // BUFFER_LENGTH
  ds_val = ds_row[i++];
  SQLINTEGER actual_buf_len = DSValueToArithmetic<SQLINTEGER>(ds_val);
  ASSERT_EQ(actual_buf_len, exp_sql_columns_data.buf_len);
  // DECIMAL_DIGITS
  ds_val = ds_row[i++];
  SQLSMALLINT actual_dec_digits = DSValueToArithmetic<SQLSMALLINT>(ds_val);
  ASSERT_EQ(actual_dec_digits, exp_sql_columns_data.dec_digits);
  // NUM_PREC_RADIX
  ds_val = ds_row[i++];
  SQLSMALLINT actual_radix = DSValueToArithmetic<SQLSMALLINT>(ds_val);
  ASSERT_EQ(actual_radix, exp_sql_columns_data.radix);
  // NULLABLE
  ds_val = ds_row[i++];
  SQLSMALLINT actual_nullable = DSValueToArithmetic<SQLSMALLINT>(ds_val);
  ASSERT_EQ(actual_nullable, exp_sql_columns_data.nullable);
  // REMARKS
  ds_val = ds_row[i++];
  std::string remarks;
  DSValueToString(ds_val, remarks);
  ASSERT_EQ(remarks, exp_sql_columns_data.field_schema.description);
  // COLUMN_DEF
  ds_val = ds_row[i++];
  std::string col_def;
  DSValueToString(ds_val, col_def);
  ASSERT_EQ(col_def,
            exp_sql_columns_data.field_schema.default_value_expression);
  // SQL_DATA_TYPE
  ds_val = ds_row[i++];
  SQLSMALLINT actual_sql_data_type = DSValueToArithmetic<SQLSMALLINT>(ds_val);
  ASSERT_EQ(actual_sql_data_type, exp_sql_columns_data.sql_data_type);
  // SQL_DATETIME_SUB
  ds_val = ds_row[i++];
  SQLSMALLINT actual_sql_date_time_sub =
      DSValueToArithmetic<SQLSMALLINT>(ds_val);
  ASSERT_EQ(actual_sql_date_time_sub, exp_sql_columns_data.sql_datetime_sub);
  // CHAR_OCTET_LENGTH
  ds_val = ds_row[i++];
  SQLINTEGER actual_octet_len = DSValueToArithmetic<SQLINTEGER>(ds_val);
  ASSERT_EQ(actual_octet_len, exp_sql_columns_data.octet_len);
  // ORDINAL_POSITION
  ds_val = ds_row[i++];
  SQLSMALLINT actual_pos = DSValueToArithmetic<SQLSMALLINT>(ds_val);
  ASSERT_EQ(actual_pos, exp_sql_columns_data.ord_pos);
  // IS_NULLABLE
  ds_val = ds_row[i++];
  std::string is_nullable;
  DSValueToString(ds_val, is_nullable);
  ASSERT_EQ(is_nullable, exp_sql_columns_data.is_nullable);
}

std::vector<ColumnSchema> CreateExpectedRowSchema() {
  std::vector<ColumnSchema> expected = {
      {ColumnSchema{0, BQDataType::kString}},
      {ColumnSchema{1, BQDataType::kString}},
      {ColumnSchema{2, BQDataType::kString}},
      {ColumnSchema{3, BQDataType::kString}},
      {ColumnSchema{4, BQDataType::kInt64}},
      {ColumnSchema{5, BQDataType::kString}},
      {ColumnSchema{6, BQDataType::kInt64}},
      {ColumnSchema{7, BQDataType::kInt64}},
      {ColumnSchema{8, BQDataType::kInt64}},
      {ColumnSchema{9, BQDataType::kInt64}},
      {ColumnSchema{10, BQDataType::kInt64}},
      {ColumnSchema{11, BQDataType::kString}},
      {ColumnSchema{12, BQDataType::kString}},
      {ColumnSchema{13, BQDataType::kInt64}},
      {ColumnSchema{14, BQDataType::kInt64}},
      {ColumnSchema{15, BQDataType::kInt64}},
      {ColumnSchema{16, BQDataType::kInt64}},
      {ColumnSchema{17, BQDataType::kString}}};
  return expected;
}

void ProcessTableResultsHelper(std::string column,
                               SQLULEN metadata_id = SQL_FALSE) {
  TableFieldSchema field_schema1;
  field_schema1.name = "StringField";
  field_schema1.type = "STRING";
  field_schema1.mode = "REQUIRED";
  field_schema1.description = "STRING";
  field_schema1.default_value_expression = "Test-String";
  field_schema1.max_length = 5000;

  TableFieldSchema field_schema2;
  field_schema2.name = "IntField";
  field_schema2.type = "INT64";
  field_schema2.mode = "REQUIRED";
  field_schema2.description = "INT64";
  field_schema2.default_value_expression = "1234";

  Table table;
  table.table_reference.project_id = kTestCatalog;
  table.table_reference.dataset_id = kTestDataset;
  table.table_reference.table_id = kTestTable;
  table.schema.fields.emplace_back(field_schema1);
  table.schema.fields.emplace_back(field_schema2);

  auto result_set_status = ProcessTableResults(table, column, metadata_id);
  ASSERT_STATUS_RECORD_OK(result_set_status);

  ResultSet result_set = *result_set_status;
  std::vector<ColumnSchema> expected_row_schema = CreateExpectedRowSchema();
  AssertVectorEquals(expected_row_schema, result_set.row_schema);

  TestSqlColumnsData expected_sql_string_row;
  expected_sql_string_row.field_schema = field_schema1;
  expected_sql_string_row.col_size = 16384;
  expected_sql_string_row.buf_len = 5000;
  expected_sql_string_row.octet_len = 5000;
  expected_sql_string_row.dec_digits = SQL_NULL_DATA;
  expected_sql_string_row.radix = 10;
  expected_sql_string_row.data_type = SQL_VARCHAR;
  expected_sql_string_row.nullable = 0;
  expected_sql_string_row.sql_data_type = SQL_VARCHAR;
  expected_sql_string_row.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_string_row.ord_pos = 1;
  expected_sql_string_row.is_nullable = "NO";

  TestSqlColumnsData expected_sql_int_row;
  expected_sql_int_row.field_schema = field_schema2;
  expected_sql_int_row.col_size = 19;
  expected_sql_int_row.buf_len = 20;
  expected_sql_int_row.octet_len = SQL_NULL_DATA;
  expected_sql_int_row.dec_digits = 0;
  expected_sql_int_row.radix = 10;
  expected_sql_int_row.data_type = SQL_BIGINT;
  expected_sql_int_row.nullable = 0;
  expected_sql_int_row.sql_data_type = SQL_BIGINT;
  expected_sql_int_row.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_int_row.ord_pos = 2;
  expected_sql_int_row.is_nullable = "NO";

  std::regex column_pattern = BuildRegex(column, metadata_id);

  if (!metadata_id && (column == "" || column == "%")) {
    ASSERT_EQ(result_set.rows.size(), 2);
    VerifyDSRow(result_set.rows[0], expected_sql_string_row);
    VerifyDSRow(result_set.rows[1], expected_sql_int_row);
  } else if (std::regex_match(field_schema1.name, column_pattern)) {
    ASSERT_EQ(result_set.rows.size(), 1);
    VerifyDSRow(result_set.rows[0], expected_sql_string_row);
  } else if (std::regex_match(field_schema2.name, column_pattern)) {
    ASSERT_EQ(result_set.rows.size(), 1);
    VerifyDSRow(result_set.rows[0], expected_sql_int_row);
  } else {
    ASSERT_TRUE(result_set.rows.empty());
  }
}

}  // namespace

TEST(CreateResultSetRowSchema, Success) {
  ResultSet result_set;
  auto actual_status = CreateResultSetRowSchema(result_set);
  ASSERT_TRUE(actual_status.ok());

  std::vector<ColumnSchema> expected_row_schema = CreateExpectedRowSchema();
  AssertVectorEquals(expected_row_schema, result_set.row_schema);
}

TEST(CreateResultSetDSRow, StringField) {
  TableFieldSchema field_schema;
  field_schema.name = "StringField";
  field_schema.type = "STRING";
  field_schema.mode = "REQUIRED";
  field_schema.description = "STRING";
  field_schema.default_value_expression = "Test-String";
  field_schema.max_length = 5000;

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 1);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 16384;
  expected_sql_columns.buf_len = 5000;
  expected_sql_columns.octet_len = 5000;
  expected_sql_columns.dec_digits = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_VARCHAR;
  expected_sql_columns.nullable = 0;
  expected_sql_columns.sql_data_type = SQL_VARCHAR;
  expected_sql_columns.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_columns.ord_pos = 1;
  expected_sql_columns.is_nullable = "NO";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, IntField) {
  TableFieldSchema field_schema;
  field_schema.name = "IntField";
  field_schema.type = "INT64";
  field_schema.mode = "REQUIRED";
  field_schema.description = "INT64";
  field_schema.default_value_expression = "1234";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 2);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 19;
  expected_sql_columns.buf_len = 20;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.dec_digits = 0;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_BIGINT;
  expected_sql_columns.nullable = 0;
  expected_sql_columns.sql_data_type = SQL_BIGINT;
  expected_sql_columns.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_columns.ord_pos = 2;
  expected_sql_columns.is_nullable = "NO";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, BoolField) {
  TableFieldSchema field_schema;
  field_schema.name = "BoolField";
  field_schema.type = "BOOL";
  field_schema.description = "BOOL";
  field_schema.default_value_expression = "true";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 3);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 1;
  expected_sql_columns.buf_len = 1;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.dec_digits = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_BIT;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_BIT;
  expected_sql_columns.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_columns.ord_pos = 3;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, TimeField) {
  TableFieldSchema field_schema;
  field_schema.name = "TimeField";
  field_schema.type = "TIME";
  field_schema.description = "TIME";
  field_schema.default_value_expression = "0:0:0";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 4);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 15;
  expected_sql_columns.buf_len = 6;
  expected_sql_columns.dec_digits = 6;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_TYPE_TIME;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_DATETIME;
  expected_sql_columns.sql_datetime_sub = SQL_CODE_TIME;
  expected_sql_columns.ord_pos = 4;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, DateField) {
  TableFieldSchema field_schema;
  field_schema.name = "DateField";
  field_schema.type = "DATE";
  field_schema.description = "DATE";
  field_schema.default_value_expression = "12/10/2024";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 5);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 10;
  expected_sql_columns.buf_len = 6;
  expected_sql_columns.dec_digits = SQL_NULL_DATA;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_TYPE_DATE;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_DATETIME;
  expected_sql_columns.sql_datetime_sub = SQL_CODE_DATE;
  expected_sql_columns.ord_pos = 5;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, TimestampField) {
  TableFieldSchema field_schema;
  field_schema.name = "TimestampField";
  field_schema.type = "TIMESTAMP";
  field_schema.description = "TIMESTAMP";
  field_schema.default_value_expression = "1234567";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 6);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 26;
  expected_sql_columns.buf_len = 16;
  expected_sql_columns.dec_digits = 6;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_TYPE_TIMESTAMP;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_DATETIME;
  expected_sql_columns.sql_datetime_sub = SQL_CODE_TIMESTAMP;
  expected_sql_columns.ord_pos = 6;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, DateTimeField) {
  TableFieldSchema field_schema;
  field_schema.name = "DateTimeField";
  field_schema.type = "DATETIME";
  field_schema.description = "DATETIME";
  field_schema.default_value_expression = "12/10/2024 00:12:34";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 7);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 26;
  expected_sql_columns.buf_len = 16;
  expected_sql_columns.dec_digits = 6;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_TYPE_TIMESTAMP;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_DATETIME;
  expected_sql_columns.sql_datetime_sub = SQL_CODE_TIMESTAMP;
  expected_sql_columns.ord_pos = 7;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, NumericField) {
  TableFieldSchema field_schema;
  field_schema.name = "NumericField";
  field_schema.type = "NUMERIC";
  field_schema.description = "NUMERIC";
  field_schema.default_value_expression = "1234.456";
  field_schema.precision = 10;
  field_schema.scale = 3;

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 8);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 10;
  expected_sql_columns.buf_len = 40;
  expected_sql_columns.dec_digits = 3;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_NUMERIC;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_NUMERIC;
  expected_sql_columns.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_columns.ord_pos = 8;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, DecimalField) {
  TableFieldSchema field_schema;
  field_schema.name = "DecimalField";
  field_schema.type = "DECIMAL";
  field_schema.description = "DECIMAL";
  field_schema.default_value_expression = "1234.456";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 9);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 38;
  expected_sql_columns.buf_len = 40;
  expected_sql_columns.dec_digits = 9;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_NUMERIC;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_NUMERIC;
  expected_sql_columns.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_columns.ord_pos = 9;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(CreateResultSetDSRow, BigNumericField) {
  TableFieldSchema field_schema;
  field_schema.name = "BigNumericField";
  field_schema.type = "BIGNUMERIC";
  field_schema.description = "BIGNUMERIC";
  field_schema.default_value_expression = "1234.456";

  auto ds_row_status = CreateResultSetDSRow(kTestCatalog, kTestDataset,
                                            kTestTable, field_schema, 10);
  ASSERT_STATUS_RECORD_OK(ds_row_status);

  TestSqlColumnsData expected_sql_columns;
  expected_sql_columns.field_schema = field_schema;
  expected_sql_columns.col_size = 38;
  expected_sql_columns.buf_len = 40;
  expected_sql_columns.dec_digits = 9;
  expected_sql_columns.octet_len = SQL_NULL_DATA;
  expected_sql_columns.radix = 10;
  expected_sql_columns.data_type = SQL_NUMERIC;
  expected_sql_columns.nullable = 1;
  expected_sql_columns.sql_data_type = SQL_NUMERIC;
  expected_sql_columns.sql_datetime_sub = SQL_NULL_DATA;
  expected_sql_columns.ord_pos = 10;
  expected_sql_columns.is_nullable = "YES";

  VerifyDSRow(*ds_row_status, expected_sql_columns);
}

TEST(FetchBQTableData, failure_empty_catalog_name) {
  ConnectionHandle handle;
  auto status_record_or =
      FetchBQTableData(handle, "", kTestDataset, kTestTable);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY000(),
                     HasSubstr("Catalog cannot be empty for BQ Data source")));
}

TEST(FetchBQTableData, failure_empty_dataset_name) {
  ConnectionHandle handle;
  auto status_record_or =
      FetchBQTableData(handle, kTestCatalog, "", kTestTable);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY000(),
                     HasSubstr("Dataset cannot be empty for BQ Data source")));
}

TEST(FetchBQTableData, failure_empty_table_name) {
  ConnectionHandle handle;
  auto status_record_or =
      FetchBQTableData(handle, kTestCatalog, kTestDataset, "");

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY000(),
                     HasSubstr("Table cannot be empty for BQ Data source")));
}

TEST(FetchBQTableData, failure_invalid_connection_handle) {
  ConnectionHandle handle;
  auto status_record_or =
      FetchBQTableData(handle, kTestCatalog, kTestDataset, kTestTable);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_08S01(),
                     HasSubstr("Connection to the data source is broken")));
}

TEST(ProcessTableResults, AllColumns_UsingEmptyColumnName_FALSE) {
  ProcessTableResultsHelper("");
}

TEST(ProcessTableResults, AllColumns_UsingEmptyColumnName_TRUE) {
  ProcessTableResultsHelper("", SQL_TRUE);
}

TEST(ProcessTableResults, AllColumns_UsingSearchPattern_FALSE) {
  ProcessTableResultsHelper("%");
}

TEST(ProcessTableResults, AllColumns_UsingSearchPattern_TRUE) {
  ProcessTableResultsHelper("%", SQL_TRUE);
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_FALSE) {
  ProcessTableResultsHelper("StringField");
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_TRUE) {
  ProcessTableResultsHelper("StringField", SQL_TRUE);
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn) {
  ProcessTableResultsHelper("IntField");
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn_TRUE) {
  ProcessTableResultsHelper("IntField", SQL_TRUE);
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_SP1) {
  ProcessTableResultsHelper("%StringField");
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_SP1_TRUE) {
  ProcessTableResultsHelper("%StringField", SQL_TRUE);
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn_SP1) {
  ProcessTableResultsHelper("%IntField");
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn_SP1_TRUE) {
  ProcessTableResultsHelper("%IntField", SQL_TRUE);
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_SP2) {
  ProcessTableResultsHelper("StringField%");
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_SP2_TRUE) {
  ProcessTableResultsHelper("StringField%", SQL_TRUE);
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn_SP2) {
  ProcessTableResultsHelper("IntField%");
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn_SP2_TRUE) {
  ProcessTableResultsHelper("IntField%", TRUE);
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_SP3) {
  ProcessTableResultsHelper("%StringField%");
}

TEST(ProcessTableResults, SpecificColumn_FirstColumn_SP3_TRUE) {
  ProcessTableResultsHelper("%StringField%", TRUE);
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn_SP3) {
  ProcessTableResultsHelper("%IntField%");
}

TEST(ProcessTableResults, SpecificColumn_SecondColumn_SP3_TRUE) {
  ProcessTableResultsHelper("%IntField%", TRUE);
}

}  // namespace google::cloud::odbc_bq_driver_internal
