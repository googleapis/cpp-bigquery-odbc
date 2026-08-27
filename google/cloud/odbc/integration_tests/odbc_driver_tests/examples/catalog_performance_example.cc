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
#ifdef _WIN32
#include <windows.h>
#endif

namespace google::cloud::odbc_tests {

// Primary Keys Performance Tests
TEST(CatalogPerformanceTest, BenchmarkGetPrimaryKeysExactTable) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableWithPKSchema);

  RowWiseResults results =
      Catalog::GetPrimaryKeys(conn, kDatasetName, kCatalogDatasetTableWithPK);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, BenchmarkGetPrimaryKeysNoPKTable) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableWithOutPKSchema);

  RowWiseResults results = Catalog::GetPrimaryKeys(
      conn, kDatasetName, kCatalogDatasetTableWithoutPK);

  EXPECT_TRUE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// Foreign Keys Performance Tests
TEST(CatalogPerformanceTest, BenchmarkGetForeignKeysPkAndFkTables) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results =
      Catalog::GetForeignKeys(conn, kDatasetName, kTableCustomer, kTableOrders);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, BenchmarkGetForeignKeysPkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results =
      Catalog::GetForeignKeys(conn, kDatasetName, kTableCustomer, "");

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, BenchmarkGetForeignKeysFkTableOnly) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  CreateTableDirect(conn, kTableCustomerSchema);
  CreateTableDirect(conn, kTableOrdersSchema);

  RowWiseResults results =
      Catalog::GetForeignKeys(conn, kDatasetName, "", kTableOrders);

  EXPECT_FALSE(results.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// SQLTables Performance Tests
TEST(CatalogPerformanceTest, SQLTablesFullCatalogEnumerationTableAndView) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string catalog_pattern = "%";
  std::string table_types = "TABLE,VIEW";

  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, catalog_pattern, nullptr, nullptr, table_types.c_str());

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, SQLTablesFullCatalogEnumerationWildcardCatalog) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string catalog_pattern = "%";
  std::string conn_str =
      kDefaultConnectionString + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables =
      Catalog::GetTables(conn, catalog_pattern, nullptr, nullptr, nullptr);

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, SQLTablesDatasetLevelEnumeration) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "ODBC_TEST_DATASET";
  std::string conn_str = kDefaultConnectionString +
                         ";DefaultDataset=" + dataset +
                         ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> dataset_tables =
      Catalog::GetTables(conn, kCatalogName, dataset.c_str(), nullptr, nullptr);

  ASSERT_FALSE(dataset_tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, SQLTablesExactTableLookup) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string conn_str = kDefaultConnectionString +
                         ";DefaultDataset=" + dataset +
                         ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables = Catalog::GetTables(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  ASSERT_FALSE(tables.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, SQLTablesWildcardTableSearch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_pattern = "%timestamp%";
  std::string conn_str = kDefaultConnectionString +
                         ";DefaultDataset=" + dataset +
                         ";FilterTablesOnDefaultDataset=0;";

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
TEST(CatalogPerformanceTest, SQLColumnsFullMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, SQLColumnsExactColumnLookup) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string column_name = "timestamp_col_1";
  std::string conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(conn, kCatalogName, dataset.c_str(),
                          table_name.c_str(), column_name.c_str());

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, SQLColumnsWildcardColumnSearch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string column_pattern = "%timestamp%";
  std::string conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(conn, kCatalogName, dataset.c_str(),
                          table_name.c_str(), column_pattern.c_str());

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogPerformanceTest, SQLColumnsLargeSchemaMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string dataset = "kirltest";
  std::string table_name = "300_column_timestamp";
  std::string conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  std::vector<SQLColumnsResult> columns = Catalog::GetColumns(
      conn, kCatalogName, dataset.c_str(), table_name.c_str(), nullptr);

  ASSERT_FALSE(columns.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// FilterTablesOnDefaultDataset (ON/OFF) Performance Tests
TEST(CatalogPerformanceTest, SQLTablesFullCatalogEnumerationFilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  std::string conn_str_unfiltered =
      base_conn_str + ";FilterTablesOnDefaultDataset=0;";
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables_unfiltered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string conn_str_filtered =
      base_conn_str + ";FilterTablesOnDefaultDataset=1;";
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables_filtered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#if defined(BQ_DRIVER_INTEGRATION_TESTS)
TEST(CatalogPerformanceTest, SQLColumnsColumnMetadataEnumerationFilterOnOff) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";
  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  std::string conn_str_unfiltered =
      base_conn_str + ";FilterTablesOnDefaultDataset=0;";
  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLColumnsResult> cols_unfiltered =
      Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string conn_str_filtered =
      base_conn_str + ";FilterTablesOnDefaultDataset=1;";
  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          reinterpret_cast<SQLPOINTER>(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLColumnsResult> cols_filtered =
      Catalog::GetColumns(conn, kCatalogName, nullptr, nullptr, nullptr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#endif

using DataFetchParams = std::tuple<std::string, std::string, int64_t>;

class DataFetchPerformanceParamTest
    : public ::testing::TestWithParam<DataFetchParams> {};

TEST_P(DataFetchPerformanceParamTest, Benchmark) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string connection_string =
      kDefaultConnectionString +
      ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;";
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS)
      << "Failed to connect to the database.";

  auto const& params = GetParam();
  std::string test_name = std::get<0>(params);
  std::string query = std::get<1>(params);
  int64_t expected_row_count = std::get<2>(params);

  auto ttfb_start = std::chrono::high_resolution_clock::now();
  SQLRETURN ret = SQLExecDirect(conn->hstmt, ToSqlChar(query.c_str()), SQL_NTS);
  auto ttfb_end = std::chrono::high_resolution_clock::now();
  CheckError(ret, "SQLExecDirect", conn);
  auto ttfb_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              ttfb_end - ttfb_start)
                              .count();

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
  auto fetch_start = std::chrono::high_resolution_clock::now();
  while ((ret = SQLFetch(conn->hstmt)) == SQL_SUCCESS ||
         ret == SQL_SUCCESS_WITH_INFO) {
    row_count++;
  }
  auto fetch_end = std::chrono::high_resolution_clock::now();
  auto fetch_duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(fetch_end -
                                                            fetch_start)
          .count();

  EXPECT_EQ(ret, SQL_NO_DATA)
      << "Fetch ended unexpectedly with return code: " << ret;
  EXPECT_EQ(row_count, expected_row_count)
      << "Mismatch in number of rows fetched for " << test_name;

  std::cout << "[ METRIC ] " << test_name
            << " (Time to first byte): " << ttfb_duration_ms << "ms"
            << std::endl;
  std::cout << "[ METRIC ] " << test_name
            << " (Iteration time): " << fetch_duration_ms << "ms" << std::endl;

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

struct BenchmarkConfig {
  std::string name;
  std::string base_query;
  std::vector<std::pair<std::string, int64_t>> limits;
};

inline std::vector<DataFetchParams> GetDataFetchBenchmarkParams() {
  std::vector<BenchmarkConfig> const benchmark_configs = {
      {"new_timestamp_table",
       "SELECT * FROM "
       "`bigquery-devtools-drivers.kirltest.new_timestamp_table`",
       {{"10k", 10000}, {"100k", 100000}, {"1M", 1000000}}},

      {"all_bq_types_2",
       "SELECT * FROM "
       "`bigquery-devtools-drivers.INTEGRATION_TEST_FORMAT.all_bq_types_2`",
       {{"10k", 10000}, {"100k", 100000}, {"1M", 1000000}}},

      {"nyc311_service_requests",
       "SELECT nyc311.unique_key AS V1, nyc311.descriptor AS V2, "
       "nyc311.open_data_channel_type AS V3, nyc311.status AS V4, "
       "nyc311.incident_address AS V5, nyc311.street_name AS V7, "
       "nyc311.city AS V8, nyc311.incident_zip AS V9, nyc311.borough AS V10, "
       "nyc311.x_coordinate AS V11, nyc311.y_coordinate AS V12, "
       "nyc311.latitude AS V13, nyc311.longitude AS V14, nyc311.location AS "
       "V15, "
       "nyc311.community_board AS V16, NULL AS V17, NULL AS V18, "
       "CAST(nyc311.resolution_action_updated_date AS STRING) AS V19, "
       "CAST(nyc311.created_date AS STRING) AS V20, "
       "CAST(nyc311.resolution_action_updated_date AS STRING) AS V21, "
       "CAST(nyc311.closed_date AS STRING) AS V22 FROM "
       "`bigquery-public-data.new_york_311.311_service_requests` AS nyc311",
       {{"10k", 10000}, {"100k", 100000}, {"1M", 1000000}}},

      {"AllDataTypes_2",
       "SELECT * FROM "
       "`bigquery-devtools-drivers.DATATYPERANGETEST.AllDataTypes_2`",
       {{"10k", 10000}, {"100k", 100000}, {"1M", 1000000}}},

      {"RangeIntervalTestTable_2",
       "SELECT * FROM "
       "`bigquery-devtools-drivers.DATATYPERANGETEST.RangeIntervalTestTable_2`",
       {{"10k", 10000}, {"100k", 100000}, {"1M", 1000000}}},
  };

  std::vector<DataFetchParams> params;
  for (auto const& config : benchmark_configs) {
    for (auto const& [label, limit] : config.limits) {
      std::string test_name = config.name + "_" + label;
      std::string query =
          config.base_query + " LIMIT " + std::to_string(limit) + ";";
      params.emplace_back(test_name, query, limit);
    }
  }
  return params;
}

INSTANTIATE_TEST_SUITE_P(
    , DataFetchPerformanceParamTest,
    ::testing::ValuesIn(GetDataFetchBenchmarkParams()),
    [](::testing::TestParamInfo<DataFetchParams> const& info) {
      return std::get<0>(info.param);
    });

class DataFetchPerformanceParamTest_WithSQLGetData
    : public ::testing::TestWithParam<DataFetchParams> {};

TEST_P(DataFetchPerformanceParamTest_WithSQLGetData, Benchmark) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string connection_string =
      kDefaultConnectionString +
      ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;";
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS)
      << "Failed to connect to the database.";

  auto const& params = GetParam();
  std::string test_name = std::get<0>(params);
  std::string query = std::get<1>(params);
  int64_t expected_row_count = std::get<2>(params);

  auto ttfb_start = std::chrono::high_resolution_clock::now();
  SQLRETURN ret = SQLExecDirect(conn->hstmt, ToSqlChar(query.c_str()), SQL_NTS);
  auto ttfb_end = std::chrono::high_resolution_clock::now();
  CheckError(ret, "SQLExecDirect", conn);
  auto ttfb_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              ttfb_end - ttfb_start)
                              .count();

  SQLSMALLINT num_cols;
  ret = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(ret, "SQLNumResultCols", conn);

  std::vector<std::shared_ptr<Column>> cols(num_cols);
  for (int i = 1; i <= num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i - 1] = col_ptr;

    DescribeCol(conn, col_ptr, i);

    SqlToCdataTypes(col_ptr);
  }

  int row_count = 0;
  auto fetch_start = std::chrono::high_resolution_clock::now();
  while ((ret = SQLFetch(conn->hstmt)) == SQL_SUCCESS ||
         ret == SQL_SUCCESS_WITH_INFO) {
    for (int i = 1; i <= num_cols; i++) {
      auto const& col_ptr = cols[i - 1];
      SQLRETURN get_data_ret = SQLGetData(
          conn->hstmt, i, col_ptr->data_type, col_ptr->data_buf.target_value,
          col_ptr->data_buf.buffer_length, &(col_ptr->data_buf.str_len));
      CheckError(get_data_ret, "SQLGetData(" + std::to_string(i) + ")", conn);
    }
    row_count++;
  }
  auto fetch_end = std::chrono::high_resolution_clock::now();
  auto fetch_duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(fetch_end -
                                                            fetch_start)
          .count();

  EXPECT_EQ(ret, SQL_NO_DATA)
      << "Fetch ended unexpectedly with return code: " << ret;
  EXPECT_EQ(row_count, expected_row_count)
      << "Mismatch in number of rows fetched for " << test_name;

  std::cout << "[ METRIC ] " << test_name
            << " (Time to first byte): " << ttfb_duration_ms << "ms"
            << std::endl;
  std::cout << "[ METRIC ] " << test_name
            << " (Iteration time): " << fetch_duration_ms << "ms" << std::endl;

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

inline std::vector<DataFetchParams> GetDataFetchSQLGetDataBenchmarkParams() {
  std::vector<BenchmarkConfig> const benchmark_configs = {
      {"all_bq_types_2_SQLGetData",
       "SELECT * FROM "
       "`bigquery-devtools-drivers.INTEGRATION_TEST_FORMAT.all_bq_types_2`",
       {{"1M", 1000000}}},
  };

  std::vector<DataFetchParams> params;
  for (auto const& config : benchmark_configs) {
    for (auto const& [label, limit] : config.limits) {
      std::string test_name = config.name + "_" + label;
      std::string query =
          config.base_query + " LIMIT " + std::to_string(limit) + ";";
      params.emplace_back(test_name, query, limit);
    }
  }
  return params;
}

INSTANTIATE_TEST_SUITE_P(
    , DataFetchPerformanceParamTest_WithSQLGetData,
    ::testing::ValuesIn(GetDataFetchSQLGetDataBenchmarkParams()),
    [](::testing::TestParamInfo<DataFetchParams> const& info) {
      return std::get<0>(info.param);
    });

}  // namespace google::cloud::odbc_tests

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

#ifdef _WIN32
  TerminateProcess(GetCurrentProcess(), result);
#endif
  std::_Exit(result);
}
