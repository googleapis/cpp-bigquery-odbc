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
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_tests {

// The IntegerField must be in ascending for have consistent ordering
StdRows const kBindParamTestData{
    {"Test String 1", 1, 1.1},
    {"21", 53, 5},
};

enum class DescriptorType { kAPD, kIPD };

static constexpr SQLULEN kColSize = 10;
static constexpr SQLSMALLINT kDecimalDigits = 3;

static std::map<SQLSMALLINT, ExpectedDescriptorConfig> const kImpDescTestMap = {
    {SQL_CHAR,
     {SQL_CHAR, SQL_CHAR, 0, kColSize, kColSize, kScaleUnchanged, kColSize}},
    {SQL_VARCHAR,
     {SQL_VARCHAR, SQL_VARCHAR, 0, kColSize, kColSize, kScaleUnchanged,
      kColSize}},
    {SQL_LONGVARCHAR,
     {SQL_LONGVARCHAR, SQL_LONGVARCHAR, 0, kColSize, kColSize, kScaleUnchanged,
      kColSize}},
    {SQL_BINARY,
     {SQL_BINARY, SQL_BINARY, 0, kColSize, kColSize, kScaleUnchanged,
      kColSize}},
    {SQL_VARBINARY,
     {SQL_VARBINARY, SQL_VARBINARY, 0, kColSize, kColSize, kScaleUnchanged,
      kColSize}},
    {SQL_LONGVARBINARY,
     {SQL_LONGVARBINARY, SQL_LONGVARBINARY, 0, kColSize, kColSize,
      kScaleUnchanged, kColSize}},
    {SQL_DECIMAL,
     {SQL_DECIMAL, SQL_DECIMAL, 0, kColSize, kColSize, kDecimalDigits,
      kColSize}},
    {SQL_NUMERIC,
     {SQL_NUMERIC, SQL_NUMERIC, 0, kColSize, kColSize, kDecimalDigits,
      kColSize}},
    {SQL_REAL, {SQL_REAL, SQL_REAL, 0, 7, 24, kScaleUnchanged, 14}},
    {SQL_FLOAT, {SQL_FLOAT, SQL_FLOAT, 0, 15, 53, kScaleUnchanged, 24}},
    {SQL_DOUBLE, {SQL_DOUBLE, SQL_DOUBLE, 0, 15, 53, kScaleUnchanged, 24}},
    {SQL_WCHAR,
     {SQL_WCHAR, SQL_WCHAR, 0, kColSize, kColSize, kScaleUnchanged, kColSize}},
    {SQL_WVARCHAR,
     {SQL_WVARCHAR, SQL_WVARCHAR, 0, kColSize, kColSize, kScaleUnchanged,
      kColSize}},
    {SQL_WLONGVARCHAR,
     {SQL_WLONGVARCHAR, SQL_WLONGVARCHAR, 0, kColSize, kColSize,
      kScaleUnchanged, kColSize}},
    {SQL_BIT,
     {SQL_BIT, SQL_BIT, 0, 1, kLengthUnchanged, kScaleUnchanged,
      kLengthUnchanged}},
    {SQL_TINYINT,
     {SQL_TINYINT, SQL_TINYINT, 0, 3, kLengthUnchanged, kScaleUnchanged,
      kLengthUnchanged}},
    {SQL_SMALLINT,
     {SQL_SMALLINT, SQL_SMALLINT, 0, 5, kLengthUnchanged, kScaleUnchanged,
      kLengthUnchanged}},
    {SQL_INTEGER,
     {SQL_INTEGER, SQL_INTEGER, 0, 10, kLengthUnchanged, kScaleUnchanged,
      kLengthUnchanged}},
    {SQL_BIGINT,
     {SQL_BIGINT, SQL_BIGINT, 0, 19, kLengthUnchanged, kScaleUnchanged,
      kLengthUnchanged}},
    {SQL_GUID, {SQL_GUID, SQL_GUID, 0, 36, 36, kScaleUnchanged, 36}},

    {SQL_TYPE_DATE,
     {SQL_DATETIME, SQL_TYPE_DATE, SQL_CODE_DATE, 10, 0, 0, kLengthUnchanged}},
    {SQL_TYPE_TIME,
     {SQL_DATETIME, SQL_TYPE_TIME, SQL_CODE_TIME, 12, kDecimalDigits,
      kDecimalDigits, kLengthUnchanged}},
    {SQL_TYPE_TIMESTAMP,
     {SQL_DATETIME, SQL_TYPE_TIMESTAMP, SQL_CODE_TIMESTAMP, 23, kDecimalDigits,
      kDecimalDigits, kLengthUnchanged}},

    {SQL_INTERVAL_MONTH,
     {SQL_INTERVAL, SQL_INTERVAL_MONTH, SQL_CODE_MONTH, 2, 0, 0, 2}},
    {SQL_INTERVAL_YEAR,
     {SQL_INTERVAL, SQL_INTERVAL_YEAR, SQL_CODE_YEAR, 2, 0, 0, 2}},
    {SQL_INTERVAL_YEAR_TO_MONTH,
     {SQL_INTERVAL, SQL_INTERVAL_YEAR_TO_MONTH, SQL_CODE_YEAR_TO_MONTH, 5, 0, 0,
      2}},
    {SQL_INTERVAL_DAY,
     {SQL_INTERVAL, SQL_INTERVAL_DAY, SQL_CODE_DAY, 2, 0, 0, 2}},
    {SQL_INTERVAL_HOUR,
     {SQL_INTERVAL, SQL_INTERVAL_HOUR, SQL_CODE_HOUR, 2, 0, 0, 2}},
    {SQL_INTERVAL_MINUTE,
     {SQL_INTERVAL, SQL_INTERVAL_MINUTE, SQL_CODE_MINUTE, 2, 0, 0, 2}},
    {SQL_INTERVAL_SECOND,
     {SQL_INTERVAL, SQL_INTERVAL_SECOND, SQL_CODE_SECOND, 6, kDecimalDigits,
      kDecimalDigits, 2}},
    {SQL_INTERVAL_DAY_TO_HOUR,
     {SQL_INTERVAL, SQL_INTERVAL_DAY_TO_HOUR, SQL_CODE_DAY_TO_HOUR, 5, 0, 0,
      2}},
    {SQL_INTERVAL_DAY_TO_MINUTE,
     {SQL_INTERVAL, SQL_INTERVAL_DAY_TO_MINUTE, SQL_CODE_DAY_TO_MINUTE, 8, 0, 0,
      2}},
    {SQL_INTERVAL_DAY_TO_SECOND,
     {SQL_INTERVAL, SQL_INTERVAL_DAY_TO_SECOND, SQL_CODE_DAY_TO_SECOND, 15,
      kDecimalDigits, kDecimalDigits, 2}},
    {SQL_INTERVAL_HOUR_TO_MINUTE,
     {SQL_INTERVAL, SQL_INTERVAL_HOUR_TO_MINUTE, SQL_CODE_HOUR_TO_MINUTE, 5, 0,
      0, 2}},
    {SQL_INTERVAL_HOUR_TO_SECOND,
     {SQL_INTERVAL, SQL_INTERVAL_HOUR_TO_SECOND, SQL_CODE_HOUR_TO_SECOND, 12,
      kDecimalDigits, kDecimalDigits, 2}},
    {SQL_INTERVAL_MINUTE_TO_SECOND,
     {SQL_INTERVAL, SQL_INTERVAL_MINUTE_TO_SECOND, SQL_CODE_MINUTE_TO_SECOND, 9,
      kDecimalDigits, kDecimalDigits, 2}},
};

void RandomiseDescriptorAttributes(std::shared_ptr<ODBCHandles> conn) {
  SQLRETURN status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);
  RandomizeDefaultValues(conn->apd, 1);
  RandomizeDefaultValues(conn->ipd, 1);
}

void CheckAttributes(std::shared_ptr<ODBCHandles> conn, SQLHDESC desc,
                     DescriptorType desc_type, SQLSMALLINT type) {
  auto expected = (desc_type == DescriptorType::kAPD)
                      ? kAppDescTestMap.at(type)
                      : kImpDescTestMap.at(type);
  SQLSMALLINT out_c_type;
  SQLRETURN status =
      SQLGetDescField(desc, 1, SQL_DESC_TYPE, &out_c_type, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_TYPE)", conn);
  EXPECT_EQ(expected.c_type, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescField(desc, 1, SQL_DESC_CONCISE_TYPE, &out_concise_c_type,
                           0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_CONCISE_TYPE)", conn);
  EXPECT_EQ(expected.concise_c_type, out_concise_c_type);

  SQLSMALLINT out_desc_datetime_code;
  status = SQLGetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
                           &out_desc_datetime_code, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATETIME_INTERVAL_CODE)", conn);
  EXPECT_EQ(expected.desc_datetime_interval_code, out_desc_datetime_code);

  SQLULEN out_desc_len;
  status = SQLGetDescField(desc, 1, SQL_DESC_LENGTH, &out_desc_len, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
  EXPECT_EQ(expected.desc_len, out_desc_len);

  SQLSMALLINT out_desc_precision;
  status = SQLGetDescField(desc, 1, SQL_DESC_PRECISION, &out_desc_precision, 0,
                           nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_PRECISION)", conn);
  EXPECT_EQ(expected.desc_precision, out_desc_precision);

  SQLSMALLINT out_desc_scale;
  status =
      SQLGetDescField(desc, 1, SQL_DESC_SCALE, &out_desc_scale, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_SCALE)", conn);
  EXPECT_EQ(expected.desc_scale, out_desc_scale);

  SQLINTEGER out_desc_datetime_precision;
  status = SQLGetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_PRECISION,
                           &out_desc_datetime_precision, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATETIME_INTERVAL_PRECISION)",
             conn);
  EXPECT_EQ(expected.desc_datetime_precision, out_desc_datetime_precision);
}

void CheckApdAttributes(std::shared_ptr<ODBCHandles> conn, SQLSMALLINT c_type) {
  CheckAttributes(conn, conn->apd, DescriptorType::kAPD, c_type);
}

void CheckIpdAttributes(std::shared_ptr<ODBCHandles> conn,
                        SQLSMALLINT sql_type) {
  CheckAttributes(conn, conn->ipd, DescriptorType::kIPD, sql_type);
}

void BindParameterAndTest(std::shared_ptr<ODBCHandles> conn,
                          SQLSMALLINT value_type, SQLSMALLINT param_type) {
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;
  RandomiseDescriptorAttributes(conn);

  SQLRETURN status = SQLBindParameter(conn->hstmt, 1, in_out_type, value_type,
                                      param_type, kColSize, kDecimalDigits,
                                      &param_val, buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  SQLSMALLINT desc_in_out_type = 0;
  status = SQLGetDescField(conn->ipd, 1, SQL_DESC_PARAMETER_TYPE,
                           &desc_in_out_type, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_PARAMETER_TYPE)", conn);
  EXPECT_EQ(in_out_type, desc_in_out_type);

  CheckApdAttributes(conn, value_type);
  CheckIpdAttributes(conn, param_type);

  SQLPOINTER desc_data_ptr = nullptr;
  status =
      SQLGetDescField(conn->apd, 1, SQL_DESC_DATA_PTR, &desc_data_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATA_PTR)", conn);
  EXPECT_EQ(&param_val, desc_data_ptr);

  SQLLEN desc_octet_length = 0;
  status = SQLGetDescField(conn->apd, 1, SQL_DESC_OCTET_LENGTH,
                           &desc_octet_length, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_OCTET_LENGTH)", conn);
  EXPECT_EQ(buff_len, desc_octet_length);

  SQLPOINTER desc_octet_length_ptr = nullptr;
  status = SQLGetDescField(conn->apd, 1, SQL_DESC_OCTET_LENGTH_PTR,
                           &desc_octet_length_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_OCTET_LENGTH_PTR)", conn);
  EXPECT_EQ(&str_len, desc_octet_length_ptr);

  SQLPOINTER desc_indicator_ptr = nullptr;
  status = SQLGetDescField(conn->apd, 1, SQL_DESC_INDICATOR_PTR,
                           &desc_indicator_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_INDICATOR_PTR)", conn);
  EXPECT_EQ(&str_len, desc_indicator_ptr);
}

void BindParameterForLength(std::shared_ptr<ODBCHandles> conn,
                            SQLSMALLINT value_type, SQLSMALLINT param_type,
                            SQLSMALLINT digits) {
  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  SQLRETURN status = SQLBindParameter(conn->hstmt, param_number, in_out_type,
                                      value_type, param_type, kColSize, digits,
                                      &param_val, buff_len, &str_len);
  CheckError(status, "SQLBindParameter", conn);

  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);
}

///////////////////////////////////////////////////////////////////////
//  Check all SQL types except datetime and interval types
///////////////////////////////////////////////////////////////////////

TEST(SQLBindParameter, Bind_SQL_CHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_CHAR, SQL_CHAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_VARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_CHAR, SQL_VARCHAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_LONGVARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_CHAR, SQL_LONGVARCHAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_BINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_BINARY, SQL_BINARY);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_VARBINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_BINARY, SQL_VARBINARY);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_LONGVARBINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_BINARY, SQL_LONGVARBINARY);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_DECIMAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_CHAR, SQL_DECIMAL);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_CHAR, SQL_NUMERIC);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_REAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_FLOAT, SQL_REAL);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_FLOAT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_DOUBLE, SQL_FLOAT);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_DOUBLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_DOUBLE, SQL_DOUBLE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// Not applicable for DriverManager testing.
// DriverManager sends SQL_C_WCHAR as SQL_C_CHAR for ANSI
// API version of SQLBindParameter so these tests will give the same
// results as when sending C type as SQL_C_CHAR.
// For more details take a look at the Driver Manager code
// https://github.com/openlink/iODBC/blob/c0b9ece094ec40bb72dcb4868a7507072ec221b7/iodbc/prepare.c#L445
#ifndef DRIVER_MANAGER_TESTING_ENABLED
TEST(SQLBindParameter, Bind_SQL_WCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_WCHAR, SQL_WCHAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_WVARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_WCHAR, SQL_WVARCHAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_WLONGVARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_WCHAR, SQL_WLONGVARCHAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#endif

TEST(SQLBindParameter, Bind_SQL_BIT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_BIT, SQL_BIT);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_TINYINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_STINYINT, SQL_TINYINT);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_SMALLINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_SSHORT, SQL_SMALLINT);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTEGER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_SLONG, SQL_INTEGER);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_BIGINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_SBIGINT, SQL_BIGINT);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_GUID) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_GUID, SQL_GUID);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////
//  Check all SQL datetime types
///////////////////////////////////////////////////////////////////////

TEST(SQLBindParameter, Bind_SQL_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_TYPE_DATE, SQL_TYPE_DATE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_TYPE_TIME, SQL_TYPE_TIME);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Check_SQL_LENGTH_For_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  for (SQLSMALLINT digits = 0; digits < 10; digits++) {
    BindParameterForLength(conn, SQL_C_TYPE_TIME, SQL_TYPE_TIME, digits);

    SQLULEN length = 0;
    SQLRETURN status =
        SQLGetDescField(conn->ipd, 1, SQL_DESC_LENGTH, &length, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
    EXPECT_EQ((digits == 0) ? 8 : 9 + digits, length);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Check_SQL_LENGTH_For_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  for (SQLSMALLINT digits = 0; digits < 10; digits++) {
    BindParameterForLength(conn, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP,
                           digits);

    SQLULEN length = 0;
    SQLRETURN status =
        SQLGetDescField(conn->ipd, 1, SQL_DESC_LENGTH, &length, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
    EXPECT_EQ((digits == 0) ? 19 : 20 + digits, length);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////
//  Check all SQL interval types
///////////////////////////////////////////////////////////////////////

TEST(SQLBindParameter, Bind_SQL_INTERVAL_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_MONTH, SQL_INTERVAL_MONTH);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_YEAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_YEAR, SQL_INTERVAL_YEAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_YEAR_TO_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_YEAR_TO_MONTH,
                       SQL_INTERVAL_YEAR_TO_MONTH);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_DAY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_DAY, SQL_INTERVAL_DAY);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_HOUR, SQL_INTERVAL_HOUR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_MINUTE, SQL_INTERVAL_MINUTE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_SECOND, SQL_INTERVAL_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Check_SQL_LENGTH_For_SQL_INTERVAL_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  for (SQLSMALLINT digits = 0; digits < 10; digits++) {
    BindParameterForLength(conn, SQL_C_INTERVAL_SECOND, SQL_INTERVAL_SECOND,
                           digits);

    SQLULEN length = 0;
    SQLRETURN status =
        SQLGetDescField(conn->ipd, 1, SQL_DESC_LENGTH, &length, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
    EXPECT_EQ((digits == 0) ? 2 : 3 + digits, length);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_DAY_TO_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_DAY_TO_HOUR,
                       SQL_INTERVAL_DAY_TO_HOUR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_DAY_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_DAY_TO_MINUTE,
                       SQL_INTERVAL_DAY_TO_MINUTE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_DAY_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_DAY_TO_SECOND,
                       SQL_INTERVAL_DAY_TO_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Check_SQL_LENGTH_For_SQL_INTERVAL_DAY_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  for (SQLSMALLINT digits = 0; digits < 10; digits++) {
    BindParameterForLength(conn, SQL_C_INTERVAL_DAY_TO_SECOND,
                           SQL_INTERVAL_DAY_TO_SECOND, digits);

    SQLULEN length = 0;
    SQLRETURN status =
        SQLGetDescField(conn->ipd, 1, SQL_DESC_LENGTH, &length, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
    EXPECT_EQ((digits == 0) ? 11 : 12 + digits, length);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_HOUR_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_HOUR_TO_MINUTE,
                       SQL_INTERVAL_HOUR_TO_MINUTE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_HOUR_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_HOUR_TO_SECOND,
                       SQL_INTERVAL_HOUR_TO_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Check_SQL_LENGTH_For_SQL_INTERVAL_HOUR_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  for (SQLSMALLINT digits = 0; digits < 10; digits++) {
    BindParameterForLength(conn, SQL_C_INTERVAL_HOUR_TO_SECOND,
                           SQL_INTERVAL_HOUR_TO_SECOND, digits);

    SQLULEN length = 0;
    SQLRETURN status =
        SQLGetDescField(conn->ipd, 1, SQL_DESC_LENGTH, &length, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
    EXPECT_EQ((digits == 0) ? 8 : 9 + digits, length);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Bind_SQL_INTERVAL_MINUTE_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  BindParameterAndTest(conn, SQL_C_INTERVAL_MINUTE_TO_SECOND,
                       SQL_INTERVAL_MINUTE_TO_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLBindParameter, Check_SQL_LENGTH_For_SQL_INTERVAL_MINUTE_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  for (SQLSMALLINT digits = 0; digits < 10; digits++) {
    BindParameterForLength(conn, SQL_C_INTERVAL_MINUTE_TO_SECOND,
                           SQL_INTERVAL_MINUTE_TO_SECOND, digits);

    SQLULEN length = 0;
    SQLRETURN status =
        SQLGetDescField(conn->ipd, 1, SQL_DESC_LENGTH, &length, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
    EXPECT_EQ((digits == 0) ? 5 : 6 + digits, length);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST(SQLBindParameter, BindWithExecDirect) {
  SQLRETURN status;
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_BIND_PARAM_EXEC_DIRECT_TEST";
  Table table(table_name);
  Schema schema{{"StringField", "STRING"},
                {"IntegerField", "INT64"},
                {"FloatField", "FLOAT64"}};

  auto conn = std::make_shared<ODBCHandles>();
  // Create table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(conn, getSchemaStr(schema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data into the table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  std::string insert_stmt =
      "INSERT INTO " + table_name + " VALUES (?, ?, ?), (?, ?, ?)";

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  StdRow row1 = kBindParamTestData[0];
  status = SQLBindParameter(
      conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
      (SQLPOINTER)row1.str_field.c_str(), row1.str_field.size(), NULL);
  CheckError(status, "SQLBindParameter(SQL_C_CHAR->SQL_VARCHAR)", conn);

  status = SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_UBIGINT,
                            SQL_BIGINT, 0, 0, &row1.int_field, 0, NULL);
  CheckError(status, "SQLBindParameter(SQL_C_UBIGINT->SQL_BIGINT)", conn);

  status = SQLBindParameter(conn->hstmt, 3, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                            SQL_DOUBLE, 0, 0, &row1.float_field, 0, NULL);
  CheckError(status, "SQLBindParameter(SQL_C_DOUBLE->SQL_DOUBLE)", conn);

  StdRow row2 = kBindParamTestData[1];
  status = SQLBindParameter(
      conn->hstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_DOUBLE, 50, 0,
      (SQLPOINTER)row2.str_field.c_str(), row2.str_field.size(), NULL);
  CheckError(status, "SQLBindParameter(SQL_C_CHAR->SQL_DOUBLE)", conn);

  status = SQLBindParameter(conn->hstmt, 5, SQL_PARAM_INPUT, SQL_C_UBIGINT,
                            SQL_DOUBLE, 0, 0, &row2.int_field, 0, NULL);
  CheckError(status, "SQLBindParameter(SQL_C_UBIGINT->SQL_DOUBLE)", conn);

  status = SQLBindParameter(conn->hstmt, 6, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                            SQL_BIGINT, 0, 0, &row2.float_field, 0, NULL);
  CheckError(status, "SQLBindParameter(SQL_C_DOUBLE->SQL_DOUBLE)", conn);

  status = SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Validate inserted data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto const query = "SELECT * FROM " + table_name + " ORDER BY IntegerField";
  RowWiseResults const& results = table.Fetch(conn, query);
  VerifyRowWiseResults(results, {row1, row2});

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
