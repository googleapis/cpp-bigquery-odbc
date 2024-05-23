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
#include <gmock/gmock.h>

namespace google::cloud::odbc_tests {

TEST(DiagnosticsTest, SQLGetDiagRec) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  int output;
  // Simulate error by providing wrong infoVal
  auto status = SQLGetInfo(conn->hdbc, 65535, &output, 0, nullptr);
  EXPECT_EQ(status, SQL_ERROR);

  // Get Diagnostics info
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[6];
  SQLINTEGER native_error;
  SQLSMALLINT string_length_ptr;

  status = SQLGetDiagRec(SQL_HANDLE_DBC, conn->hdbc, 1, sqlstate, &native_error,
                         buf, kBufferLength, &string_length_ptr);

  EXPECT_EQ(status, SQL_SUCCESS);
  std::string actual_sqlstate = reinterpret_cast<char*>(sqlstate);
  EXPECT_EQ("HY096", actual_sqlstate);
  std::string actual_message = reinterpret_cast<char*>(buf);
  if (kIsBqDriver) {
    EXPECT_EQ(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::HasSubstr("[Google][ODBC BigQuery Driver] SQLGetInfo"));
  } else {
    EXPECT_NE(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::ContainsRegex("\\[\\w+\\]\\[ODBC\\] \\(\\w+\\) SQLGetInfo"));
  }
  EXPECT_EQ(actual_message.size(), string_length_ptr);
}

TEST(DiagnosticsTest, SQLGetDiagRec_ANSI) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  int output;
  // Simulate error by providing wrong infoVal
  auto status = SQLGetInfoA(conn->hdbc, 65535, &output, 0, nullptr);
  EXPECT_EQ(status, SQL_ERROR);

  // Get Diagnostics info
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[6];
  SQLINTEGER native_error;
  SQLSMALLINT string_length_ptr;

  status =
      SQLGetDiagRecA(SQL_HANDLE_DBC, conn->hdbc, 1, sqlstate, &native_error,
                     buf, kBufferLength, &string_length_ptr);

  EXPECT_EQ(status, SQL_SUCCESS);
  std::string actual_sqlstate = reinterpret_cast<char*>(sqlstate);
  EXPECT_EQ("HY096", actual_sqlstate);
  std::string actual_message = reinterpret_cast<char*>(buf);
  if (kIsBqDriver) {
    EXPECT_EQ(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::HasSubstr("[Google][ODBC BigQuery Driver] SQLGetInfo"));
  } else {
    EXPECT_NE(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::ContainsRegex("\\[\\w+\\]\\[ODBC\\] \\(\\w+\\) SQLGetInfo"));
  }
  EXPECT_EQ(actual_message.size(), string_length_ptr);
}

TEST(DiagnosticsTest, SQLGetDiagField) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  int output;
  // Simulate error by providing wrong infoVal
  auto status = SQLGetInfo(conn->hdbc, 65535, &output, 0, nullptr);
  EXPECT_EQ(status, SQL_ERROR);

  // Get Diagnostics info
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[6];
  SQLINTEGER native_error;
  SQLSMALLINT string_length_ptr;

  status = SQLGetDiagField(SQL_HANDLE_DBC, conn->hdbc, 1, SQL_DIAG_SQLSTATE,
                           &sqlstate, 6, &string_length_ptr);
  EXPECT_EQ(status, SQL_SUCCESS);
  status = SQLGetDiagField(SQL_HANDLE_DBC, conn->hdbc, 1, SQL_DIAG_NATIVE,
                           &native_error, 0, &string_length_ptr);
  EXPECT_EQ(status, SQL_SUCCESS);
  status = SQLGetDiagField(SQL_HANDLE_DBC, conn->hdbc, 1, SQL_DIAG_MESSAGE_TEXT,
                           &buf, kBufferLength, &string_length_ptr);
  EXPECT_EQ(status, SQL_SUCCESS);

  std::string actual_sqlstate = reinterpret_cast<char*>(sqlstate);
  EXPECT_EQ("HY096", actual_sqlstate);
  std::string actual_message = reinterpret_cast<char*>(buf);
  if (kIsBqDriver) {
    EXPECT_EQ(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::HasSubstr("[Google][ODBC BigQuery Driver] SQLGetInfo"));
  } else {
    EXPECT_NE(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::ContainsRegex("\\[\\w+\\]\\[ODBC\\] \\(\\w+\\) SQLGetInfo"));
  }
  EXPECT_EQ(actual_message.size(), string_length_ptr);
}

TEST(DiagnosticsTest, SQLGetDiagFieldA) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  int output;
  // Simulate error by providing wrong infoVal
  auto status = SQLGetInfoA(conn->hdbc, 65535, &output, 0, nullptr);
  EXPECT_EQ(status, SQL_ERROR);

  // Get Diagnostics info
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[6];
  SQLINTEGER native_error;
  SQLSMALLINT string_length_ptr;

  status = SQLGetDiagFieldA(SQL_HANDLE_DBC, conn->hdbc, 1, SQL_DIAG_SQLSTATE,
                            &sqlstate, 6, &string_length_ptr);
  EXPECT_EQ(status, SQL_SUCCESS);
  status = SQLGetDiagFieldA(SQL_HANDLE_DBC, conn->hdbc, 1, SQL_DIAG_NATIVE,
                            &native_error, 0, &string_length_ptr);
  EXPECT_EQ(status, SQL_SUCCESS);
  status =
      SQLGetDiagFieldA(SQL_HANDLE_DBC, conn->hdbc, 1, SQL_DIAG_MESSAGE_TEXT,
                       &buf, kBufferLength, &string_length_ptr);
  EXPECT_EQ(status, SQL_SUCCESS);

  std::string actual_sqlstate = reinterpret_cast<char*>(sqlstate);
  EXPECT_EQ("HY096", actual_sqlstate);
  std::string actual_message = reinterpret_cast<char*>(buf);
  if (kIsBqDriver) {
    EXPECT_EQ(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::HasSubstr("[Google][ODBC BigQuery Driver] SQLGetInfo"));
  } else {
    EXPECT_NE(0, native_error);
    EXPECT_THAT(
        actual_message,
        ::testing::ContainsRegex("\\[\\w+\\]\\[ODBC\\] \\(\\w+\\) SQLGetInfo"));
  }
  EXPECT_EQ(actual_message.size(), string_length_ptr);
}

}  // namespace google::cloud::odbc_tests
