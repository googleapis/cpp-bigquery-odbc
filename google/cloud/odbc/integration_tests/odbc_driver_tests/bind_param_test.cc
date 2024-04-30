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

#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_tests {

#ifndef BQ_DRIVER_INTEGRATION_TESTS

inline constexpr int kPrecisionUnchanged = 111;
inline constexpr int kScaleUnchanged = 112;
inline constexpr int kDatetimePrecisionUnchanged = 113;
inline constexpr int kDatetimeCodeUnchanged = SQL_CODE_MINUTE_TO_SECOND;

class BindParameterParameterizedTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutANSI,
                         BindParameterParameterizedTest,
                         testing::Values(false, true));

void RandomizeDefaultValues(SQLHDESC desc, SQLUSMALLINT param_number) {
  ASSERT_EQ(SQL_SUCCESS,
            SQLSetDescField(desc, param_number, SQL_DESC_PRECISION,
                            (SQLPOINTER)kPrecisionUnchanged, NULL));
  ASSERT_EQ(SQL_SUCCESS, SQLSetDescField(desc, param_number, SQL_DESC_SCALE,
                                         (SQLPOINTER)kScaleUnchanged, NULL));
  ASSERT_EQ(
      SQL_SUCCESS,
      SQLSetDescField(desc, param_number, SQL_DESC_DATETIME_INTERVAL_PRECISION,
                      (SQLPOINTER)kDatetimePrecisionUnchanged, NULL));
  ASSERT_EQ(SQL_SUCCESS,
            SQLSetDescField(desc, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                            (SQLPOINTER)SQL_CODE_MINUTE_TO_SECOND, NULL));
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_NUMERIC;
  SQLSMALLINT param_type = SQL_NUMERIC;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check in_out_type behavior
  SQLSMALLINT desc_in_out_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PARAMETER_TYPE,
                        &desc_in_out_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(in_out_type, desc_in_out_type);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_NUMERIC, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_NUMERIC, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(38, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(38, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  //   Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_NUMERIC, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_NUMERIC, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(38, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(col_size, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_scale);

  // Check param_val behavior
  SQLPOINTER desc_data_ptr = nullptr;
  status = GetDescField(conn->apd, param_number, SQL_DESC_DATA_PTR,
                        &desc_data_ptr, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(&param_val, desc_data_ptr);

  // Check buff_len behavior
  SQLLEN desc_octet_length = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_OCTET_LENGTH,
                        &desc_octet_length, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(buff_len, desc_octet_length);

  // Check str_len behavior
  SQLPOINTER desc_octet_length_ptr = nullptr;
  status = GetDescField(conn->apd, param_number, SQL_DESC_OCTET_LENGTH_PTR,
                        &desc_octet_length_ptr, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(&str_len, desc_octet_length_ptr);
  SQLPOINTER desc_indicator_ptr = nullptr;
  status = GetDescField(conn->apd, param_number, SQL_DESC_INDICATOR_PTR,
                        &desc_indicator_ptr, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(&str_len, desc_indicator_ptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_TYPE_DATE;
  SQLSMALLINT param_type = SQL_TYPE_DATE;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  // Set fake data to check that it was updated
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DATETIME, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_TYPE_DATE, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_DATE, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  // Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DATETIME, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_TYPE_DATE, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_DATE, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_TYPE_TIMESTAMP;
  SQLSMALLINT param_type = SQL_TYPE_TIMESTAMP;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 5;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  // Set fake data to check that it was updated
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DATETIME, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_TYPE_TIMESTAMP, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_TIMESTAMP, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(6, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(6, desc_scale);

  // Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DATETIME, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_TYPE_TIMESTAMP, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_TIMESTAMP, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_scale);  // ???????????????????????????????

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_MONTH;
  SQLSMALLINT param_type = SQL_INTERVAL_MONTH;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  // Set fake data to check that it was updated
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTERVAL, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_INTERVAL_MONTH, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_MONTH, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(2, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  // Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTERVAL, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTERVAL_MONTH, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_MONTH, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(2, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_HOUR_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_HOUR_TO_SECOND;
  SQLSMALLINT param_type = SQL_INTERVAL_HOUR_TO_SECOND;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 4;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  // Set fake data to check that it was updated
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTERVAL, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_INTERVAL_HOUR_TO_SECOND, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_HOUR_TO_SECOND, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(2, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(6, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(6, desc_scale);

  // Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTERVAL, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTERVAL_HOUR_TO_SECOND, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_HOUR_TO_SECOND, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(2, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_DECIMAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_DOUBLE;
  SQLSMALLINT param_type = SQL_DECIMAL;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_DOUBLE, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_DOUBLE, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(53, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(53, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  //   Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DECIMAL, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DECIMAL, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(53, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(col_size, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_CHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_CHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_CHAR, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_CHAR, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(1, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(1, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  //   Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CHAR, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CHAR, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(1, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(col_size, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(kScaleUnchanged, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_TYPE_DATE;
  SQLSMALLINT param_type = SQL_TIMESTAMP;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 3;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DATETIME, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_TYPE_DATE, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_DATE, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  //   Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_DATETIME, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_TYPE_TIMESTAMP, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_CODE_TIMESTAMP, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_precision);

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(decimal_digits, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_REAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_FLOAT;
  SQLSMALLINT param_type = SQL_REAL;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_FLOAT, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_FLOAT, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(24, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(24, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  //   Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_REAL, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_REAL, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(24, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(24, desc_precision);  //                         ?????????????????
                                  //                         should be col_size

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(kScaleUnchanged, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTEGER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_SSHORT;
  SQLSMALLINT param_type = SQL_INTEGER;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_SSHORT, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_SSHORT, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  //   Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTEGER, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_INTEGER, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(kDatetimePrecisionUnchanged,
            desc_precision);  //     ????????? Why is not kPrecisionUnchanged

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(kScaleUnchanged, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_GUID) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_BINARY;
  SQLSMALLINT param_type = SQL_GUID;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  SQLSMALLINT desc_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_BINARY, desc_type);
  SQLSMALLINT desc_concise_type = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_C_BINARY, desc_concise_type);
  SQLSMALLINT desc_datetime_code = 0;
  status =
      GetDescField(conn->apd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  SQLSMALLINT desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(1, desc_datetime_precision);
  SQLSMALLINT desc_precision = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(1, desc_precision);
  SQLSMALLINT desc_scale = 0;
  status = GetDescField(conn->apd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_scale);

  //   Check param_type behavior
  desc_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_TYPE, &desc_type, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_GUID, desc_type);
  desc_concise_type = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_CONCISE_TYPE,
                        &desc_concise_type, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(SQL_GUID, desc_concise_type);
  desc_datetime_code = 0;
  status =
      GetDescField(conn->ipd, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                   &desc_datetime_code, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(0, desc_datetime_code);
  desc_datetime_precision = 0;
  status = GetDescField(conn->apd, param_number,
                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                        &desc_datetime_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(1, desc_datetime_precision);

  // Check col_size behavior
  desc_precision = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_PRECISION,
                        &desc_precision, 0, NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(
      36,
      desc_precision);  //                                       ?????????????

  // Check decimal_digits behavior
  desc_scale = 0;
  status = GetDescField(conn->ipd, param_number, SQL_DESC_SCALE, &desc_scale, 0,
                        NULL, GetParam());
  CheckError(status, "GetDescField", conn);
  EXPECT_EQ(kScaleUnchanged, desc_scale);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
