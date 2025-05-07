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

namespace google::cloud::odbc_tests {

#ifndef _WIN32
TEST(BQDriverTest, SQLGetSetEnvAttr_ConnectionPool_OnePerDriver) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER set_val = SQL_CP_ONE_PER_DRIVER;
  SQLUINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(SQLSetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING,
                          (SQLPOINTER)set_val, 0),
            SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING, &get_val, 0,
                          nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_CP_ONE_PER_DRIVER);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ConnectionPool_Default) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER set_val = SQL_CP_DEFAULT;
  SQLUINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(SQLSetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING,
                          (SQLPOINTER)set_val, 0),
            SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING, &get_val, 0,
                          nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_CP_OFF);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ConnectionPool_CPOff) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER set_val = SQL_CP_OFF;
  SQLUINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(SQLSetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING,
                          (SQLPOINTER)set_val, 0),
            SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING, &get_val, 0,
                          nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_CP_OFF);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ConnectionPool_OnePerHenv) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER set_val = SQL_CP_ONE_PER_HENV;
  SQLUINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(SQLSetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING,
                          (SQLPOINTER)set_val, 0),
            SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING, &get_val, 0,
                          nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_CP_ONE_PER_HENV);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ConnectionPoolMatch_Default) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER set_val = SQL_CP_MATCH_DEFAULT;
  SQLUINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(
      SQLSetEnvAttr(conn->henv, SQL_ATTR_CP_MATCH, (SQLPOINTER)set_val, 0),
      SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CP_MATCH, &get_val, 0, nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_CP_STRICT_MATCH);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ConnectionPoolMatch_StrictMatch) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER set_val = SQL_CP_STRICT_MATCH;
  SQLUINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(
      SQLSetEnvAttr(conn->henv, SQL_ATTR_CP_MATCH, (SQLPOINTER)set_val, 0),
      SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CP_MATCH, &get_val, 0, nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_CP_STRICT_MATCH);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ConnectionPoolMatch_RelaxedMatch) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER set_val = SQL_CP_RELAXED_MATCH;
  SQLUINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(
      SQLSetEnvAttr(conn->henv, SQL_ATTR_CP_MATCH, (SQLPOINTER)set_val, 0),
      SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CP_MATCH, &get_val, 0, nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_CP_RELAXED_MATCH);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ODBCVersion_ODBC2) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLINTEGER set_val = SQL_OV_ODBC2;
  SQLINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(
      SQLSetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)set_val, 0),
      SQL_SUCCESS);
  EXPECT_EQ(
      SQLGetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION, &get_val, 0, nullptr),
      SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_OV_ODBC2);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_ODBCVersion_ODBC3) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLINTEGER set_val = SQL_OV_ODBC3;
  SQLINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(
      SQLSetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)set_val, 0),
      SQL_SUCCESS);
  EXPECT_EQ(
      SQLGetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION, &get_val, 0, nullptr),
      SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_OV_ODBC3);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetSetEnvAttr_OutputNTS_True) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLINTEGER set_val = SQL_TRUE;
  SQLINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(
      SQLSetEnvAttr(conn->henv, SQL_ATTR_OUTPUT_NTS, (SQLPOINTER)set_val, 0),
      SQL_SUCCESS);
  EXPECT_EQ(
      SQLGetEnvAttr(conn->henv, SQL_ATTR_OUTPUT_NTS, &get_val, 0, nullptr),
      SQL_SUCCESS);
  EXPECT_EQ(get_val, SQL_TRUE);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

#ifdef BQ_DRIVER_INTEGRATION_TESTS
TEST(BQDriverTest, SQLGetEnvAttr_AllDefaults) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUINTEGER get_val1;
  SQLINTEGER get_val2;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CONNECTION_POOLING, &get_val1, 0,
                          nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val1, SQL_CP_OFF);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ATTR_CP_MATCH, &get_val1, 0, nullptr),
            SQL_SUCCESS);
  EXPECT_EQ(get_val1, SQL_CP_STRICT_MATCH);
  EXPECT_EQ(
      SQLGetEnvAttr(conn->henv, SQL_ATTR_OUTPUT_NTS, &get_val2, 0, nullptr),
      SQL_SUCCESS);
  EXPECT_EQ(get_val2, SQL_TRUE);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}
#endif  // BQ_DRIVER_INTEGRATION_TESTS

#endif  //_WIN32

TEST(BQDriverTest, SQLGetSetEnvAttr_UnSupportedAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLINTEGER set_val = SQL_TRUE;
  SQLINTEGER get_val;

  EXPECT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv), SQL_SUCCESS);
  EXPECT_EQ(SQLSetEnvAttr(conn->henv, SQL_ACCESS_MODE, (SQLPOINTER)set_val, 0),
            SQL_ERROR);
  EXPECT_EQ(SQLGetEnvAttr(conn->henv, SQL_ACCESS_MODE, &get_val, 0, nullptr),
            SQL_ERROR);
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_ENV, conn->henv), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
