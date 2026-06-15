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
#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include <gtest/gtest.h>
#include <chrono>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace google::cloud::odbc_tests {

// -------------------------------------------------------------------------
// Primary Keys Performance Tests
// -------------------------------------------------------------------------

TEST(CatalogPerformance, Benchmark_GetPrimaryKeys_ExactTable) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  // Ensure the table with primary keys exists before measuring
  CreateTableDirect(conn, kTableWithPKSchema);

  auto start = std::chrono::high_resolution_clock::now();
  
  RowWiseResults results = Catalog::GetPrimaryKeys(
      conn, kDatasetName, kCatalogDatasetTableWithPK);
      
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetPrimaryKeys "
            << "(Exact Table with PK) : " 
            << elapsed.count() << " ms\n";

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, Benchmark_GetPrimaryKeys_NoPKTable) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  // Ensure the table without primary keys exists before measuring
  CreateTableDirect(conn, kTableWithOutPKSchema);

  auto start = std::chrono::high_resolution_clock::now();
  
  RowWiseResults results = Catalog::GetPrimaryKeys(
      conn, kDatasetName, kCatalogDatasetTableWithoutPK);
      
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetPrimaryKeys "
            << "(Table without PK) : " 
            << elapsed.count() << " ms\n";

  EXPECT_TRUE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// -------------------------------------------------------------------------
// Foreign Keys Performance Tests
// -------------------------------------------------------------------------

TEST(CatalogPerformance, Benchmark_GetForeignKeys_PkAndFkTables) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  // Ensure the relationship tables exist before measuring
  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  auto start = std::chrono::high_resolution_clock::now();
  
  // Supplying both the primary key table and the foreign key table
  RowWiseResults results = Catalog::GetForeignKeys(
      conn, kDatasetName, kTableCustomer, kTableOrders);
      
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetForeignKeys "
            << "(Both PK and FK tables supplied) : " 
            << elapsed.count() << " ms\n";

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, Benchmark_GetForeignKeys_PkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  auto start = std::chrono::high_resolution_clock::now();
  
  // Supplying ONLY the primary key table to find all dependent foreign keys
  RowWiseResults results = Catalog::GetForeignKeys(
      conn, kDatasetName, kTableCustomer, "");
      
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetForeignKeys "
            << "(Only PK table supplied) : " 
            << elapsed.count() << " ms\n";

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, Benchmark_GetForeignKeys_FkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  auto start = std::chrono::high_resolution_clock::now();
  
  // Supplying ONLY the foreign key table to find the primary keys it references
  RowWiseResults results = Catalog::GetForeignKeys(
      conn, kDatasetName, "", kTableOrders);
      
  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetForeignKeys "
            << "(Only FK table supplied) : " 
            << elapsed.count() << " ms\n";

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// -------------------------------------------------------------------------
// SQLTables Performance Tests
// -------------------------------------------------------------------------

TEST(CatalogPerformance, SQLTables_FullCatalogEnumeration_TableAndView) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string catalog_pattern = "%";
  std::string table_types = "TABLE,VIEW";
  std::string conn_str = kDefaultConnectionString;

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt,
                                    SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE,
                                    0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_enum = std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, catalog_pattern.c_str(), nullptr, nullptr, table_types.c_str());

  auto end_enum = std::chrono::high_resolution_clock::now();
  auto elapsed_enum = std::chrono::duration_cast<std::chrono::milliseconds>(end_enum - start_enum);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Full Catalog Enumeration with TABLE/VIEW) : "
            << elapsed_enum.count() << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLTables_FullCatalogEnumeration_WildcardCatalog) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string catalog_pattern = "%";
  std::string conn_str = kDefaultConnectionString + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_enum = std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, catalog_pattern.c_str(), nullptr, nullptr, nullptr);

  auto end_enum = std::chrono::high_resolution_clock::now();
  auto elapsed_enum = std::chrono::duration_cast<std::chrono::milliseconds>(end_enum - start_enum);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Full Catalog Enumeration using Catalog='%') : "
            << elapsed_enum.count() << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLTables_DatasetLevelEnumeration) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "ODBC_TEST_DATASET";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_dataset_enum = std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> dataset_tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), nullptr, nullptr);

  auto end_dataset_enum = std::chrono::high_resolution_clock::now();
  auto elapsed_dataset_enum = std::chrono::duration_cast<std::chrono::milliseconds>(end_dataset_enum - start_dataset_enum);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Dataset-Level Enumeration) : "
            << elapsed_dataset_enum.count() << " ms" << std::endl;

  ASSERT_FALSE(dataset_tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLTables_ExactTableLookup) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_lookup = std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  auto end_lookup = std::chrono::high_resolution_clock::now();
  auto elapsed_lookup = std::chrono::duration_cast<std::chrono::milliseconds>(end_lookup - start_lookup);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Exact Table Lookup) : "
            << elapsed_lookup.count() << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLTables_WildcardTableSearch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_pattern = "%timestamp%";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_search = std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), table_pattern.c_str(), nullptr);

  auto end_search = std::chrono::high_resolution_clock::now();
  auto elapsed_search = std::chrono::duration_cast<std::chrono::milliseconds>(end_search - start_search);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Wildcard Table Search) : "
            << elapsed_search.count() << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// -------------------------------------------------------------------------
// SQLColumns Performance Tests
// -------------------------------------------------------------------------

TEST(CatalogPerformance, SQLColumns_FullMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Full Metadata Fetch) : "
            << elapsed.count() << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLColumns_ExactColumnLookup) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string column_name = "timestamp_col_1";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), column_name.c_str());

  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Exact Column Lookup) : "
            << elapsed.count() << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLColumns_WildcardColumnSearch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string column_pattern = "%timestamp%";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), column_pattern.c_str());

  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Wildcard Column Search) : "
            << elapsed.count() << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLColumns_LargeSchemaMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "300_column_timestamp";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start = std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  auto end = std::chrono::high_resolution_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Large Schema Metadata Fetch) : "
            << elapsed.count() << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// -------------------------------------------------------------------------
// FilterTablesOnDefaultDataset (ON/OFF) Performance Tests
// -------------------------------------------------------------------------

TEST(CatalogPerformance, SQLTables_FullCatalogEnumeration_FilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str = kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  // Scenario: Filter OFF
  std::string conn_str_unfiltered = base_conn_str + ";FilterTablesOnDefaultDataset=0;";
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  
  auto start_unfiltered = std::chrono::high_resolution_clock::now();
  std::vector<SQLTableResult> tables_unfiltered = Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);
  auto end_unfiltered = std::chrono::high_resolution_clock::now();
  
  std::cout << "[BENCHMARK] Catalog::GetTables (Filter OFF) : "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_unfiltered - start_unfiltered).count() << " ms\n";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Scenario: Filter ON
  std::string conn_str_filtered = base_conn_str + ";FilterTablesOnDefaultDataset=1;";
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_filtered = std::chrono::high_resolution_clock::now();
  std::vector<SQLTableResult> tables_filtered = Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);
  auto end_filtered = std::chrono::high_resolution_clock::now();

  std::cout << "[BENCHMARK] Catalog::GetTables (Filter ON) : "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_filtered - start_filtered).count() << " ms\n";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLColumns_ColumnMetadataEnumeration_FilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str = kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  // Scenario: Filter OFF
  std::string conn_str_unfiltered = base_conn_str + ";FilterTablesOnDefaultDataset=0;";
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_unfiltered = std::chrono::high_resolution_clock::now();
  std::vector<SQLColumnsResult> cols_unfiltered = Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);
  auto end_unfiltered = std::chrono::high_resolution_clock::now();

  std::cout << "[BENCHMARK] Catalog::GetColumns (Filter OFF) : "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_unfiltered - start_unfiltered).count() << " ms\n";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Scenario: Filter ON
  std::string conn_str_filtered = base_conn_str + ";FilterTablesOnDefaultDataset=1;";
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_filtered = std::chrono::high_resolution_clock::now();
  std::vector<SQLColumnsResult> cols_filtered = Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);
  auto end_filtered = std::chrono::high_resolution_clock::now();

  std::cout << "[BENCHMARK] Catalog::GetColumns (Filter ON) : "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_filtered - start_filtered).count() << " ms\n";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}