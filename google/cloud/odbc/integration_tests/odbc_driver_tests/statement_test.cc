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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"

namespace google::cloud::odbc_tests {

using ::google::cloud::odbc_bq_driver_internal::BQDataType;
using google::cloud::odbc_bq_driver_internal::ColumnSchema;
using ::google::cloud::odbc_bq_driver_internal::ResultSet;

#ifdef BQ_DRIVER_INTEGRATION_TESTS
bool const kIsBqDriver = true;
#else
bool const kIsBqDriver = false;
#endif

class StatementParameterizedTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutANSI, StatementParameterizedTest,
                         testing::Values(false, true));

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
  std::string const table_name = kDatasetWithTablePrefix + in_table_name;
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
  table.InsertData(conn, kSampleData, use_ansi);
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
  auto table_name = kDatasetWithTablePrefix + "ODBC_NUM_PARAMS_TEST";
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

  ////////////////
  /// USE ANSI
  ////////////////
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  status = SQLPrepareA(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn, true);
  status = SQLNumParams(conn->hstmt, &num_params);
  CheckError(status, "SQLNumParams", conn);
  EXPECT_EQ(num_params, 3);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table.Drop(conn, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLNumParamsAndSQLBindParam) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name =
      kDatasetWithTablePrefix + "ODBC_NUM_PARAMS_AND_BIND_PARAM_TEST";
  auto insert_stmt = "INSERT INTO " + table_name + " VALUES (?, ?, ?)";
  Table table(table_name);

  // Create Table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Prepare statement
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLSMALLINT num_params;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  // Bind parameter with number 10
  SQLUSMALLINT param_number = 10;
  SQLINTEGER param_val = 30;
  status = SQLBindParameter(conn->hstmt, param_number, SQL_PARAM_INPUT,
                            SQL_C_CHAR, SQL_CHAR, 10, 20, &param_val, 40, NULL);
  CheckError(status, "SQLBindParameter", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);
  SQLSMALLINT count = 0;
  status = SQLGetDescField(conn->ipd, 0, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(count, param_number);
  // Check SQLNumParams returns count of parameters from SQLPrepare
  status = SQLNumParams(conn->hstmt, &num_params);
  CheckError(status, "SQLNumParams", conn);
  EXPECT_EQ(num_params, 3);
  EXPECT_NE(num_params, count);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table.Drop(conn, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLDescribeCol) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_COLUMN_DESCRIPTION_TEST";
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
  table.InsertData(conn, kSampleData);
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
      kDatasetWithTablePrefix + "ODBC_COLUMN_DESCRIPTION_TEST_ANSI";
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
  table_ansi.InsertData(conn, kSampleData, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckColumnData(conn, table_name_ansi, schema, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  table_ansi.Drop(conn, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void FetchDataTest(bool use_bind_col, bool use_ansi = false) {
  auto const table_name = kDatasetWithTablePrefix + "ODBC_CHECK_RESULTS_TEST_" +
                          (use_ansi ? "ANSI_" : "NON_ANSI") +
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
  table.InsertData(conn, kSampleData, use_ansi);
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
  auto const table_name = kDatasetWithTablePrefix + "ODBC_SCROLL_RESULTS_TEST";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertData(conn, kSampleData);
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
  auto const table_name = kDatasetWithTablePrefix + "ODBC_GET_DATA_TEST";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertData(conn, kSampleData);
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
  auto const table_name_ansi =
      kDatasetWithTablePrefix + "ODBC_GET_DATA_TEST_ANSI";
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
  table_ansi.InsertData(conn, kSampleData, true);
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
  auto const table_name = kDatasetWithTablePrefix + "ODBC_PUT_DATA_TEST";
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

TEST(StatementTest, FetchDirectRowWise) {
  std::string const table_name = kDatasetWithTablePrefix + "ROW_WISE_FETCH";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  table.Create(conn,
               "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)",
               false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  table.InsertData(conn, kSampleData, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  auto const query = "SELECT StringField, IntegerField FROM " + table_name;
  auto results = *FetchDirectRowWise(conn, query, 1);
  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  table.Drop(conn, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

TEST_P(StatementParameterizedTest, FreeExplicitDescriptor) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;

  // Create explicit descriptor and set it to a statement handle
  status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->apd);
  CheckError(status, "SQLAllocHandle", conn);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, conn->apd, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  // Free explicit descriptor
  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_DESC, conn->apd), SQL_SUCCESS);

  // Check if statement handle reverted to use implicit descriptor
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr", conn);
  SQLSMALLINT alloc_type = 0;
  GetDescField(conn->apd, 0, SQL_DESC_ALLOC_TYPE, &alloc_type, 0, NULL,
               GetParam());

  EXPECT_EQ(SQL_DESC_ALLOC_AUTO, alloc_type);

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

TEST_P(StatementParameterizedTest, SetAndGetStatementDescriptorAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  // Set attribute using statement handle
  SQLULEN arr_size = 5;
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                          (SQLPOINTER)arr_size, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE)", conn);

  // Get attribute using statement handle
  SQLULEN arr_size_stmt_handle = 0;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                       &arr_size_stmt_handle, 0, NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE)", conn);

  EXPECT_EQ(arr_size, arr_size_stmt_handle);

  // Get descriptor using statement handle
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &conn->ard, 0, NULL,
                       GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);

  // Get attribute using descriptor handle
  SQLULEN arr_size_desc_handle = 0;
  GetDescField(conn->ard, 0, SQL_DESC_ARRAY_SIZE, &arr_size_desc_handle, 0,
               NULL, GetParam());

  EXPECT_EQ(arr_size, arr_size_desc_handle);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(StatementParameterizedTest, SetAndGetDescriptorAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  // Get descriptor using statement handle
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);

  // Set attribute using descriptor handle
  SQLULEN arr_size_desc_handle = 5;
  status = SQLSetDescField(conn->apd, 0, SQL_DESC_ARRAY_SIZE,
                           (SQLPOINTER)arr_size_desc_handle, 0);
  CheckError(status, "SQLSetDescField(SQL_DESC_ARRAY_SIZE)", conn);

  // Get attribute using statement handle
  SQLULEN arr_size_stmt_handle = 0;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_PARAMSET_SIZE,
                       &arr_size_stmt_handle, 0, NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE)", conn);

  EXPECT_EQ(arr_size_desc_handle, arr_size_stmt_handle);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(StatementParameterizedTest,
       SetAndGetStatementAttributes_SQL_ATTR_NOSCAN) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_NOSCAN,
                          (SQLPOINTER)SQL_NOSCAN_ON, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_NOSCAN)", conn);
  SQLULEN no_scan = 0;
  status =
      GetStmtAttr(conn->hstmt, SQL_ATTR_NOSCAN, &no_scan, 0, NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_NOSCAN)", conn);

  EXPECT_EQ(SQL_NOSCAN_ON, no_scan);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(StatementParameterizedTest,
       SetAndGetStatementAttributes_SQL_ATTR_ASYNC_ENABLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_ASYNC_ENABLE)", conn);

  SQLULEN async = 0;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE, &async, 0, NULL,
                       GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_ASYNC_ENABLE)", conn);

  EXPECT_EQ(SQL_ASYNC_ENABLE_ON, async);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(StatementParameterizedTest,
       SetAndGetStatementAttributes_SQL_ATTR_MAX_LENGTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  SQLULEN expected = 5;
  status =
      SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_LENGTH, (SQLPOINTER)expected, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_MAX_LENGTH)", conn);

  SQLULEN actual = 0;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_MAX_LENGTH, &actual, 0, NULL,
                       GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_MAX_LENGTH)", conn);

  EXPECT_EQ(expected, actual);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, FailsToSet_SQL_ATTR_ROW_NUMBER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROW_NUMBER, (SQLPOINTER)5, 0);
  EXPECT_EQ(SQL_ERROR, status);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(StatementParameterizedTest, SetAndGetExplicitDescriptor) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  // Get descriptor using statement handle and check it's implicit
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &conn->ard, 0, NULL,
                       GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);
  SQLSMALLINT alloc_type = 0;
  GetDescField(conn->ard, 0, SQL_DESC_ALLOC_TYPE, &alloc_type, 0, NULL,
               GetParam());

  EXPECT_EQ(SQL_DESC_ALLOC_AUTO, alloc_type);

  // Set descriptor field to check it after
  SQLULEN arr_size_implicit = 45;
  status = SQLSetDescField(conn->ard, 0, SQL_DESC_ARRAY_SIZE,
                           (SQLPOINTER)arr_size_implicit, 0);
  CheckError(status, "SQLSetDescField(SQL_DESC_ARRAY_SIZE)", conn);

  // Create an explicit descriptor handle
  HSTMT desc_expl = nullptr;
  status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &desc_expl);
  CheckError(status, "SQLAllocHandle", conn);

  // Set explicit descriptor as statement attribute
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, desc_expl, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);

  // Check explicit descriptor is set
  HSTMT desc_expl_new = nullptr;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &desc_expl_new, 0,
                       NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);
  alloc_type = 0;
  GetDescField(desc_expl_new, 0, SQL_DESC_ALLOC_TYPE, &alloc_type, 0, NULL,
               GetParam());

  EXPECT_EQ(SQL_DESC_ALLOC_USER, alloc_type);
  EXPECT_EQ(desc_expl, desc_expl_new);

  // Set a header field of explicit descriptor
  SQLULEN arr_size_explicit = 5;
  status = SQLSetDescField(desc_expl, 0, SQL_DESC_ARRAY_SIZE,
                           (SQLPOINTER)arr_size_explicit, 0);
  CheckError(status, "SQLSetDescField(SQL_DESC_ARRAY_SIZE)", conn);

  // Get attribute of an explicit descriptor using statement handle
  SQLULEN arr_size_stmt_handle = 0;
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                       &arr_size_stmt_handle, 0, NULL, GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_ROW_ARRAY_SIZE)", conn);

  EXPECT_EQ(arr_size_explicit, arr_size_stmt_handle);

  // Dissociate explicit descriptor from a statement handle
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, NULL, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);

  // Get descriptor using statement handle and check it's implicit again
  status = GetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &conn->ard, 0, NULL,
                       GetParam());
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);
  alloc_type = 0;
  GetDescField(conn->ard, 0, SQL_DESC_ALLOC_TYPE, &alloc_type, 0, NULL,
               GetParam());
  SQLULEN arr_size_new = 0;
  GetDescField(conn->ard, 0, SQL_DESC_ARRAY_SIZE, &arr_size_new, 0, NULL,
               GetParam());

  EXPECT_EQ(SQL_DESC_ALLOC_AUTO, alloc_type);
  EXPECT_EQ(arr_size_implicit, arr_size_new);

  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_DESC, desc_expl), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLPrepare) {
  auto conn = std::make_shared<ODBCHandles>();

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  if (kIsBqDriver) {
    // Cast hstmt to StatementHandle*
    auto stmt_handle =
        static_cast<google::cloud::odbc_bq_driver_internal::StatementHandle*>(
            conn->hstmt);

    // Retrieve the result set
    ResultSet const& result_set = stmt_handle->GetResultSet();

    ASSERT_EQ(result_set.row_schema.size(), 1);

    ColumnSchema const& column = result_set.row_schema[0];
    EXPECT_EQ(column.col_index, 0);
    EXPECT_EQ(column.col_type, BQDataType::kInt64);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
