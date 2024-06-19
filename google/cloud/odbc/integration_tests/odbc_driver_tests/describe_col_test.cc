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

#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include <gtest/gtest.h>
#include <regex>

namespace google::cloud::odbc_tests {

#ifndef BQ_DRIVER_INTEGRATION_TESTS

struct ExpectedResults {
  std::string bq_type;
  SQLSMALLINT column_size_source = 0;
  SQLSMALLINT decimal_digits_source = 0;
};

static std::vector<ExpectedResults> const kExpectedResults = {
    {"BIGNUMERIC", SQL_DESC_PRECISION, SQL_DESC_SCALE},
    {"BOOL", SQL_DESC_PRECISION, SQL_DESC_SCALE},
    {"BYTES", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"DATE", SQL_DESC_LENGTH, SQL_DESC_PRECISION},
    {"DATETIME", SQL_DESC_LENGTH, SQL_DESC_PRECISION},
    {"FLOAT64", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"GEOGRAPHY", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"INT64", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"INTERVAL", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"JSON", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"NUMERIC", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"RANGE<DATE>", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"RANGE<DATETIME>", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"RANGE<TIMESTAMP>", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"STRING", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"TIME", SQL_DESC_LENGTH, SQL_DESC_PRECISION},
    {"TIMESTAMP", SQL_DESC_LENGTH, SQL_DESC_PRECISION},
    {"STRUCT<x INT64, y STRING>", SQL_DESC_LENGTH, SQL_DESC_SCALE},
    {"ARRAY<INT64>", SQL_DESC_LENGTH, SQL_DESC_SCALE},
};

void ValidatePrecision(std::shared_ptr<ODBCHandles> conn,
                       SQLSMALLINT column_number, SQLSMALLINT expected) {
  SQLSMALLINT out_desc_precision;
  SQLRETURN status =
      SQLGetDescField(conn->ird, column_number, SQL_DESC_PRECISION,
                      &out_desc_precision, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_PRECISION)", conn);
  EXPECT_EQ(expected, out_desc_precision);
}

void ValidateScale(std::shared_ptr<ODBCHandles> conn, SQLSMALLINT column_number,
                   SQLSMALLINT expected) {
  SQLSMALLINT out_desc_scale;
  SQLRETURN status = SQLGetDescField(conn->ird, column_number, SQL_DESC_SCALE,
                                     &out_desc_scale, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_SCALE)", conn);
  EXPECT_EQ(expected, out_desc_scale);
}

void ValidateLength(std::shared_ptr<ODBCHandles> conn,
                    SQLSMALLINT column_number, SQLULEN expected) {
  SQLSMALLINT out_desc_len;
  SQLRETURN status = SQLGetDescField(conn->ird, column_number, SQL_DESC_LENGTH,
                                     &out_desc_len, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
  EXPECT_EQ(expected, out_desc_len);
}

void ValidateExpectedResults(std::shared_ptr<ODBCHandles> conn,
                             SQLCHAR column_name[15],
                             SQLSMALLINT column_name_Le,
                             SQLSMALLINT column_number, SQLSMALLINT sql_type,
                             SQLULEN column_size, SQLSMALLINT decimal_digits,
                             SQLSMALLINT nullable) {
  auto expected_result = kExpectedResults[column_number - 1];
  SQLSMALLINT out_concise_c_type;
  SQLRETURN status =
      SQLGetDescField(conn->ird, column_number, SQL_DESC_CONCISE_TYPE,
                      &out_concise_c_type, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_CONCISE_TYPE)", conn);
  EXPECT_EQ(sql_type, out_concise_c_type);

  if (expected_result.column_size_source == SQL_DESC_PRECISION) {
    ValidatePrecision(conn, column_number, column_size);
  } else if (expected_result.column_size_source == SQL_DESC_LENGTH) {
    ValidateLength(conn, column_number, column_size);
  }

  if (expected_result.decimal_digits_source == SQL_DESC_PRECISION) {
    ValidatePrecision(conn, column_number, decimal_digits);
  } else if (expected_result.decimal_digits_source == SQL_DESC_SCALE) {
    ValidateScale(conn, column_number, decimal_digits);
  }

  SQLSMALLINT out_nullable;
  status = SQLGetDescField(conn->ird, column_number, SQL_DESC_NULLABLE,
                           &out_nullable, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_NULLABLE)", conn);
  EXPECT_EQ(nullable, out_nullable);

  SQLCHAR out_column_Name[20];
  SQLINTEGER str_len = 0;
  status = SQLGetDescField(conn->ird, 1, SQL_DESC_NAME, &out_column_Name,
                           kBufferLength, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_NAME)", conn);
  EXPECT_STREQ((char const*)out_column_Name, (char const*)column_name);
  EXPECT_EQ(str_len, column_name_Le);
}

std::string CreateColumnName(int i) {
  std::string name = "col_" + kExpectedResults[i].bq_type + " ";
  std::replace_if(name.begin(), name.end(), ::ispunct, '_');
  std::remove_if(name.begin(), name.end(), ::isblank);
  return name;
}

TEST(SQLDescribeColumn, DescribeAllParams) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto table_name = kDatasetWithTablePrefix + "ODBC_DESCRIBE_COLUMNS_TEST";
  Table table(table_name);
  std::string table_schema =
      "(" + CreateColumnName(0) + kExpectedResults[0].bq_type;
  std::string params = "?";
  for (int i = 1; i < kExpectedResults.size(); i++) {
    table_schema.append(", " + CreateColumnName(i) +
                        kExpectedResults[i].bq_type);
    params.append(", ?");
  }
  table_schema.append(")");
  table.Create(conn, table_schema);

  auto insert_stmt = "INSERT INTO " + table_name + " VALUES (" + params + ")";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_columns;
  //status = SQLNumResultCols(conn->hstmt, &num_columns);
  //CheckError(status, "SQLNumParams", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);

  status = SQLGetDescField(conn->ird, 1, SQL_DESC_COUNT, &num_columns, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);

 // EXPECT_TRUE(num_columns > 0);

  for (int i = 1; i <= num_columns; i++) {
    SQLSMALLINT data_type = 0;
    SQLULEN column_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    SQLCHAR column_name[15];
    SQLSMALLINT column_name_Le = 0;

    status =
        SQLDescribeCol(conn->hstmt, i, column_name, 20, &column_name_Le,
                       &data_type, &column_size, &decimal_digits, &nullable);
    CheckError(status, "SQLDescribeCol[" + std::to_string(i) + "]", conn);

    std::cout << "Checking param number: " << i << "\n";
    ValidateExpectedResults(conn, column_name, column_name_Le, i, data_type,
                            column_size, decimal_digits, nullable);
  }

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
