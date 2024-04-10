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

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

namespace google::cloud::odbc_tests {

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

// Drops all tables in a dataset
void ClearDataset(
    std::string kDatasetName,
    std::shared_ptr<std::vector<std::string>> table_names_ptr = nullptr) {
  auto conn = std::make_shared<ODBCHandles>();
  std::vector<std::string> table_names;
  if (!table_names_ptr) {
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);
    table_names = (*Catalog::GetTables(conn, kDatasetName))[kDatasetName];
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  } else {
    table_names = *table_names_ptr;
  }

  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
  for (auto table_name : table_names) {
    std::string table_name_full = kDatasetName + "." + table_name;
    Table(table_name_full).Drop(conn);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(CatalogTest, SQLTables) {
  auto conn = std::make_shared<ODBCHandles>();

  // Create tables
  for (auto it : kTables) {
    std::string table_name = it.first;
    std::string table_name_full = kDatasetName + "." + table_name;
    // Create Table
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    Table(table_name_full).Create(conn, getSchemaStr(it.second));
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  // Verify if the tables returned by SQLTables are the same as the ones created
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);

  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);

  auto table_names = (*Catalog::GetTables(conn, kDatasetName))[kDatasetName];
  std::vector<std::string> test_table_names;
  for (auto it : kTables) {
    EXPECT_NE(std::find(table_names.begin(), table_names.end(), it.first),
              table_names.end());
    test_table_names.push_back(it.first);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  ClearDataset(kDatasetName,
               std::make_shared<std::vector<std::string>>(test_table_names));
}

TEST(CatalogTest, SQLTablesA) {
  auto conn = std::make_shared<ODBCHandles>();

  // Create tables
  for (auto it : kTablesAnsi) {
    std::string table_name = it.first;
    std::string table_name_full = kDatasetName + "." + table_name;
    // Create Table
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, true), SQL_SUCCESS);
    Table(table_name_full).Create(conn, getSchemaStr(it.second), true);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  // Verify if the tables returned by SQLTables are the same as the ones created
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, true), SQL_SUCCESS);

  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);

  auto table_names =
      (*Catalog::GetTables(conn, kDatasetName, true))[kDatasetName];
  std::vector<std::string> test_table_names;
  for (auto it : kTablesAnsi) {
    EXPECT_NE(std::find(table_names.begin(), table_names.end(), it.first),
              table_names.end());
    test_table_names.push_back(it.first);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  ClearDataset(kDatasetName,
               std::make_shared<std::vector<std::string>>(test_table_names));
}

}  // namespace google::cloud::odbc_tests

#endif  // BQ_DRIVER_INTEGRATION_TESTS
