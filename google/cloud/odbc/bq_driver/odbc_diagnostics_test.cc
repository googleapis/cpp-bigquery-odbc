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

class EnvironmentHandleTest : public ::testing::Test {
 protected:
  EnvironmentHandle* handle_;
  HandleWrapped* wrapped_handle_;

  void SetUp() override {
    handle_ = new EnvironmentHandle();
    wrapped_handle_ = new HandleWrapped(HandleType::kEnvHandle, handle_);
  }

  void TearDown() override {
    delete wrapped_handle_;
    delete handle_;
  }
};

static StatusRecord const kRecord = {.sql_state = SQLStates::k_HY000(),
                                     .message = "message",
                                     .native_error_code = 11,
                                     .column_number = 22,
                                     .row_number = 33,
                                     .connection_name = "connection",
                                     .server_name = "server"};

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
  auto* handle = new EnvironmentHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_DBC, wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
  delete wrapped_handle;
  delete handle;
}

TEST(SQLGetDiagFieldInternal, InvalidHandle_ConnectionHandle) {
  auto* handle = new ConnectionHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kConnHandle, handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_STMT, wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
  delete wrapped_handle;
  delete handle;
}

TEST(SQLGetDiagFieldInternal, InvalidHandle_StatementHandle) {
  auto* handle = new StatementHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kStatementHandle, handle);
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
  delete wrapped_handle;
  delete handle;
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_DYNAMIC_FUNCTION_Success) {
  handle_->GetDiagnostics().GetHeaderRecord().function = "test-function";
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("test-function", actual);
  EXPECT_EQ(13, diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_DYNAMIC_FUNCTION_CODE_Success) {
  handle_->GetDiagnostics().GetHeaderRecord().function_code = 11;
  SQLSMALLINT diag_identifier = SQL_DIAG_DYNAMIC_FUNCTION_CODE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(11, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_CURSOR_ROW_COUNT_Success) {
  handle_->GetDiagnostics().GetHeaderRecord().cursor_row_count = 22;
  SQLSMALLINT diag_identifier = SQL_DIAG_CURSOR_ROW_COUNT;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(22, *diag_info);
  EXPECT_EQ(sizeof(SQLLEN), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_ROW_COUNT_Success) {
  handle_->GetDiagnostics().GetHeaderRecord().row_count = 33;
  SQLSMALLINT diag_identifier = SQL_DIAG_ROW_COUNT;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(33, *diag_info);
  EXPECT_EQ(sizeof(SQLLEN), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest, SQLGetDiagFieldInternal_SQL_DIAG_NUMBER_Success) {
  handle_->GetDiagnostics().AddStatusRecord({});
  SQLSMALLINT diag_identifier = SQL_DIAG_NUMBER;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 0, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(1, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest, SQLGetDiagFieldInternal_Fail_NegativeRecNumber) {
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;
  SQLSMALLINT rec_number = -5;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, rec_number, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_ERROR, status);
}

TEST_F(EnvironmentHandleTest, SQLGetDiagFieldInternal_Fail_ZeroRecNumber) {
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;
  SQLSMALLINT rec_number = 0;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, rec_number, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_ERROR, status);
}

TEST_F(EnvironmentHandleTest, SQLGetDiagFieldInternal_Fail_RecNumber_GT_Size) {
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;
  SQLSMALLINT rec_number = 100;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, rec_number, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_NO_DATA, status);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_SQLSTATE_Success) {
  handle_->GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_SQLSTATE;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(kRecord.sql_state, actual);
  EXPECT_EQ(kRecord.sql_state.size(), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_MESSAGE_TEXT_Success) {
  std::string expected = "[Google][ODBC BigQuery Driver]" + kRecord.message;
  handle_->GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_MESSAGE_TEXT;
  SQLCHAR diag_info[40];
  SQLSMALLINT diag_info_buffer_len = 40;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(expected.size(), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest, SQLGetDiagFieldInternal_SQL_DIAG_NATIVE_Success) {
  handle_->GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_NATIVE;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(kRecord.native_error_code, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_COLUMN_NUMBER_Success) {
  handle_->GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_COLUMN_NUMBER;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(kRecord.column_number, *diag_info);
  EXPECT_EQ(sizeof(SQLINTEGER), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_ROW_NUMBER_Success) {
  handle_->GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_ROW_NUMBER;
  SQLULEN diag_info[1] = {0};
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(kRecord.row_number, *diag_info);
  EXPECT_EQ(sizeof(SQLLEN), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_CONNECTION_NAME_Success) {
  handle_->GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_CONNECTION_NAME;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(kRecord.connection_name, actual);
  EXPECT_EQ(kRecord.connection_name.size(), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_SERVER_NAME_Success) {
  handle_->GetDiagnostics().AddStatusRecord(kRecord);
  SQLSMALLINT diag_identifier = SQL_DIAG_SERVER_NAME;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ(kRecord.server_name, actual);
  EXPECT_EQ(kRecord.server_name.size(), diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_CLASS_ORIGIN_Success_ODBC3) {
  handle_->GetDiagnostics().AddStatusRecord({SQLStates::k_IM001(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_CLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ODBC 3.0", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_CLASS_ORIGIN_Success_ISO) {
  handle_->GetDiagnostics().AddStatusRecord({SQLStates::k_HY000(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_CLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ISO 9075", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_SUBCLASS_ORIGIN_Success_ODBC3) {
  handle_->GetDiagnostics().AddStatusRecord({SQLStates::k_IM001(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_SUBCLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ODBC 3.0", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

TEST_F(EnvironmentHandleTest,
       SQLGetDiagFieldInternal_SQL_DIAG_SUBCLASS_ORIGIN_Success_ISO) {
  handle_->GetDiagnostics().AddStatusRecord({SQLStates::k_HY000(), "message"});
  SQLSMALLINT diag_identifier = SQL_DIAG_SUBCLASS_ORIGIN;
  SQLCHAR diag_info[15];
  SQLSMALLINT diag_info_buffer_len = 15;
  SQLSMALLINT diag_info_string_len;

  SQLRETURN status = SQLGetDiagFieldInternal(
      SQL_HANDLE_ENV, wrapped_handle_, 1, diag_identifier, diag_info,
      diag_info_buffer_len, &diag_info_string_len);

  ASSERT_EQ(SQL_SUCCESS, status);
  std::string actual = reinterpret_cast<char*>(diag_info);
  EXPECT_EQ("ISO 9075", actual);
  EXPECT_EQ(8, diag_info_string_len);
}

}  // namespace google::cloud::odbc_bq_driver
