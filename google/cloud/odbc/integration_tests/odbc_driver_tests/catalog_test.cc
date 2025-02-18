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
#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include "gmock/gmock.h"
#include <chrono>
#include <thread>

namespace google::cloud::odbc_tests {

using ::testing::StartsWith;
namespace {

std::string const kTable = kIsBqDriver ? "BASE TABLE" : "TABLE";
std::string const kView = "VIEW";
std::string const kExternal = "EXTERNAL";
std::string const kMaterializedView =
    kIsBqDriver ? "MATERIALIZED VIEW" : "MATERIALIZED_VIEW";
std::string const kSnapshot = "SNAPSHOT";

// Tables and schema for SQLPrimaryKeys and SQLForeignKeys.
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
    "CREATE TABLE IF NOT EXISTS " + kCatalogDatasetTableWithPKFull +
    " "
    "(StringField STRING, IntField INT64, FloatField FLOAT64, "
    "PRIMARY KEY (StringField, IntField) NOT ENFORCED)";

std::string const kTableWithOutPKSchema =
    "CREATE TABLE IF NOT EXISTS " + kCatalogDatasetTableWithoutPKFull +
    " "
    "(StringField STRING, IntField INT64, FloatField FLOAT64)";

// Tables and schema for SQLForeignKeys.
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

// Table and Schema used to test SQLColumns API
std::string const kSqlColumnsTable = "ODBC_SQLColumns_TABLE_LATEST_2";
std::string const kSqlColumnsTableFull =
    kCatalogFnsDataset + "." + kSqlColumnsTable;
std::string const kSQLColumnsTableSchema =
    "CREATE TABLE IF NOT EXISTS " + kSqlColumnsTableFull +
    " (StringField STRING(5000) DEFAULT 'TEST' NOT NULL,"
    " IntField INT64,"
    " BoolField BOOL,"
    " BytesField BYTES(5000),"
    " DateField DATE,"
    " DateTimeField DATETIME,"
    " IntervalField INTERVAL,"
    " TimeField TIME,"
    " TimestampField TIMESTAMP,"
    " DecimalField DECIMAL(10,2),"
    " BigDecimalField BIGDECIMAL(10,5)"
    ")";

std::string const kSqlColumnsEmptyDefaultTable =
    "ODBC_SQLColumns_TABLE_EMPTY_DEFAULT";
std::string const kSqlColumnsEmptyDefaultTableFull =
    kCatalogFnsDataset + "." + kSqlColumnsEmptyDefaultTable;
std::string const kSqlColumnsEmptyDefaultTableSchema =
    "CREATE TABLE IF NOT EXISTS " + kSqlColumnsEmptyDefaultTableFull +
    " (StringField STRING(5000) DEFAULT '' NOT NULL,"
    " IntField INT64"
    ")";

/// Test Helper functions for SQLColumns API

// Prints the SQLColumns results output. Used for debugging.
void PrintData(SQLColumnsResult const& result) {
  std::cout << "***************************************************"
            << std::endl;
  std::cout << "TABLE_CAT = " << result.project_name << std::endl;
  std::cout << "TABLE_SCHEMA = " << result.dataset_name << std::endl;
  std::cout << "TABLE_NAME = " << result.table_name << std::endl;
  std::cout << "COLUMN_NAME = " << result.column_name << std::endl;
  std::cout << "DATA_TYPE = " << result.data_type << std::endl;
  std::cout << "TYPE_NAME = " << result.col_type_name << std::endl;
  std::cout << "COLUMN_SIZE = " << result.col_size << std::endl;
  std::cout << "BUFFER_LENGTH = " << result.buffer_len << std::endl;
  std::cout << "DECIMAL_DIGITS = " << result.decimal_digits << std::endl;
  std::cout << "NUM_PREC_RADIX = " << result.radix << std::endl;
  std::cout << "NULLABLE = " << result.nullable << std::endl;
  std::cout << "REMARKS = " << result.description << std::endl;
  std::cout << "COLUMN_DEF = " << result.col_default << std::endl;
  std::cout << "SQL_DATA_TYPE = " << result.sql_data_type << std::endl;
  std::cout << "SQL_DATETIME_SUB = " << result.sql_date_time_sub << std::endl;
  std::cout << "CHAR_OCTET_LENGTH = " << result.char_octet_len << std::endl;
  std::cout << "ORDINAL_POSITION = " << result.ord_pos << std::endl;
  std::cout << "IS_NULLABLE = " << result.is_nullable << std::endl;

  std::cout << "***************************************************"
            << std::endl;
}

void VerifyColumnsResults(std::vector<SQLColumnsResult>& actual_results,
                          std::vector<SQLColumnsResult>& expected_results) {
  // Check if both result sets have the same number of rows
  ASSERT_EQ(actual_results.size(), expected_results.size())
      << "Number of results mismatch";

  // sort the results so they are in the same order in both
  // expected and actual.
  std::sort(actual_results.begin(), actual_results.end());
  std::sort(expected_results.begin(), expected_results.end());

  for (size_t j = 0; j < actual_results.size(); ++j) {
    ASSERT_EQ(actual_results[j].project_name, expected_results[j].project_name)
        << "Mismatch project name: Actual = " << actual_results[j].project_name
        << ", expected = " << expected_results[j].project_name;
    ASSERT_EQ(actual_results[j].dataset_name, expected_results[j].dataset_name)
        << "Mismatch dataset_name name: Actual = "
        << actual_results[j].dataset_name
        << ", expected = " << expected_results[j].dataset_name;
    ASSERT_EQ(actual_results[j].table_name, expected_results[j].table_name)
        << "Mismatch table name: Actual = " << actual_results[j].table_name
        << ", expected = " << expected_results[j].table_name;
    ASSERT_EQ(actual_results[j].column_name, expected_results[j].column_name)
        << "Mismatch column name: Actual = " << actual_results[j].column_name
        << ", expected = " << expected_results[j].column_name;
    ASSERT_EQ(actual_results[j].description, expected_results[j].description)
        << "Mismatch description: Actual = " << actual_results[j].description
        << ", expected = " << expected_results[j].description;
    ASSERT_EQ(actual_results[j].col_type_name,
              expected_results[j].col_type_name)
        << "Mismatch type name: Actual = " << actual_results[j].col_type_name
        << ", expected = " << expected_results[j].col_type_name;
    ASSERT_EQ(actual_results[j].col_default, expected_results[j].col_default)
        << "Mismatch col default: Actual = " << actual_results[j].col_default
        << ", expected = " << expected_results[j].col_default;
    ASSERT_EQ(actual_results[j].is_nullable, expected_results[j].is_nullable)
        << "Mismatch nullable: Actual = " << actual_results[j].is_nullable
        << ", expected = " << expected_results[j].is_nullable;

    ASSERT_EQ(actual_results[j].data_type, expected_results[j].data_type)
        << "Mismatch data type: Actual = " << actual_results[j].data_type
        << ", expected = " << expected_results[j].data_type;
    ASSERT_EQ(actual_results[j].sql_data_type,
              expected_results[j].sql_data_type)
        << "Mismatch sql_data_type: Actual = "
        << actual_results[j].sql_data_type
        << ", expected = " << expected_results[j].sql_data_type;
    ASSERT_EQ(actual_results[j].sql_date_time_sub,
              expected_results[j].sql_date_time_sub)
        << "Mismatch sql data time sub: Actual = "
        << actual_results[j].sql_date_time_sub
        << ", expected = " << expected_results[j].sql_date_time_sub;
    ASSERT_EQ(actual_results[j].decimal_digits,
              expected_results[j].decimal_digits)
        << "Mismatch decimal digits: Actual = "
        << actual_results[j].decimal_digits
        << ", expected = " << expected_results[j].decimal_digits;
    ASSERT_EQ(actual_results[j].radix, expected_results[j].radix)
        << "Mismatch radix: Actual = " << actual_results[j].radix
        << ", expected = " << expected_results[j].radix;
    ASSERT_EQ(actual_results[j].nullable, expected_results[j].nullable)
        << "Mismatch nullable: Actual = " << actual_results[j].nullable
        << ", expected = " << expected_results[j].nullable;

    ASSERT_EQ(actual_results[j].col_size, expected_results[j].col_size)
        << "Mismatch col size: Actual = " << actual_results[j].col_size
        << ", expected = " << expected_results[j].col_size;
    ASSERT_EQ(actual_results[j].buffer_len, expected_results[j].buffer_len)
        << "Mismatch buffer len: Actual = " << actual_results[j].buffer_len
        << ", expected = " << expected_results[j].buffer_len;
    ASSERT_EQ(actual_results[j].char_octet_len,
              expected_results[j].char_octet_len)
        << "Mismatch char octet len: Actual = "
        << actual_results[j].char_octet_len
        << ", expected = " << expected_results[j].char_octet_len;
    ASSERT_EQ(actual_results[j].ord_pos, expected_results[j].ord_pos)
        << "Mismatch ordinal position: Actual = " << actual_results[j].ord_pos
        << ", expected = " << expected_results[j].ord_pos;
  }
}

void TestSQLColumns(std::string const column,
                    std::vector<SQLColumnsResult>& expected_results,
                    bool use_identifier = false,
                    std::string const& schema = kSQLColumnsTableSchema,
                    std::string const& columns_table = kSqlColumnsTable) {
  auto conn = std::make_shared<ODBCHandles>();
  std::cout << "Creating table with schema : " << schema << std::endl;
  // Create table for SQLColumns.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, schema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Set statement attribute so the parameters are passed as literal values.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  if (use_identifier) {
    status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                            (SQLPOINTER)SQL_TRUE, 0);
  } else {
    status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                            (SQLPOINTER)SQL_FALSE, 0);
  }
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLColumnsResult> results =
      Catalog::GetColumns(conn, kCatalogName, kCatalogFnsDataset.c_str(),
                          columns_table.c_str(), column.c_str());
  VerifyColumnsResults(results, expected_results);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

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
    EXPECT_EQ(kCatalogName, result.project_name.value());
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name.value());
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kCatalogName, result.description.value());
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
    EXPECT_EQ(kCatalogName, result.project_name.value());
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name.value());
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kCatalogName, result.description.value());
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
    std::cout<<"result.project_name.value() "<<result.project_name.value()<<std::endl;
    project_found =
        project_found || (kCatalogName == result.project_name.value());
    EXPECT_FALSE(result.dataset_name.has_value());
    EXPECT_FALSE(result.table_name.has_value());
    EXPECT_FALSE(result.table_type.has_value());
    EXPECT_FALSE(result.description.has_value());
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
    EXPECT_FALSE(result.project_name.has_value());
    catalog_found =
        catalog_found || kCatalogFnsDataset == result.dataset_name.value();
    EXPECT_FALSE(result.table_name.has_value());
    EXPECT_FALSE(result.table_type.has_value());
    EXPECT_FALSE(result.description.has_value());
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
    EXPECT_FALSE(result.project_name.has_value());
    EXPECT_FALSE(result.dataset_name.has_value());
    EXPECT_FALSE(result.table_name.has_value());
    EXPECT_NE(std::find(expected_types.begin(), expected_types.end(),
                        result.table_type),
              expected_types.end());
    EXPECT_FALSE(result.description.has_value());
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
    EXPECT_THAT(result.project_name.value(), StartsWith(project_to_filter));
    EXPECT_THAT(result.dataset_name.value(), StartsWith(dataset_to_filter));
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kTable, result.table_type.value());
    EXPECT_EQ(result.project_name.value(), result.description.value());
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
    EXPECT_EQ(kCatalogName, result.project_name.value());
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name.value());
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    view_found = view_found || (view_name == result.table_name.value());
    EXPECT_TRUE(result.table_type.value() == kTable ||
                result.table_type.value() == kView)
        << "Actual type is " << result.table_type.value();
    EXPECT_EQ(result.project_name.value(), result.description.value());
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
    EXPECT_EQ(kCatalogName, result.project_name.value());
    EXPECT_EQ(kCatalogFnsDataset, result.dataset_name.value());
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kTable, result.table_type.value());
    EXPECT_EQ(result.project_name.value(), result.description.value());
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLPrimaryKeys_CreatePrimaryKeysTables) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create primary keys table via Simba Driver since execute is not
  // implemented
  // for BQ Drivers.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithPKSchema);
  CreateTableDirect(conn, kTableWithOutPKSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLForeignKeys_CreateForeignKeysTables) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create primary keys table via Simba Driver since execute is not
  // implemented
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

TEST(CatalogTest, CreateSQLColumnsTables) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create primary keys table via Simba Driver since execute is not
  // implemented
  // for BQ Drivers.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Create SQLColumns Table with default value for StringField.
  CreateTableDirect(conn, kSQLColumnsTableSchema);
  // Create SQLColumns Table with empty default value for StringField.
  CreateTableDirect(conn, kSqlColumnsEmptyDefaultTableSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLColumns_AllColumns_MetadataID_False) {
  std::vector<SQLColumnsResult> expected_results;
  // StringField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "StringField", "STRING", "STRING", "'TEST'", "NO",
       SQL_VARCHAR, SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA, 0,
       5000, 5000, 5000, 1});
  // IntField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "IntField", "INTEGER", "INT64", "", "YES", SQL_BIGINT,
       SQL_BIGINT, SQL_NULL_DATA, 0, 10, 1, 19, 20, SQL_NULL_DATA, 2});
  // BoolField.
  expected_results.push_back({"bigquery-devtools-drivers",
                              "ODBC_TEST_DATASET_CATALOG_FNS", kSqlColumnsTable,
                              "BoolField", "BOOLEAN", "BOOL", "", "YES",
                              SQL_BIT, SQL_BIT, SQL_NULL_DATA, SQL_NULL_DATA,
                              SQL_NULL_DATA, 1, 1, 1, SQL_NULL_DATA, 3});
  // BytesField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "BytesField", "BYTES", "BYTES", "", "YES",
       SQL_VARBINARY, SQL_VARBINARY, SQL_NULL_DATA, SQL_NULL_DATA,
       SQL_NULL_DATA, 1, 5000, 5000, 5000, 4});
  // DateField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "DateField", "DATE", "DATE", "", "YES", SQL_TYPE_DATE,
       SQL_DATETIME, SQL_CODE_DATE, SQL_NULL_DATA, SQL_NULL_DATA, 1, 10, 6,
       SQL_NULL_DATA, 5});
  // DateTimeField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "DateTimeField", "DATETIME", "DATETIME", "", "YES",
       SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 6, SQL_NULL_DATA,
       1, 26, 16, SQL_NULL_DATA, 6});
  // IntervalField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "IntervalField", "INTERVAL", "INTERVAL", "", "YES",
       SQL_VARCHAR, SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA, 1,
       16384, 16384, 16384, 7});
  // TimeField.
  expected_results.push_back({"bigquery-devtools-drivers",
                              "ODBC_TEST_DATASET_CATALOG_FNS", kSqlColumnsTable,
                              "TimeField", "TIME", "TIME", "", "YES",
                              SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME, 6,
                              SQL_NULL_DATA, 1, 15, 6, SQL_NULL_DATA, 8});
  // TimestampField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "TimestampField", "TIMESTAMP", "TIMESTAMP", "", "YES",
       SQL_TYPE_TIMESTAMP, SQL_DATETIME, SQL_CODE_TIMESTAMP, 6, SQL_NULL_DATA,
       1, 26, 16, SQL_NULL_DATA, 9});
  // Decimalield.
  expected_results.push_back({"bigquery-devtools-drivers",
                              "ODBC_TEST_DATASET_CATALOG_FNS", kSqlColumnsTable,
                              "DecimalField", "NUMERIC", "NUMERIC", "", "YES",
                              SQL_NUMERIC, SQL_NUMERIC, SQL_NULL_DATA, 2, 10, 1,
                              10, 12, SQL_NULL_DATA, 10});
  // BigDecimalField.
  expected_results.push_back({"bigquery-devtools-drivers",
                              "ODBC_TEST_DATASET_CATALOG_FNS", kSqlColumnsTable,
                              "BigDecimalField", "BIGNUMERIC", "BIGNUMERIC", "",
                              "YES", SQL_NUMERIC, SQL_NUMERIC, SQL_NULL_DATA, 5,
                              10, 1, 10, 12, SQL_NULL_DATA, 11});
  // Fetch all columns
  TestSQLColumns("%", expected_results);
}

TEST(CatalogTest, SQLColumns_StringColumn_MetadataID_True) {
  std::vector<SQLColumnsResult> expected_results;
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "StringField", "STRING", "STRING", "'TEST'", "NO",
       SQL_VARCHAR, SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA, 0,
       5000, 5000, 5000, 1});
  TestSQLColumns("StringField", expected_results, true);
}

TEST(CatalogTest, SQLColumns_StringColumn_SearchPattern_MetadataID_False) {
  std::vector<SQLColumnsResult> expected_results;
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsTable, "StringField", "STRING", "STRING", "'TEST'", "NO",
       SQL_VARCHAR, SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA, SQL_NULL_DATA, 0,
       5000, 5000, 5000, 1});
  TestSQLColumns("%StringField%", expected_results, false);
}

TEST(CatalogTest, SQLColumns_AllColumns_EmptyDefault) {
  std::vector<SQLColumnsResult> expected_results;
  // StringField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsEmptyDefaultTable, "StringField", "STRING", "STRING", "''",
       "NO", SQL_VARCHAR, SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA,
       SQL_NULL_DATA, 0, 5000, 5000, 5000, 1});
  // IntField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET_CATALOG_FNS",
       kSqlColumnsEmptyDefaultTable, "IntField", "INTEGER", "INT64", "", "YES",
       SQL_BIGINT, SQL_BIGINT, SQL_NULL_DATA, 0, 10, 1, 19, 20, SQL_NULL_DATA,
       2});
  // Fetch all columns
  TestSQLColumns("%", expected_results, false,
                 kSqlColumnsEmptyDefaultTableSchema,
                 kSqlColumnsEmptyDefaultTable);
}

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifdef BQ_DRIVER_INTEGRATION_TESTS

///////////////////////////////////////////////////////////////////////////////
// TODO(b/360988721):SQLPrimaryKeys is not implemented correctly by Simba
// Driver. Move thall SQLPrimaryKeys tests to common area once bug is fixed
// so the tests can be run for both Simba and BQ drivers.
///////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////
// TODO(b/360994080): SQLForeignKeys is not implemented
// correctly by Simba Driver. Move all SQLForeignKeys function
// to common area once the bug is fixed so the tests can be run
// for both BQ and Simba drivers.
/////////////////////////////////////////////////////////////////
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
  auto foreign_keys = Catalog::GetForeignKeys(
      conn, kCatalogFnsDataset, kTableCustomer, kTableOrders); /* both PK and
      FK
                                                table supplied*/
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
