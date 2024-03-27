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

#include "google/cloud/odbc/bq_driver/odbc_connection.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

TEST(SetConnectionAttr, SuccessNonChar) {
  SQLHENV env_handle;
  SQLHDBC handle;

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLAllocConnHandle(env_handle, &handle));
  EXPECT_EQ(SQL_SUCCESS,
            SQLSetConnectAttrInternal(handle, SQL_ATTR_ACCESS_MODE,
                                      (SQLPOINTER)SQL_MODE_READ_ONLY, 0));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_DBC, handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(SetConnectionAttr, SuccessChar) {
  SQLHENV env_handle;
  SQLHDBC handle;

  SQLCHAR buf[256] = "test";

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLAllocConnHandle(env_handle, &handle));
  EXPECT_EQ(SQL_SUCCESS, SQLSetConnectAttrInternal(
                             handle, SQL_ATTR_CURRENT_CATALOG, buf, 4));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_DBC, handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(SetConnectionAttr, FailUnSupportedAttribute) {
  SQLHENV env_handle;
  SQLHDBC handle;

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLAllocConnHandle(env_handle, &handle));
  EXPECT_EQ(SQL_ERROR, SQLSetConnectAttrInternal(handle, SQL_ATTR_ODBC_CURSORS,
                                                 (SQLPOINTER)1, 0));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_DBC, handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(GetConnectionAttr, SuccessNonChar) {
  SQLHENV env_handle;
  SQLHDBC handle;
  SQLUINTEGER val;
  SQLINTEGER str_len;

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLAllocConnHandle(env_handle, &handle));
  EXPECT_EQ(SQL_SUCCESS,
            SQLSetConnectAttrInternal(handle, SQL_ATTR_ACCESS_MODE,
                                      (SQLPOINTER)SQL_MODE_READ_ONLY, 0));
  EXPECT_EQ(SQL_SUCCESS, SQLGetConnectAttrInternal(handle, SQL_ATTR_ACCESS_MODE,
                                                   &val, 0, &str_len));
  EXPECT_EQ(val, SQL_MODE_READ_ONLY);
  EXPECT_EQ(str_len, sizeof(SQLUINTEGER));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_DBC, handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(GetConnectionAttr, SuccessChar) {
  SQLHENV env_handle;
  SQLHDBC handle;
  SQLINTEGER str_len;

  SQLCHAR buf_in[256] = "test";
  SQLCHAR buf_out[256];

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLAllocConnHandle(env_handle, &handle));
  EXPECT_EQ(SQL_SUCCESS, SQLSetConnectAttrInternal(
                             handle, SQL_ATTR_CURRENT_CATALOG, buf_in, 4));
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetConnectAttrInternal(handle, SQL_ATTR_CURRENT_CATALOG, buf_out,
                                      256, &str_len));
  std::string actual(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual, "test");
  EXPECT_EQ(str_len, 4);
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_DBC, handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(GetConnectionAttr, FailUnSupportedAttribute) {
  SQLHENV env_handle;
  SQLHDBC handle;
  SQLULEN val;
  SQLINTEGER str_len;

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLAllocConnHandle(env_handle, &handle));
  EXPECT_EQ(SQL_ERROR, SQLGetConnectAttrInternal(handle, SQL_ATTR_ODBC_CURSORS,
                                                 &val, 0, &str_len));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_DBC, handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

}  // namespace google::cloud::odbc_bq_driver
