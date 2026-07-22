// Copyright 2026 Google LLC
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
#include <tuple>
#include <vector>

namespace google::cloud::odbc_tests {

class CatalogPerformanceHtapiParamTest : public ::testing::TestWithParam<bool> {
 protected:
  static std::string GetConnectionString(std::string const& base_conn_str,
                                         bool use_htapi) {
    std::string htapi_str =
        use_htapi ? ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;"
                  : ";AllowHtapiForLargeResults=0;";
    return base_conn_str + htapi_str;
  }
};

// Primary Keys Performance Tests
TEST_P(CatalogPerformanceHtapiParamTest, BenchmarkGetPrimaryKeysExactTable) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str =
      GetConnectionString(kDefaultConnectionString, GetParam());
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableWithPKSchema);

  RowWiseResults results =
      Catalog::GetPrimaryKeys(conn, kDatasetName, kCatalogDatasetTableWithPK);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, BenchmarkGetPrimaryKeysNoPKTable) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str =
      GetConnectionString(kDefaultConnectionString, GetParam());
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableWithOutPKSchema);

  RowWiseResults results = Catalog::GetPrimaryKeys(
      conn, kDatasetName, kCatalogDatasetTableWithoutPK);

  EXPECT_TRUE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// Foreign Keys Performance Tests
TEST_P(CatalogPerformanceHtapiParamTest, BenchmarkGetForeignKeysPkAndFkTables) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str =
      GetConnectionString(kDefaultConnectionString, GetParam());
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results =
      Catalog::GetForeignKeys(conn, kDatasetName, kTableCustomer, kTableOrders);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, BenchmarkGetForeignKeysPkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str =
      GetConnectionString(kDefaultConnectionString, GetParam());
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results =
      Catalog::GetForeignKeys(conn, kDatasetName, kTableCustomer, "");

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, BenchmarkGetForeignKeysFkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str =
      GetConnectionString(kDefaultConnectionString, GetParam());
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results =
      Catalog::GetForeignKeys(conn, kDatasetName, "", kTableOrders);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// SQLTables Performance Tests
TEST_P(CatalogPerformanceHtapiParamTest,
       SQLTablesFullCatalogEnumerationTableAndView) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string catalog_pattern = "%";
  std::string table_types = "TABLE,VIEW";
  std::string conn_str =
      GetConnectionString(kDefaultConnectionString, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, catalog_pattern, nullptr, nullptr, table_types.c_str());

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest,
       SQLTablesFullCatalogEnumerationWildcardCatalog) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string catalog_pattern = "%";
  std::string base_conn_str =
      kDefaultConnectionString + ";FilterTablesOnDefaultDataset=0;";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables =
      Catalog::GetTables(conn, catalog_pattern, nullptr, nullptr, nullptr);

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, SQLTablesDatasetLevelEnumeration) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str = kDefaultConnectionString +
                              ";DefaultDataset=" + dataset +
                              ";FilterTablesOnDefaultDataset=0;";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> dataset_tables =
      Catalog::GetTables(conn, kCatalogName, dataset.c_str(), nullptr, nullptr);

  ASSERT_FALSE(dataset_tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, SQLTablesExactTableLookup) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string base_conn_str = kDefaultConnectionString +
                              ";DefaultDataset=" + dataset +
                              ";FilterTablesOnDefaultDataset=0;";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, SQLTablesWildcardTableSearch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_pattern = "%timestamp%";
  std::string base_conn_str = kDefaultConnectionString +
                              ";DefaultDataset=" + dataset +
                              ";FilterTablesOnDefaultDataset=0;";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), table_pattern.c_str(), nullptr);

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// SQLColumns Performance Tests
TEST_P(CatalogPerformanceHtapiParamTest, SQLColumnsFullMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, SQLColumnsExactColumnLookup) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string column_name = "timestamp_col_1";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(conn, kCatalogName, dataset.c_str(),
                          table_name.c_str(), column_name.c_str());

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, SQLColumnsWildcardColumnSearch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string column_pattern = "%timestamp%";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(conn, kCatalogName, dataset.c_str(),
                          table_name.c_str(), column_pattern.c_str());

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(CatalogPerformanceHtapiParamTest, SQLColumnsLargeSchemaMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "300_column_timestamp";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";
  std::string conn_str = GetConnectionString(base_conn_str, GetParam());

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// FilterTablesOnDefaultDataset (ON/OFF) Performance Tests
TEST_P(CatalogPerformanceHtapiParamTest,
       SQLTablesFullCatalogEnumerationFilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  std::string conn_str_unfiltered = GetConnectionString(
      base_conn_str + ";FilterTablesOnDefaultDataset=0;", GetParam());
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables_unfiltered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string conn_str_filtered = GetConnectionString(
      base_conn_str + ";FilterTablesOnDefaultDataset=1;", GetParam());
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables_filtered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#if defined(BQ_DRIVER_INTEGRATION_TESTS)
TEST_P(CatalogPerformanceHtapiParamTest,
       SQLColumnsColumnMetadataEnumerationFilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  std::string conn_str_unfiltered = GetConnectionString(
      base_conn_str + ";FilterTablesOnDefaultDataset=0;", GetParam());
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLColumnsResult> cols_unfiltered =
      Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string conn_str_filtered = GetConnectionString(
      base_conn_str + ";FilterTablesOnDefaultDataset=1;", GetParam());
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLColumnsResult> cols_filtered =
      Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#endif

INSTANTIATE_TEST_SUITE_P(
    HTAPIVariations, CatalogPerformanceHtapiParamTest,
    ::testing::Values(true, false),
    [](::testing::TestParamInfo<
        CatalogPerformanceHtapiParamTest::ParamType> const& info) {
      return info.param ? "WithHTAPI" : "WithoutHTAPI";
    });

using DataFetchParams = std::tuple<std::string, int>;

class DataFetchPerformanceParamTest
    : public ::testing::TestWithParam<DataFetchParams> {};

TEST_P(DataFetchPerformanceParamTest, BenchmarkPowerBIMimic) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string connection_string =
      kDefaultConnectionString +
      ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;";
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS)
      << "Failed to connect to the database.";

  auto const& params = GetParam();
  std::string target_table = std::get<0>(params);
  int limit = std::get<1>(params);
  std::string query =
      "SELECT * FROM `" + target_table + "` LIMIT " + std::to_string(limit);

  SQLRETURN ret = SQLExecDirect(conn->hstmt, ToSqlChar(query.c_str()), SQL_NTS);
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

    ret = SQLBindCol(
        conn->hstmt, i, col_ptr->data_type, col_ptr->data_buf.target_value,
        col_ptr->data_buf.buffer_length, &(col_ptr->data_buf.str_len));
    CheckError(ret, "SQLBindCol(" + std::to_string(i) + ")", conn);
  }

  int row_count = 0;
  while ((ret = SQLFetch(conn->hstmt)) == SQL_SUCCESS ||
         ret == SQL_SUCCESS_WITH_INFO) {
    row_count++;
  }
  EXPECT_EQ(ret, SQL_NO_DATA)
      << "Fetch ended unexpectedly with return code: " << ret;

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(
    Tables, DataFetchPerformanceParamTest,
    ::testing::Values(
        std::make_tuple(
            "bigquery-devtools-drivers.kirltest.new_timestamp_table", 200000),
        std::make_tuple(
            "bigquery-devtools-drivers.INTEGRATION_TEST_FORMAT.all_bq_types_2",
            200000)
        // TODO: Re-enable this benchmark once HTAPI Arrow supports all data
        // types. Currently SQLExecDirect fails with:
        // "[Google][ODBC BigQuery Driver] Internal Error: Unsupported arrow
        // data type (0)"
        //        std::make_tuple("bigquery-devtools-drivers.DATATYPERANGETEST.AllDataTypes_2",
        //        100000)
        ),
    [](::testing::TestParamInfo<DataFetchParams> const& info) {
      std::string target_table = std::get<0>(info.param);
      auto last_dot = target_table.find_last_of('.');
      std::string table_name = (last_dot != std::string::npos)
                                   ? target_table.substr(last_dot + 1)
                                   : target_table;
      return table_name;
    });
}  // namespace google::cloud::odbc_tests

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
