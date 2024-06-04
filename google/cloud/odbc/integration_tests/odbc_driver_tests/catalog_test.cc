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

#include "google/cloud/odbc/testing/odbc_utils/catalog.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"

namespace google::cloud::odbc_tests {

namespace {
// Tables and schema for SQLPrimaryKeys
std::string const kCatalogFnsDataset = "ODBC_TEST_DATASET_CATALOG_FNS";
std::string const kCatalogDatasetTableWithPK =
    "ODBC_SQLPrimaryKeys_TABLE_WITH_PK";
std::string const kCatalogDatasetTableWithoutPK =
    "ODBC_SQLPrimaryKeys_TABLE_WITHOUT_PK";
std::string const kCatalogDatasetTableWithPKFull =
    kCatalogFnsDataset + "." + kCatalogDatasetTableWithPK;
std::string const kCatalogDatasetTableWithoutPKFull =
    kCatalogFnsDataset + "." + kCatalogDatasetTableWithoutPK;

RowWiseResults const kCatalogPrimaryKeysExpected{
    {{1, "bigquery-devtools-drivers"},
     {2, "ODBC_TEST_DATASET_CATALOG_FNS"},
     {3, "ODBC_SQLPrimaryKeys_TABLE_WITH_PK"},
     {4, "StringField"},
     {5, "1"},
     {6, "ODBC_SQLPrimaryKeys_TABLE_WITH_PK.pk$"}},
    {{1, "bigquery-devtools-drivers"},
     {2, "ODBC_TEST_DATASET_CATALOG_FNS"},
     {3, "ODBC_SQLPrimaryKeys_TABLE_WITH_PK"},
     {4, "IntField"},
     {5, "2"},
     {6, "ODBC_SQLPrimaryKeys_TABLE_WITH_PK.pk$"}},
};

std::string const kTableWithPKSchema =
    "CREATE TABLE IF NOT EXISTS " + kCatalogDatasetTableWithPKFull +
    " "
    "(StringField STRING, IntField INT64, FloatField FLOAT64, "
    "PRIMARY KEY (StringField, IntField) NOT ENFORCED)";

std::string const kTableWithOutPKSchema =
    "CREATE TABLE IF NOT EXISTS " + kCatalogDatasetTableWithoutPKFull +
    " "
    "(StringField STRING, IntField INT64, FloatField FLOAT64)";

// Tables and schema for SQLForeignKeys.
std::string const kTableOrders = "ODBC_SQLForeignKeys_TABLE_ORDERS";
std::string const kTableLines = "ODBC_SQLForeignKeys_TABLE_LINES";
std::string const kTableCustomer = "ODBC_SQLForeignKeys_TABLE_CUSTOMER";
std::string const kTableOrdersFull = kCatalogFnsDataset + "." + kTableOrders;
std::string const kTableLinesFull = kCatalogFnsDataset + "." + kTableLines;
std::string const kTableCustomerFull =
    kCatalogFnsDataset + "." + kTableCustomer;

std::string const kTableCustomerSchema =
    "CREATE TABLE IF NOT EXISTS " + kTableCustomerFull +
    " "
    "(CustId STRING, CustName STRING, CustAddress STRING, "
    "PRIMARY KEY (CustId) NOT ENFORCED)";

std::string const kTableOrdersSchema =
    "CREATE TABLE IF NOT EXISTS " + kTableOrdersFull +
    " "
    "(OrderId STRING, CustId STRING, OrderName STRING, OrderStatus STRING, "
    "PRIMARY KEY (OrderId) NOT ENFORCED, "
    "FOREIGN KEY (CustId) "
    "REFERENCES " +
    kTableCustomerFull +
    " (CustId) "
    "NOT ENFORCED)";

std::string const kTableLinesSchema =
    "CREATE TABLE IF NOT EXISTS " + kTableLinesFull +
    " "
    "(LineItemId STRING, OrderId STRING, Quantity INT64, "
    "PRIMARY KEY (LineItemId) NOT ENFORCED, "
    "FOREIGN KEY (OrderId) "
    "REFERENCES " +
    kTableOrdersFull +
    " (OrderId) "
    "NOT ENFORCED)";
}  // namespace
// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

std::map<std::string, Schema> kTables = {
    {"ODBC_SQLTables1_TEST_1", {{"Str1", SQL_VARCHAR}}},
    {"ODBC_SQLTables1_TEST_2",
     {
         {"Str2", SQL_VARCHAR},
         {"Int2", SQL_INTEGER},
         {"Float2", SQL_FLOAT},
     }},
    {"ODBC_SQLTables1_TEST_3",
     {{"Str3", SQL_VARCHAR},
      {"Int3", SQL_INTEGER},
      {"Float3", SQL_FLOAT},
      {"Date3", SQL_DATETIME}}}};

// Tables for ANSI tests
std::map<std::string, Schema> kTablesAnsi = {
    {"ODBC_SQLTablesAnsi_TEST_1", {{"Str1", SQL_VARCHAR}}},
    {"ODBC_SQLTablesAnsi_TEST_2",
     {
         {"Str2", SQL_VARCHAR},
         {"Int2", SQL_INTEGER},
         {"Float2", SQL_FLOAT},
     }},
    {"ODBC_SQLTableAnsi_TEST_3",
     {{"Str3", SQL_VARCHAR},
      {"Int3", SQL_INTEGER},
      {"Float3", SQL_FLOAT},
      {"Date3", SQL_DATETIME}}}};

TEST(CatalogTest, SQLTables) {
  auto conn = std::make_shared<ODBCHandles>();

  // Create tables
  for (auto it : kTables) {
    std::string table_name = it.first;
    std::string table_name_full = kDatasetWithTablePrefix + table_name;
    // Create Table
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    Table(table_name_full).Create(conn, getSchemaStr(it.second));
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);

  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);

  auto table_names = (*Catalog::GetTables(conn, kDatasetName))[kDatasetName];
  std::vector<std::string> test_table_names;
  for (auto it : kTables) {
    EXPECT_NE(std::find(table_names.begin(), table_names.end(),
                        kTableNamePrefix + it.first),
              table_names.end());
    test_table_names.push_back(it.first);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTablesA) {
  auto conn = std::make_shared<ODBCHandles>();

  // Create tables
  for (auto it : kTablesAnsi) {
    std::string table_name = it.first;
    std::string table_name_full = kDatasetWithTablePrefix + table_name;
    // Create Table
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, true), SQL_SUCCESS);
    Table(table_name_full).Create(conn, getSchemaStr(it.second), true);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, true), SQL_SUCCESS);

  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);

  auto table_names =
      (*Catalog::GetTables(conn, kDatasetName, true))[kDatasetName];
  std::vector<std::string> test_table_names;
  for (auto it : kTablesAnsi) {
    EXPECT_NE(std::find(table_names.begin(), table_names.end(),
                        kTableNamePrefix + it.first),
              table_names.end());
    test_table_names.push_back(it.first);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLPrimaryKeys_CreatePrimaryKeysTables) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create primary keys table via Simba Driver since execute is not implemented
  // for BQ Drivers.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithPKSchema);
  CreateTableDirect(conn, kTableWithOutPKSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLPrimaryKeys_CreateForeignKeysTables) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create primary keys table via Simba Driver since execute is not implemented
  // for BQ Drivers.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#else

void VerifyRowWiseResults(RowWiseResults& actual_results,
                          RowWiseResults const& expected_results) {
  // Check if both result sets have the same number of rows
  EXPECT_EQ(actual_results.size(), expected_results.size())
      << "Number of rows mismatch";

  // Iterate over each row and compare the maps
  for (size_t i = 0; i < actual_results.size(); ++i) {
    auto const& actual_row = actual_results[i];
    auto const& expected_row = expected_results[i];
    EXPECT_EQ(actual_row.size(), expected_row.size())
        << "Number of elements in row " << i << " mismatch";

    // Sort map elements for comparison to ensure ordering consistency
    std::vector<std::pair<int, std::string> > sorted_actual_row(
        actual_row.begin(), actual_row.end());
    std::vector<std::pair<int, std::string> > sorted_expected_row(
        expected_row.begin(), expected_row.end());

    std::sort(sorted_actual_row.begin(), sorted_actual_row.end());
    std::sort(sorted_expected_row.begin(), sorted_expected_row.end());

    for (size_t j = 0; j < sorted_actual_row.size(); ++j) {
      // Check if keys and values match
      EXPECT_EQ(sorted_actual_row[j].first, sorted_expected_row[j].first)
          << "Key mismatch at row " << i << ", position " << j;
      EXPECT_EQ(sorted_actual_row[j].second, sorted_expected_row[j].second)
          << "Value mismatch at row " << i << ", position " << j;
    }
  }
}
/////////////////////////////////////////////////////
// SQLPrimaryKeys is not implemented by Simba Driver.
////////////////////////////////////////////////////
TEST(CatalogTest, SQLPrimaryKeys_TableWithPrimaryKeys) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create table if not exists.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithPKSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  RowWiseResults primary_keys = Catalog::GetPrimaryKeys(
      conn, kCatalogFnsDataset, kCatalogDatasetTableWithPK);
  VerifyRowWiseResults(primary_keys, kCatalogPrimaryKeysExpected);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLPrimaryKeys_TableWithoutPrimaryKeys) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create table if not exists.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithOutPKSchema);
  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto primary_keys = Catalog::GetPrimaryKeys(conn, kCatalogFnsDataset,
                                              kCatalogDatasetTableWithoutPK);
  EXPECT_TRUE(primary_keys.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, ANSI_SQLPrimaryKeys_TableWithPrimaryKeys) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create table if not exists.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithPKSchema, true);
  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto primary_keys = Catalog::GetPrimaryKeys(conn, kCatalogFnsDataset,
                                              kCatalogDatasetTableWithPK, true);
  VerifyRowWiseResults(primary_keys, kCatalogPrimaryKeysExpected);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, ANSI_SQLPrimaryKeys_TableWithoutPrimaryKeys) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create table if not exists.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithOutPKSchema, true);
  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto primary_keys = Catalog::GetPrimaryKeys(
      conn, kCatalogFnsDataset, kCatalogDatasetTableWithoutPK, true);
  EXPECT_TRUE(primary_keys.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

/////////////////////////////////////////////////////
// SQLForeignKeys is not implemented by Simba Driver.
////////////////////////////////////////////////////
TEST(CatalogTest, SQLForeignKeys_With_PkTableAndFkTableName) {
  auto conn = std::make_shared<ODBCHandles>();
  // Connect to DS
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto foreign_keys =
      Catalog::GetForeignKeys(conn, kCatalogFnsDataset, kTableCustomer,
                              kTableOrders); /* both PK and FK table supplied*/

  // Uncomment the statement below once the function is fully implemented
  // with SQLFetch.
  // EXPECT_FALSE(primary_keys.empty());
  if (foreign_keys.empty()) {
    std::cout << "SQLForeignKeys not yet implemented" << std::endl;
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLForeignKeys_With_PkTable) {
  auto conn = std::make_shared<ODBCHandles>();
  // Connect to DS
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto foreign_keys = Catalog::GetForeignKeys(
      conn, kCatalogFnsDataset, kTableCustomer); /* empty FK table */

  // Uncomment the statement below once the function is fully implemented
  // with SQLFetch.
  // EXPECT_FALSE(primary_keys.empty());
  if (foreign_keys.empty()) {
    std::cout << "SQLForeignKeys not yet implemented" << std::endl;
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLForeignKeys_With_FkTableName) {
  auto conn = std::make_shared<ODBCHandles>();
  // Connect to DS
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto foreign_keys = Catalog::GetForeignKeys(
      conn, kCatalogFnsDataset, "" /*empty PK Table*/, kTableOrders);

  // Uncomment the statement below once the function is fully implemented
  // with SQLFetch.
  // EXPECT_FALSE(primary_keys.empty());
  if (foreign_keys.empty()) {
    std::cout << "SQLForeignKeys not yet implemented" << std::endl;
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLForeignKeys_With_PkTableAndFkTableName_ANSI) {
  auto conn = std::make_shared<ODBCHandles>();
  // Connect to DS
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto foreign_keys = Catalog::GetForeignKeys(
      conn, kCatalogFnsDataset, kTableCustomer, kTableOrders,
      true); /* both PK and FK table supplied*/

  // Uncomment the statement below once the function is fully implemented
  // with SQLFetch.
  // EXPECT_FALSE(primary_keys.empty());
  if (foreign_keys.empty()) {
    std::cout << "SQLForeignKeys not yet implemented" << std::endl;
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLForeignKeys_With_PkTable_ANSI) {
  auto conn = std::make_shared<ODBCHandles>();
  // Connect to DS
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto foreign_keys = Catalog::GetForeignKeys(
      conn, kCatalogFnsDataset, kTableCustomer, "" /* empty FK table */, true);

  // Uncomment the statement below once the function is fully implemented
  // with SQLFetch.
  // EXPECT_FALSE(primary_keys.empty());
  if (foreign_keys.empty()) {
    std::cout << "SQLForeignKeys not yet implemented" << std::endl;
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLForeignKeys_With_FkTableName_ANSI) {
  auto conn = std::make_shared<ODBCHandles>();
  // Connect to DS
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // Create Customer Table.
  CreateTableDirect(conn, kTableCustomerSchema);
  // Create Orders Table.
  CreateTableDirect(conn, kTableOrdersSchema);
  // Create Lines Table.
  CreateTableDirect(conn, kTableLinesSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto foreign_keys = Catalog::GetForeignKeys(
      conn, kCatalogFnsDataset, "" /*empty PK Table*/, kTableOrders, true);

  // Uncomment the statement below once the function is fully implemented
  // with SQLFetch.
  // EXPECT_FALSE(primary_keys.empty());
  if (foreign_keys.empty()) {
    std::cout << "SQLForeignKeys not yet implemented" << std::endl;
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
