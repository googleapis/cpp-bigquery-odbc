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

#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "google/cloud/odbc/testing/odbc_utils/statement.h"

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

struct IntervalBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_INTERVAL_STRUCT interval_value;
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

StdIntervalRows const kIntervalSampleData{
    {1, {SQL_IS_YEAR, 1, {.year_month = {3, 0}}}},              // 3 years
    {2, {SQL_IS_MONTH, 1, {.year_month = {0, 8}}}},             // 8 months
    {3, {SQL_IS_DAY, 1, {.day_second = {15, 0, 0, 0, 0}}}},     // 15 days
    {4, {SQL_IS_HOUR, 1, {.day_second = {0, 20, 0, 0, 0}}}},    // 20 hours
    {5, {SQL_IS_MINUTE, 1, {.day_second = {0, 0, 45, 0, 0}}}},  // 45 minutes
    {6, {SQL_IS_SECOND, 1, {.day_second = {0, 0, 0, 30, 0}}}},  // 30 seconds
    {7,
     {SQL_IS_YEAR_TO_MONTH, 1, {.year_month = {2, 5}}}},  // 2 years, 5 months
    {8,
     {SQL_IS_DAY_TO_HOUR,
      1,
      {.day_second = {10, 14, 0, 0, 0}}}},  // 10 days, 14 hours
    {9,
     {SQL_IS_DAY_TO_MINUTE,
      1,
      {.day_second = {5, 0, 30, 0, 0}}}},  // 5 days, 30 minutes
    {10,
     {SQL_IS_DAY_TO_SECOND,
      1,
      {.day_second = {2, 0, 0, 20,
                      500}}}},  // 2 days, 20 seconds, 500 milliseconds
    {11,
     {SQL_IS_HOUR_TO_MINUTE,
      1,
      {.day_second = {0, 9, 45, 0, 0}}}},  // 9 hours, 45 minutes
    {12,
     {SQL_IS_HOUR_TO_SECOND,
      1,
      {.day_second = {0, 11, 0, 25,
                      300}}}},  // 11 hours, 25 seconds, 300 milliseconds
    {13,
     {SQL_IS_MINUTE_TO_SECOND,
      1,
      {.day_second = {0, 0, 50, 10,
                      100}}}}  // 50 minutes, 10 seconds, 100 milliseconds
};

std::vector<IntervalBasicTestStruct> const kConversionFromIntervalTestData{
    {SQL_C_CHAR, {SQL_IS_YEAR, 1, {.year_month = {3, 0}}}, SQL_SUCCESS},
    {SQL_C_CHAR, {SQL_IS_MONTH, 1, {.year_month = {0, 8}}}, SQL_SUCCESS},
    {SQL_C_CHAR,
     {SQL_IS_DAY, 1, {.day_second = {15, 0, 0, 0, 0}}},
     SQL_SUCCESS},
    {SQL_C_BINARY,
     {SQL_IS_HOUR, 1, {.day_second = {0, 20, 0, 0, 0}}},
     SQL_SUCCESS}};

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

inline std::string formatIntervalString(SQL_INTERVAL_STRUCT interval) {
  char buffer[50];

  switch (interval.interval_type) {
    case SQL_IS_YEAR:
      snprintf(buffer, sizeof(buffer), "%d-0 0 0:0:0",
               interval.intval.year_month.year);
      break;
    case SQL_IS_MONTH:
      snprintf(buffer, sizeof(buffer), "0-%d 0 0:0:0",
               interval.intval.year_month.month);
      break;
    case SQL_IS_YEAR_TO_MONTH:
      snprintf(buffer, sizeof(buffer), "%d-%d 0 0:0:0",
               interval.intval.year_month.year,
               interval.intval.year_month.month);
      break;
    case SQL_IS_DAY:
      snprintf(buffer, sizeof(buffer), "0-0 %d 0:0:0",
               interval.intval.day_second.day);
      break;
    case SQL_IS_HOUR:
      snprintf(buffer, sizeof(buffer), "0-0 0 %d:0:0",
               interval.intval.day_second.hour);
      break;
    case SQL_IS_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:0",
               interval.intval.day_second.minute);
      break;
    case SQL_IS_SECOND:
      snprintf(buffer, sizeof(buffer), "0-0 0 0:0:%d",
               interval.intval.day_second.second);
      break;
    case SQL_IS_DAY_TO_HOUR:
      snprintf(buffer, sizeof(buffer), "0-0 %d %d:0:0",
               interval.intval.day_second.day, interval.intval.day_second.hour);
      break;
    case SQL_IS_DAY_TO_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:0",
               interval.intval.day_second.day, interval.intval.day_second.hour,
               interval.intval.day_second.minute);
      break;
    case SQL_IS_DAY_TO_SECOND:
      snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:%d",
               interval.intval.day_second.day, interval.intval.day_second.hour,
               interval.intval.day_second.minute,
               interval.intval.day_second.second);
      break;
    case SQL_IS_HOUR_TO_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:0",
               interval.intval.day_second.hour,
               interval.intval.day_second.minute);
      break;
    case SQL_IS_HOUR_TO_SECOND:
      snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:%d",
               interval.intval.day_second.hour,
               interval.intval.day_second.minute,
               interval.intval.day_second.second);
      break;
    case SQL_IS_MINUTE_TO_SECOND:
      snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:%d",
               interval.intval.day_second.minute,
               interval.intval.day_second.second);
      break;
    default:
      snprintf(buffer, sizeof(buffer), "Unknown interval type");
      break;
  }
  return std::string(buffer);
}

// bool parseIntervalString(char* input_str, SQL_INTERVAL_STRUCT& interval){
//    int days, hours, minutes, seconds;
//    if (sscanf(input_str, "0-0 %d %d:%d:%d", &days, &hours, &minutes,
//    &seconds) != 4) {
//         std::cerr << "Error parsing input string: " << input_str <<
//         std::endl; return false;
//     }
//     interval.interval_type = SQL_IS_HOUR;  // Set the interval type to
//     SQL_IS_HOUR interval.interval_sign = 1;            // Positive interval

//     // Populate the day_second struct with the parsed values
//     interval.intval.day_second.day = days;
//     interval.intval.day_second.hour = hours;
//     interval.intval.day_second.minute = minutes;
//     interval.intval.day_second.second = seconds;
//     interval.intval.day_second.fraction = 0;  // Assume no fraction in input
//     return true;
// }

void TestTranslationFromInterval(std::shared_ptr<ODBCHandles> conn,
                                 std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;

  StrToChar(read_stmt, query.c_str());
  std::cout << "fetch Query " << query << std::endl;

  int row_count = 0;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromIntervalTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }

    if (SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        std::string expected_val =
            formatIntervalString(expected.interval_value);
        EXPECT_EQ(expected_val, returned_val);
        break;
      }
      case SQL_C_WCHAR: {
        // To be implemented
        std::string returned_val = reinterpret_cast<char*>(data);
      }
      case SQL_C_BINARY: {
        // SImba data not converting to SQL interval struut.
      }

      default:
        break;
    }
    ++row_count;
  }
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
  TestTranslationsFromString(conn, query);
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
  TestTranslationsFromArithmetic<NumericBasicTestStruct>(
      conn, query, kConversionFromNumericTestData);
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
  TestTranslationsFromArithmetic<Int64BasicTestStruct>(
      conn, query, kConversionFromInt64TestData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DataTranslationTest, From_Interval_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_INTERVAL";
  Table table(table_name);
  auto conn = std::make_shared<ODBCHandles>();

  // Create Table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Create(conn, "(index INT64, IntervalField INTERVAL)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertIntervalData(conn, kIntervalSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Read data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string qry =
      "SELECT IntervalField FROM " + table_name + " ORDER BY index";
  TestTranslationFromInterval(conn, qry);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // drop table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
