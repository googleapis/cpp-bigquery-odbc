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
#include <set>
#include <string>
#include <vector>

namespace google::cloud::odbc_tests {
// Primary Keys Performance Tests
TEST(CatalogPerformance, Benchmark_GetPrimaryKeys_ExactTable) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  CreateTableDirect(conn, kTableWithPKSchema);

  RowWiseResults results = Catalog::GetPrimaryKeys(
      conn, kDatasetName, kCatalogDatasetTableWithPK);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, Benchmark_GetPrimaryKeys_NoPKTable) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  CreateTableDirect(conn, kTableWithOutPKSchema);

  RowWiseResults results = Catalog::GetPrimaryKeys(
      conn, kDatasetName, kCatalogDatasetTableWithoutPK);

  EXPECT_TRUE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
// Foreign Keys Performance Tests
TEST(CatalogPerformance, Benchmark_GetForeignKeys_PkAndFkTables) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results = Catalog::GetForeignKeys(
      conn, kDatasetName, kTableCustomer, kTableOrders);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, Benchmark_GetForeignKeys_PkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results = Catalog::GetForeignKeys(
      conn, kDatasetName, kTableCustomer, "");

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, Benchmark_GetForeignKeys_FkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results = Catalog::GetForeignKeys(
      conn, kDatasetName, "", kTableOrders);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
// SQLTables Performance Tests
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

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, catalog_pattern.c_str(), nullptr, nullptr, table_types.c_str());

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

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, catalog_pattern.c_str(), nullptr, nullptr, nullptr);

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

  std::vector<SQLTableResult> dataset_tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), nullptr, nullptr);

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

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

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

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), table_pattern.c_str(), nullptr);

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
// SQLColumns Performance Tests
TEST(CatalogPerformance, SQLColumns_FullMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

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

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), column_name.c_str());

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

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), column_pattern.c_str());

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLColumns_LargeSchemaMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "300_column_timestamp";
  std::string conn_str = kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
// FilterTablesOnDefaultDataset (ON/OFF) Performance Tests
TEST(CatalogPerformance, SQLTables_FullCatalogEnumeration_FilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str = kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  std::string conn_str_unfiltered = base_conn_str + ";FilterTablesOnDefaultDataset=0;";
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  
  std::vector<SQLTableResult> tables_unfiltered = Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);
  
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string conn_str_filtered = base_conn_str + ";FilterTablesOnDefaultDataset=1;";
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables_filtered = Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformance, SQLColumns_ColumnMetadataEnumeration_FilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str = kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  std::string conn_str_unfiltered = base_conn_str + ";FilterTablesOnDefaultDataset=0;";
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLColumnsResult> cols_unfiltered = Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string conn_str_filtered = base_conn_str + ";FilterTablesOnDefaultDataset=1;";
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLColumnsResult> cols_filtered = Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DataFetchPerformance, Benchmark_PowerBI_Mimic_NewTimestampTable) {
  auto conn = std::make_shared<ODBCHandles>();
  
  std::string connection_string = kDefaultConnectionString + ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;";
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS) << "Failed to connect to the database.";

  std::string target_table = "bigquery-devtools-drivers.kirltest.new_timestamp_table";
  std::string query = "SELECT * FROM `" + target_table + "` LIMIT 1000000";

  SQLRETURN ret = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(ret, "SQLExecDirect", conn); 

  SQLSMALLINT num_cols;
  ret = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(ret, "SQLNumResultCols", conn);

  std::vector<std::shared_ptr<Column>> cols(num_cols);
  for (int i = 1; i <= num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i - 1] = col_ptr;

    DescribeCol(conn, col_ptr, i);
    
    SqlToCdataTypes(col_ptr);
    
    ret = SQLBindCol(conn->hstmt, i, col_ptr->data_type, col_ptr->data_buf.target_value,
                     col_ptr->data_buf.buffer_length, &(col_ptr->data_buf.str_len));
    CheckError(ret, "SQLBindCol(" + std::to_string(i) + ")", conn);
  }

  int row_count = 0;
  while ((ret = SQLFetch(conn->hstmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) { 
    row_count++;
  }
  EXPECT_EQ(ret, SQL_NO_DATA) << "Fetch ended unexpectedly with return code: " << ret; 

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS); 
}


TEST(DataFetchPerformance, Benchmark_PowerBI_Mimic_AllBqTypes) {
  auto conn = std::make_shared<ODBCHandles>();
  
  std::string connection_string = kDefaultConnectionString + ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;";
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS) << "Failed to connect to the database.";

  std::string target_table = "bigquery-devtools-drivers.INTEGRATION_TEST_FORMAT.all_bq_types";
  std::string query = "SELECT * FROM `" + target_table + "` LIMIT 1000000";

  SQLRETURN ret = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(ret, "SQLExecDirect", conn); 

  SQLSMALLINT num_cols;
  ret = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(ret, "SQLNumResultCols", conn);

  std::vector<std::shared_ptr<Column>> cols(num_cols);
  for (int i = 1; i <= num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i - 1] = col_ptr;

    DescribeCol(conn, col_ptr, i);
    
    switch (col_ptr->data_type) {
        case SQL_BIGINT:
        case SQL_INTEGER: col_ptr->data_type = SQL_C_SBIGINT; break;
        case SQL_FLOAT:
        case SQL_DOUBLE:  col_ptr->data_type = SQL_C_DOUBLE; break;
        case SQL_BIT:     col_ptr->data_type = SQL_C_BIT; break;
        default:          col_ptr->data_type = SQL_C_CHAR; break; 
    }
    
    ret = SQLBindCol(conn->hstmt, i, col_ptr->data_type, col_ptr->data_buf.target_value,
                     col_ptr->data_buf.buffer_length, &(col_ptr->data_buf.str_len));
    CheckError(ret, "SQLBindCol(" + std::to_string(i) + ")", conn);
  }

  int row_count = 0;
  while ((ret = SQLFetch(conn->hstmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) { 
    row_count++;
  }
  EXPECT_EQ(ret, SQL_NO_DATA) << "Fetch ended unexpectedly with return code: " << ret; 

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS); 
}

TEST(DataFetchPerformance, Benchmark_PowerBI_Mimic_AllDataTypes) {
  auto conn = std::make_shared<ODBCHandles>();
  
  std::string connection_string = kDefaultConnectionString + ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;";
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS) << "Failed to connect to the database.";

  std::string target_table = "bigquery-devtools-drivers.DATATYPERANGETEST.AllDataTypes";
  std::string query = "SELECT * FROM `" + target_table + "` LIMIT 1000000";

  SQLRETURN ret = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(ret, "SQLExecDirect", conn); 

  SQLSMALLINT num_cols;
  ret = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(ret, "SQLNumResultCols", conn);

  std::vector<std::shared_ptr<Column>> cols(num_cols);
  for (int i = 1; i <= num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i - 1] = col_ptr;

    DescribeCol(conn, col_ptr, i);
    
    switch (col_ptr->data_type) {
        case SQL_BIGINT:
        case SQL_INTEGER: col_ptr->data_type = SQL_C_SBIGINT; break;
        case SQL_FLOAT:
        case SQL_DOUBLE:  col_ptr->data_type = SQL_C_DOUBLE; break;
        case SQL_BIT:     col_ptr->data_type = SQL_C_BIT; break;
        default:          col_ptr->data_type = SQL_C_CHAR; break; 
    }
    
    ret = SQLBindCol(conn->hstmt, i, col_ptr->data_type, col_ptr->data_buf.target_value,
                     col_ptr->data_buf.buffer_length, &(col_ptr->data_buf.str_len));
    CheckError(ret, "SQLBindCol(" + std::to_string(i) + ")", conn);
  }

  int row_count = 0;
  while ((ret = SQLFetch(conn->hstmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) { 
    row_count++;
  }
  EXPECT_EQ(ret, SQL_NO_DATA) << "Fetch ended unexpectedly with return code: " << ret; 

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS); 
}
}  // namespace google::cloud::odbc_tests

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
