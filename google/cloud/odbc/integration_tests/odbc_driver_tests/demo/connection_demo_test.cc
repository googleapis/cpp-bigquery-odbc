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

#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include <gtest/gtest.h>
#include <iostream>

namespace google::cloud::odbc_tests {

// Helper functions for this test only.
namespace {
SQLSMALLINT NUMTCHAR(SQLTCHAR x) { return (sizeof(x) / sizeof(SQLTCHAR)); }
}  // namespace

TEST(ConnectionDemoTest, SQLDriverConnect) {
  short buf_len;
  std::string in_conn_str = "DSN=SampleDSN";
  short in_conn_str_len = strlen(in_conn_str.c_str());
  SQLTCHAR out_conn_str[4096];
  SQLSMALLINT out_conn_str_buf_len = (sizeof(out_conn_str) / sizeof(SQLTCHAR));
  HENV henv;
  HDBC hdbc;
  SQLRETURN rc = SQL_SUCCESS;

  // 1) Allocate the environment handle.
  std::cout << "Allocating environment handle..." << std::endl;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &henv), SQL_SUCCESS);
  std::cout << "Successfully allocated environment handle" << std::endl;
  // 2) Allocate the connection handle.
  std::cout << "Allocating connection handle..." << std::endl;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc), SQL_SUCCESS);
  std::cout << "Successfully allocated connection handle" << std::endl;
  // 3) Connect to the data source.
  std::cout << "Connecting to the data source" << std::endl;
  rc = SQLDriverConnect(hdbc, 0, (SQLCHAR*)in_conn_str.c_str(), in_conn_str_len,
                        (SQLCHAR*)out_conn_str, out_conn_str_buf_len, &buf_len,
                        SQL_DRIVER_COMPLETE);
  ASSERT_EQ(rc, SQL_SUCCESS);
  std::cout << "Successfully connected to the data source!" << std::endl
            << std::endl;

  std::cout << "Freeing connection handle" << std::endl;
  rc = SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
  std::cout << "Freeing environment handle" << std::endl;
  rc = SQLFreeHandle(SQL_HANDLE_ENV, henv);
  std::cout << "Successfully freed all handles!" << std::endl;
}

}  // namespace google::cloud::odbc_tests
