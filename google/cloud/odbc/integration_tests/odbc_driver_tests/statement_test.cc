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
                     Schema schema, bool use_ansi = false) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, "SELECT * FROM " + table_name);

  if (use_ansi) {
    status = SQLPrepareA(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  } else {
    status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  }
  CheckError(status, "SQLPrepare", conn, use_ansi);

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

    DescribeCol(conn, col_ptr, i + 1, use_ansi);

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

void ExecDirectWithFetchTest(std::string const in_table_name, bool is_async,
                             bool use_ansi = false) {
  std::string const table_name = kDatasetName + "." + in_table_name;
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);
  table.Create(conn,
               "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)",
               use_ansi);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);
  table.Insert(conn, kSampleData, use_ansi);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  auto const query = "SELECT StringField FROM " + table_name;
  auto results = *FetchDirect(conn, query, 1, is_async, use_ansi);
  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);
  table.Drop(conn, use_ansi);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecDirect) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(InsertDirectStatement(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  // Sleep for 5 secs to avoid rate limit errors from BQ
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  ////////////////
  /// USE ANSI
  ////////////////
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  EXPECT_EQ(InsertDirectStatement(conn, true), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecute) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(InsertStatement(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  // Sleep for 5 secs to avoid rate limit errors from BQ
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  ////////////////
  /// USE ANSI
  ////////////////
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  EXPECT_EQ(InsertStatement(conn, true), SQL_SUCCESS);
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
  // Sleep for 5 secs to avoid rate limit errors from BQ
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  ////////////////
  /// USE ANSI
  ////////////////
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  EXPECT_EQ(InsertStatementWithBindParameter(conn, true), SQL_SUCCESS);

  // We inserted a row using first statement handle.
  // Now we're going to do the same using a new statement handle,
  // but without SQLBindParameter calls.
  // We reuse desc handle instead.

  // Free existing statement handle (within same connection)
  SQLCloseCursor(conn->hstmt);
  status = SQLFreeHandle(SQL_HANDLE_STMT, conn->hstmt);
  CheckError(status, "SQLFreeHandle", conn);

  // Allocate a new statement handle (within same connection)
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
  CheckError(status, "SQLAllocHandle", conn);

  EXPECT_EQ(InsertStatementWithoutBindParameter(conn, true), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLNumParams) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetName + ".ODBC_NUM_PARAMS_TEST";
  auto insert_stmt = "INSERT INTO " + table_name + " VALUES (?, ?, ?)";
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
  ////////////////
  /// USE ANSI
  ////////////////
  auto const table_name_ansi = kDatasetName + ".ODBC_NUM_PARAMS_TEST_ANSI";
  auto const insert_stmt_ansi =
      "INSERT INTO " + table_name_ansi + " VALUES (?, ?, ?)";
  Table table_ansi(table_name_ansi);

  // Create Table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)",
      true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  status =
      SQLPrepareA(conn->hstmt, (SQLCHAR*)insert_stmt_ansi.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn, true);
  status = SQLNumParams(conn->hstmt, &num_params);
  CheckError(status, "SQLNumParams", conn);
  EXPECT_EQ(num_params, 3);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Drop(conn, true);
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

  ////////////////
  /// USE ANSI
  ////////////////
  auto const table_name_ansi =
      kDatasetName + ".ODBC_COLUMN_DESCRIPTION_TEST_ANSI";
  Table table_ansi(table_name_ansi);

  // Create Table
  conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)",
      true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Insert(conn, kSampleData, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckColumnData(conn, table_name_ansi, schema, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Drop(conn, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void FetchDataTest(bool use_bind_col, bool use_ansi = false) {
  auto const table_name = kDatasetName + ".ODBC_CHECK_RESULTS_TEST_" +
                          (use_bind_col ? "true" : "false");
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  if (use_ansi) {
    EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
    table.Create(
        conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)",
        true);
  } else {
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    table.Create(
        conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);
  table.Insert(conn, kSampleData, use_ansi);
  SQLLEN rows_count = 0;
  auto status = SQLRowCount(conn->hstmt, &rows_count);
  CheckError(status, "SQLRowCount", conn);
  EXPECT_EQ(rows_count, kSampleData.size());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  auto const query = "SELECT StringField FROM " + table_name;
  auto results = *FetchResults(conn, query, use_bind_col);

  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);
  table.Drop(conn, use_ansi);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLFetch) { FetchDataTest(true); }
TEST(StatementTest, SQLFetch_Ansi) { FetchDataTest(true, true); }

TEST(StatementTest, SQLFetch_WithoutSQLBindCol) { FetchDataTest(false); }
TEST(StatementTest, SQLFetch_WithoutSQLBindCol_Ansi) {
  FetchDataTest(false, true);
}

TEST(StatementTest, SQLFetch_with_SQLExecDirect) {
  ExecDirectWithFetchTest("ODBC_FETCH_WITH_EXECDIRECT_SYNC_TEST_1", false);
}
TEST(StatementTest, SQLFetch_with_SQLExecDirect_Ansi) {
  ExecDirectWithFetchTest("ODBC_FETCH_WITH_EXECDIRECT_SYNC_TEST_2", false,
                          true);
}

TEST(StatementTest, SQLFetch_with_SQLExecDirectAsync) {
  ExecDirectWithFetchTest("ODBC_FETCH_WITH_EXECDIRECT_ASYNC_TEST_3", true);
}
TEST(StatementTest, SQLFetch_with_SQLExecDirectAsync_Ansi) {
  ExecDirectWithFetchTest("ODBC_FETCH_WITH_EXECDIRECT_ASYNC_TEST_4", true,
                          true);
}

// No ANSI version.
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

  ////////////////
  /// USE ANSI
  ////////////////
  auto const table_name_ansi = kDatasetName + ".ODBC_GET_DATA_TEST_ANSI";
  Table table_ansi(table_name_ansi);

  // Create Table
  conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)",
      true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Insert(conn, kSampleData, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  query = "SELECT StringField FROM " + table_name_ansi;
  results = *FetchResultsWithSqlGetData(conn, query);

  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Drop(conn, true);
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

  ////////////////
  /// USE ANSI
  ////////////////
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLCHAR cursor_name_ansi[kBufferLength] = "INSERT_CURSOR_ANSI",
          cursor_name_ret_ansi[kBufferLength];

  status = SQLSetCursorNameA(conn->hstmt, cursor_name_ansi, kBufferLength);
  CheckError(status, "SQLSetCursorName", conn, true);

  status =
      SQLGetCursorNameA(conn->hstmt, cursor_name_ret_ansi, kBufferLength, NULL);
  CheckError(status, "SQLGetCursorName", conn, true);

  EXPECT_STREQ((char*)cursor_name_ret_ansi, (char*)cursor_name_ansi);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SetAndGet_SQL_ATTR_METADATA_ID) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;

  // Set Connection Attr and then retrieve Statement attr
  SQLULEN metadata_id_dbc = SQL_TRUE;
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_METADATA_ID,
                             (SQLPOINTER)metadata_id_dbc, 0);
  CheckError(status, "SQLSetConnectAttr", conn);
  SQLULEN metadata_id_stmt;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, &metadata_id_stmt,
                          0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(metadata_id_dbc, metadata_id_stmt);

  // Set Statement Attr and then retrieve it from both Statement and Connection
  // attrs
  SQLULEN metadata_id_stmt_set = SQL_FALSE;
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          (SQLPOINTER)metadata_id_stmt_set, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  SQLULEN metadata_id_stmt_get = 0;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          &metadata_id_stmt_get, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);
  metadata_id_dbc = 0;
  status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_METADATA_ID, &metadata_id_dbc,
                             0, NULL);
  CheckError(status, "SQLGetConnectAttr", conn);

  EXPECT_EQ(metadata_id_stmt_set, metadata_id_stmt_get);
  EXPECT_NE(metadata_id_dbc, metadata_id_stmt_set);

  // Create a new statement handle and check this attribute there
  HSTMT new_hstmt;
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &new_hstmt);
  CheckError(status, "SQLAllocHandle", conn);

  SQLULEN metadata_id_stmt_new = 0;
  status = SQLGetStmtAttr(new_hstmt, SQL_ATTR_METADATA_ID,
                          &metadata_id_stmt_new, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(metadata_id_dbc, metadata_id_stmt_new);

  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_STMT, new_hstmt), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, ANSI_SetAndGet_SQL_ATTR_METADATA_ID) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  // Set Connection Attr and then retrieve Statement attr
  SQLULEN metadata_id_dbc = SQL_TRUE;
  status = SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_METADATA_ID,
                              (SQLPOINTER)metadata_id_dbc, 0);
  CheckError(status, "SQLSetConnectAttr", conn, true);
  SQLULEN metadata_id_stmt;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, &metadata_id_stmt,
                          0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(metadata_id_dbc, metadata_id_stmt);

  // Set Statement Attr and then retrieve it from both Statement and Connection
  // attrs
  SQLULEN metadata_id_stmt_set = SQL_FALSE;
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          (SQLPOINTER)metadata_id_stmt_set, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  SQLULEN metadata_id_stmt_get = 0;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          &metadata_id_stmt_get, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);
  metadata_id_dbc = 0;
  status = SQLGetConnectAttrA(conn->hdbc, SQL_ATTR_METADATA_ID,
                              &metadata_id_dbc, 0, NULL);
  CheckError(status, "SQLGetConnectAttr", conn, true);

  EXPECT_EQ(metadata_id_stmt_set, metadata_id_stmt_get);
  EXPECT_NE(metadata_id_dbc, metadata_id_stmt_set);

  // Create a new statement handle and check this attribute there
  HSTMT new_hstmt;
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &new_hstmt);
  CheckError(status, "SQLAllocHandle", conn);

  SQLULEN metadata_id_stmt_new = 0;
  status = SQLGetStmtAttr(new_hstmt, SQL_ATTR_METADATA_ID,
                          &metadata_id_stmt_new, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(metadata_id_dbc, metadata_id_stmt_new);

  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_STMT, new_hstmt), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SetAndGet_SQL_ATTR_ASYNC_ENABLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;

  // Set Connection Attr and then retrieve Statement attr
  SQLULEN async_enable_dbc = SQL_TRUE;
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                             (SQLPOINTER)async_enable_dbc, 0);
  CheckError(status, "SQLSetConnectAttr", conn);
  SQLULEN async_enable_stmt;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          &async_enable_stmt, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(async_enable_dbc, async_enable_stmt);

  // Set Statement Attr and then retrieve it from both Statement and Connection
  // attrs
  SQLULEN async_enable_stmt_set = SQL_FALSE;
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          (SQLPOINTER)async_enable_stmt_set, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  SQLULEN async_enable_stmt_get = 0;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          &async_enable_stmt_get, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);
  async_enable_dbc = 0;
  status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                             &async_enable_dbc, 0, NULL);
  CheckError(status, "SQLGetConnectAttr", conn);

  EXPECT_EQ(async_enable_stmt_set, async_enable_stmt_get);
  EXPECT_NE(async_enable_dbc, async_enable_stmt_set);

  // Create a new statement handle and check this attribute there
  HSTMT new_hstmt;
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &new_hstmt);
  CheckError(status, "SQLAllocHandle", conn);

  SQLULEN metadata_id_stmt_new = 0;
  status = SQLGetStmtAttr(new_hstmt, SQL_ATTR_ASYNC_ENABLE,
                          &metadata_id_stmt_new, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(async_enable_dbc, metadata_id_stmt_new);

  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_STMT, new_hstmt), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, ANSI_SetAndGet_SQL_ATTR_ASYNC_ENABLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  // Set Connection Attr and then retrieve Statement attr
  SQLULEN async_enable_dbc = SQL_TRUE;
  status = SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                              (SQLPOINTER)async_enable_dbc, 0);
  CheckError(status, "SQLSetConnectAttr", conn, true);
  SQLULEN async_enable_stmt;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          &async_enable_stmt, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(async_enable_dbc, async_enable_stmt);

  // Set Statement Attr and then retrieve it from both Statement and Connection
  // attrs
  SQLULEN async_enable_stmt_set = SQL_FALSE;
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          (SQLPOINTER)async_enable_stmt_set, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  SQLULEN async_enable_stmt_get = 0;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          &async_enable_stmt_get, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);
  async_enable_dbc = 0;
  status = SQLGetConnectAttrA(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                              &async_enable_dbc, 0, NULL);
  CheckError(status, "SQLGetConnectAttr", conn, true);

  EXPECT_EQ(async_enable_stmt_set, async_enable_stmt_get);
  EXPECT_NE(async_enable_dbc, async_enable_stmt_set);

  // Create a new statement handle and check this attribute there
  HSTMT new_hstmt;
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &new_hstmt);
  CheckError(status, "SQLAllocHandle", conn);

  SQLULEN metadata_id_stmt_new = 0;
  status = SQLGetStmtAttr(new_hstmt, SQL_ATTR_ASYNC_ENABLE,
                          &metadata_id_stmt_new, 0, NULL);
  CheckError(status, "SQLGetStmtAttr", conn);

  EXPECT_EQ(async_enable_dbc, metadata_id_stmt_new);

  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_STMT, new_hstmt), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
