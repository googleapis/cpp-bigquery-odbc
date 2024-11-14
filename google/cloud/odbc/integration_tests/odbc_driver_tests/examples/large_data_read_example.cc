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
#include <ctime>
#include <iostream>
#include <map>

namespace google::cloud::odbc_tests {

// Helper functions for this test only.
namespace {

void BindColPtr(std::shared_ptr<ODBCHandles> conn, int index,
                std::shared_ptr<Column> col_ptr) {
  SqlToCdataTypes(col_ptr);
  SQLRETURN status = SQLBindCol(
      conn->hstmt, index, col_ptr->data_type, col_ptr->data_buf.target_value,
      col_ptr->data_buf.buffer_length, &(col_ptr->data_buf.str_len));
  CheckError(status, "SQLBindCol(" + std::to_string(index) + ")", conn);
}

void DescribeAndBindColumns(std::shared_ptr<ODBCHandles> conn,
                            std::vector<std::shared_ptr<Column>>& cols,
                            SQLSMALLINT num_cols) {
  SQLRETURN status;
  for (int i = 1; i <= num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i - 1] = col_ptr;

    DescribeCol(conn, col_ptr, i);
    BindColPtr(conn, i, col_ptr);
  }
}

}  // namespace

TEST(SQLExecute, SimpleLargeDataRead) {
  std::time_t currentTime = std::time(nullptr);  // Get the current time
  std::cout << "Current time: " << std::ctime(&currentTime) << std::endl;

  auto conn = std::make_shared<ODBCHandles>();
  SQLRETURN status;

  // 1) Connect to the data source.
  std::cout << "Connecting to the data source..." << std::endl << std::endl;
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::cout << "Successfully connected to the data source!" << std::endl
            << std::endl;
  SQLSetStmtAttr(conn->hstmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)1, 0);
  CheckError(status, "SQLSetStmtAttr", conn, false);

  // (5) Fetch Rows
  std::string insert_stmt =
      "SELECT mean_dew_point, num_mean_temp_samples FROM "
      "bigquery-public-data.samples.gsod WHERE mean_dew_point IS NOT NULL "
      "LIMIT 500000";
  std::cout << "Prepare a read query ..." << std::endl << std::endl;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                      insert_stmt.size());
  CheckError(status, "SQLPrepare", conn, false);
  std::cout << "Finished preparing: " << insert_stmt << std::endl
            << std::endl
            << std::endl;

  SQLSMALLINT num_cols;
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  std::cout << "SQLNumResultCols-> num_cols:: " << num_cols << std::endl
            << std::endl;

  std::cout << "Binding application buffers ..." << std::endl << std::endl;
  std::vector<std::shared_ptr<Column>> cols(num_cols);
  DescribeAndBindColumns(conn, cols, num_cols);
  std::cout << "Finished binding buffers!" << std::endl
            << std::endl
            << std::endl;

  std::cout << "Executing read statement: " << insert_stmt << std::endl
            << std::endl;
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
  }
  std::cout << "No more rows to read!" << std::endl << std::endl;

  currentTime = std::time(nullptr);  // Get the current time
  std::cout << "Current time: " << std::ctime(&currentTime) << std::endl;

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
