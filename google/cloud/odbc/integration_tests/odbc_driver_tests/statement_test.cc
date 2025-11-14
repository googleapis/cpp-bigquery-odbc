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
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "absl/strings/match.h"
#include <gmock/gmock.h>
using ::testing::Contains;
using ::testing::HasSubstr;

namespace google::cloud::odbc_tests {
using ::testing::StartsWith;

class StatementParameterizedTest : public ::testing::TestWithParam<bool> {};

INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutANSI, StatementParameterizedTest,
                         testing::Values(false, true));

class MultiStatementTest : public ::testing::TestWithParam<bool> {};
INSTANTIATE_TEST_SUITE_P(TestingWithOrWithoutPrepare, MultiStatementTest,
                         testing::Values(true, false));

class HTAPIParameterizedTest : public ::testing::TestWithParam<bool> {};
INSTANTIATE_TEST_SUITE_P(TestingWithOrWithouthtapi, HTAPIParameterizedTest,
                         testing::Values(false, true));

StdRows const kSampleData{
    {"Test String 1", 1, 1.1},      {"", 237, 2.22},
    {"Test String 3", NULL, 3.333}, {"Test String 4", 49, 0.0},
    {"Test String 5", 53, 5},       {"Test String 6", 698, 0.31},
    {"Test String 7", 12, 71.6},    {"Test String 8", 83, 8.8},
};

std::vector<std::string> const kSampleLargeStringData{
    {GetRandomString(100)},
    {GetRandomString(1800)},
    {GetRandomString(3000)},
    {GetRandomString(50)},
};

StdUnicodeRows const kUnicodeSampleData{
    {1, L"हिंदी", L"中国人"},
    {2, L"random string 1", L"random string 2"},
    // The test case doesn't fetch the values below
    {3, L"untested val 1", L"untested val 2"}};

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
        std::string data_str;
        // For the existing driver in Unicode mode on Windows, data is
        // received(by the application) encoded in CP_ACP but is transmitted as
        // UTF-8. In contrast, our driver consistently uses UTF-8 for Unicode
        // data, because we don't want to conform to the legacy CP_ACP encoding
#ifdef _WIN32
#ifdef BQ_DRIVER_INTEGRATION_TESTS

        data_str = Utf16ToUtf8(data.str_field2);
#else
        data_str = Utf16ToUtf8(data.str_field2, CP_ACP);
#endif  // BQ_DRIVER_INTEGRATION_TESTS
#else
        data_str = Utf16ToUtf8(data.str_field2);
#endif  //_WIN32
        input_col_values.emplace_back(data_str);
      }
    } else if (col_name.compare("Chinese")) {
      for (auto data : input_data) {
        std::string data_str;
#ifdef _WIN32
#ifdef BQ_DRIVER_INTEGRATION_TESTS

        data_str = Utf16ToUtf8(data.str_field1);
#else
        data_str = Utf16ToUtf8(data.str_field1, CP_ACP);
#endif  // BQ_DRIVER_INTEGRATION_TESTS
#else
        data_str = Utf16ToUtf8(data.str_field1);
#endif  //_WIN32

        input_col_values.emplace_back(data_str);
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

  SQLBIGINT int_data = 42;
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

  SQLBIGINT result_int = 0;
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
    EXPECT_EQ(SQLGetData(conn->hstmt, i + 1, validations[i].c_type,
                         validations[i].data_ptr, validations[i].data_size,
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
  // The IntegerField value is supposed to be unique and used as an index to
  // sort
  auto const query =
      "SELECT Hindi, Chinese FROM " + table_name + " ORDER BY IntegerField";

  SQLULEN max_rows = 9193;
  SQLRETURN status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, &max_rows, 0, nullptr);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_MAX_ROWS)", conn);
  EXPECT_EQ(max_rows, 0);

  // validating if SQL_ATTR_MAX_ROWS attr works.
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)2, 0);
  CheckError(status, "SQLSetStmtAttr(SQL_ATTR_MAX_ROWS)", conn);

  auto results = *FetchResults(conn, query, true);
  VerifyColumnWiseUnicodeResults({kUnicodeSampleData[0], kUnicodeSampleData[1]},
                                 results, std::vector<std::string>());
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

TEST(StatementTest, SQLExecDirect_htapi_basictypes) {
  SQLRETURN status;
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(
      Connect(kDefaultConnectionString +
                  ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0",
              conn),
      SQL_SUCCESS);
  std::string query =
      R"(SELECT )"
      R"(CAST(123 AS INT64) AS int_col1,)"
      R"('example string' AS str_col,)"
      R"(CAST(3.14 AS FLOAT64) AS float_col,)"
      R"(TRUE AS bool_col,)"
      R"(NUMERIC '12345.6789' AS numeric_col,)"
      R"(BIGNUMERIC '9876543210987654321.123456789012345678' AS bignumeric_col,)"
      R"(PARSE_JSON('{"name": "John", "age": 30}') AS json_col,)"
      R"(TIMESTAMP '2025-11-12 23:22:27.500' AS timestamp_col,)"
      R"(TIME(DATETIME '2024-06-01 12:34:56') AS time_col,)"
      R"(DATETIME(TIMESTAMP '2024-05-01 08:00:00') AS datetime_col,)"
      R"(DATE '2023-04-01' AS date_col,)"
      R"([3, 4, 5] AS array_int_col,)";

  RowWiseResults const kValidationData{
      {
          {0, "123"},
          {1, "example string"},
          {2, "3.14"},
          {3, kIsBqDriver ? "true" : "1"},
          {4, "12345.6789"},
          {5, "9876543210987654321.123456789012345678"},
          {6, "{\"age\":30,\"name\":\"John\"}"},
          {7, "2025-11-12 23:22:27.500000"},
          {8, kIsBqDriver ? "12:34:56" : "12:34:56.000000"},
          {9, kIsBqDriver
                  ? (kIsWin32 ? "2024-05-01T08:00:00" : "2024-05-01 08:00:00")
                  : "2024-05-01 08:00:00.000000"},
          {10, "2023-04-01"},
          {11, kIsBqDriver
                   ? (kIsWin32 ? "[\"3\",\"4\",\"5\"]" : "[3, 4, 5]")
                   : "{\"v\":[{\"v\":\"3\"},{\"v\":\"4\"},{\"v\":\"5\"}]}"},
      },
  };
  // The table name here doesn't matter because we didn't create one.
  Table table("Random_table_name");
  RowWiseResults const& results = table.Fetch(conn, query);
  VerifyRowWiseResults(results, kValidationData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecDirect_htapi_bytes_type) {
  SQLRETURN status;
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(
      Connect(kDefaultConnectionString +
                  ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0",
              conn),
      SQL_SUCCESS);
  std::string query =
      "SELECT CAST(b'\\xDE\\xAD\\xBE\\xEF' AS BYTES) AS bytes_col";

  status = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect(bytes)", conn);

  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);

  uint8_t buffer[16] = {0};
  SQLLEN indicator = 0;

  status = SQLGetData(conn->hstmt, 1, SQL_C_BINARY, buffer, sizeof(buffer),
                      &indicator);
  CheckError(status, "SQLGetData(SQL_C_BINARY)", conn);

  std::vector<uint8_t> expected = {0xDE, 0xAD, 0xBE, 0xEF};
  // TODO(sachnpro): We need to validate indicator as well
  // Right now it fails for our driver but passes for the existing one
  // EXPECT_EQ(indicator, expected.size());
  EXPECT_TRUE(std::equal(buffer, buffer + indicator, expected.begin()));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#ifdef BQ_DRIVER_INTEGRATION_TESTS

TEST(StatementTest, ReadAPI_RegionalEndpoint) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string connection_string =
      kDefaultConnectionString +
      ";PrivateServiceConnectUris=BIGQUERY=https://"
      "bigquery.us-east1.rep.googleapis.com/"
      ",READ_API=bigquerystorage.us-east1.rep.googleapis.com"
      ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0;"
      "UseDefaultLargeResultsDataset=0;"
      // The default LargeResultsDataSetId `_bqodbc_temp_tables` cannot be
      // created in us_east1 because it already exists in `US`
      "LargeResultsDataSetId=_bqodbc_temp_tables_us_east1";
  EXPECT_EQ(Connect(connection_string, conn), SQL_SUCCESS);
  // `ODBC_HTAPI_TESTING` table doesn't exist in us-east1
  std::string query =
      "SELECT * EXCEPT (index) FROM ODBC_HTAPI_TESTING.300_columns_string "
      "ORDER BY index LIMIT 10";

  SQLRETURN status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  // If the test is using the regional endpoint, we should see an error
  EXPECT_EQ(status, SQL_ERROR);

  SQLCHAR sql_state[6];
  SQLINTEGER native_error;
  SQLCHAR message[1024];
  SQLSMALLINT message_len;
  SQLRETURN diag_ret =
      SQLGetDiagRec(SQL_HANDLE_STMT, conn->hstmt, 1, sql_state, &native_error,
                    message, sizeof(message), &message_len);
  ASSERT_EQ(diag_ret, SQL_SUCCESS);

  std::string error_message(reinterpret_cast<char*>(message), message_len);
  EXPECT_STREQ(reinterpret_cast<char*>(sql_state), "HY000");
  EXPECT_THAT(error_message,
              HasSubstr("Error in non-idempotent operation: Not found: Dataset "
                        "bigquery-devtools-drivers:ODBC_HTAPI_TESTING was not "
                        "found in location us-east1"));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(connection_string, conn), SQL_SUCCESS);
  // `ODBC_HTAPI_TESTING_US_EAST1` table exists only in us-east1
  query =
      "SELECT * EXCEPT (index) FROM "
      "ODBC_HTAPI_TESTING_US_EAST1.300_columns_string "
      "ORDER BY index LIMIT 10";
  status = SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

TEST_P(HTAPIParameterizedTest, SQLExecDirect_with_pagination) {
  bool is_htapi = GetParam();
  SQLRETURN status;
  auto conn = std::make_shared<ODBCHandles>();
  std::string connection_string = kDefaultConnectionString;
  int limit = 3000;
  if (is_htapi) {
    connection_string =
        kDefaultConnectionString +
        ";AllowHtapiForLargeResults=1;HTAPI_ActivationThreshold=0";
    limit = 500;
  }
  EXPECT_EQ(Connect(connection_string, conn), SQL_SUCCESS);

  // This table has 300 string columns and one for `index`
  // The values follow this pattern: col<col_index>_row<row_index>
  std::string query =
      "SELECT * EXCEPT (index) FROM ODBC_HTAPI_TESTING.300_columns_string "
      "ORDER BY index LIMIT " +
      std::to_string(limit) + ";";
  // The table name here doesn't matter because we didn't create one.
  Table table("Random_table_name");
  RowWiseResults const& results = table.Fetch(conn, query);
  int const expected_num_cols = 300;
  ASSERT_EQ(results.size(), limit) << "Row count mismatch.";
  for (int i = 0; i < limit; ++i) {
    Row const& row = results[i];
    ASSERT_EQ(row.size(), expected_num_cols)
        << "Row " << i << ": Column count mismatch.";
    for (int j = 0; j < expected_num_cols; ++j) {
      // Construct the expected string: "col<j>_row<i>"
      std::string expected_value =
          "col" + std::to_string(j) + "_row" + std::to_string(i);
      ASSERT_TRUE(row.count(j))
          << "Row " << i << ": Missing expected column with index " << j;
      ASSERT_EQ(row.at(j), expected_value)
          << "Row " << i << ", Col " << j << ": Value mismatch.";
    }
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecDirectW) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::wstring const table_name =
      ToWStr(kDatasetWithTablePrefix) + L"ODBC_INSERT_SQLEXECDIRECTW_TEST";
  Table table(table_name);

  table.CreateW(conn, L"(string_field STRING)");

  std::wstring const string_field = L"Some Test String नमस्ते";
  std::wstring query =
      L"INSERT INTO " + table_name + L" VALUES ('" + string_field + L"')";
  std::vector<SQLWCHAR> insert_stmt(query.begin(), query.end());
  insert_stmt.emplace_back(L'\0');

  SQLRETURN status =
      SQLExecDirectW(conn->hstmt, (SQLWCHAR*)insert_stmt.data(), SQL_NTS);
  CheckError(status, "SQLExecDirectW", conn);

  table.DropW(conn);

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

TEST(StatementTest, Check_SQL_Primary_key) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLRETURN ret = SQLPrimaryKeys(
      conn->hstmt, (SQLCHAR*)"bigquery-devtools-drivers", SQL_NTS,
      (SQLCHAR*)"INTEGRATION_TESTS", SQL_NTS, (SQLCHAR*)"Test_Table", SQL_NTS);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  SQLSMALLINT col_count = 0;
  ret = SQLNumResultCols(conn->hstmt, &col_count);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(col_count, 6);

  struct ExpectedCol {
    char const* name;
    SQLSMALLINT type;
    SQLULEN size;
    SQLSMALLINT decimals;
    SQLSMALLINT nullable;
  };

  ExpectedCol expected[] = {
      {"TABLE_CAT", SQL_WVARCHAR, 128, 0, SQL_NULLABLE},
      {"TABLE_SCHEM", SQL_WVARCHAR, 1024, 0, SQL_NULLABLE},
      {"TABLE_NAME", SQL_WVARCHAR, 1024, 0, SQL_NO_NULLS},
      {"COLUMN_NAME", SQL_WVARCHAR, 128, 0, SQL_NO_NULLS},
      {"KEY_SEQ", SQL_SMALLINT, 5, 0, SQL_NO_NULLS},
      {"PK_NAME", SQL_WVARCHAR, 128, 0, SQL_NULLABLE},
  };

  for (SQLSMALLINT i = 1; i <= col_count; ++i) {
    SQLCHAR col_name[256] = {0};
    SQLSMALLINT name_len = 0;
    SQLSMALLINT data_type = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    SQLULEN col_size = 0;

    ret = SQLDescribeCol(conn->hstmt, i, col_name, sizeof(col_name), &name_len,
                         &data_type, &col_size, &decimal_digits, &nullable);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));

    auto const& exp = expected[i - 1];

    EXPECT_STREQ(reinterpret_cast<char const*>(col_name), exp.name)
        << "Column " << i << " name mismatch";
    EXPECT_EQ(data_type, exp.type) << "Column " << i << " type mismatch";
    EXPECT_EQ(col_size, exp.size) << "Column " << i << " size mismatch";
    EXPECT_EQ(decimal_digits, exp.decimals)
        << "Column " << i << " decimal digits mismatch";
    EXPECT_EQ(nullable, exp.nullable)
        << "Column " << i << " nullable flag mismatch";
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, Check_SQL_Foreign_key) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLRETURN ret = SQLForeignKeys(
      conn->hstmt, (SQLCHAR*)"bigquery-devtools-drivers", SQL_NTS,
      (SQLCHAR*)"INTEGRATION_TESTS", SQL_NTS, (SQLCHAR*)"Test_Table", SQL_NTS,
      NULL, 0, NULL, 0, NULL, 0);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));

  SQLSMALLINT col_count = 0;
  ret = SQLNumResultCols(conn->hstmt, &col_count);
  ASSERT_TRUE(SQL_SUCCEEDED(ret));
  EXPECT_EQ(col_count, 14);

  struct ExpectedCol {
    char const* name;
    SQLSMALLINT type;
    SQLULEN size;
    SQLSMALLINT decimals;
    SQLSMALLINT nullable;
  };

  ExpectedCol expected[] = {
      {"PKTABLE_CAT", SQL_WVARCHAR, 128, 0, SQL_NULLABLE},
      {"PKTABLE_SCHEM", SQL_WVARCHAR, 1024, 0, SQL_NULLABLE},
      {"PKTABLE_NAME", SQL_WVARCHAR, 1024, 0, SQL_NO_NULLS},
      {"PKCOLUMN_NAME", SQL_WVARCHAR, 128, 0, SQL_NO_NULLS},
      {"FKTABLE_CAT", SQL_WVARCHAR, 128, 0, SQL_NULLABLE},
      {"FKTABLE_SCHEM", SQL_WVARCHAR, 1024, 0, SQL_NULLABLE},
      {"FKTABLE_NAME", SQL_WVARCHAR, 1024, 0, SQL_NO_NULLS},
      {"FKCOLUMN_NAME", SQL_WVARCHAR, 128, 0, SQL_NO_NULLS},
      {"KEY_SEQ", SQL_SMALLINT, 5, 0, SQL_NO_NULLS},
      {"UPDATE_RULE", SQL_SMALLINT, 5, 0, SQL_NULLABLE},
      {"DELETE_RULE", SQL_SMALLINT, 5, 0, SQL_NULLABLE},
      {"FK_NAME", SQL_WVARCHAR, 128, 0, SQL_NULLABLE},
      {"PK_NAME", SQL_WVARCHAR, 128, 0, SQL_NULLABLE},
      {"DEFERRABILITY", SQL_SMALLINT, 5, 0, SQL_NULLABLE},
  };

  for (SQLSMALLINT i = 1; i <= col_count; ++i) {
    SQLCHAR col_name[256] = {0};
    SQLSMALLINT name_len = 0;
    SQLSMALLINT data_type = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    SQLULEN col_size = 0;

    ret = SQLDescribeCol(conn->hstmt, i, col_name, sizeof(col_name), &name_len,
                         &data_type, &col_size, &decimal_digits, &nullable);
    ASSERT_TRUE(SQL_SUCCEEDED(ret));

    auto const& exp = expected[i - 1];

    EXPECT_STREQ(reinterpret_cast<char const*>(col_name), exp.name)
        << "Column " << i << " name mismatch";
    EXPECT_EQ(data_type, exp.type) << "Column " << i << " type mismatch";
    EXPECT_EQ(col_size, exp.size) << "Column " << i << " size mismatch";
    EXPECT_EQ(decimal_digits, exp.decimals)
        << "Column " << i << " decimal digits mismatch";
    EXPECT_EQ(nullable, exp.nullable)
        << "Column " << i << " nullable flag mismatch";
  }

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

TEST(StatementTest, SQLFetchScroll_All_Types) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto const query =
      "SELECT StringField FROM UNNEST([STRUCT(\"Test String 1\" AS "
      "StringField, 1 AS IntegerField, 1.1 AS FloatField),STRUCT(NULL AS "
      "StringField, 237 AS IntegerField, 2.22 AS FloatField),     "
      "STRUCT(\"Test String 3\" AS StringField, NULL AS IntegerField, 3.333 AS "
      "FloatField),     STRUCT(\"Test String 4\" AS StringField, 49 AS "
      "IntegerField, NULL AS FloatField),     STRUCT(\"Test String 5\" AS "
      "StringField, 53 AS IntegerField, 5 AS FloatField),     STRUCT(\"Test "
      "String 6\" AS StringField, 698 AS IntegerField, 0.31 AS FloatField),    "
      " STRUCT(\"Test String 7\" AS StringField, 12 AS IntegerField, 71.6 AS "
      "FloatField),     STRUCT(\"Test String 8\" AS StringField, 83 AS "
      "IntegerField, 8.8 AS FloatField) ])";

  SQLRETURN status;
  SQLCHAR buf_sql_fetch_absolute[kBufferLength];
  SQLCHAR buf_sql_fetch_relative[kBufferLength];
  SQLCHAR buf_sql_fetch_prior[kBufferLength];
  SQLCHAR buf_sql_fetch_first[kBufferLength];
  SQLCHAR buf_sql_fetch_last[kBufferLength];
  SQLCHAR buf_sql_fetch_bookmark[kBufferLength];
  SQLSMALLINT string_length_ptr;

  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);  // No ANSI version
  CheckError(status, "SQLExecute", conn);

  // Absolute position
  status = SQLFetchScroll(conn->hstmt, SQL_FETCH_ABSOLUTE, 0);
  EXPECT_EQ(status, SQL_ERROR);
  status = SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1,
                           SQL_DIAG_MESSAGE_TEXT, &buf_sql_fetch_absolute,
                           kBufferLength, &string_length_ptr);

  std::string actual_message = reinterpret_cast<char*>(buf_sql_fetch_absolute);
  EXPECT_THAT(actual_message, ::testing::HasSubstr("Fetch type not supported"));

  // Fetch the next row
  status = SQLFetchScroll(conn->hstmt, SQL_FETCH_NEXT, 0);
  CheckError(status, "SQLFetchScroll - SQL_FETCH_NEXT", conn);
  EXPECT_EQ(status, SQL_SUCCESS);

  // Fetch Relative
  status = SQLFetchScroll(conn->hstmt, SQL_FETCH_RELATIVE, 2);
  EXPECT_EQ(status, SQL_ERROR);
  status = SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1,
                           SQL_DIAG_MESSAGE_TEXT, &buf_sql_fetch_relative,
                           kBufferLength, &string_length_ptr);
  actual_message = reinterpret_cast<char*>(buf_sql_fetch_relative);
  EXPECT_THAT(actual_message, ::testing::HasSubstr("Fetch type not supported"));

  // Fetch row backward
  status = SQLFetchScroll(conn->hstmt, SQL_FETCH_PRIOR, 3);
  EXPECT_EQ(status, SQL_ERROR);
  status =
      SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, SQL_DIAG_MESSAGE_TEXT,
                      &buf_sql_fetch_prior, kBufferLength, &string_length_ptr);
  actual_message = reinterpret_cast<char*>(buf_sql_fetch_prior);
  EXPECT_THAT(actual_message, ::testing::HasSubstr("Fetch type not supported"));

  // Fetch First Row
  status = SQLFetchScroll(conn->hstmt, SQL_FETCH_FIRST, 0);
  EXPECT_EQ(status, SQL_ERROR);
  status =
      SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, SQL_DIAG_MESSAGE_TEXT,
                      &buf_sql_fetch_first, kBufferLength, &string_length_ptr);
  actual_message = reinterpret_cast<char*>(buf_sql_fetch_first);
  EXPECT_THAT(actual_message, ::testing::HasSubstr("Fetch type not supported"));

  // Fetch Last Row
  status = SQLFetchScroll(conn->hstmt, SQL_FETCH_LAST, 0);
  EXPECT_EQ(status, SQL_ERROR);
  status =
      SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, SQL_DIAG_MESSAGE_TEXT,
                      &buf_sql_fetch_last, kBufferLength, &string_length_ptr);
  actual_message = reinterpret_cast<char*>(buf_sql_fetch_last);
  EXPECT_THAT(actual_message, ::testing::HasSubstr("Fetch type not supported"));

  // Fetch Bookmark row
  status = SQLFetchScroll(conn->hstmt, SQL_FETCH_BOOKMARK, 0);
  EXPECT_EQ(status, SQL_ERROR);
  status = SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, SQL_DIAG_SQLSTATE,
                           &buf_sql_fetch_bookmark, kBufferLength,
                           &string_length_ptr);
  actual_message = reinterpret_cast<char*>(buf_sql_fetch_bookmark);
  EXPECT_EQ(actual_message, "HY106");

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, CheckSqlGetData) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_CHECK_SQLGETDATA_LARGE_DATA";
  Table table(table_name);
  auto conn = std::make_shared<ODBCHandles>();
  // create table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(conn, "(id INT64, str_col STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // insert data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertStrData(conn, kSampleLargeStringData, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // fetch and validate data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto select_stmt = "SELECT  str_col FROM " + table_name + " ORDER BY id";

  auto status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)select_stmt.c_str(), SQL_NTS);
  ASSERT_TRUE(SQL_SUCCEEDED(status));

  SQLSMALLINT num_cols = 0;
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  ASSERT_TRUE(SQL_SUCCEEDED(status));
  ASSERT_EQ(num_cols, 1);

  int row_count = 0;
  while ((status = SQLFetch(conn->hstmt)) != SQL_NO_DATA) {
    ASSERT_TRUE(SQL_SUCCEEDED(status));

    for (SQLUSMALLINT col = 1; col <= num_cols; col++) {
      std::wstring col_data;
      SQLLEN indicator = 0;
      SQLWCHAR buf[500];  // small buffer → forces truncation on long strings
      do {
        memset(buf, 0, sizeof(buf));
        status = SQLGetData(conn->hstmt, col, SQL_C_WCHAR, buf, sizeof(buf),
                            &indicator);
        CheckError(status, "SQLGetData", conn);

        if (indicator == SQL_NULL_DATA) {
          col_data = L"<NULL>";
          break;
        }
        ASSERT_TRUE(SQL_SUCCEEDED(status) || status == SQL_SUCCESS_WITH_INFO);
        size_t chunk_len = 0;
        while (buf[chunk_len] != 0 &&
               chunk_len < (sizeof(buf) / sizeof(SQLWCHAR))) {
          col_data.push_back(static_cast<wchar_t>(buf[chunk_len]));
          chunk_len++;
        }

      } while (status == SQL_SUCCESS_WITH_INFO);
      std::string const& expected_str = kSampleLargeStringData[row_count];
      std::wstring expected_data(expected_str.begin(), expected_str.end());

      EXPECT_EQ(col_data, expected_data);
      EXPECT_GT(indicator, 0);
    }
    row_count++;
  }
  ASSERT_EQ(row_count, kSampleLargeStringData.size());
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
  table_ansi.CreateWithPrepare(
      conn, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
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

TEST(StatementTest, SQLGetData_insufficientBuffer) {
  auto conn = std::make_shared<ODBCHandles>();
  auto table_name = kDatasetWithTablePrefix + "ODBC_INSUFFICIENT_BUFFER_TEST";
  Table table(table_name);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn,
      "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64, "
      "JsonField JSON,StructField STRUCT<int_value BIGINT, double_value "
      "FLOAT64, string_value STRING>, ByteField1 BYTES, ByteField2 BYTES)");

  // Insert test data
  auto insert_query =
      "INSERT INTO " + table_name +
      " (StringField, IntegerField, FloatField, JsonField, StructField, "
      "ByteField1, ByteField2) VALUES "
      "('TestString', 42, 3.14, JSON '{\"age\": 90, \"name\": \"Ram\"}', "
      "STRUCT(1,2,'TestStruct'), B'0x48656C6C6F', B'0x48656C6C6F')";
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)insert_query.c_str(),
                        insert_query.size()),
             "SQLPrepare", conn);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  // Prepare and execute select query
  auto select_query =
      "SELECT StringField, IntegerField, FloatField, JsonField, StructField, "
      "ByteField1, ByteField2 "
      "FROM " +
      table_name;
  CheckError(SQLPrepare(conn->hstmt, (SQLCHAR*)select_query.c_str(),
                        select_query.size()),
             "SQLPrepare", conn);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_SUCCESS);

  // Fetch and verify data
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);

  SQLCHAR string_data[256];
  SQLCHAR json_data[256];
  SQLCHAR json_data2[256];
  SQLCHAR struct_data[256];
  SQLCHAR byte_data_char[256];
  SQLCHAR byte_data_binary[256];
  SQLCHAR buf[kBufferLength];
  SQLSMALLINT string_length_ptr;
  int int_data;
  double float_data;
  SQLLEN int_len, float_len, string_len, json_len, struct_len, byte_len;
  EXPECT_EQ(SQLGetData(conn->hstmt, 1, SQL_C_CHAR, string_data,
                       sizeof(string_data), &string_len),
            SQL_SUCCESS);
  EXPECT_STREQ((char*)string_data, "TestString");

  EXPECT_EQ(SQLGetData(conn->hstmt, 2, SQL_C_LONG, &int_data, 0, &int_len),
            SQL_SUCCESS);
  EXPECT_EQ(int_data, 42);

  EXPECT_EQ(
      SQLGetData(conn->hstmt, 3, SQL_C_DOUBLE, &float_data, 0, &float_len),
      SQL_SUCCESS);
  EXPECT_EQ(float_data, 3.14);

  EXPECT_EQ(SQLGetData(conn->hstmt, 4, SQL_C_CHAR, json_data, 10, &json_len),
            SQL_SUCCESS_WITH_INFO);
  EXPECT_STREQ((char*)json_data, "{\"age\":90");

  EXPECT_EQ(SQLGetData(conn->hstmt, 4, SQL_C_CHAR, json_data, 10, &json_len),
            SQL_SUCCESS_WITH_INFO);
  EXPECT_STREQ((char*)json_data, ",\"name\":\"");

  EXPECT_EQ(SQLGetData(conn->hstmt, 4, SQL_C_CHAR, json_data, 10, &json_len),
            SQL_SUCCESS);
  EXPECT_STREQ((char*)json_data, "Ram\"}");

  EXPECT_EQ(
      SQLGetData(conn->hstmt, 5, SQL_C_CHAR, struct_data, 20, &struct_len),
      SQL_SUCCESS_WITH_INFO);

  EXPECT_STREQ((char*)struct_data, "{\"v\":{\"f\":[{\"v\":\"1\"");
  EXPECT_EQ(SQLGetData(conn->hstmt, 4, SQL_C_CHAR, json_data2, 10, &json_len),
            SQL_SUCCESS_WITH_INFO);
  EXPECT_STREQ((char*)json_data2, "{\"age\":90");

  EXPECT_EQ(SQLGetData(conn->hstmt, 4, SQL_C_BINARY, json_data2, 10, &json_len),
            SQL_ERROR);

  auto status =
      SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, SQL_DIAG_MESSAGE_TEXT,
                      &buf, kBufferLength, &string_length_ptr);
  EXPECT_THAT((char*)buf, HasSubstr("Changing types between multipart "
                                    "SQLGetData() calls is not supported"));

  EXPECT_EQ(
      SQLGetData(conn->hstmt, 6, SQL_C_CHAR, byte_data_char, 5, &byte_len),
      SQL_SUCCESS_WITH_INFO);
  // For the google driver, raw bytes are returned directly.
  std::string expected_val;
  if (kIsBqDriver) {
    expected_val = "MHg0";
  } else {
    expected_val = "3078";
  }
  EXPECT_STREQ((char*)byte_data_char, expected_val.c_str());

  EXPECT_EQ(
      SQLGetData(conn->hstmt, 6, SQL_C_CHAR, byte_data_char, 5, &byte_len),
      SQL_SUCCESS_WITH_INFO);
  // For the google driver, raw bytes are returned directly.
  std::string expected_str;
  if (kIsBqDriver) {
    // Build expected value dynamically to by-pass checkers error.
    expected_str = std::string({'O', 'D', 'Y', '1'});
  } else {
    expected_str = "3438";
  }
  EXPECT_STREQ((char*)byte_data_char, expected_str.c_str());

  EXPECT_EQ(
      SQLGetData(conn->hstmt, 7, SQL_C_BINARY, byte_data_binary, 5, &byte_len),
      SQL_SUCCESS_WITH_INFO);
  EXPECT_THAT((char*)byte_data_binary, HasSubstr("0x486"));

  EXPECT_EQ(
      SQLGetData(conn->hstmt, 7, SQL_C_BINARY, byte_data_binary, 5, &byte_len),
      SQL_SUCCESS_WITH_INFO);
  EXPECT_THAT((char*)byte_data_binary, HasSubstr("56C6C"));

  EXPECT_EQ(
      SQLGetData(conn->hstmt, 7, SQL_C_BINARY, byte_data_binary, 5, &byte_len),
      SQL_SUCCESS);
  EXPECT_THAT((char*)byte_data_binary, HasSubstr("6F"));

  SQLFreeStmt(conn->hstmt, SQL_CLOSE);
  table.DropWithPrepare(conn);
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

TEST(StatementTest, SQLGetCursorNameAndW) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "SELECT 1;";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Test the ANSI version
  SQLCHAR cursor_name_ret[kBufferLength];
  status = SQLGetCursorName(conn->hstmt, cursor_name_ret, kBufferLength, NULL);
  CheckError(status, "SQLGetCursorName", conn);
  std::string actual_a = reinterpret_cast<char*>(cursor_name_ret);
  EXPECT_THAT(actual_a, StartsWith("SQL_CUR"));

  // Test the Wide version
  SQLWCHAR cursor_name_ret_w[kBufferLength];
  status =
      SQLGetCursorNameW(conn->hstmt, cursor_name_ret_w, kBufferLength, NULL);
  CheckError(status, "SQLGetCursorNameW", conn);
  std::string actual_w = ConvertSQLWCHARToString(cursor_name_ret_w, NULL);
  EXPECT_THAT(actual_w, StartsWith("SQL_CUR"));

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

  if (!kIsUnixODBC) {
    // Skipping this, as unixODBC the driver manager is unable to reset
    // descriptors for both the existing driver and the internal driver.
    EXPECT_EQ(SQL_DESC_ALLOC_AUTO, alloc_type);
    EXPECT_EQ(arr_size_implicit, arr_size_new);
  }

  EXPECT_EQ(SQLFreeHandle(SQL_HANDLE_DESC, desc_expl), SQL_SUCCESS);
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

  std::string query = "SELECT * from INTEGRATION_TESTS.Test_Table where id=?";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  status =
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

TEST(SQLNumResultCols, ValidateSimpleResultSets) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "SELECT 1";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));

  SQLSMALLINT columnCount;
  status = SQLNumResultCols(conn->hstmt, &columnCount);
  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(columnCount, 1);

  SQLFreeStmt(conn->hstmt, SQL_CLOSE);

  std::string query2 = "SELECT id, name from INTEGRATION_TESTS.Test_Table";
  char read_stmt2[kBufferLength];
  StrToChar(read_stmt2, query2);
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt2, strlen(read_stmt2));
  CheckError(status, "SQLPrepare", conn);

  status = SQLNumResultCols(conn->hstmt, &columnCount);
  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(columnCount, 2);
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

#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("address")))
#endif
// Both Prepare and Execute are async.
void SQLCancel_Prepare_Execute_CancelAsync_StillExecuting() {
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
// TODO(b/400632420): Validate and compare SQLPrepare and SQLCancel return
// status
#ifndef _WIN32
      // In unixODBC, the status code is `HY010`, but in other Driver Managers,
      // it is `S1010`. Updating it to match.
      ASSERT_TRUE(absl::StrContains(error, "S1010") ||
                  absl::StrContains(error, "HY010"));
      ASSERT_TRUE(absl::StrContains(error, "Function sequence error"))
          << "SQLExecute failed with unexpected error: " << error;
#endif  // _WIN32
    }
  }
}

TEST(SQLCancel, Prepare_Execute_CancelAsync_StillExecuting) {
  SQLCancel_Prepare_Execute_CancelAsync_StillExecuting();
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
    SQLLEN column_size;

    // Validate column attributes
    SQLColAttribute(conn->hstmt, i, SQL_DESC_NAME, column_name,
                    sizeof(column_name), &name_length, NULL);
    SQLColAttribute(conn->hstmt, i, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL,
                    &column_size);

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
    SQLLEN column_size;

    // Validate column attributes
    SQLColAttribute(conn->hstmt, i, SQL_DESC_NAME, column_name,
                    sizeof(column_name), &name_length, NULL);
    SQLColAttribute(conn->hstmt, i, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL,
                    &column_size);

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

  Schema schema{{"TextField1", "STRING"}, {"TextField2", "STRING"}};

  // Create table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, getSchemaStr(schema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Prepare and bind parameters
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (? ,?)";

  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  std::string data = "SomeData";

  // Scenario 1: Call SQLPutData with an invalid handle
  SQLHSTMT invalid_stmt = nullptr;
  EXPECT_EQ(SQLPutData(invalid_stmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_INVALID_HANDLE);

  // Scenario 2: Call SQLPutData before SQLExecute (sequence issue)
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_ERROR);  // Should fail before SQLExecute

  SQLLEN indicator = SQL_DATA_AT_EXEC;
  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &indicator),
            SQL_SUCCESS);

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 100, 0, nullptr, 0, &indicator),
            SQL_SUCCESS);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);

  // Scenario 3: Call SQLPutData with null data and valid size
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_NEED_DATA);
  EXPECT_EQ(SQLPutData(conn->hstmt, nullptr, data.size()), SQL_ERROR);

  // Scenario 4: Call SQLPutData with valid data and valid size
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_SUCCESS);

  std::string data1 = std::string(101, 'Z');
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_NEED_DATA);

  // Scenario 5: Data Truncation - inserting data that exceeds column size
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data1.c_str(), data1.size()),
            SQL_ERROR);

  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_ERROR);

  // Cleanup before disconnecting
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLPutDataSpecialCases) {
  // Test SQLPutData error scenarios with proper sequence and data validation

  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_PUT_DATA_SPECIAL_CASES_TEST";
  Table table(table_name);

  Schema schema{{"IntField1", "INT64"},
                {"TextField2", "STRING"},
                {"TextField3", "STRING"},
                {"TextField4", "STRING"},
                {"TextField5", "STRING"}};

  // Create table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, getSchemaStr(schema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Prepare and bind parameters
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (?, ?, ?, ?, ?)";

  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  // TODO(b/404480454): Issue with SQLBindParameter When Using
  // the Same Indicator Pointer for Multiple Parameters
  SQLLEN indicator = SQL_DATA_AT_EXEC;
  int num_params = schema.size();
  int param;
  // Bind parameters
  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                             SQL_BIGINT, 0, 0, &param, 0, &indicator),
            SQL_SUCCESS);

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &indicator),
            SQL_SUCCESS);

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 3, SQL_PARAM_INPUT, SQL_C_WCHAR,
                             SQL_WLONGVARCHAR, 0, 0, nullptr, 0, &indicator),
            SQL_SUCCESS);

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &indicator),
            SQL_SUCCESS);

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &indicator),
            SQL_SUCCESS);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);

  // Scenario 1: Call SQLPutData with mismatched data type
  std::string data = "SomeData";
  SQLPOINTER param_target = nullptr;
  EXPECT_EQ(SQLParamData(conn->hstmt, &param_target), SQL_NEED_DATA);
  EXPECT_EQ(static_cast<SQLPOINTER>(&param), param_target);
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_SUCCESS);

  // Scenario 2: Invalid Data in Different Encoding
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_NEED_DATA);
  std::wstring latin1_data =
      L"Latin-1 data \xE9";  // Character 'é' in Latin-1 encoding
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)latin1_data.c_str(),
                       latin1_data.size()),
            SQL_SUCCESS);

  // Scenario 3: Call SQLPutData with null data and a negative size
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_NEED_DATA);
  EXPECT_EQ(SQLPutData(conn->hstmt, nullptr, SQL_NULL_DATA), SQL_SUCCESS);

  // Scenario 4: Call SQLPutData with size as 0
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_NEED_DATA);
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), 0), SQL_SUCCESS);

  // Scenario 5: Call SQLPutData with valid data and a negative size
  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_NEED_DATA);
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), SQL_NULL_DATA),
            SQL_SUCCESS);

  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_SUCCESS);

  // Cleanup before disconnecting
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

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

TEST(SQLMoreResults, ProcedureWithNoParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name =
      kDatasetWithTablePrefix + "ODBC_SCRIPTS_NO_PARAMS_TABLE";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(ID INT64, Name STRING)");

  std::string procedure_name =
      kDatasetWithTablePrefix + "ODBC_PROCEDURE_INSERT_NO_PARAMS";
  std::string procedure_create =
      "CREATE OR REPLACE PROCEDURE " + procedure_name +
      "()\n"
      "BEGIN\n"
      "  INSERT INTO " +
      table_name +
      " VALUES(1, 'John Doe') , (2, 'Jack'),(3, 'Jameson');\n"
      "  SELECT * FROM " +
      table_name +
      ";\n"
      "END";

  SQLRETURN status =
      SQLPrepare(conn->hstmt, (SQLCHAR*)procedure_create.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  ASSERT_EQ(SQLSetStmtAttr(conn->hstmt, SQL_ATTR_MAX_ROWS, (SQLPOINTER)1, 0),
            SQL_SUCCESS);

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

  const SQLULEN ROW_ARRAY_SIZE = 10;
  SQLULEN rows_fetched = 0;
  std::vector<SQLUSMALLINT> row_status(ROW_ARRAY_SIZE, 0);

  auto rc = SQLSetStmtAttr(conn_->hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                           (SQLPOINTER)ROW_ARRAY_SIZE, 0);
  ASSERT_EQ(rc, SQL_SUCCESS);
  rc =
      SQLSetStmtAttr(conn_->hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &rows_fetched, 0);
  ASSERT_EQ(rc, SQL_SUCCESS);
  rc = SQLSetStmtAttr(conn_->hstmt, SQL_ATTR_ROW_STATUS_PTR, row_status.data(),
                      0);
  ASSERT_EQ(rc, SQL_SUCCESS);

  rc = SQLFetchScroll(conn_->hstmt, SQL_FETCH_NEXT, 0);
  ASSERT_EQ(rc, SQL_SUCCESS) << "The first SQLFetchScroll call failed, "
                                "indicating a cursor initialization error.";

  ASSERT_EQ(rows_fetched, 3) << "Driver failed to update the rows_fetched "
                                "pointer with the correct count.";

  for (size_t i = 0; i < rows_fetched; i++) {
    ASSERT_EQ(row_status[i], SQL_ROW_SUCCESS)
        << "Row " << (i + 1) << " status is not SUCCESS.";
  }

  for (size_t i = 3; i < ROW_ARRAY_SIZE; i++) {
    ASSERT_EQ(row_status[i], SQL_ROW_NOROW)
        << "Row " << (i + 1) << " was expected to be SQL_ROW_NOROW.";
  }

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
      EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
      return;
    }
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

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
  table.CreateWithPrepare(conn, "(StringField1 STRING, StringField2 STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = L"INSERT INTO " + table_name + L" VALUES (?, ?)";
  std::vector<SQLWCHAR> sql_wstr(query.begin(), query.end());
  sql_wstr.emplace_back(L'\0');
  EXPECT_EQ(SQLPrepareW(conn->hstmt, sql_wstr.data(), SQL_NTS), SQL_SUCCESS);

  int const large_data_size = (1024 * 512) / sizeof(wchar_t);
  std::wstring large_data1(large_data_size, L'あ');
  std::wstring large_data2(large_data_size, L'い');

  SQLLEN param_size1 = SQL_LEN_DATA_AT_EXEC(large_data_size * sizeof(wchar_t));
  SQLLEN param_size2 = SQL_LEN_DATA_AT_EXEC(large_data_size * sizeof(wchar_t));

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
                             SQL_WLONGVARCHAR, large_data1.size(), 0,
                             (SQLPOINTER)1, 0, &param_size1),
            SQL_SUCCESS);

  EXPECT_EQ(SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_WCHAR,
                             SQL_WLONGVARCHAR, large_data2.size(), 0, nullptr,
                             0, &param_size2),
            SQL_SUCCESS);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);  // No ANSI version.

  SQLPOINTER data_ptr = nullptr;
  EXPECT_EQ(SQLParamData(conn->hstmt, &data_ptr), SQL_NEED_DATA);

  // Send data for the first parameter
  int const chunk_size = 64 * 1024 / sizeof(wchar_t);
  for (auto val = 0; val < large_data1.size(); val += chunk_size) {
    int byte_left = large_data1.size() - val;
    int byte_to_put = std::min(chunk_size, byte_left);
    EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)(large_data1.data() + val),
                         byte_to_put * sizeof(wchar_t)),
              SQL_SUCCESS);
  }

  // Send data for the second parameter
  EXPECT_EQ(SQLParamData(conn->hstmt, &data_ptr), SQL_NEED_DATA);

  for (auto val = 0; val < large_data2.size(); val += chunk_size) {
    int byte_left = large_data2.size() - val;
    int byte_to_put = std::min(chunk_size, byte_left);
    EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)(large_data2.data() + val),
                         byte_to_put * sizeof(wchar_t)),
              SQL_SUCCESS);
  }
  EXPECT_EQ(SQLParamData(conn->hstmt, &data_ptr), SQL_SUCCESS);
  EXPECT_EQ(SQLFreeStmt(conn->hstmt, SQL_CLOSE), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLParamData_ValidateSQLFetchStates) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_PARAM_DATA_VALIDATE_STATEMENT_STATE";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(StringField STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // insert data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (?)";
  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  std::string data = GetRandomString(100);
  SQLLEN data_len = SQL_LEN_DATA_AT_EXEC(100);
  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &data_len),
            SQL_SUCCESS);

  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);

  SQLPOINTER param_ptr = nullptr;
  EXPECT_EQ(SQLParamData(conn->hstmt, &param_ptr), SQL_NEED_DATA);
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)data.c_str(), data.size()),
            SQL_SUCCESS);

  // validate SQLFetch
  SQLRETURN fetch_ret = SQLFetch(conn->hstmt);
  EXPECT_EQ(fetch_ret,
            SQL_ERROR);  // Should be error due to invalid statement state

  // Verify error code
  SQLCHAR sqlstate[6] = {0};
  SQLINTEGER native_error = 0;
  SQLCHAR message[256] = {0};
  SQLSMALLINT msg_len = 0;
  EXPECT_EQ(SQLGetDiagRec(SQL_HANDLE_STMT, conn->hstmt, 1, sqlstate,
                          &native_error, message, sizeof(message), &msg_len),
            SQL_SUCCESS);
  // In iODBC, the Driver Manager returns the SQLState S1010, whereas the
  // unixODBC Driver Manager returns HY010.
  EXPECT_THAT((char*)sqlstate, HasSubstr("010"));
  EXPECT_THAT((char*)message, HasSubstr("Function sequence error"));

  // Final SQLParamData to finish execution
  SQLRETURN final_ret = SQLParamData(conn->hstmt, &param_ptr);
  EXPECT_TRUE(final_ret == SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Clean up table
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

TEST(StatementTest, SQLParamData_MixedBindingModes) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_PARAM_DATA_MIXED_BINDING_TEST";
  Table table(table_name);

  Schema schema{
      {"Field1", "STRING"},  // Will use SQL_DATA_AT_EXEC
      {"Field2", "STRING"},  // Will pass directly in SQLBindParameter
      {"Field3", "STRING"}   // Will use SQL_DATA_AT_EXEC
  };

  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, getSchemaStr(schema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto query = "INSERT INTO " + table_name + " VALUES (?, ?, ?)";

  EXPECT_EQ(SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  // Bind 1st Parameter
  SQLLEN indicator1 = SQL_DATA_AT_EXEC;
  EXPECT_EQ(SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &indicator1),
            SQL_SUCCESS);

  // Bind 2nd Parameter
  char const* param2_data = "DirectData";
  SQLLEN indicator2 = SQL_NTS;  // Null-Terminated String
  EXPECT_EQ(
      SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                       SQL_LONGVARCHAR, strlen(param2_data), 0,
                       (SQLPOINTER)param2_data, strlen(param2_data), NULL),
      SQL_SUCCESS);

  // Bind 3rd Parameter
  SQLLEN indicator3 = SQL_DATA_AT_EXEC;
  EXPECT_EQ(SQLBindParameter(conn->hstmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_LONGVARCHAR, 0, 0, nullptr, 0, &indicator3),
            SQL_SUCCESS);
  EXPECT_EQ(SQLExecute(conn->hstmt), SQL_NEED_DATA);

  SQLPOINTER target_value = nullptr;
  EXPECT_EQ(SQLParamData(conn->hstmt, &target_value), SQL_NEED_DATA);

  // Pass Data for 1st Parameter
  std::string param1_data = "FirstParamData";
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)param1_data.c_str(),
                       param1_data.size()),
            SQL_SUCCESS);

  EXPECT_EQ(SQLParamData(conn->hstmt, &target_value), SQL_NEED_DATA);

  // Pass Data for 3rd Parameter
  std::string param3_data = "ThirdParamData";
  EXPECT_EQ(SQLPutData(conn->hstmt, (SQLPOINTER)param3_data.c_str(),
                       param3_data.size()),
            SQL_SUCCESS);

  EXPECT_EQ(SQLParamData(conn->hstmt, nullptr), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // validate data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string select_query = "SELECT Field1, Field2, Field3 FROM " + table_name;
  EXPECT_EQ(SQLExecDirect(conn->hstmt, (SQLCHAR*)select_query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  char field1[256] = {0};
  char field2[256] = {0};
  char field3[256] = {0};
  SQLLEN field1_len = 0;
  SQLLEN field2_len = 0;
  SQLLEN field3_len = 0;

  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLGetData(conn->hstmt, 1, SQL_C_CHAR, field1, sizeof(field1),
                       &field1_len),
            SQL_SUCCESS);
  EXPECT_EQ(SQLGetData(conn->hstmt, 2, SQL_C_CHAR, field2, sizeof(field2),
                       &field2_len),
            SQL_SUCCESS);
  EXPECT_EQ(SQLGetData(conn->hstmt, 3, SQL_C_CHAR, field3, sizeof(field3),
                       &field3_len),
            SQL_SUCCESS);

  EXPECT_EQ(std::string(field1), param1_data);
  EXPECT_EQ(std::string(field2), param2_data);
  EXPECT_EQ(std::string(field3), param3_data);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

std::string const kTableName = "ODBC_NATIVE_SQL_TEST";
std::vector<std::string> const kNativeSqlQueries = {
    "SELECT * FROM " + kTableName + " WHERE Column1 = {d '2023-01-01'}",
    "SELECT {fn UCASE(Name)} FROM " + kTableName,
    "SELECT {fn SUBSTRING(Name, 1, CHARINDEX(',', Name) - 1)} FROM " +
        kTableName,
    "SELECT Name FROM " + kTableName +
        " WHERE Name LIKE '\\%AAA%' {escape '\\'}",
    "SELECT Customers.CustID, Customers.Name, Orders.OrderID, Orders.Status "
    "FROM {oj Customers LEFT OUTER JOIN Orders ON "
    "Customers.CustID=Orders.CustID} "
    "WHERE Orders.Status='OPEN'",
    "abcjqwdxnasxw,wdhxqwdxq,dwhdnwkxwxn"};

TEST(StatementTest, SQLNativeSql_AllValidations) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLCHAR native_sql[kBufferLength];
  SQLINTEGER native_sql_length;

  for (auto const& sql_query : kNativeSqlQueries) {
    SCOPED_TRACE("Testing query: " + sql_query);

    // Valid SQLNativeSql execution
    EXPECT_EQ(SQLNativeSql(conn->hdbc, (SQLCHAR*)sql_query.c_str(), SQL_NTS,
                           native_sql, sizeof(native_sql), &native_sql_length),
              SQL_SUCCESS);

    EXPECT_EQ(std::string((char*)native_sql), sql_query)
        << "Native SQL does not match input query.";
    EXPECT_EQ(native_sql_length, sql_query.size())
        << "Native SQL length mismatch.";
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLNativeSql_NegativeTest) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLCHAR native_sql[kBufferLength];
  SQLINTEGER native_sql_length;

  // Null inStatementText should return SQL_ERROR
  {
    SCOPED_TRACE("Testing null inStatementText");
    EXPECT_EQ(SQLNativeSql(conn->hdbc, nullptr, SQL_NTS, native_sql,
                           sizeof(native_sql), &native_sql_length),
              SQL_ERROR);
  }

  // Null outStatementText, but valid SQL should still return SQL_SUCCESS and
  // fill length
  {
    SCOPED_TRACE("Testing null outStatementText");
    std::string sql_query = "SELECT * FROM DummyTable";
    EXPECT_EQ(SQLNativeSql(conn->hdbc, (SQLCHAR*)sql_query.c_str(), SQL_NTS,
                           nullptr, sizeof(native_sql), &native_sql_length),
              SQL_SUCCESS);
    EXPECT_EQ(native_sql_length, sql_query.size());
  }

  // Negative outStatementTextBufferLen should return SQL_ERROR
  {
    SCOPED_TRACE("Testing negative buffer length");
    std::string sql_query = "SELECT * FROM DummyTable";
    EXPECT_EQ(SQLNativeSql(conn->hdbc, (SQLCHAR*)sql_query.c_str(), SQL_NTS,
                           native_sql, -1, &native_sql_length),
              SQL_ERROR);
  }

  // Insufficient buffer: should return SQL_SUCCESS_WITH_INFO
  {
    SCOPED_TRACE("Testing insufficient buffer length");
    std::string sql_query = "SELECT * FROM Orders";
    SQLCHAR small_buffer[10];
    SQLINTEGER small_buffer_length;
    EXPECT_EQ(
        SQLNativeSql(conn->hdbc, (SQLCHAR*)sql_query.c_str(), SQL_NTS,
                     small_buffer, sizeof(small_buffer), &small_buffer_length),
        SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(small_buffer_length, sql_query.size());
  }

  // Zero outStatementTextBufferLen, but valid SQL should still return
  // SQL_SUCCESS_WITH_INFO and fill length
  {
    SCOPED_TRACE("Testing zero outStatementTextBufferLen");
    std::string sql_query = "SELECT * FROM DummyTable";
    EXPECT_EQ(SQLNativeSql(conn->hdbc, (SQLCHAR*)sql_query.c_str(), SQL_NTS,
                           native_sql, 0, &native_sql_length),
              SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(native_sql_length, sql_query.size());
  }

// simba driver returning SQL_SUCCESS for windows platform
//  but giving SQL_SUCCESS_WITH_INFO for non_windows platforms
#if defined(BQ_DRIVER_INTEGRATION_TESTS) || !defined(_WIN32)
  //  Null outStatementText and Zero outStatementTextBufferLen, but valid SQL
  //  should still return SQL_SUCCESS and
  // full length
  {
    SCOPED_TRACE(
        "Testing null outStatementText and zero outStatementTextBufferLen");
    std::string sql_query = "SELECT * FROM DummyTable";
    EXPECT_EQ(SQLNativeSql(conn->hdbc, (SQLCHAR*)sql_query.c_str(), SQL_NTS,
                           nullptr, 0, &native_sql_length),
              SQL_SUCCESS_WITH_INFO);
    EXPECT_EQ(native_sql_length, sql_query.size());
  }
#endif  // _WIN32

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLNativeSqlW_UnicodeQuery) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLWCHAR native_sql[kBufferLength];
  SQLINTEGER native_sql_length = 0;

  SCOPED_TRACE("Testing full match of SQLNativeSqlW with Unicode query");
  std::wstring sql_query = L"SELECT N'東京'";  // Full Unicode query

  std::vector<SQLWCHAR> sql_wstr(sql_query.begin(), sql_query.end());

  EXPECT_EQ(SQLNativeSqlW(conn->hdbc, sql_wstr.data(), sql_wstr.size(),
                          native_sql, sizeof(native_sql), &native_sql_length),
            SQL_SUCCESS);

  std::wstring returned_sql(native_sql, native_sql + native_sql_length);

  EXPECT_STREQ(sql_query.data(), returned_sql.data());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, BasicScriptWithQueryParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name = kDatasetWithTablePrefix + "ODBC_SCRIPTS_PARAM_TEST";
  Table table(table_name);

  // Create Table
  table.CreateWithPrepare(conn, "(Name STRING, Age INT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Reconnect for test execution
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Multi-statement script with parameters: INSERT + SELECT
  std::string insert_stmt = "INSERT INTO " + table_name + " VALUES (?, ?);";
  std::string select_stmt = "SELECT * FROM " + table_name + " WHERE Age = ?;";
  std::string script = insert_stmt + select_stmt;

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)script.c_str(), SQL_NTS);
  ASSERT_EQ(status, SQL_SUCCESS) << "SQLPrepare failed";

  // Bind parameters: Name, Age for INSERT; Age for SELECT
  std::string name = "TestUser";
  SQLLEN name_ind = SQL_NTS;
  int64_t insert_age = 35;
  int64_t select_age = 35;
  SQLLEN age_ind = 0;

  ASSERT_EQ(
      SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                       0, 0, (SQLPOINTER)name.c_str(), 0, &name_ind),
      SQL_SUCCESS);
  ASSERT_EQ(SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                             SQL_BIGINT, 0, 0, &insert_age, 0, &age_ind),
            SQL_SUCCESS);
  ASSERT_EQ(SQLBindParameter(conn->hstmt, 3, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                             SQL_BIGINT, 0, 0, &select_age, 0, &age_ind),
            SQL_SUCCESS);

  // Execute the combined script
  status = SQLExecute(conn->hstmt);
  ASSERT_EQ(status, SQL_SUCCESS) << "SQLExecute failed";

  SQLSMALLINT num_cols = -1;
  SQLLEN row_count = -1;

  // Step 1: Check result for INSERT
  SQLNumResultCols(conn->hstmt, &num_cols);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);
  EXPECT_EQ(SQLRowCount(conn->hstmt, &row_count), SQL_SUCCESS);
  EXPECT_EQ(row_count, 1);

  // Step 2: Move to SELECT result set
  ASSERT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);

  SQLNumResultCols(conn->hstmt, &num_cols);
  EXPECT_EQ(num_cols, 2);

  // Bind output columns
  char fetched_name[100] = {0};
  int64_t fetched_age = 0;
  SQLLEN fetched_name_ind = 0, fetched_age_ind = 0;

  ASSERT_EQ(SQLBindCol(conn->hstmt, 1, SQL_C_CHAR, fetched_name,
                       sizeof(fetched_name), &fetched_name_ind),
            SQL_SUCCESS);
  ASSERT_EQ(SQLBindCol(conn->hstmt, 2, SQL_C_SBIGINT, &fetched_age, 0,
                       &fetched_age_ind),
            SQL_SUCCESS);

  ASSERT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);

  // No more results after SELECT
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_NO_DATA);
  SQLFreeStmt(conn->hstmt, SQL_CLOSE);

  // Cleanup
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  ASSERT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLMoreResults, ProcedureWithDescriptorAndQueryParams) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name = kDatasetWithTablePrefix + "ODBC_DESC_PROC_TABLE";
  std::string procedure_name = kDatasetWithTablePrefix + "ODBC_DESC_PROC";

  // Create Procedure with parameters and 2 SELECT statements
  std::string create_proc =
      "CREATE OR REPLACE PROCEDURE " + procedure_name +
      "(IN str_param STRING, IN int_param INT64, IN float_param FLOAT64) "
      "BEGIN "
      "  CREATE OR REPLACE TABLE " +
      table_name +
      " (StringField STRING, IntegerField INTEGER, FloatField FLOAT64); "
      "  INSERT INTO " +
      table_name +
      " VALUES(str_param, int_param, float_param); "
      "  SELECT * FROM " +
      table_name +
      "; "
      "  SELECT StringField FROM " +
      table_name +
      " WHERE StringField = str_param; "
      "END";

  SQLRETURN status =
      SQLPrepare(conn->hstmt, (SQLCHAR*)create_proc.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare (create procedure)", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute (create procedure)", conn);

  // Prepare CALL statement
  std::string call_proc = "CALL " + procedure_name + "(?, ?, ?)";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)call_proc.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare (call procedure)", conn);

  // Bind parameters
  SQLCHAR str_val[] = "Test String 5";
  SQLLEN str_ind = SQL_NTS;
  SQLLEN int_val = 5;
  double float_val = 5.5;

  status = SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                            SQL_CHAR, 0, 0, str_val, 0, &str_ind);
  CheckError(status, "SQLBindParameter (str_param)", conn);
  status = SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG,
                            SQL_INTEGER, 0, 0, &int_val, 0, nullptr);
  CheckError(status, "SQLBindParameter (int_param)", conn);
  status = SQLBindParameter(conn->hstmt, 3, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                            SQL_DOUBLE, 0, 0, &float_val, 0, nullptr);
  CheckError(status, "SQLBindParameter (float_param)", conn);

  // Execute procedure
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute (call procedure)", conn);

  SQLSMALLINT num_cols;
  SQLLEN row_count;

  // CREATE TABLE
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols (create table)", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);
  EXPECT_EQ(SQLRowCount(conn->hstmt, &row_count), SQL_SUCCESS);

  // INSERT
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols (insert)", conn);
  EXPECT_EQ(num_cols, 0);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_ERROR);
  EXPECT_EQ(SQLRowCount(conn->hstmt, &row_count), SQL_SUCCESS);

  // SELECT *
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols (select *)", conn);
  EXPECT_EQ(num_cols, 3);

  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
  }  // consume all rows
  EXPECT_EQ(SQLRowCount(conn->hstmt, &row_count), SQL_SUCCESS);
  EXPECT_EQ(row_count, -1);

  // SELECT with WHERE
  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_SUCCESS);
  status = SQLNumResultCols(conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols (select where)", conn);
  EXPECT_EQ(num_cols, 1);

  while (SQLFetch(conn->hstmt) == SQL_SUCCESS) {
  }
  EXPECT_EQ(SQLRowCount(conn->hstmt, &row_count), SQL_SUCCESS);
  EXPECT_EQ(row_count, -1);

  EXPECT_EQ(SQLMoreResults(conn->hstmt), SQL_NO_DATA);
  SQLFreeStmt(conn->hstmt, SQL_CLOSE);

  // Cleanup
  Procedure procedure(procedure_name);
  procedure.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  Table table(table_name);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
