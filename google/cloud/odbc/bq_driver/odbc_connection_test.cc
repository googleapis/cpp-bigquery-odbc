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
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::internal::GetEnv;
using google::cloud::odbc_bq_driver::ToSqlChar;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateExplicitDescriptor;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

std::string GetValidOutputStr(SQLCHAR* buffer, SQLSMALLINT length) {
  if (buffer != nullptr && length > 0) {
    return std::string(reinterpret_cast<char*>(buffer), length);
  } else {
    return std::string();
  }
}

TEST(SQLAllocConnHandle, SQLAllocConnHandle) {
  EnvironmentHandle env_handle;
  SQLPOINTER output;

  auto status = SQLAllocConnHandle(&env_handle, &output);

  ASSERT_EQ(SQL_SUCCESS, status);
  auto* conn_handle = reinterpret_cast<ConnectionHandle*>(output);
  std::set<ConnectionHandle*>& conn_handles = env_handle.GetConnectionHandles();
  EXPECT_FALSE(conn_handles.empty());
  EXPECT_TRUE(conn_handles.find(conn_handle) != conn_handles.end());
  delete conn_handle;
}

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

TEST(SQLDisconnectInternal, Disconnect) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  DescriptorHandle impl_desc;
  auto* stmt_handle = new StatementHandle(
      &conn_handle, {impl_desc, impl_desc, impl_desc, impl_desc});
  conn_handle.GetStatementHandles().emplace(stmt_handle);
  auto* desc_handle = new DescriptorHandle();
  conn_handle.GetDescriptorHandles().emplace(desc_handle);
  desc_handle->SetConnectionHandle(&conn_handle);

  auto status = SQLDisconnectInternal(&conn_handle);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(conn_handle.GetStatementHandles().empty());
  EXPECT_TRUE(conn_handle.GetDescriptorHandles().empty());
}

TEST(SQLDisconnectInternal, Fail_InvalidHandle) {
  auto status = SQLDisconnectInternal(nullptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLDisconnectInternal, Fail_NotConnectedHandle) {
  ConnectionHandle conn_handle = CreateConnectionHandle(false);

  auto status = SQLDisconnectInternal(&conn_handle);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_08003(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDisconnectInternal, Fail_ActiveTransaction) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  conn_handle.SetTransactionActive(true);

  auto status = SQLDisconnectInternal(&conn_handle);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_25000(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLConnectInternal, Fail_InvalidConnectionHandle) {
  auto status = SQLConnectInternal(NULL, NULL, 0, NULL, 0, NULL, 0);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLConnectInternal, Fail_InvalidServerNameLen) {
  ConnectionHandle conn_handle = CreateConnectionHandle(false);

  auto status =
      SQLConnectInternal(&conn_handle, ToSqlChar("Test"), -1, NULL, 0, NULL, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Invalid server name length",
            conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLConnectInternal, Fail_InvalidUserNameLen) {
  ConnectionHandle conn_handle = CreateConnectionHandle(false);

  auto status =
      SQLConnectInternal(&conn_handle, NULL, 0, ToSqlChar("Test"), -1, NULL, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Invalid user name length",
            conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLConnectInternal, Fail_InvalidAuthLen) {
  ConnectionHandle conn_handle = CreateConnectionHandle(false);

  auto status =
      SQLConnectInternal(&conn_handle, NULL, 0, NULL, 0, ToSqlChar("Test"), -1);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Invalid auth string length",
            conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLConnectInternal, Fail_DSNLess_EmptyUser) {
  ConnectionHandle conn_handle = CreateConnectionHandle(false);

  auto status = SQLConnectInternal(&conn_handle, NULL, 0, ToSqlChar(""),
                                   SQL_NTS, NULL, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Username cannot be empty for DSN-less usecase",
            conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLConnectInternal, Fail_DSNLess_EmptyAuthString) {
  ConnectionHandle conn_handle = CreateConnectionHandle(false);

  auto status = SQLConnectInternal(&conn_handle, NULL, 0, ToSqlChar("TEST"),
                                   SQL_NTS, ToSqlChar(""), SQL_NTS);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Auth String cannot be empty for DSN-less usecase",
            conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLConnectInternal, Fail_DSNLess_InvalidUser) {
  ConnectionHandle conn_handle = CreateConnectionHandle(false);

  auto status = SQLConnectInternal(&conn_handle, NULL, 0, ToSqlChar("TEST"),
                                   SQL_NTS, ToSqlChar("TEST"), SQL_NTS);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Username needs to be an email address",
            conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLBrowseConnectInternal, Fail_InvalidConnectionHandle) {
  auto status = SQLBrowseConnectInternal(NULL, NULL, NULL, NULL, 0, 0);
  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLBrowseConnectInternal, Fail_MissingRequiredKeyword) {
  std::string const conn_str =
      "DRIVER=Simba ODBC Driver for Google BigQuery;"
      "Catalog=bigquery-devtools-drivers;";

  SQLCHAR* in_conn_str = ToSqlChar(conn_str.c_str());
  SQLSMALLINT conn_str_len = strlen(reinterpret_cast<char*>(in_conn_str));
  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  ConnectionHandle conn_handle = CreateConnectionHandle(false);
  auto status = SQLBrowseConnectInternal(
      &conn_handle, in_conn_str, conn_str_len, (SQLCHAR*)out_conn_str,
      sizeof(out_conn_str), &out_conn_str_len);

  EXPECT_EQ(SQL_NEED_DATA, status);

  std::string res_out_conn_str(reinterpret_cast<char const*>(out_conn_str));
  EXPECT_EQ(res_out_conn_str,
            "OAuthMechanism:OAuthMechanism=?;KeyFilePath:KeyFilePath=?;");
}

TEST(SQLBrowseConnectInternal, Fail_ExtraAttributeInConnStr) {
  std::string conn_str =
      "DRIVER=Simba ODBC Driver for Google BigQuery;"
      "Catalog=bigquery-devtools-drivers;";

  SQLCHAR* in_conn_str = ToSqlChar(conn_str.c_str());
  SQLSMALLINT conn_str_len = strlen(reinterpret_cast<char*>(in_conn_str));
  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  ConnectionHandle conn_handle = CreateConnectionHandle(false);
  auto status = SQLBrowseConnectInternal(
      &conn_handle, in_conn_str, conn_str_len, (SQLCHAR*)out_conn_str,
      sizeof(out_conn_str), &out_conn_str_len);

  std::string res_out_conn_str(reinterpret_cast<char const*>(out_conn_str));
  EXPECT_EQ(SQL_NEED_DATA, status);

  EXPECT_EQ(conn_handle.GetDsn().driver,
            "Simba ODBC Driver for Google BigQuery");
  EXPECT_EQ(conn_handle.GetDsn().catalog, "bigquery-devtools-drivers");
  EXPECT_EQ(res_out_conn_str,
            "OAuthMechanism:OAuthMechanism=?;KeyFilePath:KeyFilePath=?;");

  // connection string with an extra attribute `AllowLargeResults`
  conn_str = "OAuthMechanism=0;KeyFilePath=/path/to/file;AllowLargeResults=0;";
  in_conn_str = ToSqlChar(conn_str.c_str());
  conn_str_len = strlen(reinterpret_cast<char*>(in_conn_str));

  status = SQLBrowseConnectInternal(&conn_handle, in_conn_str, conn_str_len,
                                    (SQLCHAR*)out_conn_str,
                                    sizeof(out_conn_str), &out_conn_str_len);

  EXPECT_EQ(status, SQL_ERROR);

  EXPECT_EQ(SQLStates::k_HY000(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ(
      "Non Requested connection attribute AllowLargeResults in "
      "ConnectionString",
      conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLBrowseConnectInternal, Fail_DuplicateAttributeInConnStr) {
  auto conn_str =
      "driver=Simba ODBC Driver for Google BigQuery;"
      "OAuthMechanism=0;";

  SQLCHAR* in_conn_str = ToSqlChar(conn_str);
  SQLSMALLINT conn_str_len = strlen(reinterpret_cast<char*>(in_conn_str));
  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  ConnectionHandle conn_handle = CreateConnectionHandle(false);
  auto status = SQLBrowseConnectInternal(
      &conn_handle, in_conn_str, conn_str_len, (SQLCHAR*)out_conn_str,
      sizeof(out_conn_str), &out_conn_str_len);

  std::string res_out_conn_str(reinterpret_cast<char const*>(out_conn_str));

  EXPECT_EQ(SQL_NEED_DATA, status);

  EXPECT_EQ(conn_handle.GetDsn().driver,
            "Simba ODBC Driver for Google BigQuery");
  EXPECT_EQ(conn_handle.GetDsn().oauthmechanism, "0");
  EXPECT_EQ(res_out_conn_str, "Catalog:Catalog=?;KeyFilePath:KeyFilePath=?;");

  conn_str =
      "Catalog=bigquery-devtools-drivers;KeyFilePath=/path/to/"
      "file;Catalog=bigquery-devtools-drivers;";
  in_conn_str = ToSqlChar(conn_str);
  conn_str_len = strlen(reinterpret_cast<char*>(in_conn_str));

  status = SQLBrowseConnectInternal(&conn_handle, in_conn_str, conn_str_len,
                                    (SQLCHAR*)out_conn_str,
                                    sizeof(out_conn_str), &out_conn_str_len);

  EXPECT_EQ(status, SQL_ERROR);
  EXPECT_EQ(SQLStates::k_HY000(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Connection Attribute Catalog already found!",
            conn_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

}  // namespace google::cloud::odbc_bq_driver
