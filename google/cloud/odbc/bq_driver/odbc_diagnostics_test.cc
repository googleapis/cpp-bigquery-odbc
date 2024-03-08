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

#include "google/cloud/odbc/bq_driver/odbc_diagnostics.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

static StatusRecord const kRecord = {
    SQLStates::k_HY000(), "message", 11, 22, 33, "connection", "server"};

TEST(SQLGetDiagFieldInternal, InvalidHandle_Null) {
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_DBC, nullptr, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetDiagFieldInternal, InvalidHandle_EnvironmentHandle) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_DBC, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetDiagFieldInternal, InvalidHandle_ConnectionHandle) {
  ConnectionHandle handle;
  HandleWrapped wrapped_handle(HandleType::kConnHandle, &handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_STMT, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetDiagFieldInternal, InvalidHandle_StatementHandle) {
  StatementHandle handle;
  HandleWrapped wrapped_handle(HandleType::kStatementHandle, &handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_DYNAMIC_FUNCTION_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().GetHeaderRecord().function = "test-function";
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("test-function", actual);
  EXPECT_EQ(13, diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_DYNAMIC_FUNCTION_CODE_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().GetHeaderRecord().function_code = 11;
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION_CODE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(11, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_CURSOR_ROW_COUNT_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().GetHeaderRecord().cursor_row_count = 22;
  SQLSMALLINT diag_identifier = SQL_DIAG_CURSOR_ROW_COUNT;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(22, *diag_info);
  EXPECT_EQ(sizeof(SQLLEN), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_ROW_COUNT_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().GetHeaderRecord().row_count = 33;
  SQLSMALLINT diag_identifier = SQL_DIAG_ROW_COUNT;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(33, *diag_info);
  EXPECT_EQ(sizeof(SQLLEN), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_NUMBER_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord({});
  SQLSMALLINT diag_identifier = SQL_DIAG_NUMBER;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(1, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, Fail_NegativeRecNumber) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;
  SQLSMALLINT rec_number = -5;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, rec_number, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLGetDiagFieldInternal, Fail_ZeroRecNumber) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;
  SQLSMALLINT rec_number = 0;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, rec_number, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLGetDiagFieldInternal, Fail_RecNumber_GT_Size) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;
  SQLSMALLINT rec_number = 100;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, rec_number, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_NO_DATA, status);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_SQLSTATE_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(kRecord.sql_state, actual);
  EXPECT_EQ(kRecord.sql_state.size(), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_MESSAGE_TEXT_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  std::string expected = "[Google][ODBC BigQuery Driver]" + kRecord.message;
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_MESSAGE_TEXT;
  SQLCHAR diag_info[40];
  SQLSMALLINT diag_info_buffer_len = 40;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(expected.size(), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_NATIVE_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_NATIVE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(kRecord.native_error_code, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_COLUMN_NUMBER_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_COLUMN_NUMBER;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(kRecord.column_number, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_ROW_NUMBER_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_ROW_NUMBER;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(kRecord.row_number, *diag_info);
  EXPECT_EQ(sizeof(SQLLEN), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_CONNECTION_NAME_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_CONNECTION_NAME;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(kRecord.connection_name, actual);
  EXPECT_EQ(kRecord.connection_name.size(), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_SERVER_NAME_Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_SERVER_NAME;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(kRecord.server_name, actual);
  EXPECT_EQ(kRecord.server_name.size(), diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_CLASS_ORIGIN_Success_ODBC3) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord({SQLStates::k_IM001(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_CLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ODBC 3.0", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_CLASS_ORIGIN_Success_ISO) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord({SQLStates::k_HY000(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_CLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ISO 9075", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_SUBCLASS_ORIGIN_Success_ODBC3) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord({SQLStates::k_IM001(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_SUBCLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ODBC 3.0", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, SQL_DIAG_SUBCLASS_ORIGIN_Success_ISO) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord({SQLStates::k_HY000(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_SUBCLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ISO 9075", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

TEST(SQLGetDiagFieldInternal, Fail_DiagIdentifier_INVALID) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord({SQLStates::k_HY000(), "message"});
  SQLSMALLINT diag_identifier = 111;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLGetDiagRecInternal, Success) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLCHAR sql_state[6];
  SQLINTEGER native_error[1] = {0};
  SQLCHAR message_text[15];
  SQLSMALLINT message_text_buffer_len = 15;
  SQLSMALLINT message_text_len;

  SQLRETURN status = SQLGetDiagRecInternal(
      SQL_HANDLE_ENV, &wrapped_handle, 1, sql_state, native_error, message_text,
      message_text_buffer_len, &message_text_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(kRecord.native_error_code, *native_error);
  std::string actual_sqlstate = reinterpret_cast<char*>(sql_state);
  EXPECT_EQ(kRecord.sql_state, actual_sqlstate);
  std::string actual_message = reinterpret_cast<char*>(message_text);
  EXPECT_EQ(kRecord.message, actual_message);
  EXPECT_EQ(kRecord.message.size(), message_text_len);
}

TEST(SQLGetDiagRecInternal, Fail_NegativeRecNumber) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLCHAR sql_state[5];
  SQLINTEGER native_error[1] = {0};
  SQLCHAR message_text[15];
  SQLSMALLINT message_text_buffer_len = 15;
  SQLSMALLINT message_text_len;
  SQLSMALLINT rec_number = -5;

  SQLRETURN status = SQLGetDiagRecInternal(
      SQL_HANDLE_ENV, &wrapped_handle, rec_number, sql_state, native_error,
      message_text, message_text_buffer_len, &message_text_len);

  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLGetDiagRecInternal, Fail_ZeroRecNumber) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLCHAR sql_state[5];
  SQLINTEGER native_error[1] = {0};
  SQLCHAR message_text[15];
  SQLSMALLINT message_text_buffer_len = 15;
  SQLSMALLINT message_text_len;
  SQLSMALLINT rec_number = 0;

  SQLRETURN status = SQLGetDiagRecInternal(
      SQL_HANDLE_ENV, &wrapped_handle, rec_number, sql_state, native_error,
      message_text, message_text_buffer_len, &message_text_len);

  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLGetDiagRecInternal, Fail_RecNumber_GT_Size) {
  EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);
  handle.GetDiagnostics().AddStatusRecord(kRecord);
  SQLCHAR sql_state[5];
  SQLINTEGER native_error[1] = {0};
  SQLCHAR message_text[15];
  SQLSMALLINT message_text_buffer_len = 15;
  SQLSMALLINT message_text_len;
  SQLSMALLINT rec_number = 100;

  SQLRETURN status = SQLGetDiagRecInternal(
      SQL_HANDLE_ENV, &wrapped_handle, rec_number, sql_state, native_error,
      message_text, message_text_buffer_len, &message_text_len);

  ASSERT_EQ(SQL_NO_DATA, status);
}

}  // namespace google::cloud::odbc_bq_driver
