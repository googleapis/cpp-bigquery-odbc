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

#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"

namespace google::cloud::odbc_tests {

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

StdRows const kSampleData{
    {"Test String 1", 1, 1.1},      {.int_field = 237, .float_field = 2.22},
    {"Test String 3", NULL, 3.333}, {"Test String 4", 49},
    {"Test String 5", 53, 5},       {"Test String 6", 698, 0.31},
    {"Test String 7", 12, 71.6},    {"Test String 8", 83, 8.8},
};

// Checks if the column description returned by DescribeCol matches the schema
void CheckColumnData(std::shared_ptr<ODBCHandles> conn, std::string table_name,
                     Schema schema) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, "SELECT * FROM " + table_name);

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  // Check if the number of columns returned is correct
  SQLSMALLINT num_cols;
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, schema.size());

  // Loop through columns and verify descriptions
  std::vector<std::shared_ptr<Column>> cols(num_cols);
  for (int i = 0; i < num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i] = col_ptr;

    DescribeCol(conn, col_ptr, i + 1);

    // Verify returned column descriptions with the table schema
    EXPECT_STREQ((char const*)col_ptr->name, schema[i].name.c_str());
    EXPECT_EQ(col_ptr->name_len, schema[i].name.length());
    EXPECT_EQ(col_ptr->data_type, schema[i].type);
    EXPECT_EQ(col_ptr->nullable, SQL_NULLABLE);
  }
}

// Verify if the inserted data(<input_data>) is the same as the data fetched
// col-wise Note: This doesn't verify the integrity of the fetched rows
void VerifyColumnWiseResults(StdRows input_data, Results col_wise_data,
                             std::vector<std::string> col_names) {
  if (!col_names.size()) {
    std::vector<std::string> all_col_names;
    for (auto it = col_wise_data.begin(); it != col_wise_data.end(); it++) {
      all_col_names.emplace_back(it->first);
    }
    col_names = all_col_names;
  }
  for (auto col_name : col_names) {
    auto ret_col_values = col_wise_data[col_name];

    // We have to sort inserted and returned values because we haven't specified
    // the ordering
    sort(ret_col_values.begin(), ret_col_values.end(), str_comparison);

    std::vector<std::string> input_col_values;
    for (auto data : input_data) {
      input_col_values.emplace_back(data.str_field);
    }
    sort(input_col_values.begin(), input_col_values.end(), str_comparison);

    // Check if the sorted inserted and returned vectors have same values
    EXPECT_EQ(ret_col_values.size(), input_col_values.size());
    for (int i = 0; i < ret_col_values.size(); i++) {
      EXPECT_EQ(ret_col_values[i], input_col_values[i]);
    }
  }
}

void ExecDirectWithFetchTest(std::string const in_table_name, bool is_async) {
  std::string const table_name = kDatasetName + "." + in_table_name;
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Insert(conn, kSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  auto const query = "SELECT StringField FROM " + table_name;
  auto results = *FetchDirect(conn, query, 1, is_async);
  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecDirect) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(InsertDirectStatement(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecute) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(InsertStatement(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecute_UsingDescriptor) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(InsertStatementWithBindParameter(conn), SQL_SUCCESS);

  // We inserted a row using first statement handle.
  // Now we're going to do the same using a new statement handle,
  // but without SQLBindParameter calls.
  // We reuse desc handle instead.

  // Free existing statement handle (within same connection)
  SQLCloseCursor(conn->hstmt);
  auto status = SQLFreeHandle(SQL_HANDLE_STMT, conn->hstmt);
  CheckError(status, "SQLFreeHandle", conn);

  // Allocate a new statement handle (within same connection)
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
  CheckError(status, "SQLAllocHandle", conn);

  EXPECT_EQ(InsertStatementWithoutBindParameter(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLNumParams) {
  auto conn = std::make_shared<ODBCHandles>();
  auto const table_name = kDatasetName + ".ODBC_NUM_PARAMS_TEST";
  auto const insert_stmt = "INSERT INTO " + table_name + " VALUES (?, ?, ?)";
  Table table(table_name);

  // Create Table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLSMALLINT num_params;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLNumParams(conn->hstmt, &num_params);
  CheckError(status, "SQLNumParams", conn);
  EXPECT_EQ(num_params, 3);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLDescribeCol) {
  auto const table_name = kDatasetName + ".ODBC_COLUMN_DESCRIPTION_TEST";
  Table table(table_name);

  Schema schema{{"StringField", SQL_VARCHAR},
                {"IntegerField", SQL_BIGINT},
                {"FloatField", SQL_DOUBLE}};

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Insert(conn, kSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckColumnData(conn, table_name, schema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void FetchDataTest(bool use_bind_col) {
  auto const table_name = kDatasetName + ".ODBC_CHECK_RESULTS_TEST_" +
                          (use_bind_col ? "true" : "false");
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Insert(conn, kSampleData);
  SQLLEN rows_count = 0;
  auto status = SQLRowCount(conn->hstmt, &rows_count);
  CheckError(status, "SQLRowCount", conn);
  EXPECT_EQ(rows_count, kSampleData.size());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  auto const query = "SELECT StringField FROM " + table_name;
  auto results = *FetchResults(conn, query, use_bind_col);

  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLFetch) { FetchDataTest(true); }

TEST(StatementTest, SQLFetch_WithoutSQLBindCol) { FetchDataTest(false); }

TEST(StatementTest, SQLFetch_with_SQLExecDirect) {
  ExecDirectWithFetchTest("ODBC_FETCH_WITH_EXECDIRECT_SYNC_TEST", false);
}

TEST(StatementTest, SQLFetch_with_SQLExecDirectAsync) {
  ExecDirectWithFetchTest("ODBC_FETCH_WITH_EXECDIRECT_ASYNC_TEST", true);
}

TEST(StatementTest, SQLFetchScroll) {
  auto const table_name = kDatasetName + ".ODBC_SCROLL_RESULTS_TEST";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Insert(conn, kSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto const query = "SELECT StringField FROM " + table_name;
  auto results = *ScrollResults(conn, query, 3);
  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLGetData) {
  auto const table_name = kDatasetName + ".ODBC_GET_DATA_TEST";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Insert(conn, kSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  std::string query = "SELECT StringField FROM " + table_name;
  auto results = *FetchResultsWithSqlGetData(conn, query);

  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This test is temporarily disabled till we are able to debug this with help
// from the vendor
TEST(StatementTest, DISABLED_SQLPutData) {
  auto const table_name = kDatasetName + ".ODBC_PUT_DATA_TEST";
  Table table(table_name);

  // TODO(#14): Add integer and floating point fields too
  // Schema returned by the query
  Schema schema{{"StringField1", SQL_VARCHAR},
                {"StringField2", SQL_VARCHAR},
                {"StringField3", SQL_VARCHAR}};

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  table.Create(conn, getSchemaStr(schema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert a row
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (?, ?, ?)";
  std::vector<std::string> data;
  for (int i = 0; i < schema.size(); i++) {
    data.emplace_back(GetRandomString(50));
  }
  InsertDataWithSqlPut(conn, query, data);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  query = "SELECT StringField1, StringField2, StringField3 FROM " + table_name;
  auto results = *FetchResultsWithSqlGetData(conn, query);

  for (int i = 0; i < schema.size(); i++) {
    auto col_name = schema[i].name;
    EXPECT_EQ(results[col_name][0], data[i]);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLSetCursorName) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLCHAR cursor_name[kBufferLength] = "INSERT_CURSOR",
          cursor_name_ret[kBufferLength];

  auto status = SQLSetCursorName(conn->hstmt, cursor_name, kBufferLength);
  CheckError(status, "SQLSetCursorName", conn);

  status = SQLGetCursorName(conn->hstmt, cursor_name_ret, kBufferLength, NULL);
  CheckError(status, "SQLGetCursorName", conn);

  EXPECT_STREQ((char*)cursor_name_ret, (char*)cursor_name);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetStmtAttribute, SetAllAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;

  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)SQL_ASYNC_ENABLE_ON, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_ASYNC_ENABLE\n";
  }
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)SQL_ASYNC_ENABLE_OFF, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_ASYNC_ENABLE\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_READ_ONLY, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CONCURRENCY - SQL_CONCUR_READ_ONLY\n";
  }
      CheckError(status, "SQL_ATTR_CONCURRENCY", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_LOCK, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CONCURRENCY - SQL_CONCUR_LOCK\n";
  }
      CheckError(status, "SQL_ATTR_CONCURRENCY", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_ROWVER, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CONCURRENCY - SQL_CONCUR_ROWVER\n";
  }
      CheckError(status, "SQL_ATTR_CONCURRENCY", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CONCURRENCY, (SQLPOINTER)SQL_CONCUR_VALUES, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CONCURRENCY - SQL_CONCUR_VALUES\n";
  }
      CheckError(status, "SQL_ATTR_CONCURRENCY", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER)SQL_NONSCROLLABLE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_SCROLLABLE - SQL_NONSCROLLABLE\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_SCROLLABLE", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_SCROLLABLE, (SQLPOINTER)SQL_SCROLLABLE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_SCROLLABLE - SQL_SCROLLABLE\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_SCROLLABLE", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER)SQL_UNSPECIFIED, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_SENSITIVITY - SQL_UNSPECIFIED\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_SENSITIVITY", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER)SQL_INSENSITIVE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_SENSITIVITY - SQL_INSENSITIVE\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_SENSITIVITY", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_SENSITIVITY, (SQLPOINTER)SQL_SENSITIVE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_SENSITIVITY - SQL_SENSITIVE\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_SENSITIVITY", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_FORWARD_ONLY\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_TYPE", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_STATIC, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_STATIC\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_TYPE", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_KEYSET_DRIVEN, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_KEYSET_DRIVEN\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_TYPE", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_TYPE, (SQLPOINTER)SQL_CURSOR_DYNAMIC, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_TYPE - SQL_CURSOR_DYNAMIC\n";
  }
      CheckError(status, "SQL_ATTR_CURSOR_TYPE", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ENABLE_AUTO_IPD, (SQLPOINTER)SQL_FALSE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_ENABLE_AUTO_IPD\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ENABLE_AUTO_IPD, (SQLPOINTER)SQL_TRUE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_ENABLE_AUTO_IPD\n";
  }
  SQLULEN bookmark = 10;
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_FETCH_BOOKMARK_PTR, &bookmark, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_FETCH_BOOKMARK_PTR\n";
  }
      CheckError(status, "SQL_ATTR_FETCH_BOOKMARK_PTR", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_KEYSET_SIZE, (SQLPOINTER)10, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_KEYSET_SIZE\n";
  }
      CheckError(status, "SQL_ATTR_KEYSET_SIZE", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_LENGTH, (SQLPOINTER)10, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_MAX_LENGTH\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)10, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_MAX_ROWS\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_TRUE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_METADATA_ID\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)SQL_FALSE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_METADATA_ID\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_NOSCAN, (SQLPOINTER)SQL_NOSCAN_OFF, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_NOSCAN\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_NOSCAN, (SQLPOINTER)SQL_NOSCAN_ON, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_NOSCAN\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)10, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_QUERY_TIMEOUT\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_RETRIEVE_DATA, (SQLPOINTER)SQL_RD_ON, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_RETRIEVE_DATA\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_RETRIEVE_DATA, (SQLPOINTER)SQL_RD_OFF, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_RETRIEVE_DATA\n";
  }
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_SIMULATE_CURSOR, (SQLPOINTER)SQL_SC_NON_UNIQUE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_SIMULATE_CURSOR - SQL_SC_NON_UNIQUE\n";
  }
      CheckError(status, "SQL_ATTR_SIMULATE_CURSOR", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_SIMULATE_CURSOR, (SQLPOINTER)SQL_SC_TRY_UNIQUE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_SIMULATE_CURSOR - SQL_SC_TRY_UNIQUE\n";
  }
      CheckError(status, "SQL_ATTR_SIMULATE_CURSOR", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_SIMULATE_CURSOR, (SQLPOINTER)SQL_SC_UNIQUE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_SIMULATE_CURSOR - SQL_SC_UNIQUE\n";
  }
      CheckError(status, "SQL_ATTR_SIMULATE_CURSOR", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER)SQL_UB_OFF, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_USE_BOOKMARKS - SQL_UB_OFF\n";
  }
      CheckError(status, "SQL_ATTR_USE_BOOKMARKS", conn);
status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER)SQL_UB_VARIABLE, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_USE_BOOKMARKS - SQL_UB_VARIABLE\n";
  }
      CheckError(status, "SQL_ATTR_USE_BOOKMARKS", conn);



  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetStmtAttribute, GetAllAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  SQLULEN out = 0;

  out = 0;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_ASYNC_ENABLE\n";
  } else {
    std::cout << "Default value for SQL_ATTR_ASYNC_ENABLE: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, 29, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_ASYNC_STMT_EVENT\n";
  } else {
    std::cout << "Default value for SQL_ATTR_ASYNC_STMT_EVENT: " << out << "\n";
  }
      CheckError(status, "SQL_ATTR_ASYNC_STMT_EVENT", conn);
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_CONCURRENCY, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CONCURRENCY\n";
  } else {
    std::cout << "Default value for SQL_ATTR_CONCURRENCY: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_SCROLLABLE, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_SCROLLABLE\n";
  } else {
    std::cout << "Default value for SQL_ATTR_CURSOR_SCROLLABLE: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_SENSITIVITY, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_SENSITIVITY\n";
  } else {
    std::cout << "Default value for SQL_ATTR_CURSOR_SENSITIVITY: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_CURSOR_TYPE, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_CURSOR_TYPE\n";
  } else {
    std::cout << "Default value for SQL_ATTR_CURSOR_TYPE: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ENABLE_AUTO_IPD, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_ENABLE_AUTO_IPD\n";
  } else {
    std::cout << "Default value for SQL_ATTR_ENABLE_AUTO_IPD: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_FETCH_BOOKMARK_PTR, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_FETCH_BOOKMARK_PTR\n";
  } else {
    std::cout << "Default value for SQL_ATTR_FETCH_BOOKMARK_PTR: " << out << "\n";
  }
      CheckError(status, "SQL_ATTR_FETCH_BOOKMARK_PTR", conn);
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_KEYSET_SIZE, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_KEYSET_SIZE\n";
  } else {
    std::cout << "Default value for SQL_ATTR_KEYSET_SIZE: " << out << "\n";
  }
      CheckError(status, "SQL_ATTR_KEYSET_SIZE", conn);
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_MAX_LENGTH, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_MAX_LENGTH\n";
  } else {
    std::cout << "Default value for SQL_ATTR_MAX_LENGTH: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_MAX_ROWS\n";
  } else {
    std::cout << "Default value for SQL_ATTR_MAX_ROWS: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_METADATA_ID\n";
  } else {
    std::cout << "Default value for SQL_ATTR_METADATA_ID: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_NOSCAN, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_NOSCAN\n";
  } else {
    std::cout << "Default value for SQL_ATTR_NOSCAN: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_QUERY_TIMEOUT, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_QUERY_TIMEOUT\n";
  } else {
    std::cout << "Default value for SQL_ATTR_QUERY_TIMEOUT: " << out << "\n";
  }
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_RETRIEVE_DATA, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_RETRIEVE_DATA\n";
  } else {
    std::cout << "Default value for SQL_ATTR_RETRIEVE_DATA: " << out << "\n";
  }
//status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ROW_NUMBER, &out, 0, NULL);
//  if (status != SQL_SUCCESS) {
//    std::cout << "Not supported: SQL_ATTR_ROW_NUMBER\n";
//  } else {
//    std::cout << "Default value for SQL_ATTR_ROW_NUMBER: " << out << "\n";
//  }
//      CheckError(status, "SQL_ATTR_ROW_NUMBER", conn);
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_SIMULATE_CURSOR, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_SIMULATE_CURSOR\n";
  } else {
    std::cout << "Default value for SQL_ATTR_SIMULATE_CURSOR: " << out << "\n";
  }
      CheckError(status, "SQL_ATTR_SIMULATE_CURSOR", conn);
status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_USE_BOOKMARKS, &out, 0, NULL);
  if (status != SQL_SUCCESS) {
    std::cout << "Not supported: SQL_ATTR_USE_BOOKMARKS\n";
  } else {
    std::cout << "Default value for SQL_ATTR_USE_BOOKMARKS: " << out << "\n";
  }


  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
