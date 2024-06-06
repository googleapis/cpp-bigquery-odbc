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

#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/testing/odbc_utils/catalog.h"
#include <gtest/gtest.h>
#include <iostream>

namespace google::cloud::odbc_tests {

// Helper functions for this test only.
namespace {

std::string const kCatalog = kCatalogName;
std::string const kDataset = kCatalogFnsDataset;
std::string const kPKTable = kTableCustomer;
std::string const kFKTable = kTableOrders;

SQLCHAR* const kSqlCatalog =
    reinterpret_cast<SQLCHAR*>(const_cast<char*>(kCatalog.c_str()));
SQLCHAR* const kSqlDataset =
    reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDataset.c_str()));
SQLCHAR* const kSqlPKTable =
    reinterpret_cast<SQLCHAR*>(const_cast<char*>(kPKTable.c_str()));
SQLCHAR* const kSqlFKTable =
    reinterpret_cast<SQLCHAR*>(const_cast<char*>(kFKTable.c_str()));

SQLSMALLINT const kSqlCatalogLen = kCatalog.length();
SQLSMALLINT const kSqlDatasetLen = kDataset.length();
SQLSMALLINT const kSqlPKTableLen = kPKTable.length();
SQLSMALLINT const kSqlFKTableLen = kFKTable.length();

}  // namespace

TEST(CatalogDemoTest, SQLForeignKeys) {
  short buf_len;
  std::string in_conn_str = "DSN=SampleDSN";
  short in_conn_str_len = strlen(in_conn_str.c_str());
  SQLTCHAR out_conn_str[4096];
  SQLSMALLINT out_conn_str_buf_len = (sizeof(out_conn_str) / sizeof(SQLTCHAR));
  HENV henv;
  HDBC hdbc;
  HSTMT hstmt;
  SQLRETURN rc = SQL_SUCCESS;

  // 1) Allocate the environment handle.
  std::cout << "Allocating environment handle..." << std::endl << std::endl;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, NULL, &henv), SQL_SUCCESS);
  std::cout << "Successfully allocated environment handle" << std::endl
            << std::endl;
  // 2) Allocate the connection handle.
  std::cout << "Allocating connection handle..." << std::endl << std::endl;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc), SQL_SUCCESS);
  std::cout << "Successfully allocated connection handle" << std::endl
            << std::endl;
  // 3) Connect to the data source.
  std::cout << "Connecting to the data source" << std::endl << std::endl;
  rc = SQLDriverConnect(hdbc, 0, (SQLCHAR*)in_conn_str.c_str(), in_conn_str_len,
                        (SQLCHAR*)out_conn_str, out_conn_str_buf_len, &buf_len,
                        SQL_DRIVER_COMPLETE);
  ASSERT_EQ(rc, SQL_SUCCESS);
  std::cout << "Successfully connected to the data source!" << std::endl
            << std::endl;
  // 4) Allocate Statement Handle.
  std::cout << "Allocating statement handle..." << std::endl << std::endl;
  rc = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
  ASSERT_EQ(rc, SQL_SUCCESS);
  std::cout << "Successfully allocated statement handle" << std::endl
            << std::endl;
  // (5) Fetching Foreign Keys.
  std::cout << "Fetching Foreign Keys from the data source" << std::endl
            << std::endl;

  rc = SQLForeignKeys(hstmt, kSqlCatalog, kSqlCatalogLen, kSqlDataset,
                      kSqlDatasetLen, kSqlPKTable, kSqlPKTableLen, kSqlCatalog,
                      kSqlCatalogLen, kSqlDataset, kSqlDatasetLen, kSqlFKTable,
                      kSqlFKTableLen);
  ASSERT_EQ(rc, SQL_SUCCESS);
  std::cout << "Successfully fetched foreign Keys for Catalog: " << kCatalog
            << ", and Dataset: " << kDataset << std::endl
            << std::endl;
  // (6) TODO(sachinpro): Add SQLFetch statements here and printout the results.
  std::cout << "Freeing statement handle" << std::endl << std::endl;
  rc = SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
  std::cout << "Freeing connection handle" << std::endl << std::endl;
  rc = SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
  std::cout << "Freeing environment handle" << std::endl << std::endl;
  rc = SQLFreeHandle(SQL_HANDLE_ENV, henv);
  std::cout << "Successfully freed all handles!" << std::endl;
}

}  // namespace google::cloud::odbc_tests
