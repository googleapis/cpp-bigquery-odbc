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

std::string const kTable = "TABLE";
std::string const kView = "VIEW";
std::string const kExternal = "EXTERNAL";
std::string const kMaterializedView =
    kIsBqDriver ? "MATERIALIZED VIEW" : "MATERIALIZED_VIEW";
std::string const kSnapshot = "SNAPSHOT";
std::string const kCatalog = "bigquery-devtools-drivers";
std::string const kSampleDataset = "RangeIntervalTestTable";
std::string const kNullString = "";
static std::vector<std::string> const kTableMetaDataSchema{{"TABLE_CAT"},
                                                           {"TABLE_SCHEM"},
                                                           {"TABLE_NAME"},
                                                           {"TABLE_TYPE"},
                                                           {"REMARKS"}};

static std::vector<std::string> const kColumnMetaDataSchema = {
    {"TABLE_CAT"},         {"TABLE_SCHEM"},      {"TABLE_NAME"},
    {"COLUMN_NAME"},       {"DATA_TYPE"},        {"TYPE_NAME"},
    {"COLUMN_SIZE"},       {"BUFFER_LENGTH"},    {"DECIMAL_DIGITS"},
    {"NUM_PREC_RADIX"},    {"NULLABLE"},         {"REMARKS"},
    {"COLUMN_DEF"},        {"SQL_DATA_TYPE"},    {"SQL_DATETIME_SUB"},
    {"CHAR_OCTET_LENGTH"}, {"ORDINAL_POSITION"}, {"IS_NULLABLE"}};

RowWiseResults const kCatalogPrimaryKeysExpected{
    {{1, "bigquery-devtools-drivers"},
     {2, "ODBC_TEST_DATASET"},
     {3, kCatalogDatasetTableWithPK},
     {4, "StringField"},
     {5, "1"},
     {6, kCatalogDatasetTableWithPK + ".pk$"}},
    {{1, "bigquery-devtools-drivers"},
     {2, "ODBC_TEST_DATASET"},
     {3, kCatalogDatasetTableWithPK},
     {4, "IntField"},
     {5, "2"},
     {6, kCatalogDatasetTableWithPK + ".pk$"}},
};

RowWiseResults const kCatalogForeignKeysExpected{
    {
        {1, "bigquery-devtools-drivers"},
        {2, "ODBC_TEST_DATASET"},
        {3, kTableCustomer},
        {4, "CustId"},
        {5, "bigquery-devtools-drivers"},
        {6, "ODBC_TEST_DATASET"},
        {7, kTableOrders},
        {8, "CustId"},
        {9, "1"},
        {10, "NULL"},
        {11, "NULL"},
        {12, kTableOrders + ".fk$1"},
        {13, kTableCustomer + ".pk$"},
        {14, "7"},
    },
};

// Table and Schema used to test SQLColumns API
std::string const kSqlColumnsTable = "ODBC_SQLColumns_TABLE_LATEST_2";
std::string const kSqlColumnsTableFull = kDatasetName + "." + kSqlColumnsTable;
std::string const kSQLColumnsTableSchema =
    "CREATE OR REPLACE TABLE " + kSqlColumnsTableFull +
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
    " BigDecimalField BIGDECIMAL(10,5),"
    " ArrayIntField ARRAY<INT64>"
    ")";

std::string const kSqlColumnsEmptyDefaultTable =
    "ODBC_SQLColumns_TABLE_EMPTY_DEFAULT";
std::string const kSqlColumnsEmptyDefaultTableFull =
    kDatasetName + "." + kSqlColumnsEmptyDefaultTable;
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
                    std::string const& columns_table = kSqlColumnsTable,
                    bool check_max_rows = false) {
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
  if (check_max_rows) {
    ASSERT_EQ(SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)1, 0),
              SQL_SUCCESS);
    std::vector<SQLColumnsResult> results =
        Catalog::GetColumns(conn, kCatalogName, kDatasetName.c_str(),
                            columns_table.c_str(), column.c_str());
    std::vector<SQLColumnsResult> single_expected{expected_results[0]};
    VerifyColumnsResults(results, single_expected);
  } else {
    std::vector<SQLColumnsResult> results =
        Catalog::GetColumns(conn, kCatalogName, kDatasetName.c_str(),
                            columns_table.c_str(), column.c_str());
    VerifyColumnsResults(results, expected_results);
  }
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

TEST(SQLColumns, Check_DefaultStringColumnSize) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string connection_string =
      kDefaultConnectionString + "; DefaultStringColumnLength=4;";
  EXPECT_EQ(Connect(connection_string, conn), SQL_SUCCESS);
  // Create SQLColumns Table with default value for StringField.
  std::string create_table_str =
      "CREATE OR REPLACE TABLE " + kDatasetName +
      ".AllDataTypes (int_col INT64, float_col FLOAT64, string_col STRING, "
      "bool_col BOOL, bytes_col BYTES, date_col DATE, datetime_col DATETIME, "
      "time_col TIME, timestamp_col TIMESTAMP, numeric_col NUMERIC, bignum_col "
      "BIGNUMERIC, interval_col INTERVAL, geography_col GEOGRAPHY,json_col "
      "JSON);";
  CreateTableDirect(conn, create_table_str);

  auto status = SQLColumns(conn->hstmt, (SQLCHAR*)kCatalog.c_str(), SQL_NTS,
                           (SQLCHAR*)kDatasetName.c_str(), SQL_NTS,
                           (SQLCHAR*)"AllDataTypes", SQL_NTS,
                           (SQLCHAR*)"string_col", SQL_NTS);
  EXPECT_EQ(status, SQL_SUCCESS);

  SQLRETURN ret;
  int row_num = 1;
  while ((ret = SQLFetch(conn->hstmt)) == SQL_SUCCESS) {
    SQLCHAR buffer[1024] = {0};
    SQLLEN indicator = 0;

    SQLGetData(conn->hstmt, 7, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);

    EXPECT_STREQ((char*)buffer, "4");
  }

  EXPECT_TRUE(ret == SQL_NO_DATA || ret == SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {
      kTableNamePrefix + "ODBC_SQLTables1_TEST_1",
      kTableNamePrefix + "ODBC_SQLTables1_TEST_2",
      kTableNamePrefix + "ODBC_SQLTables1_TEST_3"};
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, kCatalogName, kDatasetName.c_str());
  int count_tables = 0;
  for (auto const& result : results) {
    EXPECT_EQ(kCatalogName, result.project_name.value());
    EXPECT_EQ(kDatasetName, result.dataset_name.value());
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kCatalogName, result.description.value());
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Drop(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTablesA) {
  auto conn = std::make_shared<ODBCHandles>();

  // Create tables
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {
      kTableNamePrefix + "ODBC_SQLTablesAnsi_TEST_1",
      kTableNamePrefix + "ODBC_SQLTablesAnsi_TEST_2",
      kTableNamePrefix + "ODBC_SQLTableAnsi_TEST_3"};
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results = Catalog::GetTables(
      conn, kCatalogName, kDatasetName.c_str(), nullptr, nullptr, true);
  int count_tables = 0;
  for (auto const& result : results) {
    EXPECT_EQ(kCatalogName, result.project_name.value());
    EXPECT_EQ(kDatasetName, result.dataset_name.value());
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kCatalogName, result.description.value());
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Drop(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_AllProjects) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string additional_project = "publicdata";
  std::string connection_string =
      kDefaultConnectionString + ";AdditionalProjects=" + additional_project;
  EXPECT_EQ(Connect(connection_string, conn), SQL_SUCCESS);

  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, SQL_ALL_CATALOGS, "", "");

  std::set<std::string> catalogs;
  for (auto const& result : results) {
    if (result.project_name.has_value()) {
      catalogs.insert(result.project_name.value());
    }

    EXPECT_FALSE(result.dataset_name.has_value());
    EXPECT_FALSE(result.table_name.has_value());
    EXPECT_FALSE(result.table_type.has_value());
    EXPECT_FALSE(result.description.has_value());
  }

  EXPECT_TRUE(catalogs.find(kCatalogName) != catalogs.end())
      << "Default project/catalog not found in results.";
  EXPECT_TRUE(catalogs.find(additional_project) != catalogs.end())
      << "Additional project/catalog not found in results.";

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
        catalog_found || kDatasetName == result.dataset_name.value();
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
      kTableNamePrefix + "ODBC_SQLTables_SQLTables_WithFiltering_1",
      kTableNamePrefix + "ODBC_SQLTables_SQLTables_WithFiltering_2"};
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::string project_to_filter = kCatalogName.substr(0, 8);
  std::string dataset_to_filter = kDatasetName.substr(0, 8);

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
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Drop(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, Tables_Clones_Views) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string base_table = "ODBC_SQLTables_TablesAndClones_base";
  Table table(kDatasetWithTablePrefix + base_table);
  table.CreateWithPrepare(conn, "(StringField STRING)");

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string insert_stmt =
      "INSERT INTO `" + kDatasetWithTablePrefix + base_table +
      "` (StringField) VALUES ('TestValue1'), ('TestValue2')";
  SQLRETURN insert_ret = ExecWithPrepare(conn, insert_stmt);
  ASSERT_EQ(insert_ret, SQL_SUCCESS) << "Insert into base table failed";

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string clone_table = base_table + "_clone";
  std::string clone_stmt = "CREATE OR REPLACE TABLE `" +
                           kDatasetWithTablePrefix + clone_table + "` CLONE `" +
                           kDatasetWithTablePrefix + base_table + "`";
  SQLRETURN clone_ret = ExecWithPrepare(conn, clone_stmt);
  ASSERT_EQ(clone_ret, SQL_SUCCESS) << "Failed to create clone table";

  std::string select_query =
      "SELECT * FROM `" + kDatasetWithTablePrefix + clone_table + "`";
  auto ret = ExecWithPrepare(conn, select_query);
  EXPECT_TRUE(SQL_SUCCEEDED(ret));

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string view_name = base_table + "_view";
  std::string view_creation = "CREATE OR REPLACE VIEW `" +
                              kDatasetWithTablePrefix + view_name +
                              "` AS (SELECT StringField FROM `" +
                              kDatasetWithTablePrefix + base_table + "`)";
  CreateTableDirect(conn, view_creation);

  std::string view_select =
      "SELECT * FROM `" + kDatasetWithTablePrefix + view_name + "`";
  SQLRETURN view_sel_ret = ExecWithPrepare(conn, view_select);
  EXPECT_TRUE(SQL_SUCCEEDED(view_sel_ret));

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  ExecWithPrepare(conn, "DROP VIEW IF EXISTS `" + kDatasetWithTablePrefix +
                            view_name + "`");

  ExecWithPrepare(conn, "DROP TABLE IF EXISTS `" + kDatasetWithTablePrefix +
                            clone_table + "`");
  ExecWithPrepare(conn, "DROP TABLE IF EXISTS `" + kDatasetWithTablePrefix +
                            base_table + "`");

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_MetadataId_True) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> table_names = {
      kTableNamePrefix + "ODBC_SQLTables_SQLTables_MetadataId_True_1",
      kTableNamePrefix + "odbc_sqltables_sqltables_metadataid_true_1"};
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Create(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Verify if the tables returned by SQLTables are the same as the ones
  // created
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                               (SQLPOINTER)SQL_TRUE, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_METADATA_ID)", conn);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)2, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_MAX_ROWS)", conn);
  std::vector<SQLTableResult> results =
      Catalog::GetTables(conn, kCatalogName, kDatasetName.c_str(),
                         (table_names[0] + "   ").c_str(), nullptr, false, 2);

  int count_tables = 0;
  for (auto const& result : results) {
    EXPECT_EQ(kCatalogName, result.project_name.value());
    EXPECT_EQ(kDatasetName, result.dataset_name.value());
    if (FindTableInVector(result.table_name.value(), table_names)) {
      count_tables++;
    }
    EXPECT_EQ(kTable, result.table_type.value());
    EXPECT_EQ(result.project_name.value(), result.description.value());
  }
  EXPECT_EQ(table_names.size(), count_tables) << "Not all tables were found";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  for (auto const& name : table_names) {
    Table(kDatasetName + "." + name).Drop(conn);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLPrimaryKeys_CreatePrimaryKeysTables) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create primary keys table via existing Driver since execute is not
  // implemented
  // for BQ Drivers.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithPKSchema);
  CreateTableDirect(conn, kTableWithOutPKSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLForeignKeys_CreateForeignKeysTables) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create primary keys table via existing Driver since execute is not
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
  // Create primary keys table via existing Driver since execute is not
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
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "StringField", "STRING",
                              "STRING", "'TEST'", "NO", SQL_VARCHAR,
                              SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              10
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              0, 5000, 5000, 5000, 1});
  // IntField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "IntField", "INTEGER", "INT64",
                              "", "YES", SQL_BIGINT, SQL_BIGINT, SQL_NULL_DATA,
                              0, 10, 1, 19, 20, SQL_NULL_DATA, 2});
  // BoolField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "BoolField", "BOOLEAN", "BOOL",
                              "", "YES", SQL_BIT, SQL_BIT, SQL_NULL_DATA,
                              SQL_NULL_DATA, SQL_NULL_DATA, 1, 1, 1,

                              SQL_NULL_DATA, 3});
  // BytesField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "BytesField", "BYTES", "BYTES",
                              "", "YES", SQL_VARBINARY, SQL_VARBINARY,
                              SQL_NULL_DATA, SQL_NULL_DATA,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              10
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              1, 5000, 5000, 5000, 4});
  // DateField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "DateField", "DATE", "DATE", "",
                              "YES", SQL_TYPE_DATE, SQL_DATETIME, SQL_CODE_DATE,
                              SQL_NULL_DATA, SQL_NULL_DATA, 1, 10, 6,
                              SQL_NULL_DATA, 5});
  // DateTimeField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "DateTimeField", "DATETIME",
                              "DATETIME", "", "YES", SQL_TYPE_TIMESTAMP,
                              SQL_DATETIME, SQL_CODE_TIMESTAMP, 6,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              2
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              1, 26, 16, SQL_NULL_DATA, 6});
  // IntervalField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "IntervalField", "INTERVAL",
                              "INTERVAL", "", "YES", SQL_VARCHAR, SQL_VARCHAR,
                              SQL_NULL_DATA, SQL_NULL_DATA,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              10
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              1, 16384, 16384, 16384, 7});
  // TimeField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "TimeField", "TIME", "TIME", "",
                              "YES", SQL_TYPE_TIME, SQL_DATETIME, SQL_CODE_TIME,
                              6, SQL_NULL_DATA, 1, 15, 6, SQL_NULL_DATA, 8});
  // TimestampField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "TimestampField", "TIMESTAMP",
                              "TIMESTAMP", "", "YES", SQL_TYPE_TIMESTAMP,
                              SQL_DATETIME, SQL_CODE_TIMESTAMP, 6,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              2
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              1, 26, 16, SQL_NULL_DATA, 9});
  // Decimalield.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET", kSqlColumnsTable,
       "DecimalField", "NUMERIC", "NUMERIC", "", "YES", SQL_NUMERIC,
       SQL_NUMERIC, SQL_NULL_DATA, 2, 10, 1, 10, 12, SQL_NULL_DATA, 10});
  // BigDecimalField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET", kSqlColumnsTable,
       "BigDecimalField", "BIGNUMERIC", "BIGNUMERIC", "", "YES", SQL_NUMERIC,
       SQL_NUMERIC, SQL_NULL_DATA, 5, 10, 1, 10, 12, SQL_NULL_DATA, 11});

  // ArrayIntField.
  expected_results.push_back(
      {"bigquery-devtools-drivers", "ODBC_TEST_DATASET", kSqlColumnsTable,
       "ArrayIntField", "INTEGER", "ARRAY", "", "NO", SQL_VARCHAR, SQL_VARCHAR,
       SQL_NULL_DATA, -1, -1, 0, 16384, 16384, 16384, 12});
  // Fetch all columns
  TestSQLColumns("%", expected_results);

  // TEST SQL_MAX_ROWS
  TestSQLColumns("%", expected_results, false, kSQLColumnsTableSchema,
                 kSqlColumnsTable, true);
}

TEST(CatalogTest, SQLColumns_StringColumn_MetadataID_True) {
  std::vector<SQLColumnsResult> expected_results;
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "StringField", "STRING",
                              "STRING", "'TEST'", "NO", SQL_VARCHAR,
                              SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              10
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              0, 5000, 5000, 5000, 1});
  TestSQLColumns("StringField", expected_results, true);
}

TEST(CatalogTest, SQLColumns_StringColumn_SearchPattern_MetadataID_False) {
  std::vector<SQLColumnsResult> expected_results;
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsTable, "StringField", "STRING",
                              "STRING", "'TEST'", "NO", SQL_VARCHAR,
                              SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              10
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              0, 5000, 5000, 5000, 1});
  TestSQLColumns("%StringField%", expected_results, false);
}

TEST(CatalogTest, SQLColumns_AllColumns_EmptyDefault) {
  std::vector<SQLColumnsResult> expected_results;
  // StringField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsEmptyDefaultTable, "StringField",
                              "STRING", "STRING", "''", "NO", SQL_VARCHAR,
                              SQL_VARCHAR, SQL_NULL_DATA, SQL_NULL_DATA,
// Our driver is consistent with the column metadata returned by SQLColumns and
// SQLProcedureColumns. The existing driver isn't.
#ifdef BQ_DRIVER_INTEGRATION_TESTS
                              10
#else
                              SQL_NULL_DATA
#endif  // BQ_DRIVER_INTEGRATION_TESTS
                              ,
                              0, 5000, 5000, 5000, 1});
  // IntField.
  expected_results.push_back({"bigquery-devtools-drivers", "ODBC_TEST_DATASET",
                              kSqlColumnsEmptyDefaultTable, "IntField",
                              "INTEGER", "INT64", "", "YES", SQL_BIGINT,
                              SQL_BIGINT, SQL_NULL_DATA, 0, 10, 1, 19, 20,
                              SQL_NULL_DATA, 2});

  // Fetch all columns
  auto conn = std::make_shared<ODBCHandles>();
  std::cout << "Creating table with schema : "
            << kSqlColumnsEmptyDefaultTableSchema << std::endl;
  // Create table for SQLColumns.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kSqlColumnsEmptyDefaultTableSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Set statement attribute so the parameters are passed as literal values.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  // We are deliberately using an empty catalog name here to test the behaviour
  // of assigning a default catalog value(b/399756489)
  std::vector<SQLColumnsResult> results =
      Catalog::GetColumns(conn, "", kDatasetName.c_str(),
                          kSqlColumnsEmptyDefaultTable.c_str(), "%");
  VerifyColumnsResults(results, expected_results);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifdef BQ_DRIVER_INTEGRATION_TESTS

///////////////////////////////////////////////////////////////////////////////
// TODO(b/360988721):SQLPrimaryKeys is not implemented correctly by existing
// Driver. Move thall SQLPrimaryKeys tests to common area once bug is fixed
// so the tests can be run for both existing and BQ drivers.
///////////////////////////////////////////////////////////////////////////////
TEST(CatalogTest, SQLPrimaryKeys_TableWithPrimaryKeys) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create table if not exists.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithPKSchema);

  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.

  // Set max rows to 1
  ASSERT_EQ(SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)1, 0),
            SQL_SUCCESS);

  RowWiseResults primary_keys =
      Catalog::GetPrimaryKeys(conn, kDatasetName, kCatalogDatasetTableWithPK);
  EXPECT_EQ(primary_keys.size(), 1);
  RowWiseResults single_row_expected{kCatalogPrimaryKeysExpected[0]};
  VerifyRowWiseResults(primary_keys, single_row_expected);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(CatalogTest, Benchmark_GetPrimaryKeys_ExactTable) {
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

TEST(CatalogTest, Benchmark_GetPrimaryKeys_NoPKTable) {
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
TEST(CatalogTest, SQLPrimaryKeys_TableWithoutPrimaryKeys) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create table if not exists.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithOutPKSchema);
  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto primary_keys = Catalog::GetPrimaryKeys(conn, kDatasetName,
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
  auto primary_keys = Catalog::GetPrimaryKeys(conn, kDatasetName,
                                              kCatalogDatasetTableWithPK, true);
  VerifyRowWiseResults(primary_keys, kCatalogPrimaryKeysExpected);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(CatalogTest, Benchmark_GetForeignKeys_PkAndFkTables) {
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

TEST(CatalogTest, Benchmark_GetForeignKeys_PkTableOnly) {
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

TEST(CatalogTest, Benchmark_GetForeignKeys_FkTableOnly) {
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
TEST(CatalogTest, ANSI_SQLPrimaryKeys_TableWithoutPrimaryKeys) {
  auto conn = std::make_shared<ODBCHandles>();
  // Create table if not exists.
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CreateTableDirect(conn, kTableWithOutPKSchema, true);
  // Use existing dataset and table created with primary keys.
  // We are not creating and dropping tables. This existing
  // table resource can be reused for other catalog functions as well.
  auto primary_keys = Catalog::GetPrimaryKeys(
      conn, kDatasetName, kCatalogDatasetTableWithoutPK, true);
  EXPECT_TRUE(primary_keys.empty());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

////////////////////////////////////////////////////////////////
// TODO(b/360994080): SQLForeignKeys is not implemented
// correctly by existing Driver. Move all SQLForeignKeys function
// to common area once the bug is fixed so the tests can be run
// for both BQ and existing drivers.
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

  // Set max rows to 1
  ASSERT_EQ(SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)1, 0),
            SQL_SUCCESS);

  auto foreign_keys = Catalog::GetForeignKeys(
      conn, kDatasetName, kTableCustomer, kTableOrders); /* both PK and
      FK
                                                table supplied*/
  EXPECT_EQ(foreign_keys.size(), 1);
  RowWiseResults single_row_expected{kCatalogForeignKeysExpected[0]};
  VerifyRowWiseResults(foreign_keys, single_row_expected);
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
      conn, kDatasetName, kTableCustomer); /* empty FK table */

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
      conn, kDatasetName, "" /*empty PK Table*/, kTableOrders);

  VerifyRowWiseResults(foreign_keys, kCatalogForeignKeysExpected);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

struct ExpectedProcedureColumnValues {
  std::string procedure_catalog;
  std::string procedure_schema;
  std::string procedure_name;
  std::string column_name;
  SQLSMALLINT column_type;
  SQLSMALLINT data_type;
  std::string type_name;
  SQLINTEGER column_size;
  SQLINTEGER buffer_length;
  SQLSMALLINT decimal_digits;
  SQLSMALLINT num_pred_radix;
  SQLSMALLINT nullable;
  std::string remarks;
  std::string column_default;
  SQLSMALLINT sql_data_type;
  SQLSMALLINT datetime_sub;
  SQLINTEGER char_octet_length;
  SQLSMALLINT ordinal_position;
  std::string is_nullable;
};

ExpectedProcedureColumnValues CreateExpectedInt64Param(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param1",
          SQL_PARAM_INPUT,
          SQL_BIGINT,
          "INT64",
          19,
          20,
          0,
          10,
          SQL_NULLABLE,
          "",
          "",
          SQL_BIGINT,
          0,
          0,
          1,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedStringParam(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param2",
          SQL_PARAM_INPUT,
          SQL_VARCHAR,
          "STRING",
          16384,
          16384,
          0,
          10,
          SQL_NULLABLE,
          "",
          "",
          SQL_VARCHAR,
          0,
          16384,
          2,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedFloat64Param(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param3",
          SQL_PARAM_INPUT,
          SQL_DOUBLE,
          "FLOAT64",
          53,
          8,
          9,
          2,
          SQL_NULLABLE,
          "",
          "",
          SQL_DOUBLE,
          0,
          16384,
          3,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedBoolParam(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param4",
          SQL_PARAM_INPUT,
          SQL_BIT,
          "BOOL",
          1,
          1,
          9,
          2,
          SQL_NULLABLE,
          "",
          "",
          SQL_BIT,
          0,
          16384,
          4,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedTimestampParam(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param5",
          SQL_PARAM_INPUT,
          SQL_TYPE_TIMESTAMP,
          "TIMESTAMP",
          26,
          16,
          6,
          2,
          SQL_NULLABLE,
          "",
          "",
          9,
          3,
          16384,
          5,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedOutputInt64Param(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param6",
          SQL_PARAM_OUTPUT,
          SQL_BIGINT,
          "INT64",
          19,
          20,
          0,
          10,
          SQL_NULLABLE,
          "",
          "",
          SQL_BIGINT,
          3,
          16384,
          6,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedOutputStringParam(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param7",
          SQL_PARAM_OUTPUT,
          SQL_VARCHAR,
          "STRING",
          16384,
          16384,
          0,
          10,
          SQL_NULLABLE,
          "",
          "",
          SQL_VARCHAR,
          3,
          16384,
          7,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedInOutInt64Param(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param1",
          SQL_PARAM_INPUT_OUTPUT,
          SQL_BIGINT,
          "INT64",
          19,
          20,
          0,
          10,
          SQL_NULLABLE,
          "",
          "",
          SQL_BIGINT,
          0,
          0,
          1,
          "YES"};
}

ExpectedProcedureColumnValues CreateExpectedInOutStringParam(
    std::string const& procedure_pattern) {
  return {"bigquery-devtools-drivers",
          kDatasetName,
          procedure_pattern,
          "param2",
          SQL_PARAM_INPUT_OUTPUT,
          SQL_VARCHAR,
          "STRING",
          16384,
          16384,
          0,
          10,
          SQL_NULLABLE,
          "",
          "",
          SQL_VARCHAR,
          0,
          16384,
          2,
          "YES"};
}

void ValidateProcedureColumns(
    SQLHSTMT h_stmt,
    std::vector<ExpectedProcedureColumnValues> const& expected_columns,
    bool check_max_rows = false) {
  SQLCHAR procedure_catalog[128] = {0}, procedure_schema[128] = {0},
          procedure_name[128] = {0};
  SQLCHAR column_name[128] = {0}, type_name[128] = {0}, remarks[256] = {0},
          column_default[128] = {0};
  SQLCHAR is_nullable[10] = {0};
  SQLSMALLINT column_type = 0, data_type = 0, decimal_digits = 0,
              num_prec_radix = 0;
  SQLSMALLINT nullable = 0, sql_data_type = 0, datetime_sub = 0,
              ordinal_position = 0;
  SQLINTEGER column_size = 0, buffer_length = 0, char_octet_length = 0;
  SQLLEN ind = 0;
  int row_count = 0;
  for (auto const& expected : expected_columns) {
    SQLRETURN ret = SQLFetch(h_stmt);
    if (ret == SQL_NO_DATA) {
      FAIL() << "SQLProcedureColumns returned fewer rows than expected.";
    }
    row_count++;
    ASSERT_TRUE(ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);

    EXPECT_EQ(SQLGetData(h_stmt, 1, SQL_C_CHAR, procedure_catalog,
                         sizeof(procedure_catalog), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(procedure_catalog),
                 expected.procedure_catalog.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 2, SQL_C_CHAR, procedure_schema,
                         sizeof(procedure_schema), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(procedure_schema),
                 expected.procedure_schema.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 3, SQL_C_CHAR, procedure_name,
                         sizeof(procedure_name), &ind),
              SQL_SUCCESS);

    EXPECT_STREQ(reinterpret_cast<char*>(procedure_name),
                 expected.procedure_name.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 4, SQL_C_CHAR, column_name,
                         sizeof(column_name), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(column_name),
                 expected.column_name.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 5, SQL_C_SSHORT, &column_type, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(column_type, expected.column_type);

    EXPECT_EQ(SQLGetData(h_stmt, 6, SQL_C_SSHORT, &data_type, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(
        SQLGetData(h_stmt, 7, SQL_C_CHAR, type_name, sizeof(type_name), &ind),
        SQL_SUCCESS);
    EXPECT_EQ(data_type, expected.data_type);

    EXPECT_EQ(SQLGetData(h_stmt, 8, SQL_C_SSHORT, &column_size, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(column_size, expected.column_size);
    EXPECT_EQ(SQLGetData(h_stmt, 9, SQL_C_SSHORT, &buffer_length, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(buffer_length, expected.buffer_length);
    EXPECT_EQ(SQLGetData(h_stmt, 10, SQL_C_SSHORT, &decimal_digits, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(decimal_digits, expected.decimal_digits);
    EXPECT_EQ(SQLGetData(h_stmt, 11, SQL_C_SSHORT, &num_prec_radix, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(num_prec_radix, expected.num_pred_radix);
    EXPECT_EQ(SQLGetData(h_stmt, 12, SQL_C_SSHORT, &nullable, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(nullable, expected.nullable);
    EXPECT_EQ(
        SQLGetData(h_stmt, 13, SQL_C_CHAR, remarks, sizeof(remarks), &ind),
        SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(remarks), expected.remarks.c_str());
    EXPECT_EQ(SQLGetData(h_stmt, 14, SQL_C_CHAR, column_default,
                         sizeof(column_default), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(column_default),
                 expected.column_default.c_str());
    EXPECT_EQ(SQLGetData(h_stmt, 15, SQL_C_SSHORT, &sql_data_type, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(sql_data_type, expected.sql_data_type);
    EXPECT_EQ(SQLGetData(h_stmt, 16, SQL_C_SSHORT, &datetime_sub, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(datetime_sub, expected.datetime_sub);
    EXPECT_EQ(
        SQLGetData(h_stmt, 17, SQL_C_SSHORT, &char_octet_length, 10, &ind),
        SQL_SUCCESS);
    EXPECT_EQ(char_octet_length, expected.char_octet_length);
    EXPECT_EQ(SQLGetData(h_stmt, 18, SQL_C_SSHORT, &ordinal_position, 10, &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(type_name),
                 expected.type_name.c_str());

    EXPECT_EQ(ordinal_position, expected.ordinal_position);

    EXPECT_EQ(SQLGetData(h_stmt, 19, SQL_C_CHAR, is_nullable,
                         sizeof(is_nullable), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(is_nullable),
                 expected.is_nullable.c_str());
  }
  if (check_max_rows) {
    EXPECT_EQ(row_count, 1);
  }
  EXPECT_EQ(SQLFetch(h_stmt), SQL_NO_DATA)
      << "SQLProcedureColumns returned more rows than expected.";
}

void TestSQLProcedureColumns(std::string const& catalog_name,
                             std::string const& procedure_suffix) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string procedure_name =
      kDatasetWithTablePrefix + "TEST_PROCEDURE_" + procedure_suffix;

  // Create Procedure
  std::string create_procedure_query = "CREATE OR REPLACE PROCEDURE " +
                                       procedure_name +
                                       "(param1 INT64, param2 STRING) "
                                       "BEGIN "
                                       "END;";
  SQLRETURN ret = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)create_procedure_query.c_str(), SQL_NTS);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Reconnect for SQLProcedureColumns
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string procedure_pattern =
      kTableNamePrefix + "TEST_PROCEDURE_" + procedure_suffix;

  ret = SQLProcedureColumns(
      conn->hstmt, catalog_name.empty() ? NULL : (SQLCHAR*)catalog_name.c_str(),
      catalog_name.empty() ? 0 : SQL_NTS, (SQLCHAR*)kDatasetName.c_str(),
      SQL_NTS,
      (SQLCHAR*)procedure_pattern.c_str(),  // Wildcard match
      SQL_NTS, NULL, 0);

  EXPECT_EQ(ret, SQL_SUCCESS);

  // Validate results allowing for any prefix
  std::vector<ExpectedProcedureColumnValues> expected_columns = {
      CreateExpectedInt64Param(procedure_pattern),
      CreateExpectedStringParam(procedure_pattern),
  };

  // Validate procedure columns
  ValidateProcedureColumns(conn->hstmt, expected_columns);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Cleanup
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  Procedure procedure(procedure_name);
  procedure.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLProcedureColumns, BasicProcedure) {
  TestSQLProcedureColumns("", "BASIC");
}

TEST(SQLProcedureColumns, ProcedureWithCatalog) {
  TestSQLProcedureColumns("bigquery-devtools-drivers", "CATALOG");
}

TEST(SQLProcedureColumns, ComplexProcedure) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string procedure_name =
      kDatasetWithTablePrefix + "TEST_COMPLEX_PROCEDURE";

  std::string create_procedure_query = "CREATE OR REPLACE PROCEDURE " +
                                       procedure_name +
                                       "("
                                       "param1 INT64, "
                                       "param2 STRING, "
                                       "param3 FLOAT64, "
                                       "param4 BOOL, "
                                       "param5 TIMESTAMP, "
                                       "OUT param6 INT64, "
                                       "OUT param7 STRING) "
                                       "BEGIN "
                                       "SET param6 = param1; "
                                       "SET param7 = param2; "
                                       "END;";

  SQLRETURN ret = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)create_procedure_query.c_str(), SQL_NTS);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  std::string procedure_pattern = kTableNamePrefix + "TEST_COMPLEX_PROCEDURE";

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ret = SQLProcedureColumns(
      conn->hstmt, NULL, 0, (SQLCHAR*)kDatasetName.c_str(), SQL_NTS,
      (SQLCHAR*)procedure_pattern.c_str(), SQL_NTS, NULL, 0);
  EXPECT_EQ(ret, SQL_SUCCESS);

  std::vector<ExpectedProcedureColumnValues> expected_values = {
      CreateExpectedInt64Param(procedure_pattern),
      CreateExpectedStringParam(procedure_pattern),
      CreateExpectedFloat64Param(procedure_pattern),
      CreateExpectedBoolParam(procedure_pattern),
      CreateExpectedTimestampParam(procedure_pattern),
      CreateExpectedOutputInt64Param(procedure_pattern),
      CreateExpectedOutputStringParam(procedure_pattern),
  };

  ValidateProcedureColumns(conn->hstmt, expected_values);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Cleanup
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  Procedure procedure(procedure_name);
  procedure.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLProcedureColumns, NonExistentProcedure) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLRETURN ret = SQLProcedureColumns(
      conn->hstmt, NULL, 0, (SQLCHAR*)kDatasetName.c_str(), SQL_NTS,
      (SQLCHAR*)"NON_EXISTENT_PROCEDURE", SQL_NTS, NULL, 0);
  EXPECT_EQ(ret, SQL_SUCCESS);

  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA)
      << "Expected no data for non-existent procedure";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLProcedureColumns, ProcedureWithNoParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string procedure_name = kDatasetWithTablePrefix + "TEST_NO_PARAMS";
  std::string create_procedure_query =
      "CREATE OR REPLACE PROCEDURE " + procedure_name + "() BEGIN END;";

  SQLRETURN ret = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)create_procedure_query.c_str(), SQL_NTS);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string procedure_pattern = kTableNamePrefix + "TEST_NO_PARAMS";
  ret = SQLProcedureColumns(
      conn->hstmt, NULL, 0, (SQLCHAR*)kDatasetName.c_str(), SQL_NTS,
      (SQLCHAR*)procedure_pattern.c_str(), SQL_NTS, NULL, 0);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA)
      << "Expected no parameters for procedure with no parameters";
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Cleanup
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  Procedure procedure(procedure_name);
  procedure.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLProcedureColumns, ProcedureWithInOutParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string procedure_name = kDatasetWithTablePrefix + "TEST_INOUT_PARAMS";
  std::string create_procedure_query =
      "CREATE OR REPLACE PROCEDURE " + procedure_name +
      "("
      "INOUT param1 INT64, "
      "INOUT param2 STRING) "
      "BEGIN "
      "SET param1 = param1 + 1; "
      "SET param2 = CONCAT(param2, '_updated'); "
      "END;";

  SQLRETURN ret = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)create_procedure_query.c_str(), SQL_NTS);
  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string procedure_pattern = kTableNamePrefix + "TEST_INOUT_PARAMS";

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ret = SQLProcedureColumns(
      conn->hstmt, NULL, 0, (SQLCHAR*)kDatasetName.c_str(), SQL_NTS,
      (SQLCHAR*)procedure_pattern.c_str(), SQL_NTS, NULL, 0);
  EXPECT_EQ(ret, SQL_SUCCESS);

  // Expected results
  std::vector<ExpectedProcedureColumnValues> expected_values = {
      CreateExpectedInOutInt64Param(procedure_pattern),
      CreateExpectedInOutStringParam(procedure_pattern)};
  // Call validation function
  ValidateProcedureColumns(conn->hstmt, expected_values);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  ASSERT_EQ(SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)1, 0),
            SQL_SUCCESS);

  ret = SQLProcedureColumns(
      conn->hstmt, NULL, 0, (SQLCHAR*)kDatasetName.c_str(), SQL_NTS,
      (SQLCHAR*)procedure_pattern.c_str(), SQL_NTS, NULL, 0);
  EXPECT_EQ(ret, SQL_SUCCESS);

  std::vector<ExpectedProcedureColumnValues> expected_values2 = {
      CreateExpectedInOutInt64Param(procedure_pattern)};
  // Call validation function
  ValidateProcedureColumns(conn->hstmt, expected_values2, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Cleanup
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  Procedure procedure(procedure_name);
  procedure.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

struct ExpectedProcedureValues {
  std::string procedure_catalog;
  std::string procedure_schema;
  std::string procedure_name;
  SQLSMALLINT num_input_params;
  SQLSMALLINT num_output_params;
  SQLSMALLINT num_result_sets;
  std::string remarks;
  SQLSMALLINT procedure_type;
};
ExpectedProcedureValues const kExpectedProcedure = {
    kCatalog, kDatasetName, "", 5, 2, -1, "SQL", SQL_PT_PROCEDURE};

ExpectedProcedureValues const kExpectedTableRoutine = {
    kCatalog, kDatasetName, "", 1, 0, -1, "SQL", SQL_PT_UNKNOWN};

ExpectedProcedureValues const kExpectedFunction = {
    kCatalog, kDatasetName, "", 5, 0, -1, "SQL", SQL_PT_FUNCTION};

void ValidateSQLProcedures(
    SQLHSTMT h_stmt,
    std::vector<ExpectedProcedureValues> const& expected_procedures) {
  SQLCHAR procedure_catalog[128], procedure_schema[128], procedure_name[128],
      remarks[256];
  SQLSMALLINT num_input_params, num_output_params, num_result_sets,
      procedure_type;
  SQLLEN ind = 0;
  int row_count = 0;

  for (auto const& expected : expected_procedures) {
    SQLRETURN ret = SQLFetch(h_stmt);
    if (ret == SQL_NO_DATA) {
      FAIL() << "SQLProcedures returned fewer rows than expected.";
    }
    ASSERT_TRUE(ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
    row_count++;

    EXPECT_EQ(SQLGetData(h_stmt, 1, SQL_C_CHAR, procedure_catalog,
                         sizeof(procedure_catalog), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(procedure_catalog),
                 expected.procedure_catalog.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 2, SQL_C_CHAR, procedure_schema,
                         sizeof(procedure_schema), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(procedure_schema),
                 expected.procedure_schema.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 3, SQL_C_CHAR, procedure_name,
                         sizeof(procedure_name), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(procedure_name),
                 expected.procedure_name.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 4, SQL_C_SSHORT, &num_input_params, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(num_input_params, expected.num_input_params);

    EXPECT_EQ(SQLGetData(h_stmt, 5, SQL_C_SSHORT, &num_output_params, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(num_output_params, expected.num_output_params);

    EXPECT_EQ(SQLGetData(h_stmt, 6, SQL_C_SSHORT, &num_result_sets, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(num_result_sets, expected.num_result_sets);

    EXPECT_EQ(SQLGetData(h_stmt, 7, SQL_C_CHAR, remarks, sizeof(remarks), &ind),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(remarks), expected.remarks.c_str());

    EXPECT_EQ(SQLGetData(h_stmt, 8, SQL_C_SSHORT, &procedure_type, 10, &ind),
              SQL_SUCCESS);
    EXPECT_EQ(procedure_type, expected.procedure_type);
  }
  EXPECT_EQ(row_count, 1)
      << "SQL_ATTR_MAX_ROWS=1 should limit SQLProcedures to 1 row";

  EXPECT_EQ(SQLFetch(h_stmt), SQL_NO_DATA)
      << "SQLProcedures returned more rows than expected.";
}

void ExecuteSQLQuery(std::shared_ptr<ODBCHandles> conn,
                     std::string const& query) {
  SQLRETURN ret = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  EXPECT_EQ(ret, SQL_SUCCESS);
}

void CreateProcedure(std::shared_ptr<ODBCHandles> conn,
                     std::string const& procedure_name,
                     std::string const& query) {
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ExecuteSQLQuery(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void CreateFunction(std::shared_ptr<ODBCHandles> conn,
                    std::string const& function_name,
                    std::string const& query) {
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ExecuteSQLQuery(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void CallSQLProcedures(std::shared_ptr<ODBCHandles> conn,
                       std::string const& pattern,
                       std::vector<ExpectedProcedureValues>& expected_values) {
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ASSERT_EQ(SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)1, 0),
            SQL_SUCCESS);
  SQLRETURN ret =
      SQLProcedures(conn->hstmt, NULL, 0, (SQLCHAR*)kDatasetName.c_str(),
                    SQL_NTS, (SQLCHAR*)pattern.c_str(), SQL_NTS);
  EXPECT_EQ(ret, SQL_SUCCESS);
  ValidateSQLProcedures(conn->hstmt, expected_values);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void CleanupRoutine(std::shared_ptr<ODBCHandles> conn,
                    std::string const& drop_query) {
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ExecuteSQLQuery(conn, drop_query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLProcedures, ComplexSQLProcedure) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string procedure_name =
      kDatasetWithTablePrefix + "TEST_COMPLEX_SQL_PROCEDURE";
  std::string create_procedure_query =
      "CREATE OR REPLACE PROCEDURE " + procedure_name +
      "(param1 INT64, param2 STRING, param3 FLOAT64, param4 BOOL, param5 "
      "TIMESTAMP, "
      "OUT param6 INT64, OUT param7 STRING) BEGIN "
      "SET param6 = param1; SET param7 = param2; END;";

  CreateProcedure(conn, procedure_name, create_procedure_query);

  std::string procedure_pattern =
      kTableNamePrefix + "TEST_COMPLEX_SQL_PROCEDURE";
  std::vector<ExpectedProcedureValues> expected_values = {kExpectedProcedure};
  expected_values[0].procedure_name = procedure_pattern;

  CallSQLProcedures(conn, procedure_pattern, expected_values);
  CleanupRoutine(conn, "DROP PROCEDURE " + procedure_name);
}

TEST(SQLProcedures, ComplexFunction) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string function_name = kDatasetWithTablePrefix + "TEST_COMPLEX_FUNCTION";
  std::string create_function_query =
      "CREATE OR REPLACE FUNCTION " + function_name +
      "(param1 INT64, param2 STRING, param3 FLOAT64, param4 BOOL, param5 "
      "TIMESTAMP) "
      "RETURNS STRING AS ( CONCAT(param2, '-', CAST(param1 AS STRING)) );";

  CreateFunction(conn, function_name, create_function_query);

  std::string function_pattern = kTableNamePrefix + "TEST_COMPLEX_FUNCTION";
  std::vector<ExpectedProcedureValues> expected_functions = {kExpectedFunction};
  expected_functions[0].procedure_name = function_pattern;

  CallSQLProcedures(conn, function_pattern, expected_functions);
  CleanupRoutine(conn, "DROP FUNCTION " + function_name);
}

TEST(CatalogTest, SQLTables_Filter_DefaultDataset_SchemaNull) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";

  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  // Filter OFF
  std::string conn_str_unfiltered =
      base_conn_str + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables_unfiltered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);
  ASSERT_FALSE(tables_unfiltered.empty());

  std::set<std::string> ds_unfiltered;
  for (auto const& t : tables_unfiltered) {
    if (t.dataset_name.has_value()) {
      ds_unfiltered.insert(t.dataset_name.value());
    }
  }

  EXPECT_GE(ds_unfiltered.size(), 1u);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  //  Filter ON
  std::string conn_str_filtered =
      base_conn_str + ";FilterTablesOnDefaultDataset=1;";

  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::vector<SQLTableResult> tables_filtered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);
  ASSERT_FALSE(tables_filtered.empty());

  std::set<std::string> ds_filtered;
  for (auto const& t : tables_filtered) {
    if (t.dataset_name.has_value()) {
      ds_filtered.insert(t.dataset_name.value());
    }
  }

  // Only default dataset expected
  EXPECT_EQ(ds_filtered.size(), 1u);
  EXPECT_EQ(*ds_filtered.begin(), default_dataset);

  if (ds_unfiltered.size() > 1) {
    EXPECT_LT(tables_filtered.size(), tables_unfiltered.size());
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_Filter_DefaultDataset_SchemaNull1) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";

  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  // Filter OFF
  std::string conn_str_unfiltered =
      base_conn_str + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);
  SQLRETURN status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_unfiltered = std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables_unfiltered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);

  auto end_unfiltered = std::chrono::high_resolution_clock::now();

  auto elapsed_unfiltered =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_unfiltered - start_unfiltered);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(FilterTablesOnDefaultDataset=0) : "
            << elapsed_unfiltered.count() << " ms" << std::endl;

  ASSERT_FALSE(tables_unfiltered.empty());

  std::set<std::string> ds_unfiltered;
  for (auto const& t : tables_unfiltered) {
    if (t.dataset_name.has_value()) {
      ds_unfiltered.insert(t.dataset_name.value());
    }
  }

  EXPECT_GE(ds_unfiltered.size(), 1u);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Filter ON
  std::string conn_str_filtered =
      base_conn_str + ";FilterTablesOnDefaultDataset=1;";

  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_filtered = std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables_filtered =
      Catalog::GetTables(conn, kCatalogName, nullptr, nullptr, nullptr);

  auto end_filtered = std::chrono::high_resolution_clock::now();

  auto elapsed_filtered =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_filtered - start_filtered);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(FilterTablesOnDefaultDataset=1) : "
            << elapsed_filtered.count() << " ms" << std::endl;

  ASSERT_FALSE(tables_filtered.empty());

  std::set<std::string> ds_filtered;
  for (auto const& t : tables_filtered) {
    if (t.dataset_name.has_value()) {
      ds_filtered.insert(t.dataset_name.value());
    }
  }

  // Only default dataset expected
  EXPECT_EQ(ds_filtered.size(), 1u);
  EXPECT_EQ(*ds_filtered.begin(), default_dataset);

  if (ds_unfiltered.size() > 1) {
    EXPECT_LT(tables_filtered.size(), tables_unfiltered.size());
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_DatasetLevelEnumeration) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string dataset = "ODBC_TEST_DATASET";

  // Only benchmark with FilterTablesOnDefaultDataset=0
  std::string conn_str =
      kDefaultConnectionString +
      ";DefaultDataset=" + dataset +
      ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt,
                                    SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE,
                                    0);

  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_dataset_enum =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> dataset_tables =
      Catalog::GetTables(
          conn,
          kCatalogName,
          dataset.c_str(),
          nullptr,
          nullptr);

  auto end_dataset_enum =
      std::chrono::high_resolution_clock::now();

  auto elapsed_dataset_enum =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_dataset_enum - start_dataset_enum);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Dataset-Level Enumeration) : "
            << elapsed_dataset_enum.count()
            << " ms" << std::endl;

  ASSERT_FALSE(dataset_tables.empty());

  // Validate all returned tables belong to requested dataset
  for (auto const& table : dataset_tables) {
    ASSERT_TRUE(table.dataset_name.has_value());
    EXPECT_EQ(table.dataset_name.value(), dataset);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_ExactTableLookup) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";

  std::string conn_str =
      kDefaultConnectionString +
      ";DefaultDataset=" + dataset +
      ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt,
                                    SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE,
                                    0);

  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_lookup =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables =
      Catalog::GetTables(
          conn,
          kCatalogName,
          dataset.c_str(),
          table_name.c_str(),
          nullptr);

  auto end_lookup =
      std::chrono::high_resolution_clock::now();

  auto elapsed_lookup =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_lookup - start_lookup);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Exact Table Lookup) : "
            << elapsed_lookup.count()
            << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());

  // Validate returned metadata
  for (auto const& table : tables) {
    ASSERT_TRUE(table.dataset_name.has_value());
    ASSERT_TRUE(table.table_name.has_value());

    EXPECT_EQ(table.dataset_name.value(), dataset);
    EXPECT_EQ(table.table_name.value(), table_name);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_WildcardTableSearch) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string dataset = "kirltest";
  std::string table_pattern = "%timestamp%";

  std::string conn_str =
      kDefaultConnectionString +
      ";DefaultDataset=" + dataset +
      ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt,
                                    SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE,
                                    0);

  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_search =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables =
      Catalog::GetTables(
          conn,
          kCatalogName,
          dataset.c_str(),
          table_pattern.c_str(),
          nullptr);

  auto end_search =
      std::chrono::high_resolution_clock::now();

  auto elapsed_search =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_search - start_search);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Wildcard Table Search) : "
            << elapsed_search.count()
            << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());

  // Validate returned tables belong to requested dataset
  for (auto const& table : tables) {
    ASSERT_TRUE(table.dataset_name.has_value());
    ASSERT_TRUE(table.table_name.has_value());

    EXPECT_EQ(table.dataset_name.value(), dataset);

    // Ensure returned table names contain "timestamp"
    EXPECT_NE(table.table_name.value().find("timestamp"),
              std::string::npos);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables_FullCatalogEnumeration_WildcardCatalog) {
  auto conn = std::make_shared<ODBCHandles>();

  // Power BI commonly sends CatalogName="%"
  // during initial metadata discovery.
  std::string catalog_pattern = "%";

  std::string conn_str =
      kDefaultConnectionString +
      ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt,
                                    SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE,
                                    0);

  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_enum =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables =
      Catalog::GetTables(
          conn,
          catalog_pattern.c_str(),
          nullptr,
          nullptr,
          nullptr);

  auto end_enum =
      std::chrono::high_resolution_clock::now();

  auto elapsed_enum =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_enum - start_enum);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Full Catalog Enumeration using Catalog='%') : "
            << elapsed_enum.count()
            << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());

  // Validate that at least one dataset/schema is returned
  std::set<std::string> discovered_datasets;

  for (auto const& table : tables) {
    if (table.dataset_name.has_value()) {
      discovered_datasets.insert(table.dataset_name.value());
    }
  }

  EXPECT_GE(discovered_datasets.size(), 1u);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#ifdef BQ_DRIVER_INTEGRATION_TESTS
// This test case currently crashes with the existing ODBC Driver for BigQuery
// v3.1.6.1026. The crash occurs in SQLColumns when schema_name is NULL,
// triggering an abort. This scenario works correctly in older versions
// (e.g., 2.5.2.1004) and with our driver. We should re-enable or validate this
// test case when we upgrade to a newer version of the existing driver or when
// the issue is resolved.
TEST(CatalogTest,
     SQLColumns_Filter_DefaultDataset_SchemaNull_TableWithSpace) {
  // TODO(b/485172463): Enable after the performance issue is resolved
  auto conn = std::make_shared<ODBCHandles>();
  std::string default_dataset = "ODBC_TEST_DATASET";

  std::string base_conn_str =
      kDefaultConnectionString + ";DefaultDataset=" + default_dataset;

  std::string table_name = "Test Table12";
  std::string quoted_table = "`" + kDatasetWithTablePrefix + table_name + "`";

  ASSERT_EQ(Connect(base_conn_str, conn), SQL_SUCCESS);

  Table table(quoted_table);
  table.Create(conn, "(id INT64, name STRING)");

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Filter OFF  (FilterTablesOnDefaultDataset = 0)
  std::string conn_str_unfiltered =
      base_conn_str + ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str_unfiltered, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt,
                                    SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE,
                                    0);

  CheckError(status, "SQLSetStmtAttr (unfiltered)", conn);

  auto start_unfiltered =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> results_unfiltered =
      Catalog::GetColumns(conn,
                          kCatalogName,
                          /*schema_name=*/nullptr,
                          /*table_name=*/nullptr,
                          /*column_name=*/nullptr);

  auto end_unfiltered =
      std::chrono::high_resolution_clock::now();

  auto elapsed_unfiltered =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_unfiltered - start_unfiltered);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Full Column Metadata Enumeration - "
            << "FilterTablesOnDefaultDataset=0) : "
            << elapsed_unfiltered.count()
            << " ms" << std::endl;

  ASSERT_FALSE(results_unfiltered.empty());

  std::set<std::string> ds_unfiltered;

  for (auto const& r : results_unfiltered) {
    ds_unfiltered.insert(r.dataset_name);
  }

  size_t count_unfiltered = results_unfiltered.size();

  bool has_multiple_datasets =
      (ds_unfiltered.size() > 1);

  EXPECT_TRUE(
      ds_unfiltered.find(default_dataset) !=
      ds_unfiltered.end());

  bool found_unfiltered = false;

  for (auto const& r : results_unfiltered) {
    if (r.table_name.find(table_name) !=
        std::string::npos) {
      found_unfiltered = true;
      break;
    }
  }

  EXPECT_TRUE(found_unfiltered)
      << "Table with space not found in "
      << "unfiltered results";

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Filter ON  (FilterTablesOnDefaultDataset = 1)
  std::string conn_str_filtered =
      base_conn_str + ";FilterTablesOnDefaultDataset=1;";

  ASSERT_EQ(Connect(conn_str_filtered, conn), SQL_SUCCESS);

  status = SQLSetStmtAttr(conn->hstmt,
                          SQL_ATTR_METADATA_ID,
                          (SQLPOINTER)SQL_FALSE,
                          0);

  CheckError(status, "SQLSetStmtAttr (filtered)", conn);

  auto start_filtered =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> results_filtered =
      Catalog::GetColumns(conn,
                          kCatalogName,
                          /*schema_name=*/nullptr,
                          /*table_name=*/nullptr,
                          /*column_name=*/nullptr);

  auto end_filtered =
      std::chrono::high_resolution_clock::now();

  auto elapsed_filtered =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_filtered - start_filtered);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Full Column Metadata Enumeration - "
            << "FilterTablesOnDefaultDataset=1) : "
            << elapsed_filtered.count()
            << " ms" << std::endl;

  ASSERT_FALSE(results_filtered.empty());

  std::set<std::string> ds_filtered;

  for (auto const& r : results_filtered) {
    ds_filtered.insert(r.dataset_name);
  }

  EXPECT_EQ(ds_filtered.size(), 1u);
  EXPECT_EQ(*ds_filtered.begin(), default_dataset);

  if (has_multiple_datasets) {
    EXPECT_LT(results_filtered.size(),
              count_unfiltered);
  }

  bool found_filtered = false;

  for (auto const& r : results_filtered) {
    if (r.table_name.find(table_name) !=
        std::string::npos) {
      found_filtered = true;
      break;
    }
  }

  EXPECT_TRUE(found_filtered)
      << "Table with space not found in "
      << "filtered results";

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  ASSERT_EQ(Connect(base_conn_str, conn),
            SQL_SUCCESS);

  table.Drop(conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#endif  // BQ_DRIVER_INTEGRATION_TESTS

TEST(CatalogTest, SQLColumns_FullMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";

  std::string conn_str =
      kDefaultConnectionString +
      ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(
          conn,
          kCatalogName,
          dataset.c_str(),
          table_name.c_str(),
          nullptr);

  auto end =
      std::chrono::high_resolution_clock::now();

  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Full Metadata Fetch) : "
            << elapsed.count()
            << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());

  for (auto const& column : columns) {
    EXPECT_EQ(column.dataset_name, dataset);
    EXPECT_EQ(column.table_name, table_name);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLColumns_ExactColumnLookup) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";
  std::string column_name = "timestamp_col_1";

  std::string conn_str =
      kDefaultConnectionString +
      ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(
          conn,
          kCatalogName,
          dataset.c_str(),
          table_name.c_str(),
          column_name.c_str());

  auto end =
      std::chrono::high_resolution_clock::now();

  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Exact Column Lookup) : "
            << elapsed.count()
            << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());

  for (auto const& column : columns) {
    EXPECT_EQ(column.dataset_name, dataset);
    EXPECT_EQ(column.table_name, table_name);
    EXPECT_EQ(column.column_name, column_name);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(CatalogTest, SQLColumns_WildcardColumnSearch) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string dataset = "kirltest";
  std::string table_name = "new_timestamp_table";

  // Matches:
  // timestamp_col_1
  // timestamp_col_2
  // ...
  std::string column_pattern = "%timestamp%";

  std::string conn_str =
      kDefaultConnectionString +
      ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(
          conn,
          kCatalogName,
          dataset.c_str(),
          table_name.c_str(),
          column_pattern.c_str());

  auto end =
      std::chrono::high_resolution_clock::now();

  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Wildcard Column Search) : "
            << elapsed.count()
            << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());

  for (auto const& column : columns) {
    EXPECT_EQ(column.dataset_name, dataset);
    EXPECT_EQ(column.table_name, table_name);

    EXPECT_NE(column.column_name.find("timestamp"),
              std::string::npos);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(CatalogTest, SQLColumns_LargeSchemaMetadataFetch) {
  auto conn = std::make_shared<ODBCHandles>();

  std::string dataset = "kirltest";
  std::string table_name = "300_column_timestamp";

  std::string conn_str =
      kDefaultConnectionString +
      ";DefaultDataset=" + dataset + ";";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  auto start =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLColumnsResult> columns =
      Catalog::GetColumns(
          conn,
          kCatalogName,
          dataset.c_str(),
          table_name.c_str(),
          nullptr);

  auto end =
      std::chrono::high_resolution_clock::now();

  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end - start);

  std::cout << "[BENCHMARK] Catalog::GetColumns "
            << "(Large Schema Metadata Fetch) : "
            << elapsed.count()
            << " ms" << std::endl;

  ASSERT_FALSE(columns.empty());

  for (auto const& column : columns) {
    EXPECT_EQ(column.dataset_name, dataset);
    EXPECT_EQ(column.table_name, table_name);
  }

  // Validate wide schema
  EXPECT_GE(columns.size(), 300);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}


TEST(CatalogTest,
     SQLTables_FullCatalogEnumeration_TableAndViewFilter) {
  auto conn = std::make_shared<ODBCHandles>();

  // Power BI commonly sends:
  // CatalogName = "%"
  // TableType   = "TABLE,VIEW"
  // during initial Navigator metadata discovery.

  std::string catalog_pattern = "%";
  std::string table_types = "TABLE,VIEW";

  std::string conn_str =
      kDefaultConnectionString +
      ";FilterTablesOnDefaultDataset=0;";

  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLSetStmtAttr(conn->hstmt,
                                    SQL_ATTR_METADATA_ID,
                                    (SQLPOINTER)SQL_FALSE,
                                    0);

  CheckError(status, "SQLSetStmtAttr", conn);

  auto start_enum =
      std::chrono::high_resolution_clock::now();

  std::vector<SQLTableResult> tables =
      Catalog::GetTables(
          conn,
          catalog_pattern.c_str(),
          nullptr,
          nullptr,
          table_types.c_str());

  auto end_enum =
      std::chrono::high_resolution_clock::now();

  auto elapsed_enum =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          end_enum - start_enum);

  std::cout << "[BENCHMARK] Catalog::GetTables "
            << "(Full Catalog Enumeration with TABLE/VIEW Filter) : "
            << elapsed_enum.count()
            << " ms" << std::endl;

  ASSERT_FALSE(tables.empty());

  // Validate metadata
  std::set<std::string> discovered_datasets;

  for (auto const& table : tables) {
    if (table.dataset_name.has_value()) {
      discovered_datasets.insert(table.dataset_name.value());
    }

    // Validate returned object types
    if (table.table_type.has_value()) {
      EXPECT_TRUE(
          table.table_type.value() == "TABLE" ||
          table.table_type.value() == "VIEW");
    }
  }

  EXPECT_GE(discovered_datasets.size(), 1u);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLProcedures, TableFunction) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string routine_name = kDatasetWithTablePrefix + "TEST_TABLE_ROUTINE";
  std::string create_routine_query =
      "CREATE OR REPLACE TABLE FUNCTION " + routine_name +
      "(param1 INT64) AS SELECT param1 AS result;";

  CreateFunction(conn, routine_name, create_routine_query);

  std::string routine_pattern = kTableNamePrefix + "TEST_TABLE_ROUTINE";
  std::vector<ExpectedProcedureValues> expected_values = {
      kExpectedTableRoutine};
  expected_values[0].procedure_name = routine_pattern;

  CallSQLProcedures(conn, routine_pattern, expected_values);
  CleanupRoutine(conn, "DROP TABLE FUNCTION " + routine_name);
}

TEST(SQLColumns, Check_SQLColumnsDescriptors) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status =
      SQLColumns(conn->hstmt, (SQLCHAR*)kCatalog.c_str(), SQL_NTS,
                 (SQLCHAR*)"DATATYPERANGETEST", SQL_NTS,
                 (SQLCHAR*)kSampleDataset.c_str(), SQL_NTS, nullptr, 0);
  EXPECT_EQ(status, SQL_SUCCESS);

  SQLHDESC hird = nullptr;
  EXPECT_EQ(
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &hird, 0, nullptr),
      SQL_SUCCESS);

  SQLSMALLINT col_count = 0;
  ASSERT_EQ(SQLGetDescField(hird, 0, SQL_DESC_COUNT, &col_count, 0, nullptr),
            SQL_SUCCESS);
  // existing driver has a extra metadata descriptor which is optional.
  if (kIsBqDriver) {
    EXPECT_EQ(col_count,
              static_cast<SQLSMALLINT>(kColumnMetaDataSchema.size()));
  } else {
    EXPECT_EQ(col_count,
              static_cast<SQLSMALLINT>(kColumnMetaDataSchema.size() + 1));
  }

  for (SQLSMALLINT i = 1; i <= col_count; ++i) {
    SQLCHAR name[256] = {0};
    EXPECT_EQ(
        SQLGetDescField(hird, i, SQL_DESC_NAME, name, sizeof(name), nullptr),
        SQL_SUCCESS);
    std::string col_name = reinterpret_cast<char*>(name);
    // existing driver has a extra metadata descriptor which is optional.
    if (!kIsBqDriver && col_name == "USER_DATA_TYPE") {
      continue;
    }
    auto it = std::find(kColumnMetaDataSchema.begin(),
                        kColumnMetaDataSchema.end(), col_name);
    ASSERT_NE(it, kColumnMetaDataSchema.end());
    EXPECT_EQ(col_name, *it);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLTables, Check_SQLTablesDescriptors) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status =
      SQLTables(conn->hstmt, (SQLCHAR*)SQL_ALL_CATALOGS, SQL_NTS,
                (SQLCHAR*)kNullString.c_str(), 0, (SQLCHAR*)kNullString.c_str(),
                0, (SQLCHAR*)kNullString.c_str(), 0);
  EXPECT_EQ(status, SQL_SUCCESS);

  SQLHDESC hird;
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &hird, 0, nullptr);
  EXPECT_EQ(status, SQL_SUCCESS);

  SQLSMALLINT col_count = 0;
  status = SQLGetDescField(hird, 0, SQL_DESC_COUNT, &col_count, 0, nullptr);
  CheckError(status, "SQLGetDescField", conn);
  EXPECT_EQ(col_count, 5);

  for (SQLSMALLINT i = 1; i <= col_count; ++i) {
    SQLCHAR name[256] = {0};
    EXPECT_EQ(
        SQLGetDescField(hird, i, SQL_DESC_NAME, name, sizeof(name), nullptr),
        SQL_SUCCESS);
    std::string col_name = reinterpret_cast<char*>(name);
    auto it = std::find(kTableMetaDataSchema.begin(),
                        kTableMetaDataSchema.end(), col_name);
    EXPECT_EQ(col_name, *it);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
