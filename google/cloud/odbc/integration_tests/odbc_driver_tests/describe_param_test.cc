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
  SQLSMALLINT param_size_source = 0;
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

void CheckPrecision(std::shared_ptr<ODBCHandles> conn, SQLSMALLINT param_number,
                    SQLSMALLINT expected) {
  SQLSMALLINT out_desc_precision;
  SQLRETURN status =
      SQLGetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                      &out_desc_precision, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_PRECISION)", conn);
  EXPECT_EQ(expected, out_desc_precision);
}

void CheckScale(std::shared_ptr<ODBCHandles> conn, SQLSMALLINT param_number,
                SQLSMALLINT expected) {
  SQLSMALLINT out_desc_scale;
  SQLRETURN status = SQLGetDescField(conn->ipd, param_number, SQL_DESC_SCALE,
                                     &out_desc_scale, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_SCALE)", conn);
  EXPECT_EQ(expected, out_desc_scale);
}

void CheckLength(std::shared_ptr<ODBCHandles> conn, SQLSMALLINT param_number,
                 SQLULEN expected) {
  SQLSMALLINT out_desc_len;
  SQLRETURN status = SQLGetDescField(conn->ipd, param_number, SQL_DESC_LENGTH,
                                     &out_desc_len, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
  EXPECT_EQ(expected, out_desc_len);
}

void CheckExpectedResults(std::shared_ptr<ODBCHandles> conn,
                          SQLSMALLINT param_number, SQLSMALLINT sql_type,
                          SQLULEN param_size, SQLSMALLINT decimal_digits,
                          SQLSMALLINT nullable) {
  auto expected_result = kExpectedResults[param_number - 1];
  SQLSMALLINT out_concise_c_type;
  SQLRETURN status =
      SQLGetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                      &out_concise_c_type, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_CONCISE_TYPE)", conn);
  EXPECT_EQ(sql_type, out_concise_c_type);

  if (expected_result.param_size_source == SQL_DESC_PRECISION) {
    CheckPrecision(conn, param_number, param_size);
  } else if (expected_result.param_size_source == SQL_DESC_LENGTH) {
    CheckLength(conn, param_number, param_size);
  }

  if (expected_result.decimal_digits_source == SQL_DESC_PRECISION) {
    CheckPrecision(conn, param_number, decimal_digits);
  } else if (expected_result.decimal_digits_source == SQL_DESC_SCALE) {
    CheckScale(conn, param_number, decimal_digits);
  }

  SQLSMALLINT out_nullable;
  status = SQLGetDescField(conn->ipd, param_number, SQL_DESC_NULLABLE,
                           &out_nullable, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_NULLABLE)", conn);
  EXPECT_EQ(nullable, out_nullable);
}

std::string ConstructColumnName(int i) {
  std::string name = "col_" + kExpectedResults[i].bq_type + " ";
  std::replace_if(name.begin(), name.end(), ::ispunct, '_');
  std::remove_if(name.begin(), name.end(), ::isblank);
  return name;
}

TEST(SQLDescribeParam, DescribeAllParams) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto table_name = kDatasetWithTablePrefix + "ODBC_DESCRIBE_PARAMS_TEST";
  Table table(table_name);
  std::string table_schema =
      "(" + ConstructColumnName(0) + kExpectedResults[0].bq_type;
  std::string params = "?";
  for (int i = 1; i < kExpectedResults.size(); i++) {
    table_schema.append(", " + ConstructColumnName(i) +
                        kExpectedResults[i].bq_type);
    params.append(", ?");
  }
  table_schema.append(")");
  table.Create(conn, table_schema);

  auto insert_stmt = "INSERT INTO " + table_name + " VALUES (" + params + ")";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_params;
  status = SQLNumParams(conn->hstmt, &num_params);
  CheckError(status, "SQLNumParams", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);

  for (int i = 1; i <= num_params; i++) {
    SQLSMALLINT data_type = 0;
    SQLULEN param_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;

    status = SQLDescribeParam(conn->hstmt, i, &data_type, &param_size,
                              &decimal_digits, &nullable);
    CheckError(status, "SQLDescribeParam[" + std::to_string(i) + "]", conn);

    std::cout << "Checking param number: " << i << "\n";
    CheckExpectedResults(conn, i, data_type, param_size, decimal_digits,
                         nullable);
  }

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
