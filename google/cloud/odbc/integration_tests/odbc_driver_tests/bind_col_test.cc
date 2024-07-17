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
#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include <map>

namespace google::cloud::odbc_tests {

void TestCTypeBasic(std::shared_ptr<ODBCHandles> conn, SQLSMALLINT c_type) {
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &conn->ard, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);

  status = SQLBindCol(conn->hstmt, 1, c_type, buf, 20, &target_str_len);
  CheckError(status, "SQLBindCol", conn);

  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;
  status =
      SQLGetDescField(conn->ard, 1, SQL_DESC_DATA_PTR, &out_buf, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATA_PTR)", conn);
  EXPECT_EQ(buf, out_buf);

  auto expected = kAppDescTestMap.at(c_type);

  SQLSMALLINT out_c_type;
  status =
      SQLGetDescField(conn->ard, 1, SQL_DESC_TYPE, &out_c_type, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_TYPE)", conn);
  EXPECT_EQ(expected.c_type, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_CONCISE_TYPE,
                           &out_concise_c_type, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_CONCISE_TYPE)", conn);
  EXPECT_EQ(expected.concise_c_type, out_concise_c_type);

  SQLLEN out_octet_length;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_OCTET_LENGTH,
                           &out_octet_length, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_OCTET_LENGTH)", conn);
  EXPECT_EQ(20, out_octet_length);

  SQLPOINTER out_desc_ind_ptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_INDICATOR_PTR,
                           &out_desc_ind_ptr, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_INDICATOR_PTR)", conn);
  EXPECT_EQ(&target_str_len, out_desc_ind_ptr);

  SQLPOINTER out_octet_length_ptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_OCTET_LENGTH_PTR,
                           &out_octet_length_ptr, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_OCTET_LENGTH_PTR)", conn);
  EXPECT_EQ(&target_str_len, out_octet_length_ptr);

  SQLULEN out_desc_len;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_LENGTH, &out_desc_len, 0,
                           &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
  EXPECT_EQ(expected.desc_len, out_desc_len);

  SQLSMALLINT out_desc_precision;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_PRECISION,
                           &out_desc_precision, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_PRECISION)", conn);
  EXPECT_EQ(expected.desc_precision, out_desc_precision);

  SQLSMALLINT out_desc_scale;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_SCALE, &out_desc_scale, 0,
                           &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_SCALE)", conn);
  EXPECT_EQ(expected.desc_scale, out_desc_scale);

  SQLINTEGER out_desc_datetime_precision;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_DATETIME_INTERVAL_PRECISION,
                           &out_desc_datetime_precision, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATETIME_INTERVAL_PRECISION)",
             conn);
  EXPECT_EQ(expected.desc_datetime_precision, out_desc_datetime_precision);
}

TEST(BindColTest, Basic_SQL_C_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_NUMERIC);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_FLOAT) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_FLOAT);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_DOUBLE) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_DOUBLE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_BIT) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_BIT);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_GUID) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_GUID);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_MONTH);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_YEAR) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_YEAR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_YEAR_TO_MONTH) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_YEAR_TO_MONTH);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_DAY) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_DAY);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_HOUR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_MINUTE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_DAY_TO_HOUR) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_DAY_TO_HOUR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_DAY_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_DAY_TO_MINUTE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_DAY_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_DAY_TO_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_HOUR_TO_MINUTE) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_HOUR_TO_MINUTE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_HOUR_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_HOUR_TO_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_INTERVAL_MINUTE_TO_SECOND) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_INTERVAL_MINUTE_TO_SECOND);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_TYPE_DATE);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_TYPE_TIME);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BindColTest, Basic_SQL_C_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  TestCTypeBasic(conn, SQL_C_TYPE_TIMESTAMP);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
