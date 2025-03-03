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
#ifndef DRIVER_MANAGER_TESTING_ENABLED
#ifdef BQ_DRIVER_INTEGRATION_TESTS
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#endif  // BQ_DRIVER_INTEGRATION_TESTS
#endif  // DRIVER_MANAGER_TESTING_ENABLED
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "absl/strings/match.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_tests {
#ifndef DRIVER_MANAGER_TESTING_ENABLED
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
#endif  // DRIVER_MANAGER_TESTING_ENABLED
using ::testing::StartsWith;

class StatementParameterizedTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutANSI, StatementParameterizedTest,
                         testing::Values(false, true));

#ifndef BQ_DRIVER_INTEGRATION_TESTS
class MultiStatementTest : public ::testing::TestWithParam<bool> {};
INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutPrepare, MultiStatementTest,
                         testing::Values(true, false));
#endif  // BQ_DRIVER_INTEGRATION_TESTS

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

StdRows const kRowCountSampleData{
    {"Row 1", 1, 1.1}, {"Row 2", 2, 2.2}, {"Row 3", 3, 3.3}};

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

// Helper to store field information
struct DataField {
  SQLPOINTER data_ptr;
  SQLLEN data_size;
  SQLSMALLINT c_type;
  SQLSMALLINT sql_type;
  SQLLEN* str_len_or_ind_ptr;
};

// Function to insert all data types using SQLPutData
void PutAllDataTypes(std::shared_ptr<ODBCHandles> conn,
                     std::string const& table_name) {
  // Prepare data
  SQLCHAR bool_data = SQL_TRUE;
  SQLLEN bool_len = SQL_DATA_AT_EXEC;

  SQLLEN int_data = 42;
  SQLLEN int_len = SQL_DATA_AT_EXEC;

  double float_data = 3.14;
  SQLLEN float_len = SQL_DATA_AT_EXEC;

  std::string text_data = "";
  SQLLEN string_len = SQL_DATA_AT_EXEC;

  std::vector<uint8_t> binary_data = {0xDE, 0xAD, 0xBE, 0xEF};
  SQLLEN binary_len = SQL_DATA_AT_EXEC;

  DataField fields[] = {
      {&bool_data, sizeof(bool_data), SQL_C_BIT, SQL_BIT, &bool_len},
      {&int_data, sizeof(int_data), SQL_C_SBIGINT, SQL_BIGINT, &int_len},
      {&float_data, sizeof(float_data), SQL_C_DOUBLE, SQL_DOUBLE, &float_len},
      {(SQLPOINTER)text_data.c_str(), static_cast<SQLLEN>(text_data.size()),
       SQL_C_CHAR, SQL_LONGVARCHAR, &string_len},
      {(SQLPOINTER)binary_data.data(), static_cast<SQLLEN>(binary_data.size()),
       SQL_C_BINARY, SQL_LONGVARBINARY, &binary_len},
  };

  // Prepare and bind parameters
  auto query = "INSERT INTO " + table_name + " VALUES (?, ?, ?, ?, ?)";
  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(SQLBindParameter(conn->hstmt, i + 1, SQL_PARAM_INPUT,
                               fields[i].c_type, fields[i].sql_type, 0, 0,
                               nullptr, 0, fields[i].str_len_or_ind_ptr),
              SQL_SUCCESS);
  }

  // Execute and provide data using SQLPutData
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);
  SQLPOINTER param = nullptr;

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(SQLParamData(conn->hstmt, &param), SQL_NEED_DATA);
    EXPECT_EQ(SQLPutData(conn->hstmt, fields[i].data_ptr, fields[i].data_size),
              SQL_SUCCESS);
  }
   if(kIsBqDriver){
     for (int i = 0; i < 5; ++i) {
    std::cout << "data bind param "<< i+1<< std::endl;
    SQLBindParameter(conn->hstmt, i + 1, SQL_PARAM_INPUT,
                               fields[i].c_type, fields[i].sql_type, 0, 0,
                               fields[i].data_ptr, 0, &fields[i].data_size);
  }
   }

  // Finalize data execution
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_SUCCESS);
}

// Function to validate inserted data
void ValidateAllPutData(std::shared_ptr<ODBCHandles> conn,
                        std::string const& table_name) {
  // Prepare and execute query
  auto query =
      "SELECT BoolField, IntField, FloatField, StringField, BinaryField FROM " +
      table_name;
  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);

  // Define validation fields
  SQLCHAR result_bool = 0;
  SQLLEN result_bool_len = 0;

  SQLLEN result_int = 0;
  SQLLEN result_int_len = 0;

  double result_float = 0.0;
  SQLLEN result_float_len = 0;

  SQLCHAR result_string[256] = {0};
  SQLLEN result_string_len = 0;

  uint8_t result_binary[256] = {0};
  SQLLEN result_binary_len = 0;

  DataField validations[] = {
      {&result_bool, sizeof(result_bool), SQL_C_BIT, SQL_BIT, &result_bool_len},
      {&result_int, sizeof(result_int), SQL_C_SBIGINT, SQL_BIGINT,
       &result_int_len},
      {&result_float, sizeof(result_float), SQL_C_DOUBLE, SQL_DOUBLE,
       &result_float_len},
      {result_string, sizeof(result_string), SQL_C_CHAR, SQL_LONGVARCHAR,
       &result_string_len},
      {result_binary, sizeof(result_binary), SQL_C_BINARY, SQL_LONGVARBINARY,
       &result_binary_len},
  };

  // Fetch and validate data
  for (int i = 0; i < 5; ++i) {
    std::cout << "col num _> " <<validations[i].c_type << "    " <<i+1<< std::endl;
    EXPECT_EQ(SQLGetData(conn->hstmt, i + 1, validations[i].c_type,
                         validations[i].data_ptr, kBufferLength,
                         validations[i].str_len_or_ind_ptr),
              SQL_SUCCESS);
  }

  // Assertions for validation
  EXPECT_EQ(result_bool, SQL_TRUE);
  EXPECT_EQ(result_int, 42);
  EXPECT_DOUBLE_EQ(result_float, 3.14);
  EXPECT_EQ(std::string((char*)result_string), "");

  std::vector<uint8_t> expected_binary = {0xDE, 0xAD, 0xBE, 0xEF};
  EXPECT_EQ(result_binary_len, expected_binary.size());

  EXPECT_TRUE(std::equal(result_binary, result_binary + result_binary_len,
                         expected_binary.begin()));
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
    if (!col_name.compare("StringField")) {
      for (auto data : input_data) {
        input_col_values.emplace_back(data.str_field);
      }

    } else if (!col_name.compare("IntegerField")) {
      for (auto data : input_data) {
        if (data.int_field != NULL)
          input_col_values.emplace_back(std::to_string(data.int_field));
        else
          input_col_values.emplace_back("");
      }

    } else if (!col_name.compare("FloatField")) {
      for (auto data : input_data) {
        if (data.float_field != NULL)
          input_col_values.emplace_back(std::to_string(data.float_field));
        else
          input_col_values.emplace_back("");
      }
    }
    sort(input_col_values.begin(), input_col_values.end(), str_comparison);

    // Check if the sorted inserted and returned vectors have same values
    EXPECT_EQ(ret_col_values.size(), input_col_values.size());
    if ((!col_name.compare("FloatField"))) {
      for (int i = 0; i < ret_col_values.size(); i++) {
        if (ret_col_values[i].compare("") != 0)
          EXPECT_EQ(stod(ret_col_values[i]), stod(input_col_values[i]))
              << " at index: " << i;
      }
    } else {
      for (int i = 0; i < ret_col_values.size(); i++) {
        EXPECT_EQ(ret_col_values[i], input_col_values[i]) << " at index: " << i;
      }
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
  SQLRETURN status;
  auto conn = std::make_shared<ODBCHandles>();

// This test doesn't work with existing driver. It fails with error:
// "Invalid query: Cannot set destination table in jobs with ASSERT statements
// (70) SQLSTATE=42000"
#ifdef BQ_DRIVER_INTEGRATION_TESTS
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  status = SQLExecDirect(conn->hstmt, (SQLCHAR*)"ASSERT ((SELECT COUNT(*) > 5 FROM UNNEST([1, 2, 3, 4, 5, 6]))) AS 'Table must contain more than 5 rows.'", SQL_NTS);
  CheckError(status, "SQLExecDirect(ASSERT)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
#endif  // BQ_DRIVER_INTEGRATION_TESTS

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  status = SQLExecDirect(
      conn->hstmt,
      (SQLCHAR*)"SELECT num FROM UNNEST(GENERATE_ARRAY(1, 10)) AS num;",
      SQL_NTS);
  CheckError(status, "SQLExecDirect(SELECT num)", conn);
  int num_rows_returned = 0;
  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
    num_rows_returned++;
  }
  EXPECT_EQ(num_rows_returned, 10);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

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

#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST(StatementTest, SQLExecDirectW) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::wstring const table_name =
      ToWStr(kDatasetWithTablePrefix) + L"ODBC_INSERT_SQLEXECDIRECTW_TEST";
  Table table(table_name);

  table.CreateW(conn, L"(string_field STRING)");

  std::wstring const string_field = L"Some Test String नमस्ते";
  SQLWCHAR insert_stmt[kBufferLength];
  // swprintf(insert_stmt, kBufferLength, L"INSERT INTO %ls VALUES ('%ls')",
  //          table_name.c_str(), string_field.c_str());

  SQLRETURN status =
      SQLExecDirectW(conn->hstmt, (SQLWCHAR*)insert_stmt, SQL_NTS);
  CheckError(status, "SQLExecDirectW", conn);

  table.DropW(conn);

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

#endif  // BQ_DRIVER_INTEGRATION_TESTS

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

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

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
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertData(conn, kSampleData, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query =
      "SELECT StringField, IntegerField, FloatField FROM " + table_name;

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

TEST(SQLPrepare, ParameterizedQuery) {
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

// Integration tests for SQLCancel.

/////////////////////////////////////////////////////////
// 1. Tests for cancelling Asynchronous processing or
// asynchronous operations that are still executing.
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

#ifndef BQ_DRIVER_INTEGRATION_TESTS

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

#endif  // BQ_DRIVER_INTEGRATION_TESTS

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

#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST_P(MultiStatementTest, BasicScript) {
  bool use_prepare = GetParam();
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name;
  if (use_prepare) {
    table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_SQLEXECUTE_TEST_TABLE";
  } else {
    table_name =
        kDatasetWithTablePrefix + "ODBC_SCRIPTS_SQLEXECDIRECT_TEST_TABLE";
  }

  std::string create_stmt =
      "CREATE OR REPLACE TABLE " + table_name +
      " (StringField STRING, IntegerField INTEGER, FloatField FLOAT64);";
  std::string insert_stmt = GetInsertionString(table_name, kSampleData);
  std::string select_stmt_1 = "SELECT * FROM " + table_name;
  std::string select_stmt_2 = "SELECT StringField FROM " + table_name +
                              " WHERE StringField = \"Test String 5\"";

  std::string query =
      create_stmt + insert_stmt + ";" + select_stmt_1 + ";" + select_stmt_2;

  SQLRETURN status;
  if (use_prepare) {
    status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    CheckError(status, "SQLPrepare", conn);
    status = SQLExecute(conn->hstmt);
    CheckError(status, "SQLExecute", conn);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    CheckError(status, "SQLExecDirect", conn);
  }

  SQLSMALLINT num_cols;

  // Validations for create_stmt
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

  SQLLEN row_count;
  status = SQLRowCount(conn->hstmt, &row_count);
  CheckError(status, "SQLRowCount after CREATE", conn);
  EXPECT_EQ(row_count, -1);

  // Validations for insert_stmt
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

  // Check rows affected by the insert_stmt
  status = SQLRowCount(conn->hstmt, &row_count);
  CheckError(status, "SQLRowCount after INSERT", conn);
  EXPECT_EQ(row_count, kSampleData.size());

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

  // Attribute validation for select_stmt_1
  for (SQLSMALLINT i = 1; i <= num_cols; i++) {
    SQLCHAR column_name[256];
    SQLSMALLINT name_length;
    SQLULEN column_size;
    SQLLEN nullable;

    // Validate column attributes
    SQLColAttribute(conn->hstmt, i, SQL_DESC_NAME, column_name,
                    sizeof(column_name), &name_length, NULL);
    SQLColAttribute(conn->hstmt, i, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL,
                    &nullable);

    EXPECT_GT(name_length, 0);  // Column name length should be > 0
    EXPECT_GT(column_size, 0);  // Column size should be > 0
  }

  // Check rows returned by select_stmt_1
  status = SQLRowCount(conn->hstmt, &row_count);
  CheckError(status, "SQLRowCount after SELECT 1", conn);
  EXPECT_EQ(row_count, -1);

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

  // Attribute validation for select_stmt_2
  for (SQLSMALLINT i = 1; i <= num_cols; i++) {
    SQLCHAR column_name[256];
    SQLSMALLINT name_length;
    SQLULEN column_size;
    SQLLEN nullable;

    // Validate column attributes
    SQLColAttribute(conn->hstmt, i, SQL_DESC_NAME, column_name,
                    sizeof(column_name), &name_length, NULL);
    SQLColAttribute(conn->hstmt, i, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL,
                    &nullable);

    EXPECT_GT(name_length, 0);  // Column name length should be > 0
    EXPECT_GT(column_size, 0);  // Column size should be > 0
  }

  // Check rows returned by select_stmt_2
  status = SQLRowCount(conn->hstmt, &row_count);
  CheckError(status, "SQLRowCount after SELECT 2", conn);
  EXPECT_EQ(row_count, -1);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  Table table(table_name);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST_P(MultiStatementTest, ProcedureWithInOutParams) {
  bool use_prepare = GetParam();

  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name, procedure_name;
  if (use_prepare) {
    table_name =
        kDatasetWithTablePrefix + "ODBC_SCRIPTS_SQLEXECUTE_PROCEDURES_TABLE";
    procedure_name =
        kDatasetWithTablePrefix + "ODBC_PROCEDURE_SQLEXECUTE_INSERT_STD_ROW";
  } else {
    table_name = kDatasetWithTablePrefix +
                 "ODBC_SCRIPTS_SQL_EXECDIRECT_PROCEDURES_TABLE";
    procedure_name =
        kDatasetWithTablePrefix + "ODBC_PROCEDURE_SQLEXECDIRECT_INSERT_STD_ROW";
  }

  // Create table for testing
  Table table(table_name);
  table.CreateWithPrepare(
      conn, "(IntegerField INTEGER, FloatField FLOAT64, StringField STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Test for Procedure with Empty Result Set
  {
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    std::string procedure_create = "CREATE OR REPLACE PROCEDURE " +
                                   procedure_name + "() BEGIN SELECT * FROM " +
                                   table_name + "; END";

    SQLRETURN status =
        SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
    CheckError(status, "SQLPrepare", conn);

    status = SQLExecute(conn->hstmt);
    CheckError(status, "SQLExecute", conn);

    // Ensure no rows returned
    EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_NO_DATA);

    SQLFreeStmt(conn->hstmt, SQL_CLOSE);
    Procedure procedure(procedure_name);
    procedure.DropWithPrepare(conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  // Test for Procedure with No Result Set
  {
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    std::string procedure_create = "CREATE OR REPLACE PROCEDURE " +
                                   procedure_name + "() BEGIN INSERT INTO " +
                                   table_name +
                                   " (IntegerField) VALUES (100); END";

    SQLRETURN status =
        SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
    CheckError(status, "SQLPrepare", conn);

    status = SQLExecute(conn->hstmt);
    CheckError(status, "SQLExecute", conn);

    SQLSMALLINT num_cols;
    status = SQLNumResultCols(conn->hstmt, &num_cols);
    CheckError(status, "SQLNumResultCols", conn);
    EXPECT_EQ(num_cols, 0);

    SQLFreeStmt(conn->hstmt, SQL_CLOSE);
    Procedure procedure(procedure_name);
    procedure.DropWithPrepare(conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  // Test for Procedure with In/Out Parameters
  {
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    std::string procedure_create =
        "CREATE OR REPLACE PROCEDURE " + procedure_name +
        "(IntegerField INT64, FloatField FLOAT64, OUT StringField STRING) "
        "BEGIN "
        "SET StringField = GENERATE_UUID(); "
        "INSERT INTO " +
        table_name +
        " VALUES(IntegerField, FloatField, StringField); "
        "SELECT FORMAT(\"Created row %s\", StringField); END";

    SQLRETURN status =
        SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
    CheckError(status, "SQLPrepare", conn);

    status = SQLExecute(conn->hstmt);
    CheckError(status, "SQLExecute", conn);

    std::string procedure_call =
        "DECLARE OutStringField STRING; "
        "CALL " +
        procedure_name +
        "(32, 45.6, OutStringField); "
        "SELECT * FROM " +
        table_name;

    if (use_prepare) {
      status =
          SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
      CheckError(status, "SQLPrepare", conn);
      status = SQLExecute(conn->hstmt);
      CheckError(status, "SQLExecute", conn);
    } else {
      status =
          SQLExecDirect(conn->hstmt, (SQLCHAR*)procedure_call.c_str(), SQL_NTS);
      CheckError(status, "SQLExecDirect", conn);
    }

    SQLSMALLINT num_cols;
    status = SQLNumResultCols(conn->hstmt, &num_cols);
    CheckError(status, "SQLNumResultCols", conn);
    EXPECT_EQ(num_cols, 0);
    SQLLEN row_count = 0;
    status = SQLRowCount(conn->hstmt, &row_count);
    CheckError(status, "SQLRowCount", conn);
    EXPECT_GT(row_count, 0);

    status = SQLNumResultCols(conn->hstmt, &num_cols);
    CheckError(status, "SQLNumResultCols", conn);
    EXPECT_EQ(num_cols, 0);
    EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);

    EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
    status = SQLNumResultCols(conn->hstmt, &num_cols);
    CheckError(status, "SQLNumResultCols", conn);
    EXPECT_EQ(num_cols, 1);

    EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
    status = SQLNumResultCols(conn->hstmt, &num_cols);
    CheckError(status, "SQLNumResultCols", conn);
    EXPECT_EQ(num_cols, 3);

    SQLFreeStmt(conn->hstmt, SQL_CLOSE);
    Procedure procedure(procedure_name);
    procedure.DropWithPrepare(conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, FetchEmptyResultSet) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetWithTablePrefix + "ODBC_MORE_FETCH_RESULT_SET_TEST";
  Table table(table_name);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  auto const query = "SELECT StringField FROM " + table_name;
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size()),
             "SQLPrepare", conn);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA);  // Expect no data to fetch
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_NO_DATA);  // Expect no result set

  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, MultipleResultSetsViaSeparateQueries) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetWithTablePrefix +
                    "ODBC_MORE_MULTIPLE_RESULT_SET_WITH_SEPARATE_QUERIES_TEST";
  Table table(table_name);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
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

  while (SQLMoreResults(conn->hstmt) == SQL_SUCCESS) {
    while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
      // Fetch rows but do not validate the output
    }
  }

  // Execute the second query
  auto const query_2 =
      "SELECT StringField FROM " + table_name + " WHERE IntegerField = 2";
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)query_2.c_str(), query_2.size()),
             "SQLPrepare", conn);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  while (SQLMoreResults(conn->hstmt) == SQL_SUCCESS) {
    while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
      // Fetch rows but do not validate the output
    }
  }

  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ErrorHandling) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto table_name = kDatasetWithTablePrefix + "_ODBC_ERROR_SET_TEST";
  // Use an intentional error: querying a non-existent table
  std::string query = "SELECT * FROM " + table_name;
  SQLRETURN prep_result =
      SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size());

  EXPECT_EQ(prep_result, SQL_ERROR);
  // After execution failure, check for more results, which should not be
  // applicable
  EXPECT_EQ(SQLMoreResults(conn->hstmt),
            SQL_NO_DATA);  // Expect no more results due to execution failure
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLPutDataErrorTest) {
  // Test SQLPutData error scenarios with proper sequence and data validation

  auto const table_name = kDatasetWithTablePrefix + "ODBC_PUT_DATA_ERROR_TEST";
  Table table(table_name);

  Schema schema{{"TextField", "STRING"}};

  // Create table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, getSchemaStr(schema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Prepare and bind parameters
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (?)";

  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  // Indicate that data will be provided with SQLPutData
  SQLLEN indicator = SQL_DATA_AT_EXEC;
  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &indicator),
            SQL_SUCCESS);

  // ========================
  // Sequence-Related Errors
  // ========================

  // Scenario 1: Call SQLPutData before SQLExecute (sequence issue)
  std::string data = "SomeData";
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_ERROR);  // Should fail before SQLExecute

  // Scenario 2: SQLExecute called before SQLPutData (proper sequence to set
  // state)
  EXPECT_EQ(SQLExecute(conn->hstmt),
            SQL_NEED_DATA);  // Execute should move to NEED_DATA state
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_NEED_DATA);
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_SUCCESS);  // Now SQLPutData should succeed

  // ========================
  // Data-Related Errors
  // ========================

  // Scenario 3: Call SQLPutData with invalid pointer and size 0
  EXPECT_EQ(SQLPutData(conn->hstmt, nullptr, 0),
            SQL_ERROR);  // Invalid data should fail

  // Scenario 4: Call SQLPutData with a negative size
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), -1),
            SQL_ERROR);  // Negative size should fail

  // Scenario 5: Data Truncation - inserting data that exceeds column size
  std::string large_data = std::string(50000, 'Z');
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)large_data.c_str(),
                       large_data.size()),
            SQL_ERROR);  // Should fail or truncate if size exceeds column limit

  // Scenario 6: Invalid Data in Different Encodings - non-UTF-8 encoded string
  // (e.g., Latin-1 encoding)
  std::string latin1_data =
      "Latin-1 data \xE9";  // Character 'é' in Latin-1 encoding
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)latin1_data.c_str(),
                       latin1_data.size()),
            SQL_ERROR);  // Should fail if encoding is unsupported

  // ========================
  // Data Type and Handle Errors
  // ========================

  // Scenario 7: Call SQLPutData with an invalid handle (corrupted or
  // uninitialized handle)
  SQLHSTMT invalid_stmt = nullptr;
  EXPECT_EQ(SQLPutData(invalid_stmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_INVALID_HANDLE);  // Invalid handle scenario

  // Scenario 8: Call SQLPutData with mismatched data type
  EXPECT_EQ(
      SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
      SQL_ERROR);  // Should fail due to type mismatch if the bind is incorrect.

  // ========================
  // Additional Error Cases
  // ========================

  // Scenario 9: Call SQLPutData with a pointer that is not properly allocated
  char* invalid_ptr = nullptr;
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)invalid_ptr, data.size()),
            SQL_ERROR);  // Should fail due to invalid pointer

  // Disconnect
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Clean up
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithNoParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name =
      kDatasetWithTablePrefix + "ODBC_SCRIPTS_NO_PARAMS_TABLE";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(ID INT64, Name STRING)");

  std::string procedure_name =
      kDatasetWithTablePrefix + "ODBC_PROCEDURE_INSERT_NO_PARAMS";
  std::string procedure_create = "CREATE OR REPLACE PROCEDURE " +
                                 procedure_name +
                                 "()\n"
                                 "BEGIN\n"
                                 "  INSERT INTO " +
                                 table_name +
                                 " VALUES(1, 'John Doe');\n"
                                 "  SELECT * FROM " +
                                 table_name +
                                 ";\n"
                                 "END";

  SQLRETURN status =
      SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
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
  status = SQLFreeStmt(conn->hstmt, SQL_CLOSE);
  CheckError(status, "SQLFreeStmt (close cursors)", conn);

  Procedure procedure(procedure_name);
  procedure.DropWithPrepare(conn);

  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLRowCount, WrongUpdateValidation) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name =
      kDatasetWithTablePrefix + "ROWCOUNT_WRONG_UPDATE_TEST_TABLE";

  std::string update_stmt =
      "UPDATE " + table_name +
      " SET StringField = \"Updated Row\" WHERE IntegerField = 4;";

  Table table(table_name);
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  table.InsertData(conn, kRowCountSampleData);

  auto status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)update_stmt.c_str(), SQL_NTS);
  EXPECT_EQ(status, SQL_NO_DATA);

  SQLLEN row_count;
  status = SQLRowCount(conn->hstmt, &row_count);
  CheckError(status, "SQLRowCount (Update)", conn);
  EXPECT_EQ(row_count, 0);

  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLRowCount, NonExistentTable) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string non_existent_table =
      kDatasetWithTablePrefix + "NON_EXISTENT_TABLE";
  std::string select_stmt = "SELECT COUNT(*) FROM " + non_existent_table;

  auto status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)select_stmt.c_str(), SQL_NTS);
  EXPECT_NE(status, SQL_SUCCESS);

  SQLLEN row_count;
  status = SQLRowCount(conn->hstmt, &row_count);
  EXPECT_NE(status, SQL_SUCCESS);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS
TEST(StatementTest, SQLPutDataMultipleDataTypes) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_PUT_DATA_MULTIPLE_TYPES_TEST";
  Table table(table_name);

  Schema schema{{"BoolField", "BOOL"},
                {"IntField", "INT64"},
                {"FloatField", "FLOAT64"},
                {"StringField", "STRING"},
                {"BinaryField", "BYTES"}};

  // Create table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, getSchemaStr(schema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert and validate data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  PutAllDataTypes(conn, table_name);
  EXPECT_EQ(SQLFreeStmt(conn->hstmt, SQL_CLOSE), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  ValidateAllPutData(conn, table_name);
  EXPECT_EQ(SQLFreeStmt(conn->hstmt, SQL_CLOSE), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Clean up
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#ifdef _WIN32
TEST(StatementTest, SQLPutDataStringDataChunks) {
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
  std::cout << "check_. 1 " << std::endl;

  // Insert a row
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (?, ?, ?)";
  std::vector<std::string> data;
  for (int i = 0; i < schema.size(); i++) {
    data.emplace_back(GetRandomString(50));
  }
  InsertDataWithSqlPut(conn, query, data);
  std::cout << "check_. last  " << std::endl;

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
#endif  // _WIN32

TEST(SQLRowCount, SameValueUpdate) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name =
      kDatasetWithTablePrefix + "ROWCOUNT_SAME_UPDATE_TEST_TABLE";

  std::string update_stmt =
      "UPDATE " + table_name +
      " SET StringField = \"Row 3\" WHERE IntegerField = 3;";

  Table table(table_name);
  table.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");

  table.InsertData(conn, kRowCountSampleData, false, true);

  auto status = ExecWithPrepare(conn, update_stmt);
  CheckError(status, "ExecWithPrepare (Update)", conn);

  SQLLEN row_count;
  status = SQLRowCount(conn->hstmt, &row_count);
  CheckError(status, "SQLRowCount (Update)", conn);
  EXPECT_EQ(row_count, 1);

  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
class SQLRowCountTest : public ::testing::TestWithParam<bool> {
 protected:
  std::shared_ptr<ODBCHandles> conn_;
  std::string table_name_;

  void SetUp() override {
    conn_ = std::make_shared<ODBCHandles>();
    EXPECT_EQ(Connect(kDefaultConnectionString, conn_), SQL_SUCCESS);
    table_name_ =
        kDatasetWithTablePrefix +
        (GetParam() ? "ROWCOUNT_TEST_TABLE_DIRECT" : "ROWCOUNT_TEST_TABLE");
    CreateTable(table_name_);
  }

  void TearDown() override {
    DropTable(table_name_);
    EXPECT_EQ(Disconnect(conn_), SQL_SUCCESS);
  }

  void CreateTable(std::string const& table_name) {
    Table table(table_name);
    if (GetParam()) {
      table.Create(
          conn_,
          "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
    } else {
      table.CreateWithPrepare(
          conn_,
          "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
    }
  }

  void DropTable(std::string const& table_name) {
    Table table(table_name);
    if (GetParam()) {
      table.Drop(conn_);
    } else {
      table.DropWithPrepare(conn_);
    }
  }

  void ExecuteAndValidate(std::string const& query, SQLLEN expected_row_count,
                          std::string const& step) {
    SQLRETURN status;
    if (GetParam()) {
      status = SQLExecDirect(conn_->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    } else {
      status = ExecWithPrepare(conn_, query);
    }
    CheckError(status, step, conn_);

    SQLLEN row_count;
    status = SQLRowCount(conn_->hstmt, &row_count);
    CheckError(status, "SQLRowCount (" + step + ")", conn_);
    EXPECT_EQ(row_count, expected_row_count);
  }
};

INSTANTIATE_TEST_SUITE_P(WithOrWithoutExecDirect, SQLRowCountTest,
                         testing::Values(false, true));

TEST_P(SQLRowCountTest, AllValidations) {
  SQLLEN row_count;
  auto status = SQLRowCount(conn_->hstmt, &row_count);
  CheckError(status, "SQLRowCount (Create)", conn_);
  EXPECT_EQ(row_count, -1);

  Table table(table_name_);
  table.InsertData(conn_, kRowCountSampleData, false, true);

  status = SQLRowCount(conn_->hstmt, &row_count);
  CheckError(status, "SQLRowCount (Insert)", conn_);
  EXPECT_EQ(row_count, 3);

  ExecuteAndValidate(
      "UPDATE " + table_name_ +
          " SET StringField = \"Updated Row\" WHERE IntegerField <= 3;",
      3, "Update");

  ExecuteAndValidate("SELECT * FROM " + table_name_, -1, "Select");

  status = SQLCloseCursor(conn_->hstmt);
  if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO) {
    CheckError(status, "SQLCloseCursor (SELECT)", conn_);
  }

  ExecuteAndValidate("DELETE FROM " + table_name_ + " WHERE IntegerField < 3;",
                     2, "Delete");
}

TEST(SQLRowCount, Async_Execute_stillExecuting) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query = "SELECT 1";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ASYNC_ENABLE,
                          (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  status = SQLExecute(conn->hstmt);
  std::string error;
  while (status == SQL_STILL_EXECUTING) {
    SQLLEN row_count;
    SQLRETURN rc_status = SQLRowCount(conn->hstmt, &row_count);
    EXPECT_EQ(rc_status, SQL_ERROR);
// On Linux, Simba driver returns the state of S1010 and on windows as
// HY010.Hence this tag is added to only check for windows.
#ifdef _WIN32
    ASSERT_EQ(SQL_SUCCESS,
              GetCancelErrorDetails("SQLRowCount", conn->hstmt, error));
    ASSERT_TRUE(absl::StrContains(error, "HY010"))
        << "SQLRowCount failed with unexpected error: " << error;
    ASSERT_TRUE(absl::StrContains(error, "Function sequence error"))
        << "SQLRowCount failed with unexpected error: " << error;
#endif  //_WIN32
    ExponentialBackoffPolicy backoff(std::chrono::milliseconds(10),
                                     std::chrono::milliseconds(100), 2);
    status = PollODBC(SQLExecute, backoff, conn->hstmt);
    if (SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLExecute", conn);
    } else {
      ASSERT_EQ(SQL_SUCCESS,
                GetCancelErrorDetails("SQLExecute", conn->hstmt, error));
      return;
    }
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// #ifndef BQ_DRIVER_INTEGRATION_TESTS
TEST(StatementTest, SQLParamData_InvalidStatementHandle) {
  SQLHSTMT invalid_handle = nullptr;
  SQLPOINTER data_ptr = nullptr;

  auto status = SQLParamData(invalid_handle, &data_ptr);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
}

TEST(StatementTest, SQLParamData_UnicodeWideChar) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::wstring const table_name =
      ToWStr(kDatasetWithTablePrefix) + L"ODBC_PARAM_DATA_UNICODE_TEST";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(StringField STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = L"INSERT INTO " + table_name + L" VALUES (?)";
  std::vector<SQLWCHAR> sql_wstr(query.begin(), query.end());
  sql_wstr.emplace_back(L'\0');
  EXPECT_EQ(SQLPrepareW(conn->hstmt, sql_wstr.data(), SQL_NTS), SQL_SUCCESS);

  int const large_data_size = (1024 * 512) / sizeof(wchar_t);
  std::wstring large_data(large_data_size, L'あ');
  SQLLEN param_size = SQL_LEN_DATA_AT_EXEC(large_data_size * sizeof(wchar_t));

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
                             SQL_WLONGVARCHAR, large_data.size(), 0,
                             (SQLPOINTER)1, 0, &param_size),
            SQL_SUCCESS);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);  // No ANSI version.

  SQLPOINTER data_ptr = nullptr;
  EXPECT_EQ(SQLParamData(conn->hstmt, &data_ptr), SQL_NEED_DATA);
  int const chunk_size = 64 * 1024 / sizeof(wchar_t);

  for (auto val = 0; val < large_data.size(); val += chunk_size) {
    int byte_left = large_data.size() - val;
    int byte_to_put = std::min(chunk_size, byte_left);
    EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)(large_data.data() + val),
                         byte_to_put * sizeof(wchar_t)),
              SQL_SUCCESS);
  }
  // adding sample data to remove sqlputdata dependency
  if(kIsBqDriver){
  auto te = large_data.size() * sizeof(SQLWCHAR);

  auto temp = SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
                            SQL_WLONGVARCHAR, large_data_size, 0,
                            large_data.data(), 0, reinterpret_cast<SQLLEN*>(&te));
  }
  // ------------------------------------------------------------------------------          
  EXPECT_EQ(SQLParamData(conn->hstmt, &data_ptr), SQL_SUCCESS);
  EXPECT_EQ(SQLFreeStmt(conn->hstmt, SQL_CLOSE), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLParamData_StringLengthMissMatch) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string const table_name =
      kDatasetWithTablePrefix + "ODBC_PARAM_DATA_LENGTH_MISMATCH_TEST";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(StringField STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (?)";
  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  std::string data = "Short data";
  SQLLEN data_len = 1024;  // incorrect total length
  SQLLEN data_ptr = SQL_LEN_DATA_AT_EXEC(data_len);
  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &data_ptr),
            SQL_SUCCESS);

  SQLCHAR need_long_data_len[2] = {0};
  SQLSMALLINT info_len = 0;
  EXPECT_EQ(SQLGetInfo(conn->hdbc, SQL_NEED_LONG_DATA_LEN, need_long_data_len,
                       sizeof(need_long_data_len), &info_len),
            SQL_SUCCESS);

  bool strict_validation = (need_long_data_len[0] == 'Y');
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);  // No ANSI version.

  SQLPOINTER param_ptr;
  EXPECT_EQ(SQLParamData(conn->hstmt, &param_ptr), SQL_NEED_DATA);
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_SUCCESS);
  if(kIsBqDriver){
    SQLLEN data_le = static_cast<SQLLEN>(data.size());  // Safe conversion
     EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0,  (SQLPOINTER)data.c_str(), 0,&data_le),
            SQL_SUCCESS);
  }
  if (strict_validation) {
    EXPECT_EQ(SQLParamData(conn->hstmt, &param_ptr), SQL_ERROR);
  } else {
    EXPECT_EQ(SQLParamData(conn->hstmt, &param_ptr), SQL_SUCCESS);
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// #endif  // BQ_DRIVER_INTEGRATION_TESTS
}  // namespace google::cloud::odbc_tests
