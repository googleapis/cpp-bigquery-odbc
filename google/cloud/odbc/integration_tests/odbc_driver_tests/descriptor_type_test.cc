// Copyright 2023 Google LLC
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

namespace google::cloud::odbc_tests {

class DescriptorTypeParameterizedTest : public ::testing::TestWithParam<bool> {
};

INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutANSI,
                         DescriptorTypeParameterizedTest,
                         testing::Values(false, true));

void CheckType(SQLHDESC desc, SQLSMALLINT type_expected, bool use_ansi) {
  SQLSMALLINT type;
  GetDescField(desc, 1, SQL_DESC_TYPE, &type, 0, NULL, use_ansi);
  EXPECT_EQ(type_expected, type);
}

void CheckConciseType(SQLHDESC desc, SQLSMALLINT concise_type_expected,
                      bool use_ansi) {
  SQLSMALLINT concise_type;
  GetDescField(desc, 1, SQL_DESC_CONCISE_TYPE, &concise_type, 0, NULL,
               use_ansi);
  EXPECT_EQ(concise_type_expected, concise_type);
}

void CheckDatetimeIntervalPrecision(
    SQLHDESC desc, SQLSMALLINT datetime_interval_precision_expected,
    bool use_ansi) {
  SQLINTEGER datetime_interval_precision = 0;
  GetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_PRECISION,
               &datetime_interval_precision, 0, NULL, use_ansi);
  EXPECT_EQ(datetime_interval_precision_expected, datetime_interval_precision);
}

void CheckPrecision(SQLHDESC desc, SQLSMALLINT precision_expected,
                    bool use_ansi) {
  SQLSMALLINT precision = 0;
  GetDescField(desc, 1, SQL_DESC_PRECISION, &precision, 0, NULL, use_ansi);
  EXPECT_EQ(precision_expected, precision);
}

void CheckLength(SQLHDESC desc, SQLULEN length_expected, bool use_ansi) {
  SQLULEN length = 0;
  GetDescField(desc, 1, SQL_DESC_LENGTH, &length, 0, NULL, use_ansi);
  EXPECT_EQ(length_expected, length);
}

void CheckDatetimeIntervalCode(SQLHDESC desc,
                               SQLSMALLINT datetime_interval_code_expected,
                               bool use_ansi) {
  SQLSMALLINT datetime_interval_code = 0;
  GetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
               &datetime_interval_code, 0, NULL, use_ansi);
  EXPECT_EQ(datetime_interval_code_expected, datetime_interval_code);
}

void CheckScale(SQLHDESC desc, SQLSMALLINT scale_expected, bool use_ansi) {
  SQLSMALLINT scale = 0;
  GetDescField(desc, 1, SQL_DESC_SCALE, &scale, 0, NULL, use_ansi);
  EXPECT_EQ(scale_expected, scale);
}

///////////////////////////////////////////////////////////////////////
//  Set SQL_DESC_TYPE for C types except datetime and interval types
///////////////////////////////////////////////////////////////////////

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_CHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_C_CHAR,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_CHAR, GetParam());
  CheckConciseType(conn->apd, SQL_C_CHAR, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_WCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_C_WCHAR,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_WCHAR, GetParam());
  CheckConciseType(conn->apd, SQL_C_WCHAR, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// It works same for: SQL_C_USHORT, SQL_C_SLONG, SQL_C_ULONG, SQL_C_STINYINT,
// SQL_C_UTINYINT, SQL_C_SBIGINT, SQL_C_UBIGINT
TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_SSHORT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE,
                           (SQLPOINTER)SQL_C_SSHORT, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_SSHORT, GetParam());
  CheckConciseType(conn->apd, SQL_C_SSHORT, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_FLOAT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_C_FLOAT,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_FLOAT, GetParam());
  CheckConciseType(conn->apd, SQL_C_FLOAT, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 24, GetParam());
  CheckPrecision(conn->apd, 24, GetParam());
  CheckLength(conn->apd, 24, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_DOUBLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE,
                           (SQLPOINTER)SQL_C_DOUBLE, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_DOUBLE, GetParam());
  CheckConciseType(conn->apd, SQL_C_DOUBLE, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 53, GetParam());
  CheckPrecision(conn->apd, 53, GetParam());
  CheckLength(conn->apd, 53, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_BIT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status =
      SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_C_BIT, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_BIT, GetParam());
  CheckConciseType(conn->apd, SQL_C_BIT, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_BINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE,
                           (SQLPOINTER)SQL_C_BINARY, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_BINARY, GetParam());
  CheckConciseType(conn->apd, SQL_C_BINARY, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_VARBOOKMARK) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE,
                           (SQLPOINTER)SQL_C_VARBOOKMARK, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_VARBOOKMARK, GetParam());
  CheckConciseType(conn->apd, SQL_C_VARBOOKMARK, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 1, GetParam());
  CheckPrecision(conn->apd, 1, GetParam());
  CheckLength(conn->apd, 1, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE,
                           (SQLPOINTER)SQL_C_NUMERIC, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_NUMERIC, GetParam());
  CheckConciseType(conn->apd, SQL_C_NUMERIC, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 38, GetParam());
  CheckPrecision(conn->apd, 38, GetParam());
  CheckLength(conn->apd, 38, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_C_GUID) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_C_GUID,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_C_GUID, GetParam());
  CheckConciseType(conn->apd, SQL_C_GUID, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 16, GetParam());
  CheckPrecision(conn->apd, 16, GetParam());
  CheckLength(conn->apd, 16, GetParam());
  CheckDatetimeIntervalCode(conn->apd, 0, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

//////////////////////////////////////////////////////////
//  Set SQL_DESC_CONCISE_TYPE for C datetime types
//////////////////////////////////////////////////////////

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_C_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_C_TYPE_DATE, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->apd, SQL_C_TYPE_DATE, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_DATE, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_C_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_C_TYPE_TIME, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->apd, SQL_C_TYPE_TIME, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_TIME, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_C_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_C_TYPE_TIMESTAMP, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->apd, SQL_C_TYPE_TIMESTAMP, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 0, GetParam());
  CheckPrecision(conn->apd, 6, GetParam());
  CheckLength(conn->apd, 0, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_TIMESTAMP, GetParam());
  CheckScale(conn->apd, 6, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

//////////////////////////////////////////////////////////
//  Set SQL_DESC_CONCISE_TYPE for C interval types
//////////////////////////////////////////////////////////

// It works same for: SQL_C_INTERVAL_YEAR, SQL_C_INTERVAL_YEAR_TO_MONTH,
// SQL_C_INTERVAL_DAY, SQL_C_INTERVAL_HOUR, SQL_C_INTERVAL_MINUTE,
// SQL_C_INTERVAL_DAY_TO_HOUR, SQL_C_INTERVAL_DAY_TO_MINUTE,
// SQL_C_INTERVAL_HOUR_TO_MINUTE
TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_C_INTERVAL_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_C_INTERVAL_MONTH, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, SQL_C_INTERVAL_MONTH, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 0, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_MONTH, GetParam());
  CheckScale(conn->apd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// It works same for: SQL_C_INTERVAL_DAY_TO_SECOND,
// SQL_C_INTERVAL_HOUR_TO_SECOND, SQL_C_INTERVAL_MINUTE_TO_SECOND
TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_C_INTERVAL_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->apd, 1);
  status = SQLSetDescField(conn->apd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_C_INTERVAL_SECOND, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->apd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->apd, SQL_C_INTERVAL_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->apd, 2, GetParam());
  CheckPrecision(conn->apd, 6, GetParam());
  CheckLength(conn->apd, 2, GetParam());
  CheckDatetimeIntervalCode(conn->apd, SQL_CODE_SECOND, GetParam());
  CheckScale(conn->apd, 6, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////
//  Set SQL_DESC_TYPE for SQL types except datetime and interval types
///////////////////////////////////////////////////////////////////////

// It works same for SQL_VARCHAR
TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_CHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status =
      SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_CHAR, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_CHAR, GetParam());
  CheckConciseType(conn->ipd, SQL_CHAR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 1, GetParam());
  CheckPrecision(conn->ipd, 1, GetParam());
  CheckLength(conn->ipd, 1, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// It works same for: SQL_WVARCHAR, SQL_WLONGVARCHAR
TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_WCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status =
      SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_WCHAR, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_WCHAR, GetParam());
  CheckConciseType(conn->ipd, SQL_WCHAR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, kLengthUnchanged, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_DECIMAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_DECIMAL,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_DECIMAL, GetParam());
  CheckConciseType(conn->ipd, SQL_DECIMAL, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 38, GetParam());
  CheckPrecision(conn->ipd, 38, GetParam());
  CheckLength(conn->ipd, 38, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_NUMERIC,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_NUMERIC, GetParam());
  CheckConciseType(conn->ipd, SQL_NUMERIC, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 38, GetParam());
  CheckPrecision(conn->ipd, 38, GetParam());
  CheckLength(conn->ipd, 38, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_SMALLINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE,
                           (SQLPOINTER)SQL_SMALLINT, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_SMALLINT, GetParam());
  CheckConciseType(conn->ipd, SQL_SMALLINT, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 5, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_INTEGER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_INTEGER,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTEGER, GetParam());
  CheckConciseType(conn->ipd, SQL_INTEGER, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 10, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_REAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status =
      SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_REAL, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_REAL, GetParam());
  CheckConciseType(conn->ipd, SQL_REAL, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 14, GetParam());
  CheckPrecision(conn->ipd, 24, GetParam());
  CheckLength(conn->ipd, 7, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_FLOAT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status =
      SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_FLOAT, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_FLOAT, GetParam());
  CheckConciseType(conn->ipd, SQL_FLOAT, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 24, GetParam());
  CheckPrecision(conn->ipd, 53, GetParam());
  CheckLength(conn->ipd, 15, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_DOUBLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_DOUBLE,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_DOUBLE, GetParam());
  CheckConciseType(conn->ipd, SQL_DOUBLE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 24, GetParam());
  CheckPrecision(conn->ipd, 53, GetParam());
  CheckLength(conn->ipd, 15, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_BIT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status =
      SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_BIT, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_BIT, GetParam());
  CheckConciseType(conn->ipd, SQL_BIT, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 1, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_TINYINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_TINYINT,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_TINYINT, GetParam());
  CheckConciseType(conn->ipd, SQL_TINYINT, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 3, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_BIGINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_BIGINT,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_BIGINT, GetParam());
  CheckConciseType(conn->ipd, SQL_BIGINT, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckLength(conn->ipd, 19, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// It works same for: SQL_VARBINARY, SQL_LONGVARBINARY
TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_BINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_BINARY,
                           NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_BINARY, GetParam());
  CheckConciseType(conn->ipd, SQL_BINARY, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 1, GetParam());
  CheckPrecision(conn->ipd, 1, GetParam());
  CheckLength(conn->ipd, 1, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetType_SQL_GUID) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status =
      SQLSetDescField(conn->ipd, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_GUID, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_GUID, GetParam());
  CheckConciseType(conn->ipd, SQL_GUID, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 36, GetParam());
  CheckPrecision(conn->ipd, 36, GetParam());
  CheckLength(conn->ipd, 36, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, 0, GetParam());
  CheckScale(conn->ipd, kScaleUnchanged, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

//////////////////////////////////////////////////////////
//  Set SQL_DESC_CONCISE_TYPE for SQL datetime types
//////////////////////////////////////////////////////////

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_TYPE_DATE, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->ipd, SQL_TYPE_DATE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 10, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DATE, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_TYPE_TIME, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->ipd, SQL_TYPE_TIME, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 8, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_TIME, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_TYPE_TIMESTAMP, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_DATETIME, GetParam());
  CheckConciseType(conn->ipd, SQL_TYPE_TIMESTAMP, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, kLengthUnchanged, GetParam());
  CheckPrecision(conn->ipd, 6, GetParam());
  CheckLength(conn->ipd, 26, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_TIMESTAMP, GetParam());
  CheckScale(conn->ipd, 6, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

//////////////////////////////////////////////////////////
//  Set SQL_DESC_CONCISE_TYPE for SQL interval types
//////////////////////////////////////////////////////////

// It works same for: SQL_INTERVAL_YEAR,
// SQL_INTERVAL_DAY, SQL_INTERVAL_HOUR, SQL_INTERVAL_MINUTE,
TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_INTERVAL_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_MONTH, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_MONTH, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 2, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_MONTH, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_INTERVAL_YEAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_YEAR, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_YEAR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 2, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_YEAR, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_INTERVAL_DAY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_DAY, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_DAY, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 2, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_INTERVAL_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_HOUR, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_HOUR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 2, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_HOUR, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_INTERVAL_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_MINUTE, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 2, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_MINUTE, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest,
       SetConciseType_SQL_INTERVAL_YEAR_TO_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_YEAR_TO_MONTH, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_YEAR_TO_MONTH, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 5, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_YEAR_TO_MONTH, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest,
       SetConciseType_SQL_INTERVAL_DAY_TO_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_DAY_TO_HOUR, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_DAY_TO_HOUR, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 5, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY_TO_HOUR, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest,
       SetConciseType_SQL_INTERVAL_HOUR_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_HOUR_TO_MINUTE, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_HOUR_TO_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 5, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_HOUR_TO_MINUTE, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest,
       SetConciseType_SQL_INTERVAL_DAY_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_DAY_TO_MINUTE, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_DAY_TO_MINUTE, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 0, GetParam());
  CheckLength(conn->ipd, 8, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY_TO_MINUTE, GetParam());
  CheckScale(conn->ipd, 0, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest, SetConciseType_SQL_INTERVAL_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_SECOND, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 6, GetParam());
  CheckLength(conn->ipd, 9, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_SECOND, GetParam());
  CheckScale(conn->ipd, 6, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest,
       SetConciseType_SQL_INTERVAL_DAY_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_DAY_TO_SECOND, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_DAY_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 6, GetParam());
  CheckLength(conn->ipd, 18, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_DAY_TO_SECOND, GetParam());
  CheckScale(conn->ipd, 6, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest,
       SetConciseType_SQL_INTERVAL_HOUR_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_HOUR_TO_SECOND, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_HOUR_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 6, GetParam());
  CheckLength(conn->ipd, 15, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_HOUR_TO_SECOND, GetParam());
  CheckScale(conn->ipd, 6, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(DescriptorTypeParameterizedTest,
       SetConciseType_SQL_INTERVAL_MINUTE_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Setting Field
  RandomizeDefaultValues(conn->ipd, 1);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_MINUTE_TO_SECOND, NULL);
  CheckError(status, "SQLSetDescField", conn);

  // Checking fields
  CheckType(conn->ipd, SQL_INTERVAL, GetParam());
  CheckConciseType(conn->ipd, SQL_INTERVAL_MINUTE_TO_SECOND, GetParam());
  CheckDatetimeIntervalPrecision(conn->ipd, 2, GetParam());
  CheckPrecision(conn->ipd, 6, GetParam());
  CheckLength(conn->ipd, 12, GetParam());
  CheckDatetimeIntervalCode(conn->ipd, SQL_CODE_MINUTE_TO_SECOND, GetParam());
  CheckScale(conn->ipd, 6, GetParam());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
