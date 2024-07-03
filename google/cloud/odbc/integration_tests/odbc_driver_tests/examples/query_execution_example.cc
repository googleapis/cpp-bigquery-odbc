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
#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include <gtest/gtest.h>
#include <iostream>
#include <map>

namespace google::cloud::odbc_tests {

// Helper functions for this test only.
namespace {

std::string const kExecutionDemoTableName =
    "ODBC_DEMO_DATASET.ODBC_EXEC_DEMO_TEST";

StdRows const kSampleData{
    {"Test String 1", 1, 1.1},   {"Test String 2", 53, 5},
    {"Test String 3", 53, 0.31}, {"Test String 4", 81, 8.8},
    {"Test String 5", 82, 8.8},
};

static std::map<SQLSMALLINT, std::string> const kSQLDataTypeToString = {
    {SQL_CHAR, "SQL_CHAR"},
    {SQL_NUMERIC, "SQL_NUMERIC"},
    {SQL_INTEGER, "SQL_INTEGER"},
    {SQL_SMALLINT, "SQL_SMALLINT"},
    {SQL_FLOAT, "SQL_FLOAT"},
    {SQL_REAL, "SQL_REAL"},
    {SQL_DOUBLE, "SQL_DOUBLE"},
    {SQL_VARCHAR, "SQL_VARCHAR"},
    {SQL_TYPE_DATE, "SQL_TYPE_DATE"},

    {SQL_LONGVARCHAR, "SQL_LONGVARCHAR"},
    {SQL_BINARY, "SQL_BINARY"},
    {SQL_VARBINARY, "SQL_VARBINARY"},
    {SQL_LONGVARBINARY, "SQL_LONGVARBINARY"},
    {SQL_BIGINT, "SQL_BIGINT"},
    {SQL_TINYINT, "SQL_TINYINT"},
    {SQL_BIT, "SQL_BIT"},
};

void wait() {
  std::cout << "(Press enter....) " << std::endl;
  std::string input;
  std::getline(std::cin, input);
}

void BindColPtr(std::shared_ptr<ODBCHandles> conn, int index,
                std::shared_ptr<Column> col_ptr) {
  SqlToCdataTypes(col_ptr);
  SQLRETURN status = SQLBindCol(
      conn->hstmt, index, col_ptr->data_type, col_ptr->data_buf.target_value,
      col_ptr->data_buf.buffer_length, &(col_ptr->data_buf.str_len));
  CheckError(status, "SQLBindCol(" + std::to_string(index) + ")", conn);
}

void PrintCol(int i, SQLSMALLINT data_type, SQLULEN column_size,
              SQLSMALLINT decimal_digits, SQLSMALLINT nullable,
              SQLCHAR* column_name, SQLSMALLINT column_name_len) {
  std::cout << "SQLDescribeCol: " << i << " ...." << std::endl;
  std::cout << "data_type:: " << kSQLDataTypeToString.at(data_type)
            << std::endl;
  std::cout << "column_name:: " << column_name << std::endl;
  std::cout << std::endl;
}

void DescribeAndBindColumns(std::shared_ptr<ODBCHandles> conn,
                            std::vector<std::shared_ptr<Column>>& cols,
                            SQLSMALLINT num_cols) {
  SQLRETURN status;
  for (int i = 1; i <= num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i - 1] = col_ptr;

    DescribeCol(conn, col_ptr, i);
    PrintCol(i, col_ptr->data_type, col_ptr->data_size, col_ptr->decimal_digits,
             col_ptr->nullable, col_ptr->name, col_ptr->name_len);

    BindColPtr(conn, i, col_ptr);
  }
}

void PrintSampleData() {
  for (auto std_row : kSampleData) {
    std::cout << "{'" << std_row.str_field << "', " << std_row.int_field << ", "
              << std_row.float_field << "}," << std::endl;
  }
  std::cout << std::endl;
}

}  // namespace

TEST(SQLExecute, BasicWriteAndRead) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLRETURN status;

  // 1) Connect to the data source.
  std::cout << "Connecting to the data source..." << std::endl << std::endl;
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::cout << "Successfully connected to the data source!" << std::endl
            << std::endl;

  std::string table_name = "ODBC_TEST_DATASET_SACHIN.TestTable";

  // (2) Creating Tables for reading data
  std::string schema =
      "(StringField STRING, IntField INT64, FloatField FLOAT64)";
  std::cout << "Creating table with schema: " << schema
            << ", before executing read query ..." << std::endl
            << std::endl;
  wait();
  Table table(kExecutionDemoTableName);
  table.CreateWithPrepare(conn, schema);
  std::cout << "Successfully created table: " << kExecutionDemoTableName
            << std::endl
            << std::endl;

  // (3) Inserting Sample Data
  std::cout << "Inserting sample data into the created table..." << std::endl
            << std::endl;
  wait();
  table.InsertData(conn, kSampleData, false, true);
  std::cout << "Successfully inserted: " << kSampleData.size() << " rows!"
            << std::endl
            << std::endl;
  PrintSampleData();
  wait();

  // (5) Fetch Rows
  std::string insert_stmt = "SELECT FloatField, StringField, IntField FROM " +
                            kExecutionDemoTableName;
  std::cout << "Prepare a read query ..." << std::endl << std::endl;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                      insert_stmt.size());
  CheckError(status, "SQLPrepare", conn, false);
  std::cout << "Finished preparing: " << insert_stmt << std::endl
            << std::endl
            << std::endl;

  wait();

  SQLSMALLINT num_cols;
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  std::cout << "SQLNumResultCols -> num_cols:: " << num_cols << std::endl
            << std::endl;
  wait();

  std::cout << "Binding application buffers ..." << std::endl << std::endl;
  std::vector<std::shared_ptr<Column>> cols(num_cols);
  DescribeAndBindColumns(conn, cols, num_cols);
  std::cout << "Finished binding buffers!" << std::endl
            << std::endl
            << std::endl;
  wait();

  std::cout << "Executing read statement: " << insert_stmt << std::endl
            << std::endl;
  wait();
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn, false);
  EXPECT_EQ(status, SQL_SUCCESS);

  std::cout << "Fetching inserted data..." << std::endl << std::endl;
  // Read all the rows using SQLFetch
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }

    std::cout << "*******************************************************"
              << std::endl;
    for (int i_c = 0; i_c < num_cols; i_c++) {
      SQLSMALLINT data_type = cols[i_c]->data_type;
      switch (data_type) {
        case SQL_C_SBIGINT: {
          SQLBIGINT int_val = *((SQLBIGINT*)cols[i_c]->data_buf.target_value);
          std::cout << cols[i_c]->name << ": " << int_val << ", " << std::endl;
          break;
        }
        case SQL_C_DOUBLE: {
          SQLDOUBLE float_val = *((SQLDOUBLE*)cols[i_c]->data_buf.target_value);
          std::cout << cols[i_c]->name << ": " << float_val << ", "
                    << std::endl;
          break;
        }
        case SQL_C_CHAR: {
          std::string str_val = (char*)cols[i_c]->data_buf.target_value;
          std::cout << cols[i_c]->name << ": " << str_val << ", " << std::endl;
          break;
        }
        default: {
          std::cout << cols[i_c]->name << ": invalid data_type: " << data_type
                    << std::endl;
        }
      }
    }
    std::cout << "*******************************************************"
              << std::endl
              << std::endl;
    wait();
  }
  std::cout << "No more rows to read!" << std::endl << std::endl;
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // (6) Cleanup
  std::cout << "Deleting the table ..." << std::endl << std::endl;
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  std::cout << "Successfully deleted table: " << kExecutionDemoTableName
            << std::endl
            << std::endl;
  ;

  std::cout << "Freeing ODBC handles..." << std::endl << std::endl;
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  std::cout << "Successfully freed all handles!" << std::endl;
}

}  // namespace google::cloud::odbc_tests

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
