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
#include "gmock/gmock.h"

namespace google::cloud::odbc_tests {

using ::testing::StartsWith;

namespace {

std::string const kTable = kIsBqDriver ? "BASE TABLE" : "TABLE";
std::string const kView = "VIEW";
std::string const kExternal = "EXTERNAL";
std::string const kMaterializedView =
    kIsBqDriver ? "MATERIALIZED VIEW" : "MATERIALIZED_VIEW";
std::string const kSnapshot = "SNAPSHOT";

// Tables and schema for SQLPrimaryKeys
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

RowWiseResults const kCatalogForeignKeysExpected{
    {
        {1, "bigquery-devtools-drivers"},
        {2, "ODBC_TEST_DATASET_CATALOG_FNS"},
        {3, "ODBC_SQLForeignKeys_TABLE_CUSTOMER"},
        {4, "CustId"},
        {5, "bigquery-devtools-drivers"},
        {6, "ODBC_TEST_DATASET_CATALOG_FNS"},
        {7, "ODBC_SQLForeignKeys_TABLE_ORDERS"},
        {8, "CustId"},
        {9, "1"},
        {10, "NULL"},
        {11, "NULL"},
        {12, "ODBC_SQLForeignKeys_TABLE_ORDERS.fk$1"},
        {13, "ODBC_SQLForeignKeys_TABLE_CUSTOMER.pk$"},
        {14, "7"},
    },
};

std::string const kTableWithPKSchema =
    "CREATE OR REPLACE TABLE " + kCatalogDatasetTableWithPKFull +
    " "
    "(StringField STRING, IntField INT64, FloatField FLOAT64, "
    "PRIMARY KEY (StringField, IntField) NOT ENFORCED)";

std::string const kTableWithOutPKSchema =
    "CREATE OR REPLACE TABLE " + kCatalogDatasetTableWithoutPKFull +
    " "
    "(StringField STRING, IntField INT64, FloatField FLOAT64)";

// Tables and schema for SQLForeignKeys.
std::string const kTableOrdersFull = kCatalogFnsDataset + "." + kTableOrders;
std::string const kTableLinesFull = kCatalogFnsDataset + "." + kTableLines;
std::string const kTableCustomerFull =
    kCatalogFnsDataset + "." + kTableCustomer;

std::string const kTableCustomerSchema =
    "CREATE OR REPLACE TABLE " + kTableCustomerFull +
    " "
    "(CustId STRING, CustName STRING, CustAddress STRING, "
    "PRIMARY KEY (CustId) NOT ENFORCED)";

std::string const kTableOrdersSchema =
    "CREATE OR REPLACE TABLE " + kTableOrdersFull +
    " "
    "(OrderId STRING, CustId STRING, OrderName STRING, OrderStatus STRING, "
    "PRIMARY KEY (OrderId) NOT ENFORCED, "
    "FOREIGN KEY (CustId) "
    "REFERENCES " +
    kTableCustomerFull +
    " (CustId) "
    "NOT ENFORCED)";

std::string const kTableLinesSchema =
    "CREATE OR REPLACE TABLE " + kTableLinesFull +
    " "
    "(LineItemId STRING, OrderId STRING, Quantity INT64, "
    "PRIMARY KEY (LineItemId) NOT ENFORCED, "
    "FOREIGN KEY (OrderId) "
    "REFERENCES " +
    kTableOrdersFull +
    " (OrderId) "
    "NOT ENFORCED)";
}  // namespace

bool FindTableInVector(std::string const& table_name,
                       std::vector<std::string>& table_names) {
  return std::find_if(table_names.begin(), table_names.end(),
                      [table_name](std::string const& name) {
                        return name == table_name;
                      }) != table_names.end();
}

TEST(CatalogTest, SQLTables) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {"ODBC_SQLTables1_TEST_1",
                                          "ODBC_SQLTables1_TEST_2",
                                          "ODBC_SQLTables1_TEST_3"};
  for (auto const& name : table_names) {
    Table(kCatalogFnsDataset + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, kCatalogName, kCatalogFnsDataset.c_str());
  int count_tables = 0;
  for (auto const& result : results) {
    EXPECT_EQ(kCatalogName, result.project_name);
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name);
    if (FindTableInVector(result.table_name, table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kCatalogName, result.description);
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTablesA) {
  auto conn = std::make_shared<ODBCHandles>();

  // Create tables
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {"ODBC_SQLTablesAnsi_TEST_1",
                                          "ODBC_SQLTablesAnsi_TEST_2",
                                          "ODBC_SQLTableAnsi_TEST_3"};
  for (auto const& name : table_names) {
    Table(kCatalogFnsDataset + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results = Catalog::GetTables(
      conn, kCatalogName, kCatalogFnsDataset.c_str(), nullptr, nullptr, true);
  int count_tables = 0;
  for (auto const& result : results) {
    EXPECT_EQ(kCatalogName, result.project_name);
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name);
    if (FindTableInVector(result.table_name, table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kCatalogName, result.description);
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_AllProjects) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, SQL_ALL_CATALOGS, "", "");

  bool project_found = false;
  for (auto const& result : results) {
    project_found = project_found || (kCatalogName == result.project_name);
    EXPECT_EQ(kCatalogName, result.project_name);
    EXPECT_TRUE(result.dataset_name.empty());
    EXPECT_TRUE(result.table_name.empty());
    EXPECT_TRUE(result.table_type.empty());
    EXPECT_TRUE(result.description.empty());
  }
  EXPECT_TRUE(project_found);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_AllDatasets) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, "", SQL_ALL_SCHEMAS, "");

  bool catalog_found = false;
  for (auto const& result : results) {
    EXPECT_TRUE(result.project_name.empty());
    catalog_found = catalog_found || kCatalogFnsDataset == result.dataset_name;
    EXPECT_TRUE(result.table_name.empty());
    EXPECT_TRUE(result.table_type.empty());
    EXPECT_TRUE(result.description.empty());
  }
  EXPECT_TRUE(catalog_found);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_AllTableTypes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, "", "", "", SQL_ALL_TABLE_TYPES);

  std::vector<std::string> expected_types = {kTable, kView, kExternal,
                                             kMaterializedView, kSnapshot};
  EXPECT_EQ(expected_types.size(), results.size());
  for (auto const& result : results) {
    EXPECT_TRUE(result.project_name.empty());
    EXPECT_TRUE(result.dataset_name.empty());
    EXPECT_TRUE(result.table_name.empty());
    EXPECT_NE(std::find(expected_types.begin(), expected_types.end(),
                        result.table_type),
              expected_types.end());
    EXPECT_TRUE(result.description.empty());
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_WithFiltering) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {
      "ODBC_SQLTables_SQLTables_WithFiltering_1",
      "ODBC_SQLTables_SQLTables_WithFiltering_2"};
  for (auto const& name : table_names) {
    Table(kCatalogFnsDataset + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::string project_to_filter = kCatalogName.substr(0, 8);
  std::string dataset_to_filter = kCatalogFnsDataset.substr(0, 8);

  std::vector<SQLTableResult> results = Catalog::GetTables(
      conn, project_to_filter + "%", (dataset_to_filter + "%").c_str(),
      R"(%ODBC\_SQL_ables\_SQLT_bles\_With_iltering\_%)", kTable.c_str());

  int count_tables = 0;
  for (auto const& result : results) {
    EXPECT_THAT(result.project_name, StartsWith(project_to_filter));
    EXPECT_THAT(result.dataset_name, StartsWith(dataset_to_filter));
    if (FindTableInVector(result.table_name, table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kTable, result.table_type);
    EXPECT_EQ(result.project_name, result.description);
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_TablesAndViews) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {
      "ODBC_SQLTables_SQLTables_TablesAndViews_1"};
  for (auto const& name : table_names) {
    Table(kCatalogFnsDataset + "." + name).Create(conn, "(Str1 STRING)");
  }
  std::string view_name = "ODBC_SQLTables_SQLTables_TablesAndViews_View_1";
  std::string view_creation = "CREATE VIEW IF NOT EXISTS " +
                              kCatalogFnsDataset + "." + view_name +
                              " AS (SELECT Str1 FROM " + kCatalogFnsDataset +
                              "." + table_names[0] + ");";
  CreateTableDirect(conn, view_creation);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::string table_type_filter = " ' " + kTable + " ' , ' " + kView + " ' ";
  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, kCatalogName, kCatalogFnsDataset.c_str(),
                         nullptr, table_type_filter.c_str());

  EXPECT_FALSE(results.empty());
  int count_tables = 0;
  bool view_found = false;
  for (auto const& result : results) {
    EXPECT_EQ(kCatalogName, result.project_name);
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name);
    if (FindTableInVector(result.table_name, table_names)) {
      count_tables++;
    }
    view_found = view_found || (view_name == result.table_name);
    EXPECT_TRUE(result.table_type == kTable || result.table_type == kView)
        << "Actual type is " << result.table_type;
    EXPECT_EQ(result.project_name, result.description);
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";
  EXPECT_TRUE(view_found);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_MetadataId_True) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {
      "ODBC_SQLTables_SQLTables_MetadataId_True_1",
      "odbc_sqltables_sqltables_metadataid_true_1"};
  for (auto const& name : table_names) {
    Table(kCatalogFnsDataset + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_TRUE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, kCatalogName, kCatalogFnsDataset.c_str(),
                         (table_names[0] + "   ").c_str());

  int count_tables = 0;
  for (auto const& result : results) {
    EXPECT_EQ(kCatalogName, result.project_name);
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name);
    if (FindTableInVector(result.table_name, table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kTable, result.table_type);
    EXPECT_EQ(result.project_name, result.description);
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";

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

TEST(CatalogTest, SQLForeignKeys_CreateForeignKeysTables) {
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

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifdef BQ_DRIVER_INTEGRATION_TESTS

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
  VerifyRowWiseResults(foreign_keys, kCatalogForeignKeysExpected);
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

  VerifyRowWiseResults(foreign_keys, kCatalogForeignKeysExpected);
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

  VerifyRowWiseResults(foreign_keys, kCatalogForeignKeysExpected);
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

  VerifyRowWiseResults(foreign_keys, kCatalogForeignKeysExpected);
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
  VerifyRowWiseResults(foreign_keys, kCatalogForeignKeysExpected);
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
  VerifyRowWiseResults(foreign_keys, kCatalogForeignKeysExpected);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
