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
#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include <gtest/gtest.h>
#include <iostream>

namespace google::cloud::odbc_tests {

// Helper functions for this test only.
namespace {
std::string const kCatalog = kCatalogName;
std::string const kDataset = kCatalogFnsDataset;
std::string const kPKTable = kCatalogDatasetTableWithPK;

SQLCHAR* const kSqlCatalog =
    reinterpret_cast<SQLCHAR*>(const_cast<char*>(kCatalog.c_str()));
SQLCHAR* const kSqlDataset =
    reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDataset.c_str()));
SQLCHAR* const kSqlPKTable =
    reinterpret_cast<SQLCHAR*>(const_cast<char*>(kPKTable.c_str()));

SQLSMALLINT const kSqlCatalogLen = kCatalog.length();
SQLSMALLINT const kSqlDatasetLen = kDataset.length();
SQLSMALLINT const kSqlPKTableLen = kPKTable.length();

struct DataBuffer {
  SQLSMALLINT target_type;
  SQLCHAR target_value[512];
  SQLLEN buffer_length = 512;
  SQLLEN str_len;
};

inline void BindColumns(std::shared_ptr<ODBCHandles> conn, DataBuffer* columns,
                        int res_cols) {
  SQLRETURN status;
  int col_idx = 0;
  while (col_idx < res_cols) {
    if (col_idx == 4) {
      columns[col_idx].target_type = SQL_C_SSHORT;
    } else {
      // data type is Char.
      columns[col_idx].target_type = SQL_C_CHAR;
    }
    status =
        SQLBindCol(conn->hstmt, (SQLUSMALLINT)col_idx + 1,
                   columns[col_idx].target_type, columns[col_idx].target_value,
                   columns[col_idx].buffer_length, &(columns[col_idx].str_len));
    CheckError(status, "SQLBindCol", conn);
    col_idx++;
  }
}

}  // namespace

TEST(CatalogDemoTest, SQLPrimaryKeys) {
  int res_cols = 6;

  SQLRETURN status;
  auto conn = std::make_shared<ODBCHandles>();
  // 1) Connect to the data source.
  std::cout << "Connecting to the data source..." << std::endl << std::endl;
  ASSERT_EQ(Connect("DSN=SampleDSN", conn, true), SQL_SUCCESS);
  std::cout << "Successfully connected to the data source!" << std::endl
            << std::endl;
  // (2) Bind Columns
  DataBuffer columns[res_cols];
  std::cout << "Binding Columns..." << std::endl << std::endl;
  BindColumns(conn, columns, res_cols);
  // (3) Fetching Primary Keys.
  std::cout << "Fetching Primary Keys from the data source..." << std::endl
            << std::endl;

  status = SQLPrimaryKeys(conn->hstmt, kSqlCatalog, kSqlCatalogLen, kSqlDataset,
                          kSqlDatasetLen, kSqlPKTable, kSqlPKTableLen);
  CheckError(status, "SQLPrimaryKeys", conn);
  std::cout << "Successfully fetched primary Keys for Catalog: " << kCatalog
            << ", and Dataset: " << kDataset << std::endl
            << std::endl;
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }
    std::string table_cat = (char*)columns[0].target_value;
    std::string table_schema = (char*)columns[1].target_value;
    std::string table_name = (char*)columns[2].target_value;
    std::string col_name = (char*)columns[3].target_value;
    SQLSMALLINT* key_seq =
        reinterpret_cast<SQLSMALLINT*>(columns[4].target_value);
    std::string pk_name = (char*)columns[5].target_value;
    std::cout << "*******************************************************"
              << std::endl;
    std::cout << "Table Catalog: " << table_cat << ", " << std::endl;
    std::cout << "Table Schema: " << table_schema << ", " << std::endl;
    std::cout << "Table Name: " << table_name << ", " << std::endl;
    std::cout << "Column Name: " << col_name << ", " << std::endl;
    if (key_seq) {
      std::cout << "Key Sequence: " << *key_seq << ", " << std::endl;
    }
    std::cout << "PrimaryKey Name: " << pk_name << std::endl << std::endl;
    std::cout << "*******************************************************"
              << std::endl;
  }

  std::cout << "Freeing ODBC handles..." << std::endl << std::endl;
  status = SQLFreeHandle(SQL_HANDLE_STMT, conn->hstmt);
  CheckError(status, "SQLFreeHandle", conn);
  status = SQLFreeHandle(SQL_HANDLE_DBC, conn->hdbc);
  CheckError(status, "SQLFreeHandle", conn);
  status = SQLFreeHandle(SQL_HANDLE_ENV, conn->henv);
  CheckError(status, "SQLFreeHandle", conn);
  std::cout << "Successfully freed all handles!" << std::endl;
}

}  // namespace google::cloud::odbc_tests
