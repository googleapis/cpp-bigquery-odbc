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
    std::string expected_val = FormatDateTime(expected.value);
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
        std::string returned_val = FormatDateTime(*timestamp);
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

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
