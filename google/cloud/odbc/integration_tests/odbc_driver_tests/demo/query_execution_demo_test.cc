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

std::string const kConnectionString = "DSN=SampleDSN";

std::string const kExecutionDemoTableName =
    kDatasetWithTablePrefix + "ODBC_EXEC_DEMO_TEST";

StdRows const kSampleData{
    {"Test String 1", 1, 1.1},    {"Test String 2", 53, 5},
    {"Test String 3", 698, 0.31}, {"Test String 4", 12, 71.6},
    {"Test String 5", 83, 8.8},
};

void FetchData(std::shared_ptr<ODBCHandles> conn, TestingDataBuffer* columns) {
  std::string insert_stmt = "SELECT * FROM " + kExecutionDemoTableName;
  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                                insert_stmt.size());
  CheckError(status, "SQLPrepare", conn, false);
  EXPECT_EQ(status, SQL_SUCCESS);

  std::cout << "Binding Columns..." << std::endl << std::endl;
  BindStdColumns(conn, columns);

  std::cout << "Executing read statement: " << insert_stmt << std::endl
            << std::endl;
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute(1)", conn, false);
  EXPECT_EQ(status, SQL_SUCCESS);

  std::cout << "Fetching inserted data..." << std::endl << std::endl;
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }
    std::string str_val = (char*)columns[0].target_value;
    SQLBIGINT int_val = *((SQLBIGINT*)columns[1].target_value);
    SQLDOUBLE float_val = *((SQLDOUBLE*)columns[2].target_value);

    std::cout << "*******************************************************"
              << std::endl;
    std::cout << "str_val: " << str_val << ", " << std::endl;
    std::cout << "int_val: " << int_val << ", " << std::endl;
    std::cout << "float_val: " << float_val << ", " << std::endl;
    std::cout << "*******************************************************"
              << std::endl
              << std::endl;
  }
}

}  // namespace

TEST(StatementDemoTest, SQLExecute) {
  SQLRETURN status;
  auto conn = std::make_shared<ODBCHandles>();
  // 1) Connect to the data source.
  std::cout << "Connecting to the data source..." << std::endl << std::endl;
  ASSERT_EQ(Connect(kConnectionString, conn, true), SQL_SUCCESS);
  std::cout << "Successfully connected to the data source!" << std::endl
            << std::endl;
  // (2) Creating Tables for reading data
  std::cout << "Creating table before executing read query ..." << std::endl
            << std::endl;
  Table table(kExecutionDemoTableName);
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INT64, FloatField FLOAT64)");
  std::cout << "Successfully created table: " << kExecutionDemoTableName
            << std::endl;

  // (3) Inserting Sample Data
  std::cout << "Inserting sample data into the created table..." << std::endl
            << std::endl;
  table.InsertData(conn, kSampleData, false, true);
  std::cout << "Successfully inserted: " << kSampleData.size() << " rows!"
            << std::endl;

  // (5) Fetch Rows
  TestingDataBuffer columns[3];
  FetchData(conn, columns);

  // (6) Cleanup
  std::cout << "Deleting the table ..." << std::endl << std::endl;
  table.DropWithPrepare(conn);
  std::cout << "Successfully deleted table: " << kExecutionDemoTableName
            << std::endl;

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
