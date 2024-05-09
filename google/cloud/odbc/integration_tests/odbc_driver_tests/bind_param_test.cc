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

class BindParameterParameterizedTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutANSI,
                         BindParameterParameterizedTest,
                         testing::Values(false, true));

void RandomiseDescriptorAttributes(std::shared_ptr<ODBCHandles> conn,
                                   SQLUSMALLINT param_number, bool use_ansi) {
  SQLRETURN status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC,
                                 &conn->apd, 0, NULL, use_ansi);
  CheckError(status, "SQLGetStmtAttr", conn);
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, use_ansi);
  CheckError(status, "SQLGetStmtAttr", conn);
  RandomizeDefaultValues(conn->apd, param_number);
  RandomizeDefaultValues(conn->ipd, param_number);
}

///////////////////////////////////////////////////////////////////////
//  Check all SQL types except datetime and interval types
///////////////////////////////////////////////////////////////////////

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
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check in_out_type behavior
  SQLSMALLINT desc_in_out_type = 0;
  GetDescField(conn->ipd, param_number, SQL_DESC_PARAMETER_TYPE,
               &desc_in_out_type, 0, NULL, GetParam());
  EXPECT_EQ(in_out_type, desc_in_out_type);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  // Check param_val behavior
  SQLPOINTER desc_data_ptr = nullptr;
  GetDescField(conn->apd, param_number, SQL_DESC_DATA_PTR, &desc_data_ptr, 0,
               NULL, GetParam());
  EXPECT_EQ(&param_val, desc_data_ptr);

  // Check buff_len behavior
  SQLLEN desc_octet_length = 0;
  GetDescField(conn->apd, param_number, SQL_DESC_OCTET_LENGTH,
               &desc_octet_length, 0, NULL, GetParam());
  EXPECT_EQ(buff_len, desc_octet_length);

  // Check str_len behavior
  SQLPOINTER desc_octet_length_ptr = nullptr;
  GetDescField(conn->apd, param_number, SQL_DESC_OCTET_LENGTH_PTR,
               &desc_octet_length_ptr, 0, NULL, GetParam());
  EXPECT_EQ(&str_len, desc_octet_length_ptr);
  SQLPOINTER desc_indicator_ptr = nullptr;
  GetDescField(conn->apd, param_number, SQL_DESC_INDICATOR_PTR,
               &desc_indicator_ptr, 0, NULL, GetParam());
  EXPECT_EQ(&str_len, desc_indicator_ptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_VARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_VARCHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_LONGVARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_VARCHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_BINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_BINARY;
  SQLSMALLINT param_type = SQL_BINARY;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_VARBINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_BINARY;
  SQLSMALLINT param_type = SQL_VARBINARY;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_LONGVARBINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_BINARY;
  SQLSMALLINT param_type = SQL_LONGVARBINARY;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_DECIMAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_DECIMAL;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_NUMERIC;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

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
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 24, GetParam());
  CheckLength(conn->apd, 24, GetParam());
  CheckPrecision(conn->apd, 24, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 14, GetParam());
  CheckLength(conn->ipd, 7, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 24, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_FLOAT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_DOUBLE;
  SQLSMALLINT param_type = SQL_FLOAT;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 53, GetParam());
  CheckLength(conn->apd, 53, GetParam());
  CheckPrecision(conn->apd, 53, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 24, GetParam());
  CheckLength(conn->ipd, 15, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 53, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_DOUBLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_DOUBLE;
  SQLSMALLINT param_type = SQL_DOUBLE;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 53, GetParam());
  CheckLength(conn->apd, 53, GetParam());
  CheckPrecision(conn->apd, 53, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 24, GetParam());
  CheckLength(conn->ipd, 15, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 53, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_WCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_WCHAR;
  SQLSMALLINT param_type = SQL_WCHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_WVARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_WCHAR;
  SQLSMALLINT param_type = SQL_WVARCHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_WLONGVARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_WCHAR;
  SQLSMALLINT param_type = SQL_WLONGVARCHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, col_size, GetParam());
  CheckLength(conn->ipd, col_size, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, col_size, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_BIT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_BIT;
  SQLSMALLINT param_type = SQL_BIT;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 1, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_TINYINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_STINYINT;
  SQLSMALLINT param_type = SQL_TINYINT;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 3, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_SMALLINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_SSHORT;
  SQLSMALLINT param_type = SQL_SMALLINT;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 5, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTEGER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_SLONG;
  SQLSMALLINT param_type = SQL_INTEGER;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 10, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_BIGINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_SBIGINT;
  SQLSMALLINT param_type = SQL_BIGINT;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 19, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_GUID) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_GUID;
  SQLSMALLINT param_type = SQL_GUID;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, value_type, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 16, GetParam());
  CheckLength(conn->apd, 16, GetParam());
  CheckPrecision(conn->apd, 16, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, param_type, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 36, GetParam());
  CheckLength(conn->ipd, 36, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 36, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////
//  Check all SQL datetime types
///////////////////////////////////////////////////////////////////////

TEST_P(BindParameterParameterizedTest, Bind_SQL_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_TYPE_DATE;
  SQLSMALLINT param_type = SQL_TYPE_DATE;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 2;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_DATE, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DATE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 10, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;
  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_TYPE_TIME;
  SQLSMALLINT param_type = SQL_TYPE_TIME;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 3;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_TIME, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_TIME, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 12, GetParam());
  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, decimal_digits, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Check_SQL_LENGTH_For_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;
  for (SQLSMALLINT decimal_digits = 0; decimal_digits < 10; decimal_digits++) {
    SQLUSMALLINT param_number = 1;
    SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
    SQLSMALLINT value_type = SQL_C_TYPE_TIME;
    SQLSMALLINT param_type = SQL_TYPE_TIME;
    SQLULEN col_size = 10;
    SQLINTEGER param_val = 30;
    SQLLEN buff_len = 40;
    SQLLEN str_len = 50;
    status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                         NULL, GetParam());
    CheckError(status, "SQLGetStmtAttr", conn);

    status = SQLBindParameter(conn->hstmt, param_number, in_out_type,
                              value_type, param_type, col_size, decimal_digits,
                              &param_val, buff_len, &str_len);
    CheckError(status, "SQLBindParameter", conn);

    CheckLength(conn->ipd, (decimal_digits == 0) ? 8 : 9 + decimal_digits,
                GetParam());
  }
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
  SQLSMALLINT decimal_digits = 3;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_TIMESTAMP, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 6, GetParam());
  CheckScale(conn->apd, 6, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_TIMESTAMP, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 23, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, decimal_digits, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest,
       Check_SQL_LENGTH_For_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;
  for (SQLSMALLINT decimal_digits = 0; decimal_digits < 10; decimal_digits++) {
    SQLUSMALLINT param_number = 1;
    SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
    SQLSMALLINT value_type = SQL_C_TYPE_TIMESTAMP;
    SQLSMALLINT param_type = SQL_TYPE_TIMESTAMP;
    SQLULEN col_size = 10;
    SQLINTEGER param_val = 30;
    SQLLEN buff_len = 40;
    SQLLEN str_len = 50;
    status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                         NULL, GetParam());
    CheckError(status, "SQLGetStmtAttr", conn);

    status = SQLBindParameter(conn->hstmt, param_number, in_out_type,
                              value_type, param_type, col_size, decimal_digits,
                              &param_val, buff_len, &str_len);
    CheckError(status, "SQLBindParameter", conn);

    CheckLength(conn->ipd, (decimal_digits == 0) ? 19 : 20 + decimal_digits,
                GetParam());
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////
//  Check all SQL interval types
///////////////////////////////////////////////////////////////////////

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
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_MONTH, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_MONTH, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 2, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_YEAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_YEAR;
  SQLSMALLINT param_type = SQL_INTERVAL_YEAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_YEAR, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_YEAR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 2, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_YEAR_TO_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_YEAR_TO_MONTH;
  SQLSMALLINT param_type = SQL_INTERVAL_YEAR_TO_MONTH;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_YEAR_TO_MONTH, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_YEAR_TO_MONTH, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 5, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_DAY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_DAY;
  SQLSMALLINT param_type = SQL_INTERVAL_DAY;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_DAY, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 2, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_HOUR;
  SQLSMALLINT param_type = SQL_INTERVAL_HOUR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_HOUR, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_HOUR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 2, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_MINUTE;
  SQLSMALLINT param_type = SQL_INTERVAL_MINUTE;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 2, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_SECOND;
  SQLSMALLINT param_type = SQL_INTERVAL_SECOND;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 3;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 6, GetParam());
  CheckScale(conn->apd, 6, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 6, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, decimal_digits, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest,
       Check_SQL_LENGTH_For_SQL_INTERVAL_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;
  for (SQLSMALLINT decimal_digits = 0; decimal_digits < 10; decimal_digits++) {
    SQLUSMALLINT param_number = 1;
    SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
    SQLSMALLINT value_type = SQL_C_INTERVAL_SECOND;
    SQLSMALLINT param_type = SQL_INTERVAL_SECOND;
    SQLULEN col_size = 10;
    SQLINTEGER param_val = 30;
    SQLLEN buff_len = 40;
    SQLLEN str_len = 50;
    status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                         NULL, GetParam());
    CheckError(status, "SQLGetStmtAttr", conn);

    status = SQLBindParameter(conn->hstmt, param_number, in_out_type,
                              value_type, param_type, col_size, decimal_digits,
                              &param_val, buff_len, &str_len);
    CheckError(status, "SQLBindParameter", conn);

    CheckLength(conn->ipd, (decimal_digits == 0) ? 2 : 3 + decimal_digits,
                GetParam());
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_DAY_TO_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_DAY_TO_HOUR;
  SQLSMALLINT param_type = SQL_INTERVAL_DAY_TO_HOUR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_DAY_TO_HOUR, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY_TO_HOUR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 5, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_DAY_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_DAY_TO_MINUTE;
  SQLSMALLINT param_type = SQL_INTERVAL_DAY_TO_MINUTE;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_DAY_TO_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY_TO_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 8, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_DAY_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_DAY_TO_SECOND;
  SQLSMALLINT param_type = SQL_INTERVAL_DAY_TO_SECOND;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 3;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_DAY_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 6, GetParam());
  CheckScale(conn->apd, 6, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 15, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, decimal_digits, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest,
       Check_SQL_LENGTH_For_SQL_INTERVAL_DAY_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;
  for (SQLSMALLINT decimal_digits = 0; decimal_digits < 10; decimal_digits++) {
    SQLUSMALLINT param_number = 1;
    SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
    SQLSMALLINT value_type = SQL_C_INTERVAL_DAY_TO_SECOND;
    SQLSMALLINT param_type = SQL_INTERVAL_DAY_TO_SECOND;
    SQLULEN col_size = 10;
    SQLINTEGER param_val = 30;
    SQLLEN buff_len = 40;
    SQLLEN str_len = 50;
    status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                         NULL, GetParam());
    CheckError(status, "SQLGetStmtAttr", conn);

    status = SQLBindParameter(conn->hstmt, param_number, in_out_type,
                              value_type, param_type, col_size, decimal_digits,
                              &param_val, buff_len, &str_len);
    CheckError(status, "SQLBindParameter", conn);

    CheckLength(conn->ipd, (decimal_digits == 0) ? 11 : 12 + decimal_digits,
                GetParam());
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_HOUR_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_HOUR_TO_MINUTE;
  SQLSMALLINT param_type = SQL_INTERVAL_HOUR_TO_MINUTE;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_HOUR_TO_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_HOUR_TO_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 5, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, 0, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, 0, GetParam());

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
  SQLSMALLINT decimal_digits = 3;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_HOUR_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 6, GetParam());
  CheckScale(conn->apd, 6, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_HOUR_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 12, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, decimal_digits, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest,
       Check_SQL_LENGTH_For_SQL_INTERVAL_HOUR_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;
  for (SQLSMALLINT decimal_digits = 0; decimal_digits < 10; decimal_digits++) {
    SQLUSMALLINT param_number = 1;
    SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
    SQLSMALLINT value_type = SQL_C_INTERVAL_HOUR_TO_SECOND;
    SQLSMALLINT param_type = SQL_INTERVAL_HOUR_TO_SECOND;
    SQLULEN col_size = 10;
    SQLINTEGER param_val = 30;
    SQLLEN buff_len = 40;
    SQLLEN str_len = 50;
    status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                         NULL, GetParam());
    CheckError(status, "SQLGetStmtAttr", conn);

    status = SQLBindParameter(conn->hstmt, param_number, in_out_type,
                              value_type, param_type, col_size, decimal_digits,
                              &param_val, buff_len, &str_len);
    CheckError(status, "SQLBindParameter", conn);

    CheckLength(conn->ipd, (decimal_digits == 0) ? 8 : 9 + decimal_digits,
                GetParam());
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest, Bind_SQL_INTERVAL_MINUTE_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_INTERVAL_MINUTE_TO_SECOND;
  SQLSMALLINT param_type = SQL_INTERVAL_MINUTE_TO_SECOND;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 3;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn, param_number, GetParam());

  status = SQLBindParameter(conn->hstmt, param_number, in_out_type, value_type,
                            param_type, col_size, decimal_digits, &param_val,
                            buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  // Check value_type behavior
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, value_type, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_MINUTE_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 6, GetParam());
  CheckScale(conn->apd, 6, GetParam());

  //   Check param_type behavior
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, param_type, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_MINUTE_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckLength(conn->ipd, 9, GetParam());

  // Check col_size (or param_type) behavior
  CheckPrecision(conn->ipd, decimal_digits, GetParam());
  // Check decimal_digits (or param_type) behavior
  CheckScale(conn->ipd, decimal_digits, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(BindParameterParameterizedTest,
       Check_SQL_LENGTH_For_SQL_INTERVAL_MINUTE_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;
  for (SQLSMALLINT decimal_digits = 0; decimal_digits < 10; decimal_digits++) {
    SQLUSMALLINT param_number = 1;
    SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
    SQLSMALLINT value_type = SQL_C_INTERVAL_MINUTE_TO_SECOND;
    SQLSMALLINT param_type = SQL_INTERVAL_MINUTE_TO_SECOND;
    SQLULEN col_size = 10;
    SQLINTEGER param_val = 30;
    SQLLEN buff_len = 40;
    SQLLEN str_len = 50;
    status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                         NULL, GetParam());
    CheckError(status, "SQLGetStmtAttr", conn);

    status = SQLBindParameter(conn->hstmt, param_number, in_out_type,
                              value_type, param_type, col_size, decimal_digits,
                              &param_val, buff_len, &str_len);
    CheckError(status, "SQLBindParameter", conn);

    CheckLength(conn->ipd, (decimal_digits == 0) ? 5 : 6 + decimal_digits,
                GetParam());
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
