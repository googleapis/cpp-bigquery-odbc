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

#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "google/cloud/odbc/testing/odbc_utils/statement.h"
#include <nlohmann/json.hpp>

namespace google::cloud::odbc_tests {
#ifndef BQ_DRIVER_INTEGRATION_TESTS
struct StrBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  std::string value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

struct NumericBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  double value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

struct Int64BasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQLBIGINT value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<StrBasicTestStruct> const kConversionFromStrTestData{
    {SQL_C_CHAR, "Test String 1", SQL_SUCCESS},
    {SQL_C_FLOAT, "19.1", SQL_SUCCESS},
    {SQL_C_FLOAT, "2a", SQL_ERROR},
    {SQL_C_DOUBLE, "-38.3", SQL_SUCCESS},
    {SQL_C_DOUBLE, "a3", SQL_ERROR},
    {SQL_C_SSHORT, "31", SQL_SUCCESS},
    {SQL_C_SSHORT, "9223372036854775807", SQL_ERROR},
    {SQL_C_USHORT, "89", SQL_SUCCESS},
    {SQL_C_USHORT, "-9", SQL_ERROR},
    {SQL_C_USHORT, "65537" /* 2^16 + 1 */, SQL_ERROR},
    {SQL_C_SLONG, "-934934934", SQL_SUCCESS},
    {SQL_C_SLONG, "1.1",
     SQL_SUCCESS_WITH_INFO},  // SQL_SUCCESS_WITH_INFO because there is loss of
                              // precision
    {SQL_C_SLONG, "b1", SQL_ERROR},
    {SQL_C_ULONG, "934934934", SQL_SUCCESS},
    {SQL_C_ULONG, "1.1",
     SQL_SUCCESS_WITH_INFO},  // SQL_SUCCESS_WITH_INFO because there is loss of
                              // precision
    {SQL_C_ULONG, "b1", SQL_ERROR},
    {SQL_C_BIT, "0", SQL_SUCCESS},
    {SQL_C_BIT, "1", SQL_SUCCESS},
    {SQL_C_BIT, "2", SQL_ERROR},
};

std::vector<NumericBasicTestStruct> const kConversionFromNumericTestData{
    {SQL_C_CHAR, 123, SQL_SUCCESS},
    {SQL_C_FLOAT, 156.1, SQL_SUCCESS},
    {SQL_C_FLOAT, -157.8, SQL_SUCCESS},
    {SQL_C_DOUBLE, -38.3, SQL_SUCCESS},
    {SQL_C_SSHORT, 31, SQL_SUCCESS},
    {SQL_C_SSHORT, -31, SQL_SUCCESS},
    {SQL_C_USHORT, 3, SQL_SUCCESS},
    {SQL_C_USHORT, 65537 /* 2^16 + 1 */, SQL_ERROR},
    {SQL_C_SLONG, -13, SQL_SUCCESS},
    {SQL_C_SLONG, 13.3,
     SQL_SUCCESS_WITH_INFO},  // SQL_SUCCESS_WITH_INFO because there is loss of
                              // precision
    {SQL_C_ULONG, 81, SQL_SUCCESS},
    {SQL_C_ULONG, -8, SQL_ERROR},
    {SQL_C_ULONG, 1.1, SQL_SUCCESS_WITH_INFO},  // SQL_SUCCESS_WITH_INFO because
                                                // there is loss of precision
    {SQL_C_BIT, 0, SQL_SUCCESS},
    {SQL_C_BIT, 1, SQL_SUCCESS},
    {SQL_C_BIT, 2, SQL_ERROR},
};

std::vector<Int64BasicTestStruct> const kConversionFromInt64TestData{
    {SQL_C_CHAR, 123, SQL_SUCCESS},
    {SQL_C_FLOAT, 156, SQL_SUCCESS},
    {SQL_C_FLOAT, -157, SQL_SUCCESS},
    {SQL_C_FLOAT, 9223372036854775807 /* highest int64 */, SQL_SUCCESS},
    {SQL_C_DOUBLE, -38, SQL_SUCCESS},
    {SQL_C_SSHORT, 31, SQL_SUCCESS},
    {SQL_C_SSHORT, -31, SQL_SUCCESS},
    {SQL_C_USHORT, 3, SQL_SUCCESS},
    {SQL_C_USHORT, 65537 /* 2^16 + 1 */, SQL_ERROR},
    {SQL_C_USHORT, -3, SQL_ERROR},
    {SQL_C_SSHORT, 9223372036854775807, SQL_ERROR},
    {SQL_C_SLONG, -13, SQL_SUCCESS},
    {SQL_C_ULONG, 81, SQL_SUCCESS},
    {SQL_C_ULONG, -8, SQL_ERROR},
    {SQL_C_BIT, 0, SQL_SUCCESS},
    {SQL_C_BIT, 1, SQL_SUCCESS},
    {SQL_C_BIT, 2, SQL_ERROR},
};

template <typename TestStruct>
void TestTranslationsFromArithmetic(std::shared_ptr<ODBCHandles> conn,
                                    std::string query,
                                    std::vector<TestStruct> expected_config) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;

  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);
  status = SQLExecDirect(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLExecDirect", conn, false);

  // Read all the rows using SQLFetch
  int row_count = 0;
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }

    SQLSMALLINT resp_status, resp_status_len;
    while (1) {
      TestStruct expected = expected_config[row_count];
      status = SQLGetData(conn->hstmt, 1, expected.target_c_type, data,
                          kBufferLength, &strlen_or_ind);
      std::cout << "Testing row: " << expected.target_c_type << ", "
                << expected.value << ", " << expected.status << std::endl;
      EXPECT_EQ(status, expected.status);
      if (status != SQL_SUCCESS) {
        row_count++;
        break;
      }
      CheckError(status,
                 "SQLGetData(" + std::to_string(expected.target_c_type) + ")",
                 conn);
      if (SQL_SUCCEEDED(status)) {
        status = SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, 1,
                                 &resp_status, SQL_INTEGER, &resp_status_len);
        if (status == SQL_NO_DATA) {
          if (strlen_or_ind >= 0) {
            // Refer
            // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16
            // to understand the expectations regarding typecasting applications
            // buffers.
            if (expected.target_c_type == SQL_C_CHAR) {
              std::string returned_val = (char*)data;
              EXPECT_EQ(std::stod(returned_val), expected.value);
            } else if (expected.target_c_type == SQL_C_FLOAT) {
              SQLREAL* returned_val = (SQLREAL*)data;
              SQLREAL expected_val = expected.value;
              EXPECT_EQ(*returned_val, expected_val);
            } else if (expected.target_c_type == SQL_C_DOUBLE) {
              SQLDOUBLE* returned_val = (SQLDOUBLE*)data;
              EXPECT_EQ(*returned_val, expected.value);
            } else if (expected.target_c_type == SQL_C_SLONG) {
              SQLINTEGER* returned_val = (SQLINTEGER*)data;
              EXPECT_EQ(*returned_val, expected.value);
            } else if (expected.target_c_type == SQL_C_SSHORT) {
              SQLSMALLINT* returned_val = (SQLSMALLINT*)data;
              EXPECT_EQ(*returned_val, expected.value);
            } else if (expected.target_c_type == SQL_C_USHORT) {
              SQLUSMALLINT* returned_val = (SQLUSMALLINT*)data;
              EXPECT_EQ(*returned_val, expected.value);
            } else if (expected.target_c_type == SQL_C_ULONG) {
              SQLUINTEGER* returned_val = (SQLUINTEGER*)data;
              EXPECT_EQ(*returned_val, expected.value);
            } else if (expected.target_c_type == SQL_C_BIT) {
              SQLCHAR* returned_val = (SQLCHAR*)data;
              EXPECT_EQ(*returned_val, expected.value);
            }
            row_count++;
          }
          break;
        }
        CheckError(status, "SQLGetDiagField", conn);
      } else {
        break;
      }
    }
  }
  EXPECT_EQ(row_count, expected_config.size());
}

void TestTranslationsFromString(std::shared_ptr<ODBCHandles> conn,
                                std::string query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;

  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);
  status = SQLExecDirect(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLExecDirect", conn, false);

  // Read all the rows using SQLFetch
  int row_count = 0;
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }

    SQLSMALLINT resp_status, resp_status_len;
    while (1) {
      StrBasicTestStruct expected = kConversionFromStrTestData[row_count];
      status = SQLGetData(conn->hstmt, 1, expected.target_c_type, data,
                          kBufferLength, &strlen_or_ind);
      std::cout << "Testing row: " << expected.target_c_type << ", "
                << expected.value << ", " << expected.status << std::endl;
      EXPECT_EQ(status, expected.status);
      if (status != SQL_SUCCESS) {
        row_count++;
        break;
      }
      CheckError(status,
                 "SQLGetData(" + std::to_string(expected.target_c_type) + ")",
                 conn);
      if (SQL_SUCCEEDED(status)) {
        status = SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, 1,
                                 &resp_status, SQL_INTEGER, &resp_status_len);
        if (status == SQL_NO_DATA) {
          if (strlen_or_ind >= 0) {
            // Refer
            // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16
            // to understand the expectations regarding typecasting applications
            // buffers.
            if (expected.target_c_type == SQL_C_CHAR) {
              std::string returned_val = (char*)data;
              EXPECT_EQ(returned_val, expected.value);
            } else if (expected.target_c_type == SQL_C_FLOAT) {
              SQLREAL* returned_val = (SQLREAL*)data;
              EXPECT_EQ(*returned_val, std::stof(expected.value));
            } else if (expected.target_c_type == SQL_C_DOUBLE) {
              SQLDOUBLE* returned_val = (SQLDOUBLE*)data;
              EXPECT_EQ(*returned_val, std::stod(expected.value));
            } else if (expected.target_c_type == SQL_C_SSHORT) {
              SQLSMALLINT* returned_val = (SQLSMALLINT*)data;
              EXPECT_EQ(*returned_val, std::stoi(expected.value));
            } else if (expected.target_c_type == SQL_C_USHORT) {
              SQLUSMALLINT* returned_val = (SQLUSMALLINT*)data;
              EXPECT_EQ(*returned_val, std::stoi(expected.value));
            } else if (expected.target_c_type == SQL_C_SLONG) {
              SQLINTEGER* returned_val = (SQLINTEGER*)data;
              EXPECT_EQ(*returned_val, std::stoi(expected.value));
            } else if (expected.target_c_type == SQL_C_ULONG) {
              SQLUINTEGER* returned_val = (SQLUINTEGER*)data;
              EXPECT_EQ(*returned_val, std::stoi(expected.value));
            } else if (expected.target_c_type == SQL_C_BIT) {
              SQLCHAR* returned_val = (SQLCHAR*)data;
              EXPECT_EQ(*returned_val, std::stod(expected.value));
            }
            row_count++;
          }
          break;
        }
        CheckError(status, "SQLGetDiagField", conn);
      } else {
        break;
      }
    }
  }
  EXPECT_EQ(row_count, kConversionFromStrTestData.size());
}

// This test should follow translations according to
// https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-character?view=sql-server-ver16

TEST(DataTranslationTest, From_SQL_CHAR_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_CHAR";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(conn, "(index INT64, StringField STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> string_data_to_insert;
  for (auto elem : kConversionFromStrTestData) {
    string_data_to_insert.push_back(elem.value);
  }
  table.InsertStrData(conn, string_data_to_insert, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT StringField FROM " + table_name + " ORDER BY index";
#ifndef _WIN32
  // TODO(b/357794952): Handle SQLGetDiagField API Invalid Return Value WRT
  // SIMBA(WIN).
  TestTranslationsFromString(conn, query);
#endif /* WIN32 */
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This test should follow translations according to
// https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-numeric?view=sql-server-ver16
TEST(DataTranslationTest, From_NUMERIC_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_NUMERIC";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(conn, "(index INT64, NumericField NUMERIC)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<double> numeric_data_to_insert;
  for (auto elem : kConversionFromNumericTestData) {
    numeric_data_to_insert.push_back(elem.value);
  }
  table.InsertNumericData(conn, numeric_data_to_insert, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT NumericField FROM " + table_name + " ORDER BY index";

#ifndef _WIN32
  // TODO(b/357794952): Handle SQLGetDiagField API Invalid Return Value WRT
  // SIMBA(WIN).
  TestTranslationsFromArithmetic<NumericBasicTestStruct>(
      conn, query, kConversionFromNumericTestData);
#endif /* WIN32 */
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This test should follow translations according to
// https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-numeric?view=sql-server-ver16
TEST(DataTranslationTest, From_INT64_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_BIGINT";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(conn, "(index INT64, IntField INT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<SQLBIGINT> int64_data_to_insert;
  for (auto elem : kConversionFromInt64TestData) {
    int64_data_to_insert.push_back(elem.value);
  }
  table.InsertInt64Data(conn, int64_data_to_insert, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query = "SELECT IntField FROM " + table_name + " ORDER BY index";

#ifndef _WIN32
  // TODO(b/357794952): Handle SQLGetDiagField API Invalid Return Value WRT
  // SIMBA(WIN).
  TestTranslationsFromArithmetic<Int64BasicTestStruct>(
      conn, query, kConversionFromInt64TestData);
#endif /* WIN32 */
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#endif  // BQ_DRIVER_INTEGRATION_TESTS

struct TimestampBasicTestStruct {
  // The target C type SQLBindCol will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLBindCol if it succeeds
  SQL_TIMESTAMP_STRUCT value;
  // The status that should be returned by SQLBindCol for this C Type
  SQLRETURN status;
};

std::vector<TimestampBasicTestStruct> const kConversionFromTimestampTestData{
    {SQL_C_CHAR, {2024, 01, 20, 10, 20, 30, 123112}, SQL_SUCCESS},
    {SQL_C_WCHAR, {2024, 01, 20, 11, 2, 33, 1212}, SQL_SUCCESS},
    {SQL_C_BINARY, {2024, 01, 20, 2, 20, 22, 123123}, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {2024, 01, 20, 12, 22, 11, 32223}, SQL_SUCCESS},
    {SQL_C_TYPE_TIME, {2024, 01, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2024, 01, 20, 12, 21, 22, 000000}, SQL_SUCCESS},
    {SQL_C_SLONG, {2024, 01, 20, 00, 00, 00, 000000}, SQL_ERROR},
};

void TestTranslationsFromTimestamp(std::shared_ptr<ODBCHandles> conn,
                                   std::string query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromTimestampTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }

    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      break;
    }

    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        std::string expected_val = FormatTimeStamp(expected.value);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);

        std::string returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);

        std::string expected_val = FormatTimeStamp(expected.value);
        EXPECT_STREQ(returned_val.data(), expected_val.data());
        break;
      }
      case SQL_C_BINARY: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        std::string expected_val = FormatBinaryTimeStamp(expected.value);
        std::string returned_val = FormatTimeStamp(*timestamp);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_TYPE_DATE: {
        SQL_DATE_STRUCT* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
        EXPECT_EQ(date->year, expected.value.year);
        EXPECT_EQ(date->month, expected.value.month);
        EXPECT_EQ(date->day, expected.value.day);
        break;
      }
      case SQL_C_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        EXPECT_EQ(timestamp->year, expected.value.year);
        EXPECT_EQ(timestamp->month, expected.value.month);
        EXPECT_EQ(timestamp->day, expected.value.day);
        EXPECT_EQ(timestamp->hour, expected.value.hour);
        EXPECT_EQ(timestamp->minute, expected.value.minute);
        EXPECT_EQ(timestamp->second, expected.value.second);
        break;
      }
      case SQL_C_TYPE_TIME: {
        SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
        EXPECT_EQ(time->hour, expected.value.hour);
        EXPECT_EQ(time->minute, expected.value.minute);
        EXPECT_EQ(time->second, expected.value.second);
        break;
      }
      case SQL_C_SLONG: {
        EXPECT_EQ(status, expected.status);
        break;
      }
      default:
        break;
    }
  }
}

TEST(DataTranslationTest, From_SQL_Timestamp_to_all) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_INSERT_TEST_TIMESTAMP";
  Table table(table_name);
  table.CreateWithPrepare(conn, "(Id INT64, DOB timestamp)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<SQL_TIMESTAMP_STRUCT> timestamp_data;
  for (auto const& test_data : kConversionFromTimestampTestData) {
    timestamp_data.push_back(test_data.value);
  }
  table.InsertTimestampData(conn, timestamp_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query = "SELECT DOB FROM " + table_name + " Order by Id";
  TestTranslationsFromTimestamp(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#ifndef BQ_DRIVER_INTEGRATION_TESTS

#endif  // BQ_DRIVER_INTEGRATION_TESTS

struct DateBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_DATE_STRUCT value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<DateBasicTestStruct> const kConversionFromDateTestData{
    {SQL_C_CHAR, {2024, 2, 20}, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {2024, 3, 20}, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2024, 4, 20}, SQL_SUCCESS},
    {SQL_C_WCHAR, {2024, 7, 20}, SQL_SUCCESS},
    {SQL_C_BINARY, {2024, 5, 20}, SQL_SUCCESS},
    {SQL_C_USHORT, {2024, 6, 20}, SQL_ERROR},
    {SQL_C_DOUBLE, {2024, 1, 20}, SQL_ERROR},
};

// TODO(b/365915498): Data translation Utilities
void TestTranslationsFromDate(std::shared_ptr<ODBCHandles> conn,
                              std::string query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  int row_count = 0;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromDateTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);

    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      break;
    }
    EXPECT_EQ(SQL_SUCCESS, expected.status);
    std::string expected_val = FormatDate(expected.value);
    std::string returned_val;
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        std::wstring wstr = reinterpret_cast<wchar_t*>(data);
        std::string returned_val_utf8 =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), 10);
        EXPECT_STREQ(returned_val_utf8.data(), expected_val.data());
        break;
      }
      case SQL_C_BINARY: {
        if (strlen_or_ind == sizeof(SQL_DATE_STRUCT)) {
          SQL_DATE_STRUCT* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
          returned_val = FormatDate(*date);
          EXPECT_EQ(returned_val, expected_val);
        }
        break;
      }
      case SQL_C_TYPE_DATE: {
        SQL_DATE_STRUCT* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
        EXPECT_EQ(date->year, expected.value.year);
        EXPECT_EQ(date->month, expected.value.month);
        EXPECT_EQ(date->day, expected.value.day);
        break;
      }

      case SQL_C_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        EXPECT_EQ(timestamp->year, expected.value.year);
        EXPECT_EQ(timestamp->month, expected.value.month);
        EXPECT_EQ(timestamp->day, expected.value.day);

        break;
      }
      default:
        break;
    }
    ++row_count;
  }
}

// This test should follow translations according to
// https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-date?view=sql-server-ver16
TEST(DataTranslationTest, From_SQL_Date_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATE";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, DateField DATE)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<SQL_DATE_STRUCT> date_data;
  for (auto const& test_case : kConversionFromDateTestData) {
    date_data.push_back(test_case.value);
  }
  table.InsertDateData(conn, date_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query = "SELECT DateField FROM " + table_name + " Order by index";
  TestTranslationsFromDate(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
struct TimeBasicTestStruct {
  // The target C type
  SQLSMALLINT target_c_type;
  // The value that should be returned
  SQL_TIME_STRUCT value;
  // The status that should be returned for this C Type
  SQLRETURN status;
};

std::vector<TimeBasicTestStruct> const kConversionFromTimeTestData{

    {SQL_C_CHAR, {11, 20, 20}, SQL_SUCCESS},
    {SQL_C_TYPE_TIME, {22, 45, 54}, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2, 36, 29}, SQL_SUCCESS},
    {SQL_C_WCHAR, {19, 07, 20}, SQL_SUCCESS},
    {SQL_C_BINARY, {04, 06, 07}, SQL_SUCCESS},

};
// TODO (b/365915498):
// Remove assertions &
// Move as  utility function to testing/odbc_utils
void TestTranslationsFromTime(std::shared_ptr<ODBCHandles> conn,
                              std::string query) {
  SQLRETURN status;
  SQLPOINTER data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  int row_count = 0;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromTimeTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }

    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        std::string expected_val = FormatTimetoString(expected.value);
        expected_val.append(".000000");
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        std::string returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        std::string expected_val = FormatTimetoString(expected.value);
        expected_val.append(".000000");
        EXPECT_STREQ(returned_val.c_str(), expected_val.c_str());
        break;
      }
      case SQL_C_BINARY: {
        if (strlen_or_ind == sizeof(SQL_TIME_STRUCT)) {
          SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
          std::string expected_val = FormatTimetoString(expected.value);
          std::string returned_val = FormatTimetoString(*time);
          EXPECT_EQ(returned_val, expected_val);
        }
        break;
      }
      case SQL_C_TYPE_TIME: {
        SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
        EXPECT_EQ(time->hour, expected.value.hour);
        EXPECT_EQ(time->minute, expected.value.minute);
        EXPECT_EQ(time->second, expected.value.second);
        break;
      }

      case SQL_C_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        EXPECT_EQ(timestamp->hour, expected.value.hour);
        EXPECT_EQ(timestamp->minute, expected.value.minute);
        EXPECT_EQ(timestamp->second, expected.value.second);
        break;
      }
      default: {
        break;
      }
    }
    ++row_count;
  }
}

TEST(DataTranslationTest, From_SQL_Time_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_TIME";
  Table table(table_name);
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, TimeField TIME)");
  std::vector<SQL_TIME_STRUCT> time_data_to_insert;
  for (auto const& time_data : kConversionFromTimeTestData) {
    time_data_to_insert.push_back(time_data.value);
  }
  table.InsertTimeData(conn, time_data_to_insert, true);
  std::string query = "SELECT TimeField FROM " + table_name + " ORDER BY index";
  TestTranslationsFromTime(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
struct JsonBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  nlohmann::json value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<JsonBasicTestStruct> const kConversionFromJsonTestData{
    {SQL_C_CHAR, {{"age", 30}, {"name", "Sita"}}, SQL_SUCCESS},
    {SQL_C_CHAR, {{"age", 30}, {"name", "Alice"}}, SQL_SUCCESS},
    {SQL_C_CHAR, {{"age", 90}, {"name", "Ram"}}, SQL_SUCCESS},
    {SQL_C_CHAR, {{"age", 26}, {"name", "Bob"}}, SQL_SUCCESS},
    {SQL_C_CHAR, {{"age", 32}, {"name", "Kapoor"}}, SQL_SUCCESS},
    {SQL_C_SLONG, {{"age", 44}, {"name", "Shetty"}}, SQL_ERROR},
    {SQL_C_SSHORT, {{"age", 29}, {"name", "Spider"}}, SQL_ERROR},
    {SQL_C_WCHAR, {{"age", 30}, {"name", "Kiran"}}, SQL_SUCCESS},
    {SQL_C_WCHAR, {{"age", 80}, {"name", "Ravi"}}, SQL_SUCCESS},
    {SQL_C_WCHAR, {{"age", 100}, {"name", "Shanti"}}, SQL_SUCCESS},
    {SQL_C_WCHAR, {{"age", 76}, {"name", "Sushma"}}, SQL_SUCCESS},

};

void TestTranslationsFromJsonToALL(std::shared_ptr<ODBCHandles> conn,
                                   std::string table_name) {
  SQLRETURN status;
  SQLPOINTER data[kBufferLength];
  SQLLEN strlen_or_ind;
  int id = 0;
  int row_count = 0;
  for (auto const& expected : kConversionFromJsonTestData) {
    auto const query = "SELECT PersonDetails FROM " + table_name +
                       " WHERE Id = " + std::to_string(id);
    status = GetConvertedJsonData(conn, query, expected.target_c_type,
                                  &strlen_or_ind, data);
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = (char*)data;
        std::string expected_val = to_string(expected.value);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        std::string returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        std::string expected_val = to_string(expected.value);
        EXPECT_STREQ(returned_val.c_str(), expected_val.c_str());
        break;
      }
      case SQL_C_SLONG: {
        EXPECT_EQ(status, expected.status);
        break;
      }
      case SQL_C_SSHORT: {
        EXPECT_EQ(status, expected.status);
        break;
      }
      default: {
        break;
      }
    }
    id++;
    ++row_count;
  }
}

TEST(DataTranslationTest, From_Json_to_ALL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SQLRETURN status;
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_INSERT_PARAMS_TEST_JSON_STRING";
  char insert_stmt[kBufferLength];
  Table table(table_name);
  table.CreateWithPrepare(conn, "(Id INTEGER, PersonDetails JSON)");
  std::vector<nlohmann::json> json_data_to_insert;
  for (auto elem : kConversionFromJsonTestData) {
    json_data_to_insert.push_back(elem.value);
  }
  table.InsertJsonData(conn, json_data_to_insert, true);

  TestTranslationsFromJsonToALL(conn, table_name);
  // Delete table
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

struct DateTimeBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_TIMESTAMP_STRUCT value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<DateTimeBasicTestStruct> const kConversionFromDateTimeTestData{
    {SQL_C_WCHAR, {2024, 02, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_BINARY, {2024, 03, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {2024, 04, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_TIME, {2024, 05, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2024, 06, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_SLONG, {2024, 01, 20, 00, 00, 00, 000000}, SQL_ERROR},
    {SQL_C_DOUBLE, {2024, 1, 20}, SQL_ERROR},
    {SQL_C_CHAR, {2024, 2, 20}, SQL_SUCCESS},
    {SQL_C_USHORT, {2024, 6, 20}, SQL_ERROR},
};

void TestTranslationsFromDateTime(std::shared_ptr<ODBCHandles> conn,
                                  std::string query) {
  SQLRETURN status;
  SQLPOINTER data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromDateTimeTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }

    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      break;
    }
    EXPECT_EQ(SQL_SUCCESS, expected.status);
    std::string expected_val = FormatTimeStamp(expected.value);
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        std::string returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        EXPECT_STREQ(returned_val.data(), expected_val.data());
        break;
      }
      case SQL_C_BINARY: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        std::string returned_val = FormatTimeStamp(*timestamp);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_TYPE_DATE: {
        SQL_DATE_STRUCT* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
        EXPECT_EQ(date->year, expected.value.year);
        EXPECT_EQ(date->month, expected.value.month);
        EXPECT_EQ(date->day, expected.value.day);
        break;
      }
      case SQL_C_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        EXPECT_EQ(timestamp->year, expected.value.year);
        EXPECT_EQ(timestamp->month, expected.value.month);
        EXPECT_EQ(timestamp->day, expected.value.day);
        EXPECT_EQ(timestamp->hour, expected.value.hour);
        EXPECT_EQ(timestamp->minute, expected.value.minute);
        EXPECT_EQ(timestamp->second, expected.value.second);
        break;
      }
      case SQL_C_TYPE_TIME: {
        SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
        EXPECT_EQ(time->hour, expected.value.hour);
        EXPECT_EQ(time->minute, expected.value.minute);
        EXPECT_EQ(time->second, expected.value.second);
        break;
      }
      default:
        break;
    }
  }
}

TEST(DataTranslationTest, From_SQL_DateTime_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATETIME";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, DateTimeField DATETIME)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<SQL_TIMESTAMP_STRUCT> date_data;
  for (auto const& test_case : kConversionFromDateTimeTestData) {
    date_data.push_back(test_case.value);
  }
  table.InsertTimestampData(conn, date_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT DateTimeField FROM " + table_name + " Order by index";
  TestTranslationsFromDateTime(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

struct DateTimeBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_TIMESTAMP_STRUCT value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<DateTimeBasicTestStruct> const kConversionFromDateTimeTestData{
    {SQL_C_WCHAR, {2024, 02, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_BINARY, {2024, 03, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {2024, 04, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_TIME, {2024, 05, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2024, 06, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_SLONG, {2024, 01, 20, 00, 00, 00, 000000}, SQL_ERROR},
    {SQL_C_DOUBLE, {2024, 1, 20}, SQL_ERROR},
    {SQL_C_CHAR, {2024, 2, 20}, SQL_SUCCESS},
    {SQL_C_USHORT, {2024, 6, 20}, SQL_ERROR},
};

void TestTranslationsFromDateTime(std::shared_ptr<ODBCHandles> conn,
                                  std::string query) {
  SQLRETURN status;
  SQLPOINTER data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromDateTimeTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }

    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      break;
    }
    EXPECT_EQ(SQL_SUCCESS, expected.status);
    std::string expected_val = FormatTimeStamp(expected.value);
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        std::string returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        EXPECT_STREQ(returned_val.data(), expected_val.data());
        break;
      }
      case SQL_C_BINARY: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        std::string returned_val = FormatTimeStamp(*timestamp);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_TYPE_DATE: {
        SQL_DATE_STRUCT* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
        EXPECT_EQ(date->year, expected.value.year);
        EXPECT_EQ(date->month, expected.value.month);
        EXPECT_EQ(date->day, expected.value.day);
        break;
      }
      case SQL_C_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT* timestamp =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        EXPECT_EQ(timestamp->year, expected.value.year);
        EXPECT_EQ(timestamp->month, expected.value.month);
        EXPECT_EQ(timestamp->day, expected.value.day);
        EXPECT_EQ(timestamp->hour, expected.value.hour);
        EXPECT_EQ(timestamp->minute, expected.value.minute);
        EXPECT_EQ(timestamp->second, expected.value.second);
        break;
      }
      case SQL_C_TYPE_TIME: {
        SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
        EXPECT_EQ(time->hour, expected.value.hour);
        EXPECT_EQ(time->minute, expected.value.minute);
        EXPECT_EQ(time->second, expected.value.second);
        break;
      }
      default:
        break;
    }
  }
}

TEST(DataTranslationTest, From_SQL_DateTime_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATETIME";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, DateTimeField DATETIME)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<SQL_TIMESTAMP_STRUCT> date_data;
  for (auto const& test_case : kConversionFromDateTimeTestData) {
    date_data.push_back(test_case.value);
  }
  table.InsertTimestampData(conn, date_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT DateTimeField FROM " + table_name + " Order by index";
  TestTranslationsFromDateTime(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

struct IntervalBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_INTERVAL_STRUCT interval_value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

// TODO(b/368251064): Remove designated identifiers to support C++17.
std::vector<IntervalBasicTestStruct> const kConversionYearMonthIntervalTestData{
    {SQL_C_CHAR, {SQL_IS_YEAR, 1, {.year_month = {3, 0}}}, SQL_SUCCESS},
    {SQL_C_INTERVAL_YEAR,
     {SQL_IS_YEAR, 1, {.year_month = {5, 0}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_MONTH,
     {SQL_IS_MONTH, 1, {.year_month = {0, 8}}},
     SQL_SUCCESS},
    {SQL_C_DOUBLE, {SQL_IS_YEAR, 1, {.year_month = {9, 0}}}, SQL_ERROR},
    {SQL_C_WCHAR,
     {SQL_IS_YEAR_TO_MONTH, 1, {.year_month = {2, 5}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_YEAR_TO_MONTH,
     {SQL_IS_YEAR_TO_MONTH, 1, {.year_month = {1, 6}}},
     SQL_SUCCESS},
    {SQL_C_FLOAT, {SQL_IS_MONTH, 1, {.year_month = {0, 9}}}, SQL_ERROR},
};

// TODO(b/368251064): Remove designated identifiers to support C++17.
std::vector<IntervalBasicTestStruct> const kConversionDaySecondIntervalTestData{
    {SQL_C_CHAR, {SQL_IS_DAY, 1, {.day_second = {5, 0, 0, 0, 0}}}, SQL_SUCCESS},
    {SQL_C_WCHAR,
     {SQL_IS_HOUR, 1, {.day_second = {0, 2, 0, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_DAY,
     {SQL_IS_DAY, 1, {.day_second = {15, 0, 0, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_FLOAT,
     {SQL_IS_MINUTE, 1, {.day_second = {0, 0, 45, 0, 0}}},
     SQL_ERROR},
    {SQL_C_INTERVAL_HOUR,
     {SQL_IS_HOUR, 1, {.day_second = {0, 20, 0, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_MINUTE,
     {SQL_IS_MINUTE, 1, {.day_second = {0, 0, 45, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_SECOND,
     {SQL_IS_SECOND, 1, {.day_second = {0, 0, 0, 10, 0}}},
     SQL_SUCCESS},
    {SQL_C_DOUBLE,
     {SQL_IS_DAY_TO_HOUR, 1, {.day_second = {10, 14, 0, 0, 0}}},
     SQL_ERROR},
    {SQL_C_INTERVAL_DAY_TO_HOUR,
     {SQL_IS_DAY_TO_HOUR, 1, {.day_second = {10, 14, 0, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_DAY_TO_MINUTE,
     {SQL_IS_DAY_TO_MINUTE, 1, {.day_second = {1, 5, 30, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_DAY_TO_SECOND,
     {SQL_IS_DAY_TO_SECOND, 1, {.day_second = {2, 1, 2, 20, 500}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_HOUR_TO_MINUTE,
     {SQL_IS_HOUR_TO_MINUTE, 1, {.day_second = {0, 9, 45, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_INTERVAL_HOUR_TO_SECOND,
     {SQL_IS_HOUR_TO_SECOND, 1, {.day_second = {0, 11, 10, 25, 0}}},
     SQL_SUCCESS},
    {SQL_C_BIT,
     {SQL_IS_DAY_TO_SECOND, 1, {.day_second = {2, 1, 2, 20, 500}}},
     SQL_ERROR},
    {SQL_C_INTERVAL_MINUTE_TO_SECOND,
     {SQL_IS_MINUTE_TO_SECOND, 1, {.day_second = {0, 0, 50, 10, 100}}},
     SQL_SUCCESS},
};

// TODO(b/368251064): Remove designated identifiers to support C++17.
std::vector<IntervalBasicTestStruct> const
    kConversionFromSinglePrecisionIntervalData{
        {SQL_C_STINYINT, {SQL_IS_YEAR, 1, {.year_month = {1, 0}}}, SQL_SUCCESS},
        {SQL_C_UTINYINT,
         {SQL_IS_DAY, 1, {.day_second = {6, 0, 0, 0, 0}}},
         SQL_SUCCESS},
        {SQL_C_SSHORT,
         {SQL_IS_HOUR, 1, {.day_second = {0, 12, 0, 0, 0}}},
         SQL_SUCCESS},
        {SQL_C_USHORT,
         {SQL_IS_MINUTE, 1, {.day_second = {0, 0, 20, 0, 0}}},
         SQL_SUCCESS},
        {SQL_C_ULONG,
         {SQL_IS_DAY, 1, {.day_second = {4, 0, 0, 0, 0}}},
         SQL_SUCCESS},
        {SQL_C_SBIGINT,
         {SQL_IS_HOUR, 1, {.day_second = {0, 3, 0, 0, 0}}},
         SQL_SUCCESS},
        {SQL_C_NUMERIC, {SQL_IS_MONTH, 1, {.year_month = {0, 8}}}, SQL_SUCCESS},
    };

// This test should follow translations according to
// https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-year-month-intervals?view=sql-server-ver16
// TODO(b/365915498): Data translation Utilities
void TestTranslationFromIntervalYearMonth(std::shared_ptr<ODBCHandles> conn,
                                          std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  SQLCHAR data_char[kBufferLength];
  SQLLEN strlen_or_ind;
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  for (auto const& expected : kConversionYearMonthIntervalTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data_char,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }
    if (SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }
    SQL_INTERVAL_STRUCT* returned_val =
        reinterpret_cast<SQL_INTERVAL_STRUCT*>(data_char);
    auto expected_val = expected.interval_value.intval;

    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string return_char_val = reinterpret_cast<char*>(data_char);
        std::string expected_char_val =
            FormatIntervalString(expected.interval_value);
        EXPECT_EQ(expected_char_val, return_char_val);
        break;
      }
      case SQL_C_INTERVAL_YEAR: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.year_month.year,
                  returned_val->intval.year_month.year);
        break;
      }
      case SQL_C_INTERVAL_MONTH: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.year_month.month,
                  returned_val->intval.year_month.month);
        break;
      }
      case SQL_C_FLOAT:
      case SQL_C_DOUBLE: {
        EXPECT_EQ(status, expected.status);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        std::string return_wchar_val = ConvertSQLWCHARToString(
            reinterpret_cast<SQLWCHAR*>(data_char), length);
        std::string expected_wchar_val =
            FormatIntervalString(expected.interval_value);
        EXPECT_STREQ(expected_wchar_val.data(), return_wchar_val.data());
        break;
      }
      case SQL_C_INTERVAL_YEAR_TO_MONTH: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.year_month.year,
                  returned_val->intval.year_month.year);
        EXPECT_EQ(expected_val.year_month.month,
                  returned_val->intval.year_month.month);
        break;
      }
      default:
        break;
    }
  }
}

// This test should follow translations according to
// /https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-day-time-intervals?view=sql-server-ver16
// TODO(b/365915498): Data translation Utilities
void TestTranslationFromIntervalDaySecond(std::shared_ptr<ODBCHandles> conn,
                                          std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  SQLCHAR data_char[kBufferLength];
  SQLLEN strlen_or_ind;
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  for (auto const& expected : kConversionDaySecondIntervalTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data_char,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }
    if (SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }
    SQL_INTERVAL_STRUCT* returned_val =
        reinterpret_cast<SQL_INTERVAL_STRUCT*>(data_char);
    auto expected_val = expected.interval_value.intval;

    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string return_char_val = reinterpret_cast<char*>(data_char);
        std::string expected_char_val =
            FormatIntervalString(expected.interval_value);
        EXPECT_EQ(expected_char_val, return_char_val);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        std::string return_wchar_val = ConvertSQLWCHARToString(
            reinterpret_cast<SQLWCHAR*>(data_char), length);
        std::string expected_wchar_val =
            FormatIntervalString(expected.interval_value);
        EXPECT_STREQ(expected_wchar_val.data(), return_wchar_val.data());
        break;
      }
      case SQL_C_INTERVAL_DAY: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected.interval_value.intval.day_second.day,
                  returned_val->intval.day_second.day);
        break;
      }
      case SQL_C_BIT:
      case SQL_C_FLOAT:
      case SQL_C_DOUBLE: {
        EXPECT_EQ(status, expected.status);
        break;
      }
      case SQL_C_INTERVAL_HOUR: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.hour,
                  returned_val->intval.day_second.hour);
        break;
      }
      case SQL_C_INTERVAL_MINUTE: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.minute,
                  returned_val->intval.day_second.minute);
        break;
      }
      case SQL_C_INTERVAL_SECOND: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.second,
                  returned_val->intval.day_second.second);
        break;
      }
      case SQL_C_INTERVAL_DAY_TO_HOUR: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.day,
                  returned_val->intval.day_second.day);
        EXPECT_EQ(expected_val.day_second.hour,
                  returned_val->intval.day_second.hour);
        break;
      }
      case SQL_C_INTERVAL_DAY_TO_MINUTE: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.day,
                  returned_val->intval.day_second.day);
        EXPECT_EQ(expected_val.day_second.hour,
                  returned_val->intval.day_second.hour);
        EXPECT_EQ(expected_val.day_second.minute,
                  returned_val->intval.day_second.minute);
        break;
      }
      case SQL_C_INTERVAL_DAY_TO_SECOND: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.day,
                  returned_val->intval.day_second.day);
        EXPECT_EQ(expected_val.day_second.hour,
                  returned_val->intval.day_second.hour);
        EXPECT_EQ(expected_val.day_second.minute,
                  returned_val->intval.day_second.minute);
        EXPECT_EQ(expected_val.day_second.second,
                  returned_val->intval.day_second.second);
        break;
      }

      case SQL_C_INTERVAL_HOUR_TO_MINUTE: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.hour,
                  returned_val->intval.day_second.hour);
        EXPECT_EQ(expected_val.day_second.minute,
                  returned_val->intval.day_second.minute);
        break;
      }
      case SQL_C_INTERVAL_HOUR_TO_SECOND: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.hour,
                  returned_val->intval.day_second.hour);
        EXPECT_EQ(expected_val.day_second.minute,
                  returned_val->intval.day_second.minute);
        EXPECT_EQ(expected_val.day_second.second,
                  returned_val->intval.day_second.second);
        break;
      }
      case SQL_C_INTERVAL_MINUTE_TO_SECOND: {
        EXPECT_EQ(expected.interval_value.interval_type,
                  returned_val->interval_type);
        EXPECT_EQ(expected_val.day_second.minute,
                  returned_val->intval.day_second.minute);
        EXPECT_EQ(expected_val.day_second.second,
                  returned_val->intval.day_second.second);
        break;
      }
      default:
        break;
    }
  }
}

void TestIntervalArithmeticConversion(std::shared_ptr<ODBCHandles> conn,
                                      std::string query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);
  for (auto const& expected : kConversionFromSinglePrecisionIntervalData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }
    CheckError(status, "SQLFetch", conn);
    switch (expected.target_c_type) {
      case SQL_C_STINYINT: {
        int8_t* returned_val = reinterpret_cast<int8_t*>(data);
        int8_t expected_val =
            static_cast<int8_t>(expected.interval_value.intval.year_month.year);
        EXPECT_EQ(*returned_val, expected_val);
        break;
      }
      case SQL_C_UTINYINT: {
        auto returned_val = reinterpret_cast<uint8_t*>(data);
        auto expected_val =
            static_cast<uint8_t>(expected.interval_value.intval.day_second.day);

        EXPECT_EQ(*returned_val, expected_val);

        break;
      }
      case SQL_C_SSHORT: {
        SQLSMALLINT* returned_val = reinterpret_cast<SQLSMALLINT*>(data);
        auto expected_val = static_cast<SQLSMALLINT>(
            expected.interval_value.intval.day_second.hour);
        EXPECT_EQ(*returned_val, expected_val);
        break;
      }
      case SQL_C_USHORT: {
        SQLUSMALLINT* returned_val = reinterpret_cast<SQLUSMALLINT*>(data);
        auto expected_val = static_cast<SQLUSMALLINT>(
            expected.interval_value.intval.day_second.minute);
        EXPECT_EQ(*returned_val, expected_val);
        break;
      }
      case SQL_C_ULONG: {
        SQLUINTEGER* returned_val = reinterpret_cast<SQLUINTEGER*>(data);
        auto expected_val = static_cast<SQLUINTEGER>(
            expected.interval_value.intval.day_second.day);
        EXPECT_EQ(*returned_val, expected_val);
        break;
      }
      case SQL_C_SBIGINT: {
        SQLBIGINT* returned_val = reinterpret_cast<SQLBIGINT*>(data);
        auto expected_val = static_cast<SQLUINTEGER>(
            expected.interval_value.intval.day_second.hour);
        EXPECT_EQ(*returned_val, expected_val);
        break;
      }
      case SQL_C_NUMERIC: {
        SQL_NUMERIC_STRUCT* returned_val =
            reinterpret_cast<SQL_NUMERIC_STRUCT*>(data);
        std::string returned_str = SQLNumericToString(*returned_val);
        auto expected_val =
            std::to_string(expected.interval_value.intval.year_month.month);
        EXPECT_EQ(expected_val, returned_str);
        break;
      }
      default:
        break;
    }
  }
}

void IntervalTestRunner(
    std::string const& table_name,
    std::vector<SQL_INTERVAL_STRUCT> const& interval_data,
    std::function<void(std::shared_ptr<ODBCHandles>, std::string const&)> const&
        TestTranslation) {
  auto conn = std::make_shared<ODBCHandles>();
  Table table(table_name);
  // Create Table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INT64, IntervalField INTERVAL)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertIntervalData(conn, interval_data);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Read data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string qry =
      "SELECT IntervalField FROM " + table_name + " ORDER BY index;";
  TestTranslation(conn, qry);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Drop table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DataTranslationTest, From_Interval_Year_Month) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_INTERVAL_YEAR_MONTH";
  std::vector<SQL_INTERVAL_STRUCT> interval_data;
  for (auto const& test_data : kConversionYearMonthIntervalTestData) {
    interval_data.push_back(test_data.interval_value);
  }

  IntervalTestRunner(table_name, interval_data,
                     TestTranslationFromIntervalYearMonth);
}

#ifdef BQ_DRIVER_INTEGRATION_TESTS
// Disable this test case as simba returning null values
TEST(DataTranslationTest, From_Interval_Day_Second) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_INTERVAL_DAY_SECOND";
  std::vector<SQL_INTERVAL_STRUCT> interval_data;
  for (auto const& test_data : kConversionDaySecondIntervalTestData) {
    interval_data.push_back(test_data.interval_value);
  }

  IntervalTestRunner(table_name, interval_data,
                     TestTranslationFromIntervalDaySecond);
}

TEST(DataTranslationTest, From_Interval_to_Arithmetic) {
  auto const table_name = kDatasetWithTablePrefix +
                          "ODBC_DATA_TRANSLATION_SQL_INTERVAL_TO_ARTHMETIC";
  std::vector<SQL_INTERVAL_STRUCT> interval_data;
  for (auto const& test_data : kConversionFromSinglePrecisionIntervalData) {
    interval_data.push_back(test_data.interval_value);
  }

  IntervalTestRunner(table_name, interval_data,
                     TestIntervalArithmeticConversion);
}
#endif /* BQ_DRIVER_INTEGRATION_TESTS */
}  // namespace google::cloud::odbc_tests
