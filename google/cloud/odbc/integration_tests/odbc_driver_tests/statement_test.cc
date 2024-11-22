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
#ifdef BQ_DRIVER_INTEGRATION_TESTS
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#endif  // BQ_DRIVER_INTEGRATION_TESTS
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "absl/strings/match.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_tests {

#ifdef BQ_DRIVER_INTEGRATION_TESTS
using google::cloud::odbc_bq_driver_internal::BQDataType;
using google::cloud::odbc_bq_driver_internal::ColumnSchema;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
#endif  // BQ_DRIVER_INTEGRATION_TESTS
using ::testing::StartsWith;

class StatementParameterizedTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutANSI, StatementParameterizedTest,
                         testing::Values(false, true));

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
StdRows const kSampleData{
    {"Test String 1", 1, 1.1},      {.int_field = 237, .float_field = 2.22},
    {"Test String 3", NULL, 3.333}, {"Test String 4", 49},
    {"Test String 5", 53, 5},       {"Test String 6", 698, 0.31},
    {"Test String 7", 12, 71.6},    {"Test String 8", 83, 8.8},
};

StdUnicodeRows const kUnicodeSampleData{
    {1, L"हिंदी", L"中国人"},
    {2, L"नमस्ते", L"你好"},
    {3, L"परीक्षण", L"测试"},
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
    EXPECT_TRUE(AreSqlAndBqTypesSame(col_ptr->data_type, schema[i].type));
    EXPECT_EQ(col_ptr->nullable, SQL_NULLABLE);
  }
}

// Verify if the inserted data(<input_data>) is the same as the data fetched
// col-wise Note: This doesn't verify the integrity of the fetched rows
void VerifyColumnWiseUnicodeResults(StdUnicodeRows input_data,
                                    Results col_wise_data,
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
    if (col_name.compare("Hindi")) {
      for (auto data : input_data) {
        std::string dataStr = Utf16ToUtf8(data.str_field2);
        input_col_values.emplace_back(dataStr);
      }
    } else if (col_name.compare("Chinese")) {
      for (auto data : input_data) {
        std::string dataStr = Utf16ToUtf8(data.str_field1);
        input_col_values.emplace_back(dataStr);
      }
    }
    sort(input_col_values.begin(), input_col_values.end(), str_comparison);

    // Check if the sorted inserted and returned vectors have same values
    EXPECT_EQ(ret_col_values.size(), input_col_values.size());
    for (int i = 0; i < ret_col_values.size(); i++) {
      EXPECT_STREQ(ret_col_values[i].c_str(), input_col_values[i].c_str());
    }
  }
}

TEST(StatementTest, SQLFetch_Unicode) {
  std::string const table_name = kDatasetWithTablePrefix + "ODBC_UNICODE_TEST";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn, "(IntegerField INTEGER, Hindi STRING, Chinese STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertUnicodeData(conn, kUnicodeSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  auto const query = "SELECT Hindi, Chinese FROM " + table_name;
  auto results = *FetchResults(conn, query, true);
  VerifyColumnWiseUnicodeResults(kUnicodeSampleData, results,
                                 std::vector<std::string>());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
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
      EXPECT_EQ(ret_col_values[i], input_col_values[i]) << " at index: " << i;
    }
  }
}

#ifndef BQ_DRIVER_INTEGRATION_TESTS

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
#ifndef _WIN32
  // TODO(b/357795885):Handle SQLDescribeCol Api Invalid Output WRT SIMBA(WIN).
  auto results = *FetchDirect(conn, query, 1, is_async, use_ansi);
  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());
#endif  // _WIN32
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

  Schema schema{{"StringField", "STRING"},
                {"IntegerField", "INT64"},
                {"FloatField", "FLOAT64"}};

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
  Schema schema{{"StringField1", "STRING"},
                {"StringField2", "STRING"},
                {"StringField3", "STRING"}};

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

#endif  // BQ_DRIVER_INTEGRATION_TESTS

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

TEST(StatementTest, SQLSetCursorNameW) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  std::wstring cursor_name = L"INSERT_CURSOR_WIDE";
  SQLCHAR cursor_name_ret[kBufferLength];

  std::vector<SQLWCHAR> sqlWStr(cursor_name.begin(), cursor_name.end());
  sqlWStr.emplace_back(L'\0');

  auto status = SQLSetCursorNameW(conn->hstmt, sqlWStr.data(), sqlWStr.size());
  CheckError(status, "SQLSetCursorNameW", conn, true);

  status = SQLGetCursorNameW(conn->hstmt, (SQLWCHAR*)cursor_name_ret,
                             kBufferLength, NULL);
  CheckError(status, "SQLGetCursorNameW", conn, true);

  std::string expected = "INSERT_CURSOR_WIDE";
  std::string actual = ConvertSQLWCHARToString(
      reinterpret_cast<SQLWCHAR*>(cursor_name_ret), expected.size());
  EXPECT_STREQ(actual.data(), expected.data());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLGetCursorNameW) {
  SQLSMALLINT curNameLen;

  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLCHAR cursor_name_ret[kBufferLength];

  std::string query = "SELECT 1;";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  status = SQLGetCursorNameW(conn->hstmt, (SQLWCHAR*)cursor_name_ret,
                             kBufferLength, NULL);
  CheckError(status, "SQLGetCursorNameW", conn);

  std::string actual = ConvertSQLWCHARToString(
      reinterpret_cast<SQLWCHAR*>(cursor_name_ret), NULL);
  EXPECT_THAT(actual, StartsWith("SQL_CUR"));

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLGetCursorName) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLCHAR cursor_name_ret[kBufferLength];

  std::string query = "SELECT 1;";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  status = SQLGetCursorName(conn->hstmt, cursor_name_ret, kBufferLength, NULL);
  CheckError(status, "SQLGetCursorName", conn);

  std::string actual = reinterpret_cast<char*>(cursor_name_ret);
  EXPECT_THAT(actual, StartsWith("SQL_CUR"));

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, FetchRowWise) {
  std::string const table_name = kDatasetWithTablePrefix + "ROW_WISE_FETCH";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  table.InsertData(conn, kSampleData, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  // TODO(#14): Add integer and floating point fields too
  auto const query = "SELECT StringField, IntegerField FROM " + table_name;
  auto results = *FetchRowWise(conn, query, 1);
  VerifyColumnWiseResults(kSampleData, results, std::vector<std::string>());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, false), SQL_SUCCESS);
  table.Drop(conn, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, RollBackTransaction) {
  std::string const table_name =
      kDatasetWithTablePrefix + "_RollBackTransaction";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  // Insert some data to the table
  StdRow row = {"a1", 0, 0};
  table.InsertData(conn, {row}, false, true);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kSessionEnabledConnectionString, conn), SQL_SUCCESS);
  SQLUINTEGER autocommit = SQL_AUTOCOMMIT_OFF;

  auto status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_AUTOCOMMIT,
                                  (SQLPOINTER)autocommit, 0);

  // Try to update data in the table
  std::string update_stmt = "UPDATE " + table_name +
                            " SET StringField='b1' WHERE StringField = 'a1';";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)update_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare(update)", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute(update)", conn);
  // Check that the data was updated
  auto const query = "SELECT StringField FROM " + table_name;
  auto results = *FetchResults(conn, query, true);
  VerifyColumnWiseResults({{"b1", 0, 0}}, results, std::vector<std::string>());

  // ROLLBACK TRANSACTION
  status = SQLEndTran(SQL_HANDLE_DBC, conn->hdbc, SQL_ROLLBACK);
  CheckError(status, "SQLEndTran(after select)", conn);

  // Check that transaction was rolled back and the data has initial value
  results = *FetchResults(conn, query, true);
  VerifyColumnWiseResults({{"a1", 0, 0}}, results, std::vector<std::string>());

  // COMMIT TRANSACTION
  status = SQLEndTran(SQL_HANDLE_DBC, conn->hdbc, SQL_COMMIT);
  CheckError(status, "SQLEndTran(after select)", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void PrepareAndCheckQuery(std::string const& query,
                          std::shared_ptr<ODBCHandles> conn,
                          int expected_param_count,
                          std::string const& expected_param_type = "",
                          std::string const& expected_param_name = "") {
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);
// Driver manager does not expect the statement handle to be modified since
// its an input parameter. It creates its own structure from the statement
// handle and sends it to the driver hence any internal state that the
// driver sets in the input parameter will not be propagated back to the app.
// This is probably the reason why Simba does not do this as well.
#ifndef DRIVER_MANAGER_TESTING_ENABLED
#ifdef BQ_DRIVER_INTEGRATION_TESTS
  auto stmt_handle = static_cast<StatementHandle*>(conn->hstmt);
  EXPECT_EQ(stmt_handle->GetStmtState(), StmtStates::kStatementPrepared);
  EXPECT_EQ(stmt_handle->GetQueryParameters().size(), expected_param_count);

  if (expected_param_count > 0) {
    EXPECT_EQ(stmt_handle->GetQueryParameters().at(0).parameter_type.type,
              expected_param_type);
    if (!expected_param_name.empty()) {
      EXPECT_EQ(stmt_handle->GetQueryParameters().at(0).name,
                expected_param_name);
    }
  }
#endif  // BQ_DRIVER_INTEGRATION_TESTS
#endif  // DRIVER_MANAGER_TESTING_ENABLED
}

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

TEST_P(StatementParameterizedTest,
       SetAndGetStatementDescriptorAttributes_Wide) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  // Set attribute using statement handle
  SQLULEN arr_size = 5;
  status = SQLSetStmtAttrW(conn->hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                           (SQLPOINTER)arr_size, 0);
  CheckError(status, "SQLSetStmtAttrW(SQL_ATTR_ROW_ARRAY_SIZE)", conn);

  // Get attribute using statement handle
  SQLULEN arr_size_stmt_handle = 0;
  status = SQLGetStmtAttrW(conn->hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                           &arr_size_stmt_handle, 0, NULL);
  CheckError(status, "SQLGetStmtAttrW(SQL_ATTR_ROW_ARRAY_SIZE)", conn);

  EXPECT_EQ(arr_size, arr_size_stmt_handle);

  // Get descriptor using statement handle
  status =
      SQLGetStmtAttrW(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &conn->ard, 0, NULL);
  CheckError(status, "SQLGetStmtAttrW(SQL_ATTR_APP_ROW_DESC)", conn);

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

TEST(StatementTest, Get_SQL_ATTR_ROW_NUMBER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status;

  // Returns 1 column with 2 rows
  std::string query =
      "SELECT  1 AS col1\n"
      "UNION ALL\n"
      "SELECT  2 AS col1";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.length());
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  SQLULEN row_number = 0;
  // Returns error if fetching wasn't started
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ROW_NUMBER, &row_number, 0, nullptr);
  EXPECT_EQ(SQL_ERROR, status);

  // Fetching rows
  for (int i = 1; i <= 2; i++) {
    status = SQLFetch(conn->hstmt);
    CheckError(status, "SQLFetch", conn);

    row_number = 0;
    status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ROW_NUMBER, &row_number, 0,
                            nullptr);
    CheckError(status, "SQLGetStmtAttr", conn);
    EXPECT_EQ(i, row_number);
  }

  status = SQLFetch(conn->hstmt);
  EXPECT_EQ(SQL_NO_DATA, status);

  row_number = 0;
  // Returns error after fetching is over
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_ROW_NUMBER, &row_number, 0, nullptr);
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

TEST(SQLPrepare, SimpleStatementTest) {
  auto conn = std::make_shared<ODBCHandles>();

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

// Driver manager does not expect the statement handle to be modified since
// its an input parameter. It creates its own structure from the statement
// handle and sends it to the driver hence any internal state that the
// driver sets in the input parameter will not be propagated back to the app.
// This is probably the reason why Simba does not do this as well.
#ifndef DRIVER_MANAGER_TESTING_ENABLED
#ifdef BQ_DRIVER_INTEGRATION_TESTS
  // Cast hstmt to StatementHandle*
  auto stmt_handle = static_cast<StatementHandle*>(conn->hstmt);
  EXPECT_EQ(stmt_handle->GetStmtState(), StmtStates::kStatementPrepared);
  // Retrieve the result set
  ResultSet const& result_set = stmt_handle->GetResultSet();

  ASSERT_EQ(result_set.row_schema.size(), 1);

  ColumnSchema const& column = result_set.row_schema[0];
  EXPECT_EQ(column.col_index, 0);
  EXPECT_EQ(column.col_type, BQDataType::kInt64);
#endif  // BQ_DRIVER_INTEGRATION_TESTS
#endif  // DRIVER_MANAGER_TESTING_ENABLED

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLPrepare, StatementFailure) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select * from NON_EXISTENT_TABLE";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  EXPECT_EQ(SQL_ERROR, status);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLPrepare, InsertQuery) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query =
      "INSERT INTO INTEGRATION_TESTS.Test_Table VALUES(4, 'Alice', 28)";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

// Driver manager does not expect the statement handle to be modified since
// its an input parameter. It creates its own structure from the statement
// handle and sends it to the driver hence any internal state that the
// driver sets in the input parameter will not be propagated back to the app.
// This is probably the reason why Simba does not do this as well.
#ifndef DRIVER_MANAGER_TESTING_ENABLED
#ifdef BQ_DRIVER_INTEGRATION_TESTS
  // Cast hstmt to StatementHandle*
  auto stmt_handle = static_cast<StatementHandle*>(conn->hstmt);

  EXPECT_EQ(stmt_handle->GetStmtState(), StmtStates::kStatementPrepared);

  // Retrieve the result set
  ResultSet const& result_set = stmt_handle->GetResultSet();

  ASSERT_EQ(result_set.row_schema.size(), 3);

  ColumnSchema const& column = result_set.row_schema[0];
  EXPECT_EQ(column.col_index, 0);
  EXPECT_EQ(column.col_type, BQDataType::kInt64);

  ColumnSchema const& column2 = result_set.row_schema[1];
  EXPECT_EQ(column2.col_index, 1);
  EXPECT_EQ(column2.col_type, BQDataType::kString);

  ColumnSchema const& column3 = result_set.row_schema[2];
  EXPECT_EQ(column3.col_index, 2);
  EXPECT_EQ(column3.col_type, BQDataType::kInt64);
#endif  // BQ_DRIVER_INTEGRATION_TESTS
#endif  // DRIVER_MANAGER_TESTING_ENABLED
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLPrepare, ParametrizedQuery) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);

  PrepareAndCheckQuery("SELECT * from INTEGRATION_TESTS.Test_Table where id=1",
                       conn, 0);
  PrepareAndCheckQuery("SELECT * from INTEGRATION_TESTS.Test_Table where id=?",
                       conn, 1, "INT64");
#ifdef BQ_DRIVER_INTEGRATION_TESTS
  PrepareAndCheckQuery(
      "SELECT * from INTEGRATION_TESTS.Test_Table where id=@var", conn, 1,
      "INT64", "var");
#endif

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLPrepare, ValidateIpdDescForSimpleStatement) {
  auto conn = std::make_shared<ODBCHandles>();

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);

  SQLSMALLINT count = 0;
  status = SQLGetDescField(conn->ipd, 1, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(0, count);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLPrepare, ValidateIpdDescForParameterQuery) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  PrepareAndCheckQuery("SELECT * from INTEGRATION_TESTS.Test_Table where id=?",
                       conn, 1, "INT64");

  auto status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);

  SQLSMALLINT count = 0;
  status = SQLGetDescField(conn->ipd, 1, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(1, count);

  SQLINTEGER str_len = 0;
  SQLSMALLINT out_concise_c_Type;
  status = SQLGetDescField(conn->ipd, 1, SQL_DESC_CONCISE_TYPE,
                           &out_concise_c_Type, 0, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_CONCISE_TYPE)", conn);
  EXPECT_EQ(SQL_BIGINT, out_concise_c_Type);

  SQLSMALLINT out_nullable;
  status =
      SQLGetDescField(conn->ipd, 1, SQL_DESC_NULLABLE, &out_nullable, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_NULLABLE)", conn);
  EXPECT_EQ(SQL_NULLABLE, out_nullable);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLNumResultCols, ValidStatementWithResultSet) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "SELECT 1";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT columnCount;
  status = SQLNumResultCols(conn->hstmt, &columnCount);
  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(columnCount, 1);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLNumResultCols, ValidateStatement) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "SELECT id,name from INTEGRATION_TESTS.Test_Table";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT columnCount;
  status = SQLNumResultCols(conn->hstmt, &columnCount);
  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(columnCount, 2);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLNumResultCols, CheckColumns) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "SELECT * from INTEGRATION_TESTS.Test_Table";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT columnCount;
  status = SQLNumResultCols(conn->hstmt, &columnCount);
  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(columnCount, 3);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void GetColumnCount(std::shared_ptr<ODBCHandles> conn, std::string query,
                    SQLSMALLINT* colCount) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);
  SQLSMALLINT numCols;
  status = SQLNumResultCols(conn->hstmt, &numCols);
  CheckError(status, "SQLNumResultCols", conn);
  *colCount = numCols;
}

TEST(SQLNumResultCols, CheckColumnCount) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  auto const table_name =
      kDatasetWithTablePrefix +
      "ODBC_INSERT_PARAMS_TEST_SQL_SQLNumResultCols_ColumnCount";
  char insert_stmt[kBufferLength];
  Table table(table_name);
  table.CreateWithPrepare(conn,
                          "(Index INTEGER, StringField STRING, IntegerField "
                          "INTEGER, FloatField FLOAT64)");
  SQLSMALLINT colCount = 0;

  auto query = "SELECT Index FROM " + table_name;
  GetColumnCount(conn, query, &colCount);
  EXPECT_EQ(colCount, 1);

  query = "SELECT Index,StringField  FROM " + table_name;
  GetColumnCount(conn, query, &colCount);
  EXPECT_EQ(colCount, 2);

  query = "SELECT Index,FloatField  FROM " + table_name;
  GetColumnCount(conn, query, &colCount);
  EXPECT_EQ(colCount, 2);

  query = "SELECT StringField,FloatField,IntegerField  FROM " + table_name;
  GetColumnCount(conn, query, &colCount);
  EXPECT_EQ(colCount, 3);

  query = "SELECT * FROM " + table_name;
  GetColumnCount(conn, query, &colCount);
  EXPECT_EQ(colCount, 4);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLDescribeColumn, ValidateColumnDetails) {
  auto const table_name = "INTEGRATION_TESTS.Test_Table";
  Table table(table_name);

  Schema schema{{"id", "INT64"}, {"name", "STRING"}, {"age", "INT64"}};

  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, "SELECT * FROM INTEGRATION_TESTS.Test_Table");

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
    EXPECT_TRUE(AreSqlAndBqTypesSame(col_ptr->data_type, schema[i].type));
    EXPECT_EQ(col_ptr->nullable, SQL_NULLABLE);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLPrepare, SimpleStatementTest_SQL_NTS) {
  auto conn = std::make_shared<ODBCHandles>();

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 123\0";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
// Driver manager does not expect the statement handle to be modified since
// its an input parameter. It creates its own structure from the statement
// handle and sends it to the driver hence any internal state that the
// driver sets in the input parameter will not be propagated back to the app.
// This is probably the reason why Simba does not do this as well.
#ifndef DRIVER_MANAGER_TESTING_ENABLED
#ifdef BQ_DRIVER_INTEGRATION_TESTS
  // Cast hstmt to StatementHandle*
  auto stmt_handle = static_cast<StatementHandle*>(conn->hstmt);

  EXPECT_EQ(stmt_handle->GetStmtState(), StmtStates::kStatementPrepared);
  EXPECT_EQ(stmt_handle->GetQueryString(), query);

#endif  // BQ_DRIVER_INTEGRATION_TESTS
#endif  // DRIVER_MANAGER_TESTING_ENABLED
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLPrepare, ValidateIrdDescriptor) {
  auto const table_name =
      kDatasetWithTablePrefix + "ValidateIrdDescriptor_TEST";
  Table table(table_name);

  // Create Table and insert data
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, getSchemaStr(kFullSchema));

  auto select_stmt = "SELECT * FROM " + table_name;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)select_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);

  SQLSMALLINT count = 0;
  status = SQLGetDescField(conn->ird, 0, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(kFullSchema.size(), count);

  // Check each column
  for (SQLSMALLINT i = 1; i <= count; i++) {
    SQLSMALLINT nullable, concise_type, desc_type, desc_precision, desc_scale;
    SQLULEN length, column_size;
    SQLCHAR column_name[256];
    SQLINTEGER str_len;

    status =
        SQLGetDescField(conn->ird, i, SQL_DESC_NULLABLE, &nullable, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_NULLABLE)", conn);

    status = SQLGetDescField(conn->ird, i, SQL_DESC_CONCISE_TYPE, &concise_type,
                             0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_CONCISE_TYPE)", conn);

    status = SQLGetDescField(conn->ird, i, SQL_DESC_TYPE, &desc_type, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_TYPE)", conn);

    status = SQLGetDescField(conn->ird, i, SQL_DESC_NAME, column_name,
                             sizeof(column_name), &str_len);
    CheckError(status, "SQLGetDescField(SQL_DESC_NAME)", conn);

    status = SQLGetDescField(conn->ird, i, SQL_DESC_LENGTH, &length, 0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);

    status = SQLGetDescField(conn->ird, i, SQL_DESC_PRECISION, &desc_precision,
                             0, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_PRECISION)", conn);

    status =
        SQLGetDescField(conn->ird, i, SQL_DESC_SCALE, &desc_scale, 0, NULL);

    std::string bq_type = kFullSchema[i - 1].type;
    std::string col_name = kFullSchema[i - 1].name;

    EXPECT_STREQ(col_name.c_str(), (char*)column_name);

    TypeInfoRow type_info =
        kSqlToBqDataTypes.at(concise_type).at(SanitizeBQColType(bq_type));

    if (bq_type == "DATE" || bq_type == "TIME" || bq_type == "TIMESTAMP" ||
        bq_type == "DATETIME") {
      EXPECT_EQ(desc_type, SQL_DATETIME);
    } else {
      EXPECT_EQ(concise_type, desc_type);
    }
    EXPECT_EQ(nullable, type_info.nullable);
    // Specific checks for date and time types
    if (bq_type == "DATE") {
      EXPECT_EQ(concise_type, SQL_TYPE_DATE);
      EXPECT_EQ(length, 10);
      EXPECT_EQ(desc_precision, 0);
      EXPECT_EQ(desc_scale, 0);
    } else if (bq_type == "TIME") {
      EXPECT_EQ(concise_type, SQL_TYPE_TIME);
      EXPECT_EQ(length, 15);
      EXPECT_EQ(desc_precision, 6);
      EXPECT_EQ(desc_scale, 6);
    } else if (bq_type == "TIMESTAMP" || bq_type == "DATETIME") {
      EXPECT_EQ(concise_type, SQL_TYPE_TIMESTAMP);
      EXPECT_EQ(length, 26);
      EXPECT_EQ(desc_precision, 6);

      EXPECT_EQ(desc_scale, 6);
    } else if (bq_type == "FLOAT64") {
      EXPECT_EQ(length, 15);
    } else {
      EXPECT_EQ(length, type_info.col_size);
      if (type_info.interval_precision != NULL) {
        EXPECT_EQ(desc_precision, type_info.interval_precision);
      }
      EXPECT_EQ(desc_scale, type_info.maximum_scale);
    }
  }

  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLCloseCursor, CloseCursorAndExecuteAgain) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute_1", conn);

  status = SQLCloseCursor(conn->hstmt);
  CheckError(status, "SQLCloseCursor_1", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute_2", conn);

  status = SQLCloseCursor(conn->hstmt);
  CheckError(status, "SQLCloseCursor_2", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLCloseCursor, CloseCursorWhileEndingTransaction) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kSessionEnabledConnectionString, conn), SQL_SUCCESS);
  SQLUINTEGER autocommit = SQL_AUTOCOMMIT_OFF;
  auto status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_AUTOCOMMIT,
                                  (SQLPOINTER)autocommit, 0);

  std::string query = "Select 1";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute_1", conn);

  status = SQLEndTran(SQL_HANDLE_DBC, conn->hdbc, SQL_ROLLBACK);
  CheckError(status, "SQLEndTran", conn);

  // Check that there is no result set after transaction is ended
  status = SQLFetch(conn->hstmt);
  EXPECT_EQ(SQL_ERROR, status);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// Prepare is async.
TEST(SQLCancel, Prepare_CancelAsync_StillExecuting) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ExponentialBackoffPolicy backoff(std::chrono::milliseconds(10),
                                   std::chrono::milliseconds(100), 2);

  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                               (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::string query = "Select 1";
  // Prepare processed asynchronously.
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);

  if (SQL_SUCCEEDED(status)) {
    // We can't cancel an operation that is not in the process of executing.
    CheckError(status, "SQLPrepare", conn);
  } else if (status == SQL_STILL_EXECUTING) {
    // Cancel the operation
    status = SQLCancel(conn->hstmt);
    CheckError(status, "SQLCancel", conn);
    // Call SQLPrepare till completion
    status = PollODBC(SQLPrepare, backoff, conn->hstmt, (SQLCHAR*)query.c_str(),
                      SQL_NTS);
    if (SQL_SUCCEEDED(status)) {
      // Operation could not be cancelled. This is not an error as there could
      // be a race condition where execute completed before cancel could cancel
      // the operation.
      CheckError(status, "SQLPrepare", conn);
    } else {
      // Per spec, Make sure SQLState is HY008 and Message is 'Operation
      // canceled'.
      std::string error;
      ASSERT_EQ(SQL_SUCCESS,
                GetCancelErrorDetails("SQLPrepare", conn->hstmt, error));
      ASSERT_TRUE(absl::StrContains(error, "HY008"))
          << "SQLPrepare failed with unexpected error: " << error;
      ASSERT_TRUE(absl::StrContains(error, "Operation canceled"))
          << "SQLPrepare failed with unexpected error: " << error;
    }
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// Prepare is synchronous. Execute is async.
TEST(SQLCancel, Execute_CancelAsync_StillExecuting) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ExponentialBackoffPolicy backoff(std::chrono::milliseconds(10),
                                   std::chrono::milliseconds(100), 2);

  std::string query = "Select 1";
  // Prepare will be processed synchronously.
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  // Execute should process asynchronously.
  status = SQLExecute(conn->hstmt);

  if (SQL_SUCCEEDED(status)) {
    // We can't cancel an operation that is not in the process of executing.
    CheckError(status, "SQLPrepare", conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  } else if (status == SQL_STILL_EXECUTING) {
    // Cancel the operation
    status = SQLCancel(conn->hstmt);
    CheckError(status, "SQLCancel", conn);
    // Call SQLExecute till completion
    status = PollODBC(SQLExecute, backoff, conn->hstmt);
    if (SQL_SUCCEEDED(status)) {
      // Operation could not be cancelled. This is not an error as there could
      // be a race condition where execute completed before cancel could cancel
      // the operation.
      CheckError(status, "SQLExecute", conn);
      EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
    } else {
      // Per spec, Make sure SQLState is HY008 and Message is 'Operation
      // canceled'.
      std::string error;
      ASSERT_EQ(SQL_SUCCESS,
                GetCancelErrorDetails("SQLExecute", conn->hstmt, error));
// On Windows ththe SQLExecute api gives a Function Sequence error with SQLState
// as (HY010) and no other operation is allowed after that.
#ifndef _WIN32
      ASSERT_TRUE(absl::StrContains(error, "HY008"))
          << "SQLExecute failed with unexpected error: " << error;
      ASSERT_TRUE(absl::StrContains(error, "Operation canceled"))
          << "SQLExecute failed with unexpected error: " << error;
      EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
#endif  // _WIN32
    }
  }
}

// Both Prepare and Execute are async.
TEST(SQLCancel, Prepare_Execute_CancelAsync_StillExecuting) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ExponentialBackoffPolicy backoff(std::chrono::milliseconds(10),
                                   std::chrono::milliseconds(100), 2);

  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                               (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::string query = "Select 1";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);

  if (SQL_SUCCEEDED(status)) {
    // We can't cancel an operation that is not in the process of executing.
    CheckError(status, "SQLPrepare", conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  } else if (status == SQL_STILL_EXECUTING) {
    // Cancel the operation
    status = SQLCancel(conn->hstmt);
    CheckError(status, "SQLCancel", conn);
    // Call SQLExecute till completion
    status = PollODBC(SQLExecute, backoff, conn->hstmt);
    if (SQL_SUCCEEDED(status)) {
      // Operation could not be cancelled. This is not an error as there could
      // be a race condition where execute completed before cancel could cancel
      // the operation.
      CheckError(status, "SQLExecute", conn);
      EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
    } else {
      // Per spec, Make sure SQLState is HY008 and Message is 'Operation
      // canceled'.
      std::string error;
      ASSERT_EQ(SQL_SUCCESS,
                GetCancelErrorDetails("SQLExecute", conn->hstmt, error));
// On Windows ththe SQLExecute api gives a Function Sequence error with SQLState
// as (HY010) and no other operation is allowed after that.
#ifndef _WIN32
      ASSERT_TRUE(absl::StrContains(error, "HY008"))
          << "SQLExecute failed with unexpected error: " << error;
      ASSERT_TRUE(absl::StrContains(error, "Operation canceled"))
          << "SQLExecute failed with unexpected error: " << error;
      EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
#endif  // _WIN32
    }
  }
}

///////////////////////////////////////
// Tests when Cancel results in NoOp
///////////////////////////////////////
TEST(SQLCancel, CancelNoOp_NoPreviousProcessing) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto status = SQLCancel(conn->hstmt);
  CheckError(status, "SQLCancel", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLCancel, ExecDirect_CancelNoOp) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  auto status = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  // Cancel the operation
  status = SQLCancel(conn->hstmt);
  CheckError(status, "SQLCancel", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLCancel, Prepare_Execute_CancelNoOp) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Cancel the operation
  status = SQLCancel(conn->hstmt);
  CheckError(status, "SQLCancel", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#ifndef BQ_DRIVER_INTEGRATION_TESTS
// Integration tests for SQLCancel.

/////////////////////////////////////////////////////////
// 1. Tests for cancelling Asynchronous processing or
// asynchronous operations that are still executing.
//
// TODO(b/308656304): Move this to common area once SQLExecDirect
// API is implemented for BQ Driver.
/////////////////////////////////////////////////////////
TEST(SQLCancel, ExecDirect_CancelAsync_StillExecuting) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                               (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  std::string query = "Select 1";
  status = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  if (SQL_SUCCEEDED(status)) {
    // We can't cancel an operation that is not in the process of executing.
    CheckError(status, "SQLExecDirect", conn);
  } else if (status == SQL_STILL_EXECUTING) {
    char read_stmt[kBufferLength];
    StrToChar(read_stmt, query);
    // Cancel the operation
    status = SQLCancel(conn->hstmt);
    CheckError(status, "SQLCancel", conn);

    // Call ExecDirect till completion
    ExponentialBackoffPolicy backoff(std::chrono::milliseconds(10),
                                     std::chrono::milliseconds(100), 2);
    status = PollODBC(SQLExecDirect, backoff, conn->hstmt, (SQLCHAR*)read_stmt,
                      strlen(read_stmt));

    if (SQL_SUCCEEDED(status)) {
      // Operation could not be cancelled. This is not an error as there could
      // be a race condition where execute completed before cancel could cancel
      // the operation.
      CheckError(status, "SQLExecDirect", conn);
    } else {
      // Per spec, Make sure SQLState is HY008 and Message is 'Operation
      // canceled'.
      std::string error;
      ASSERT_EQ(SQL_SUCCESS,
                GetCancelErrorDetails("SQLExecDirect", conn->hstmt, error));
      ASSERT_TRUE(absl::StrContains(error, "HY008"))
          << "SQLExecDirect failed with unexpected error: " << error;
      ASSERT_TRUE(absl::StrContains(error, "Operation canceled"))
          << "SQLExecDirect failed with unexpected error: " << error;
    }
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

/////////////////////////////////////////////////////////
// 2. Tests for cancelling operations that need more data
// at execution.
//
// TODO(b/308656304): Move this to common area once SQLExecDirect
// API is implemented for BQ Driver.
/////////////////////////////////////////////////////////
TEST(SQLCancel, ExecDirect_Cancel_NeedData) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Create Table
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_EXEC_DIRECT_CANCEL_TEST";
  Table table(table_name);
  table.Create(conn, "(string_field STRING)", false);
  // Insert statement with bound values.
  std::string const string_field = "Test String 1";
  char insert_stmt_with_bnd_values[kBufferLength];
  sprintf(insert_stmt_with_bnd_values, "INSERT INTO %s VALUES ('%s')",
          table_name.c_str(), string_field.c_str());
  SQLULEN len_string_field = string_field.length();
  SQLLEN len_data_at_exec = SQL_LEN_DATA_AT_EXEC(len_string_field);
  // Insert statement without bound values.
  char insert_stmt_wo_bnd_vals[kBufferLength];
  sprintf(insert_stmt_wo_bnd_vals, "INSERT INTO %s VALUES (?)",
          table_name.c_str());
  // Indicate data-at-exec params.
  auto status = SQLBindParameter(
      conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, len_string_field,
      0, (SQLPOINTER)SQL_DATA_AT_EXEC, len_string_field, &len_data_at_exec);
  CheckError(status, "SQLBindParameter", conn);
  // Call Execute with unbound params so we can get back a SQL_NEED_DATA status.
  status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt_wo_bnd_vals, SQL_NTS);
  if (SQL_SUCCEEDED(status)) {
    // We can't cancel an operation that is not in the process of executing.
    CheckError(status, "SQLExecDirect", conn);
  } else if (status == SQL_NEED_DATA) {
    // Cancel the operation
    status = SQLCancel(conn->hstmt);
    CheckError(status, "SQLCancel", conn);
    // Call Execute again with bound parameters.
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt_with_bnd_values,
                           SQL_NTS);
    if (SQL_SUCCEEDED(status)) {
      // Operation could not be cancelled. Per spec, this is not an error as
      // execute can complete with success and the operation wasn't cancelled.
      CheckError(status, "SQLExecDirect", conn);
    } else {
      // Per spec, make sure SQLState is HY008 and Message is 'Operation
      // canceled'.
      std::string error;
      ASSERT_EQ(SQL_SUCCESS,
                GetCancelErrorDetails("SQLExecDirect", conn->hstmt, error));
      ASSERT_TRUE(absl::StrContains(error, "HY008"))
          << "SQLExecDirect failed with unexpected error: " << error;
      ASSERT_TRUE(absl::StrContains(error, "Operation canceled"))
          << "SQLExecDirect failed with unexpected error: " << error;
    }
  } else {
    // Any other error is a failure.
    CheckError(status, "SQLExecDirect", conn);
  }

  // Drop Table
  table.Drop(conn, false);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// TODO(b/308655629): Move to common area once positional support is implemented
// for SQLExecute API.
TEST(SQLCancel, Execute_Cancel_NeedData) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Create Table
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_PREPARE_EXECUTE_CANCEL_TEST";
  Table table(table_name);
  table.Create(conn, "(string_field STRING)");
  // Insert statement with bound values.
  std::string const string_field = "Test String 1";
  char insert_stmt_with_bnd_values[kBufferLength];
  sprintf(insert_stmt_with_bnd_values, "INSERT INTO %s VALUES ('%s')",
          table_name.c_str(), string_field.c_str());
  SQLULEN len_string_field = string_field.length();
  SQLLEN len_data_at_exec = SQL_LEN_DATA_AT_EXEC(len_string_field);
  // Insert statement without bound values.
  char insert_stmt_wo_bnd_vals[kBufferLength];
  sprintf(insert_stmt_wo_bnd_vals, "INSERT INTO %s VALUES (?)",
          table_name.c_str());
  // Call Prepare without bound values.
  auto status =
      SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt_wo_bnd_vals, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  // Indicate data-at-exec params.
  status = SQLBindParameter(
      conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_CHAR, len_string_field,
      0, (SQLPOINTER)SQL_DATA_AT_EXEC, len_string_field, &len_data_at_exec);
  CheckError(status, "SQLBindParameter", conn);
  // Call Execute so we can get back a SQL_NEED_DATA status.
  status = SQLExecute(conn->hstmt);
  if (SQL_SUCCEEDED(status)) {
    // We can't cancel an operation that is not in the process of executing.
    CheckError(status, "SQLExecute", conn);
  } else if (status == SQL_NEED_DATA) {
    // Cancel the operation
    status = SQLCancel(conn->hstmt);
    CheckError(status, "SQLCancel", conn);
    // Call Prepare again this time, with bound parameters.
    status =
        SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt_with_bnd_values, SQL_NTS);
    CheckError(status, "SQLPrepare", conn);
    // Call Execute again so it either succeeds or reports cancelled operation.
    status = SQLExecute(conn->hstmt);
    if (SQL_SUCCEEDED(status)) {
      // Operation could not be cancelled. Per spec, this is not an error as
      // execute can complete with success and the operation wasn't cancelled.
      CheckError(status, "SQLExecute", conn);
    } else {
      // Per spec, make sure SQLState is HY008 and Message is 'Operation
      // canceled'.
      std::string error;
      ASSERT_EQ(SQL_SUCCESS,
                GetCancelErrorDetails("SQLExecute", conn->hstmt, error));
      ASSERT_TRUE(absl::StrContains(error, "HY008"))
          << "SQLExecute failed with unexpected error: " << error;
      ASSERT_TRUE(absl::StrContains(error, "Operation canceled"))
          << "SQLExecute failed with unexpected error: " << error;
    }
  } else {
    // Any other error is a failure.
    CheckError(status, "SQLExecute", conn);
  }

  // Drop Table
  table.Drop(conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// TODO(b/358002035) Remove BQ_DRIVER_INTEGRATION_TESTS flag
TEST(SQLCloseCursor, CloseCursorAfterUsingExecDirect) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  auto status = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLCloseCursor(conn->hstmt);
  CheckError(status, "SQLCloseCursor_1", conn);

  status = SQLExecute(conn->hstmt);
  EXPECT_EQ(SQL_ERROR, status);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, Basic_script) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_TEST_TABLE";
  std::string create_stmt =
      "CREATE OR REPLACE TABLE " + table_name +
      " (StringField STRING, IntegerField INTEGER, FloatField FLOAT64);";
  std::string insert_stmt = GetInsertionString(table_name, kSampleData);
  std::string select_stmt_1 = "SELECT * FROM " + table_name;
  std::string select_stmt_2 = "SELECT StringField FROM " + table_name +
                              " WHERE StringField = \"Test String 5\"";

  std::string query =
      create_stmt + insert_stmt + ";" + select_stmt_1 + ";" + select_stmt_2;

  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  SQLSMALLINT num_cols;

  // Validations for create_stmt
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

  // Validations for insert_stmt
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

  // Validations for select_stmt_1
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 3);
  int num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, kSampleData.size());

  // Validations for select_stmt_2
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 1);
  num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 1);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  Table table(table_name);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithInOutParams) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name =
      kDatasetWithTablePrefix + "ODBC_SCRIPTS_PROCEDURES_TABLE";
  Table table(table_name);
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string procedure_name =
      kDatasetWithTablePrefix + "ODBC_PROCEDURE_INSERT_STD_ROW";
  std::string procedure_create =
      "CREATE OR REPLACE PROCEDURE " + procedure_name +
      "(IntegerField INT64, FloatField FLOAT64, OUT StringField STRING)\n"
      "BEGIN\n"
      "SET StringField = GENERATE_UUID();\n"
      "INSERT INTO " +
      table_name +
      " VALUES(StringField, IntegerField, FloatField);\n"
      "SELECT FORMAT(\"Created row %s\", StringField);\n"
      "END";

  SQLRETURN status =
      SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  SQLSMALLINT num_cols;

  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string procedure_call =
      "DECLARE OutStringField STRING;\n"
      "CALL " +
      procedure_name +
      "(32, 45.6, OutStringField);\n"
      "SELECT * FROM " +
      table_name;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Validations for INSERT INTO ...
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

  // Validations for SELECT FORMAT ...
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 1);
  int num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 1);

  // Validations for SELECT * FROM ...
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 3);
  num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 1);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, FetchEmptyResultSet) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetWithTablePrefix + "ODBC_MORE_FETCH_RESULT_SET_TEST";
  Table table(table_name);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  auto const query = "SELECT StringField FROM " + table_name;
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size()),
             "SQLPrepare", conn);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA);  // Expect no data to fetch
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_NO_DATA);  // Expect no result set

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, SingleResultSet) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name =
      kDatasetWithTablePrefix + "ODBC_MORE_SINGLE_RESULT_SET_TEST";
  Table table(table_name);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  auto const query = "SELECT StringField FROM " + table_name;
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size()),
             "SQLPrepare", conn);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLMoreResults(conn->hstmt),
            SQL_NO_DATA);  // Expect no more results

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, LoopThroughMultipleSets) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name =
      kDatasetWithTablePrefix + "ODBC_MORE_MULTIPLE_RESULT_SET_TEST";
  Table table(table_name);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  // Insert test data for the first result set
  auto insert_stmt = "INSERT INTO " + table_name +
                     " VALUES ('Test1', 1, 1.0), ('Test2', 2, 2.0)";
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                        insert_stmt.size()),
             "SQLPrepare", conn);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  // Execute multiple select statements
  auto const query = "SELECT StringField FROM " + table_name +
                     " WHERE IntegerField = 1; "
                     "SELECT StringField FROM " +
                     table_name + " WHERE IntegerField = 2;";
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size()),
             "SQLPrepare", conn);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  int resultSetCount = 0;
  do {
    int rowCount = 0;
    while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
      rowCount++;
      // Here you would typically process the row (e.g., print, store, etc.)
    }
    EXPECT_GT(rowCount,
              0);  // Ensure at least one row was fetched from each result set
    resultSetCount++;
  } while (SQLMoreResults(conn->hstmt) ==
           SQL_SUCCESS);  // Check for more result sets

  EXPECT_GT(resultSetCount, 1);  // Ensure multiple result sets were processed

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, HandleLargeResultSet) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetWithTablePrefix + "ODBC_MORE_LARGE_RESULT_SET_TEST";
  Table table(table_name);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  // Insert a large number of rows for testing
  for (int i = 0; i < 50; ++i) {
    auto insert_stmt = "INSERT INTO " + table_name + " VALUES ('Test" +
                       std::to_string(i) + "', " + std::to_string(i) + ", " +
                       std::to_string(i * 1.0) + ")";
    CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                          insert_stmt.size()),
               "SQLPrepare", conn);
    EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);
  }

  auto const query = "SELECT StringField FROM " +
                     table_name;  // Query that returns a large result set
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size()),
             "SQLPrepare", conn);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  int rowCount = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    rowCount++;
  }
  EXPECT_GT(rowCount, 0);  // Expect some rows

  EXPECT_EQ(SQLMoreResults(conn->hstmt),
            SQL_NO_DATA);  // Expect no more results after fetching all

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, CheckResultSetAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetWithTablePrefix + "ODBC_CHECK_RESULT_SET_TEST";
  Table table(table_name);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  auto const query = "SELECT StringField FROM " + table_name;
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size()),
             "SQLPrepare", conn);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  do {
    SQLSMALLINT numCols;
    SQLNumResultCols(conn->hstmt, &numCols);
    EXPECT_GT(numCols, 0);  // Expect at least one column

    // Check attributes of each column
    for (SQLSMALLINT i = 1; i <= numCols; i++) {
      SQLCHAR columnName[256];
      SQLSMALLINT nameLength;
      SQLSMALLINT dataType;
      SQLULEN columnSize;
      SQLSMALLINT decimalDigits;
      SQLLEN nullable;
      SQLLEN numericAttribute;  // Correct type for NumericAttributePtr

      SQLColAttribute(conn->hstmt, i, SQL_DESC_NAME, columnName,
                      sizeof(columnName), &nameLength, &numericAttribute);
      SQLColAttribute(conn->hstmt, i, SQL_DESC_OCTET_LENGTH, &columnSize, 0,
                      NULL, &nullable);

      EXPECT_GT(nameLength,
                0);  // Expect column name length to be greater than 0
      EXPECT_GT(columnSize, 0);  // Expect column size to be greater than 0
    }
  } while (SQLMoreResults(conn->hstmt) ==
           SQL_SUCCESS);  // Check for more results

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, MultipleResultSetsViaSeparateQueries) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetWithTablePrefix +
                    "ODBC_MORE_MULTIPLE_RESULT_SET_WITH_SEPARATE_QUERIES_TEST";
  Table table(table_name);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  // Insert some test data
  auto insert_stmt = "INSERT INTO " + table_name +
                     " VALUES ('Test1', 1, 1.0), ('Test2', 2, 2.0)";
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                        insert_stmt.size()),
             "SQLPrepare", conn);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  // Execute the first query
  auto const query1 =
      "SELECT StringField FROM " + table_name + " WHERE IntegerField = 1";
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query1.c_str(), query1.size()),
             "SQLPrepare", conn);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  int resultSetCount = 0;
  do {
    int rowCount = 0;
    while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
      rowCount++;
      // Process each row (add your row processing logic here)
    }
    EXPECT_GT(rowCount, 0);  // Ensure at least one row is fetched
    resultSetCount++;
  } while (SQLMoreResults(conn->hstmt) ==
           SQL_SUCCESS);  // Move to the next result set

  // Execute the second query
  auto const query2 =
      "SELECT StringField FROM " + table_name + " WHERE IntegerField = 2";
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query2.c_str(), query2.size()),
             "SQLPrepare", conn);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  do {
    int rowCount = 0;
    while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
      rowCount++;
      // Process each row (add your row processing logic here)
    }
    EXPECT_GT(rowCount, 0);  // Ensure at least one row is fetched
    resultSetCount++;
  } while (SQLMoreResults(conn->hstmt) ==
           SQL_SUCCESS);  // Move to the next result set

  EXPECT_EQ(resultSetCount, 2);  // Ensure two result sets were processed

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, NoResultSet) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  auto table_name = kDatasetWithTablePrefix + "ODBC_NO_RESULT_SET_TEST";
  Table table(table_name);
  table.Create(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  std::string query =
      "SELECT 1 FROM (SELECT 1) AS dummy WHERE FALSE";  // Query that returns no
                                                        // results
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size()),
             "SQLPrepare", conn);

  // Execute the query and check for success
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA);  // Expect no data fetched

  // Now call SQLMoreResults to check for more results
  EXPECT_EQ(SQLMoreResults(conn->hstmt),
            SQL_NO_DATA);  // Expect no more results

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ErrorHandling) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto table_name = kDatasetWithTablePrefix + "_ODBC_ERROR_SET_TEST";
  // Use an intentional error: querying a non-existent table
  std::string query = "SELECT * FROM " + table_name;
  SQLRETURN prepResult =
      SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size());

  if (prepResult != SQL_SUCCESS) {
    // Check if SQLPrepare failed due to table not existing
    EXPECT_EQ(prepResult, SQL_ERROR);
  } else {
    // If preparation was successful, execute the query
    EXPECT_EQ(SQLExecute(conn->hstmt),
              SQL_ERROR);  // Expect error due to non-existent table
  }

  // After execution failure, check for more results, which should not be
  // applicable
  EXPECT_EQ(SQLMoreResults(conn->hstmt),
            SQL_NO_DATA);  // Expect no more results due to execution failure

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// Stored Procedures
TEST(SQLMoreResults, SimpleProcedureNoResultSet) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Create a test table
  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_PROCEDURES_TABLE";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(StringField STRING)");

  std::string procedure_name =
      kDatasetWithTablePrefix + "ODBC_PROCEDURE_SIMPLE_NO_RESULTSET";
  std::string procedure_create =
      "CREATE OR REPLACE PROCEDURE " + procedure_name + "()\n"
      "BEGIN\n"
      "INSERT INTO " + table_name + " (StringField) VALUES ('Test Value');\n"
      "END";

  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Ensure no result set is returned
  SQLSMALLINT num_cols;
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithMultipleSQLMoreResultsCalls) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Table creation for procedure's INSERT INTO
  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_PROCEDURES_TABLE";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Procedure creation (assumes it returns two result sets)
  std::string procedure_name = kDatasetWithTablePrefix + "ODBC_PROCEDURE_INSERT_ROW";
  std::string procedure_create = 
      "CREATE OR REPLACE PROCEDURE " + procedure_name + 
      "(IntegerField INT64, FloatField FLOAT64)\n"
      "BEGIN\n"
      "  INSERT INTO " + table_name + " VALUES('SomeString', IntegerField, FloatField);\n"
      "  SELECT IntegerField, FloatField FROM " + table_name + " WHERE IntegerField = IntegerField;\n"
      "  SELECT * FROM " + table_name + ";\n"
      "END";

  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  SQLSMALLINT num_cols;

  // Call the procedure
  std::string procedure_call = 
      "CALL " + procedure_name + "(10, 20.5);";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Validations for first result set (should be the SELECT with IntegerField and FloatField)
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 2);  // Expecting two columns (IntegerField, FloatField)
  int num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 1);  // Expecting 1 row

  // Validations for second result set (should be SELECT * FROM table)
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 3);  // Expecting 3 columns in the table (StringField, IntegerField, FloatField)
  num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 1);  // Expecting 1 row

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithNoParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_NO_PARAMS_TABLE";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(ID INT64, Name STRING)");

  std::string procedure_name = kDatasetWithTablePrefix + "ODBC_PROCEDURE_INSERT_NO_PARAMS";
  std::string procedure_create = 
      "CREATE OR REPLACE PROCEDURE " + procedure_name + "()\n"
      "BEGIN\n"
      "  INSERT INTO " + table_name + " VALUES(1, 'John Doe');\n"
      "  SELECT * FROM " + table_name + ";\n"
      "END";
  
  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Call the procedure
  std::string procedure_call = "CALL " + procedure_name + "();";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Validate result set
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  SQLSMALLINT num_cols;
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 2);  // Expecting two columns (ID, Name)
  int num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 1);  // Expecting 1 row
  
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithMultipleInputParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_MULTI_INPUT_PARAMS";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(IntegerField INT64, FloatField FLOAT64)");

  std::string procedure_name = kDatasetWithTablePrefix + "ODBC_PROCEDURE_INSERT_MULTI_INPUT";
  std::string procedure_create = 
      "CREATE OR REPLACE PROCEDURE " + procedure_name + 
      "(IntegerField INT64, FloatField FLOAT64)\n"
      "BEGIN\n"
      "  INSERT INTO " + table_name + " VALUES(IntegerField, FloatField);\n"
      "  SELECT * FROM " + table_name + ";\n"
      "END";

  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Call the procedure
  std::string procedure_call = "CALL " + procedure_name + "(10, 20.5);";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Validate result set
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  SQLSMALLINT num_cols;
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 2);  // Expecting two columns (IntegerField, FloatField)
  int num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 1);  // Expecting 1 row

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithMissingInputParams) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_PROCEDURES_TABLE";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(StringField STRING, IntegerField INT64)");

  std::string procedure_name = kDatasetWithTablePrefix + "ODBC_PROCEDURE_INSERT_STD_ROW";
  std::string procedure_create =
      "CREATE OR REPLACE PROCEDURE " + procedure_name +
      "(IN IntegerField INT64, OUT StringField STRING)\n"
      "BEGIN\n"
      "  SET StringField = GENERATE_UUID();\n"
      "  INSERT INTO " + table_name + " VALUES(StringField, IntegerField);\n"
      "END";

  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  if (status != SQL_SUCCESS) {
    // Check if SQLPrepare failed due to missing input parameters
    EXPECT_EQ(status, SQL_ERROR);
  } else {
    // If preparation was successful, execute the query
    EXPECT_EQ(SQLExecute(conn->hstmt),SQL_SUCCESS);
  }
  // Try to call procedure without providing the input parameter (missing IntegerField)
  std::string procedure_call = 
      "DECLARE OutStringField STRING;\n"
      "CALL " + procedure_name + "();\n"  // Missing IntegerField
      "SELECT * FROM " + table_name;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
  if (status != SQL_SUCCESS) {
    // Check if SQLPrepare failed due to missing input parameters
    EXPECT_EQ(status, SQL_ERROR);
  } else {
    // If preparation was successful, execute the query
    EXPECT_EQ(SQLExecute(conn->hstmt),SQL_SUCCESS);
  }

  // Now, check the SQLMoreResults behavior after the failed procedure call
  // SQLMoreResults should return SQL_NO_DATA for the missing parameter case
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_NO_DATA);  // No further results should be returned

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithEmptyResultSet) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_EMPTY_RESULTSET";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(ID INT64, Name STRING)");

  std::string procedure_name = kDatasetWithTablePrefix + "ODBC_PROCEDURE_EMPTY_RESULTSET";
  std::string procedure_create = 
      "CREATE OR REPLACE PROCEDURE " + procedure_name + "()\n"
      "BEGIN\n"
      "  SELECT * FROM " + table_name + ";\n"
      "END";

  SQLRETURN status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Call the procedure (no rows expected in SELECT)
  std::string procedure_call = "CALL " + procedure_name + "();";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Ensure no rows returned
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_NO_DATA);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
