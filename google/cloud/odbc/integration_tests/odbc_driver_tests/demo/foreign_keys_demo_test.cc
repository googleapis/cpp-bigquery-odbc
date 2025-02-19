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

inline void BindColumns(std::shared_ptr<ODBCHandles> conn,
                        TestingDataBuffer* columns, int res_cols) {
  SQLRETURN status;
  for (int col_idx = 0; col_idx < res_cols; col_idx++) {
    if (col_idx == 8) {
      // data type is SMALLINT.
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
  }
}

}  // namespace

TEST(CatalogDemoTest, SQLForeignKeys) {
  int res_cols = 11;
  SQLRETURN status;
  auto conn = std::make_shared<ODBCHandles>();
  // 1) Connect to the data source.
  std::cout << "Connecting to the data source..." << std::endl << std::endl;
  ASSERT_EQ(Connect("DSN=SampleDSN", conn, true), SQL_SUCCESS);
  std::cout << "Successfully connected to the data source!" << std::endl
            << std::endl;
  // 2) Create required tables for foreign keys.
  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);
  std::cout << "Successfully created foreign key tables" << std::endl
            << std::endl;
  // Currently tables are empty when doing demo some data should be inserted.
  // (3) Bind Columns
  TestingDataBuffer columns[res_cols];
  std::cout << "Binding Columns..." << std::endl << std::endl;
  BindColumns(conn, columns, res_cols);
  // (4) Fetching Foreign Keys.
  std::cout << "Fetching Foreign Keys from the data source..." << std::endl
            << std::endl;

  status = SQLForeignKeys(conn->hstmt, kSqlCatalog, kSqlCatalogLen, kSqlDataset,
                          kSqlDatasetLen, kSqlPKTable, kSqlPKTableLen,
                          kSqlCatalog, kSqlCatalogLen, kSqlDataset,
                          kSqlDatasetLen, kSqlFKTable, kSqlFKTableLen);
  CheckError(status, "SQLForeignKeys", conn);
  std::cout << "Successfully fetched foreign Keys for Catalog: " << kCatalog
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
    // Col1: pk catalog name , Col2: pk schema name, Col3: pk table name,
    // Col4: pk column name, Col5: fk catalog name, Col6: fk schema name,
    // Col7: fk table name, Col8: fk column name,  Col9: key sequence,
    // Col10: update rule, Col 11: delete rule, Col12: fk name,
    // Col13: pk name, Col14: Deferrability
    std::string pk_table_cat = (char*)columns[0].target_value;
    std::string pk_table_schema = (char*)columns[1].target_value;
    std::string pk_table_name = (char*)columns[2].target_value;
    std::string pk_col_name = (char*)columns[3].target_value;
    std::string fk_table_cat = (char*)columns[4].target_value;
    std::string fk_table_schema = (char*)columns[5].target_value;
    std::string fk_table_name = (char*)columns[6].target_value;
    std::string fk_col_name = (char*)columns[7].target_value;
    SQLSMALLINT* key_seq =
        reinterpret_cast<SQLSMALLINT*>(columns[8].target_value);
    std::string fk_name = (char*)columns[9].target_value;
    std::string pk_name = (char*)columns[10].target_value;

    std::cout << "*******************************************************"
              << std::endl;
    std::cout << "PrimaryKey Table Catalog: " << pk_table_cat << ", "
              << std::endl;
    std::cout << "PrimaryKey Table Schema: " << pk_table_schema << ", "
              << std::endl;
    std::cout << "PrimaryKey Table Name: " << pk_table_name << ", "
              << std::endl;
    std::cout << "PrimaryKey Column Name: " << pk_col_name << ", " << std::endl;
    std::cout << "ForeignKey Table Catalog: " << fk_table_cat << ", "
              << std::endl;
    std::cout << "ForeignKey Table Schema: " << fk_table_schema << ", "
              << std::endl;
    std::cout << "ForeignKey Table Name: " << fk_table_name << ", "
              << std::endl;
    std::cout << "ForeignKey Column Name: " << fk_col_name << ", " << std::endl;
    if (key_seq) {
      std::cout << "Key Sequence: " << *key_seq << ", " << std::endl;
    }
    std::cout << "UPDATE Rule: NULL, " << std::endl;
    std::cout << "DELETE Rule: NULL, " << std::endl;
    std::cout << "ForeignKey Name: " << fk_name << std::endl;
    std::cout << "PrimaryKey Name: " << pk_name << std::endl;
    std::cout << "Deferrability: " << SQL_NOT_DEFERRABLE << std::endl;

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
