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
struct TimestampBasicTestStruct {
  // The target C type SQLBindCol will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLBindCol if it succeeds
  SQL_TIMESTAMP_STRUCT value;
  // The status that should be returned by SQLBindCol for this C Type
  SQLRETURN status;
};

using StdTimestampRows = std::vector<TimestampBasicTestStruct>;

StdTimestampRows const kConversionFromTimestampTestData{
    {SQL_C_CHAR, {2024, 01, 20, 10, 20, 30, 123112}, SQL_SUCCESS},
    {SQL_C_WCHAR, {2024, 01, 20, 11, 2, 33, 1212}, SQL_SUCCESS},
    {SQL_C_BINARY, {2024, 01, 20, 2, 20, 22, 123123}, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {2024, 01, 20, 12, 22, 11, 32223}, SQL_SUCCESS},
    {SQL_C_TYPE_TIME, {2024, 01, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2024, 01, 20, 12, 21, 22, 000000}, SQL_SUCCESS},
    {SQL_C_SLONG, {2024, 01, 20, 00, 00, 00, 000000}, SQL_ERROR},
};

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
  // The value that is inserted into the table
  std::string value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
  // The str that should be returned by SQLGetData if it succeeds
  std::string expected_str;
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
  {SQL_C_NUMERIC, "1234567891234567891", SQL_SUCCESS,
    "1234567891234567891"},  // NUMERIC(38,9) allows only 19 digits for
                             // integral value. Weird!
   {SQL_C_NUMERIC, "-1234567891234567891", SQL_SUCCESS,
    "-1234567891234567891"},
   // TODO(b/345194139): check if digits after decimal are expected to be
   
   #ifdef BQ_DRIVER_INTEGRATION_TESTS
    {SQL_C_NUMERIC, "123.78", SQL_SUCCESS, "123.78"},
    {SQL_C_NUMERIC, "999.78354", SQL_SUCCESS, "999.78354"},
    {SQL_C_NUMERIC, "123.7835", SQL_SUCCESS, "123.7835"},
    {SQL_C_DOUBLE, "123123123123123123123.222", SQL_SUCCESS},
    {SQL_C_CHAR, "-123", SQL_SUCCESS},  // in simba it returns "-123.000000000"
    {SQL_C_CHAR, "123.222",
     SQL_SUCCESS},  // in simba it returns "123.222000000"
    {SQL_C_CHAR, "-123.222", SQL_SUCCESS},
    {SQL_C_CHAR, "123123123123123123123.222",
     SQL_SUCCESS},  // simba returns  123123123123123123123.222000000
#endif

   // omitted
   {SQL_C_NUMERIC, "-123456789123456.78", SQL_SUCCESS_WITH_INFO,
    "-123456789123456"},
// TODO(b/345194139): Check why this string is returned as
// "16208844614347859008"
// {SQL_C_NUMERIC, "12345678912345678912345678912.345678912",
//  SQL_SUCCESS_WITH_INFO},
#ifdef BQ_DRIVER_INTEGRATION_TESTS
   // Existing driver returns "123.000000000" here
   {SQL_C_CHAR, "123", SQL_SUCCESS},
#endif  // BQ_DRIVER_INTEGRATION_TESTS
   {SQL_C_NUMERIC, "1234567891234567", SQL_SUCCESS, "1234567891234567"},
 {SQL_C_NUMERIC, "-1234567891234567", SQL_SUCCESS, "-1234567891234567"},

 {SQL_C_DOUBLE, "-9.9999999999999999999999999999999999999E+28", SQL_SUCCESS},
 {SQL_C_DOUBLE, "9.9999999999999999999999999999999999999E+28", SQL_SUCCESS},

 {SQL_C_DOUBLE, "38.3", SQL_SUCCESS},
 {SQL_C_DOUBLE, "-38.3", SQL_SUCCESS},

 {SQL_C_CHAR, "99999999999999999999999999999.999999999", SQL_SUCCESS},
   {SQL_C_FLOAT, "156.1", SQL_SUCCESS},
   {SQL_C_FLOAT, "-157.8", SQL_SUCCESS},
   {SQL_C_DOUBLE, "-38.3", SQL_SUCCESS},
   {SQL_C_SSHORT, "31", SQL_SUCCESS},
   {SQL_C_SSHORT, "-31", SQL_SUCCESS},
   {SQL_C_USHORT, "3", SQL_SUCCESS},
   {SQL_C_USHORT, "65537" , SQL_ERROR},
   {SQL_C_SLONG, "-13", SQL_SUCCESS},
   {SQL_C_SLONG, "13.3",
    SQL_SUCCESS_WITH_INFO},  // SQL_SUCCESS_WITH_INFO because there is loss of
                             // precision
   {SQL_C_ULONG, "81", SQL_SUCCESS},
   {SQL_C_ULONG, "-8", SQL_ERROR},
   {SQL_C_ULONG, "1.1",
    SQL_SUCCESS_WITH_INFO},  // SQL_SUCCESS_WITH_INFO because
                             // there is loss of precision
   {SQL_C_BIT, "0", SQL_SUCCESS},
   {SQL_C_BIT, "1", SQL_SUCCESS},
   {SQL_C_BIT, "2", SQL_ERROR},
};

std::vector<
  NumericBasicTestStruct> const kConversionFromNumericTestData_bignumeric{
#ifdef BQ_DRIVER_INTEGRATION_TESTS
{SQL_C_NUMERIC, "123.78", SQL_SUCCESS, "123.78"}, // SQLNumericToString returns 123 as the scale info is not correct
  {SQL_C_NUMERIC, "999.78354", SQL_SUCCESS, "999.78354"},
  {SQL_C_NUMERIC, "123.7835", SQL_SUCCESS, "123.7835"},
  {SQL_C_NUMERIC, "123456789", SQL_SUCCESS, "123456789"},
#endif
  {SQL_C_NUMERIC, "1234567891234567", SQL_SUCCESS, "1234567891234567"},
  {SQL_C_NUMERIC, "-1234567891234567", SQL_SUCCESS, "-1234567891234567"},

  {SQL_C_DOUBLE,
   "-5."
   "7896044618658097711785492504343953926634992332820282019728792003956564819"
   "968E+38",
   SQL_SUCCESS},
  {SQL_C_DOUBLE,
   "5."
   "7896044618658097711785492504343953926634992332820282019728792003956564819"
   "967E+38",
   SQL_SUCCESS},
#ifdef BQ_DRIVER_INTEGRATION_TESTS
  {SQL_C_DOUBLE, "9.9999999999999999999999999999999999999E+29", SQL_SUCCESS},
  {SQL_C_DOUBLE, "9.9999999999999999999999999999999999999E+28", SQL_SUCCESS},
  {SQL_C_DOUBLE, "123123123123123123123.222", SQL_SUCCESS},

  {SQL_C_CHAR, "-123",
   SQL_SUCCESS},  // simba returns -123.00000000000000000000000000000000000000
  {SQL_C_CHAR, "123.222", SQL_SUCCESS},
  {SQL_C_CHAR, "-123.222",
   SQL_SUCCESS},  // simba returns 123.22200000000000000000000000000000000000
  {SQL_C_CHAR, "99999999999999999999999999999.999999999", SQL_SUCCESS},
  {SQL_C_CHAR, "123123123123123123123.222", SQL_SUCCESS},
  {SQL_C_SSHORT, "31", SQL_SUCCESS},   // fails in simba  gives SQL_SUCCESS_WITH_INFO
  {SQL_C_SSHORT, "-31", SQL_SUCCESS},  // fails in simba  gives SQL_SUCCESS_WITH_INFO
  {SQL_C_USHORT, "3", SQL_SUCCESS},    // fails in simba  gives SQL_SUCCESS_WITH_INFO
  {SQL_C_SLONG, "-13", SQL_SUCCESS},   // fails in simba  gives SQL_SUCCESS_WITH_INFO
  {SQL_C_ULONG, "81", SQL_SUCCESS},    // fails in simba  gives SQL_SUCCESS_WITH_INFO
#endif
  {SQL_C_FLOAT, "156.1", SQL_SUCCESS},
  {SQL_C_FLOAT, "-157.8", SQL_SUCCESS},

  {SQL_C_DOUBLE, "38.3", SQL_SUCCESS},
  {SQL_C_DOUBLE, "-38.3", SQL_SUCCESS},

  {SQL_C_USHORT, "65537" /* 2^16 + 1 */, SQL_ERROR},

  {SQL_C_SLONG, "13.3", SQL_SUCCESS_WITH_INFO},

  {SQL_C_ULONG, "-8", SQL_ERROR},
  {SQL_C_ULONG, "1.1", SQL_SUCCESS_WITH_INFO},

  {SQL_C_BIT, "0", SQL_SUCCESS},
  {SQL_C_BIT, "1", SQL_SUCCESS},
  {SQL_C_BIT, "2", SQL_ERROR},
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

StdAllTypesRows const kConversionFromDifferentTestData{
    {
        "",
        1,
        1.1,
        {2024, 01, 20, 10, 20, 30, 123112},
        {2024, 2, 20},
        {11, 9, 20},
        {{"age", 30}, {"name", "Sita"}},
    },
    {
        "Test String 2",
        NULL,
        2.22,
        {2024, 01, 20, 11, 2, 33, 1212},
        {2024, 3, 12},
        {22, 45, 54},
        {{"age", 30}, {"name", "Alice"}},
    },
    {
        "Test String 3",
        12,
        NULL,
        {2024, 01, 20, 2, 20, 22, 123123},
        {2024, 4, 20},
        {2, 36, 29},
        {{"age", 90}, {"name", "Ram"}},
    },
    {
        "Test String 4",
        49,
        2.0,
        {00, 00, 00, 00, 00, 00, 00},
        {2024, 4, 29},
        {9, 07, 20},
        {{"age", 26}, {"name", "Bob"}},
    },
    {
        "Test String 5",
        53,
        5,
        {2024, 01, 20, 00, 00, 00, 000000},
        {00, 00, 00},
        {04, 06, 07},
        {{"age", 32}, {"name", "Kapoor"}},
    },
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
  for (TestStruct expected : expected_config) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }

    SQLSMALLINT resp_status, resp_status_len;
    std::cout << "Testing row: " << expected.target_c_type << ", "
              << expected.value << ", " << expected.status << std::endl;
    EXPECT_EQ(status, expected.status);
    if (status != SQL_SUCCESS) {
      row_count++;
      continue;
    }
    CheckError(status,
               "SQLFetch(" + std::to_string(expected.target_c_type) + ")",
               conn);
    if (strlen_or_ind >= 0) {
      row_count++;
      // Refer
      // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16
      // to understand the expectations regarding typecasting applications
      // buffers.
      switch (expected.target_c_type) {
        case SQL_C_CHAR: {
          std::string returned_val = (char*)data;
          EXPECT_EQ(std::stod(returned_val), expected.value);
          break;
        }
        case SQL_C_FLOAT: {
          SQLREAL* returned_val = (SQLREAL*)data;
          SQLREAL expected_val = expected.value;
          EXPECT_EQ(*returned_val, expected_val);
          break;
        }
        case SQL_C_DOUBLE: {
          SQLDOUBLE* returned_val = (SQLDOUBLE*)data;
          EXPECT_EQ(*returned_val, expected.value);
          break;
        }
        case SQL_C_SLONG: {
          SQLINTEGER* returned_val = (SQLINTEGER*)data;
          EXPECT_EQ(*returned_val, expected.value);
          break;
        }
        case SQL_C_SSHORT: {
          SQLSMALLINT* returned_val = (SQLSMALLINT*)data;
          EXPECT_EQ(*returned_val, expected.value);
          break;
        }
        case SQL_C_USHORT: {
          SQLUSMALLINT* returned_val = (SQLUSMALLINT*)data;
          EXPECT_EQ(*returned_val, expected.value);
          break;
        }
        case SQL_C_ULONG: {
          SQLUINTEGER* returned_val = (SQLUINTEGER*)data;
          EXPECT_EQ(*returned_val, expected.value);
          break;
        }
        case SQL_C_BIT: {
          SQLCHAR* returned_val = (SQLCHAR*)data;
          EXPECT_EQ(*returned_val, expected.value);
          break;
        }
        default: {
          break;
        }
      }
    }
  }
  EXPECT_EQ(row_count, expected_config.size());
}

#ifndef BQ_DRIVER_INTEGRATION_TESTS

void TestTranslationsFromNumeric(
  std::shared_ptr<ODBCHandles> conn, std::string query,
  std::vector<NumericBasicTestStruct> const kFromNumericTestData) {
SQLRETURN status;
SQLCHAR data[kBufferLength];
SQLLEN strlen_or_ind;

char read_stmt[kBufferLength];
StrToChar(read_stmt, query);
status = SQLExecDirect(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
CheckError(status, "SQLExecDirect", conn, false);

// Read all the rows using SQLFetch
int row_count = 0;
for (NumericBasicTestStruct expected : kFromNumericTestData) {
  status = SQLFetch(conn->hstmt);
  if (status == SQL_NO_DATA) {
    break;
  }
  if (!SQL_SUCCEEDED(status)) {
    CheckError(status, "SQLFetch", conn);
  }

  SQLSMALLINT resp_status, resp_status_len;
  status = SQLGetData(conn->hstmt, 1, expected.target_c_type, data,
                      kBufferLength, &strlen_or_ind);
  std::cout << "Testing row: " << expected.target_c_type << ", "
            << expected.value << ", " << expected.status << std::endl;
  EXPECT_EQ(status, expected.status);
  if (!SQL_SUCCEEDED(status)) {
    row_count++;
    std::cout << "Testing FAIL: " << std::endl;
    continue;
  }
  CheckError(status,
             "SQLGetData(" + std::to_string(expected.target_c_type) + ")",
             conn);
  if (strlen_or_ind >= 0) {
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = (char*)data;
        EXPECT_EQ(returned_val, expected.value);
        break;
      }
      case SQL_C_FLOAT: {
        SQLREAL* returned_val = (SQLREAL*)data;
        EXPECT_EQ(*returned_val, std::stof(expected.value));
        break;
      }
      case SQL_C_DOUBLE: {
        SQLDOUBLE* returned_val = (SQLDOUBLE*)data;
        EXPECT_EQ(*returned_val, std::stod(expected.value));
        break;
      }
      case SQL_C_SSHORT: {
        SQLSMALLINT* returned_val = (SQLSMALLINT*)data;
        EXPECT_EQ(*returned_val, std::stoi(expected.value));
        break;
      }
      case SQL_C_USHORT: {
        SQLUSMALLINT* returned_val = (SQLUSMALLINT*)data;
        EXPECT_EQ(*returned_val, std::stoi(expected.value));
        break;
      }
      case SQL_C_SLONG: {
        SQLINTEGER* returned_val = (SQLINTEGER*)data;
        EXPECT_EQ(*returned_val, std::stoi(expected.value));
        break;
      }
      case SQL_C_ULONG: {
        SQLUINTEGER* returned_val = (SQLUINTEGER*)data;
        EXPECT_EQ(*returned_val, std::stoi(expected.value));
        break;
      }
      case SQL_C_BIT: {
        SQLCHAR* returned_val = (SQLCHAR*)data;
        EXPECT_EQ(*returned_val, std::stod(expected.value));
        break;
      }
      case SQL_C_NUMERIC: {
        SQL_NUMERIC_STRUCT returned_val = *(SQL_NUMERIC_STRUCT*)data;
        EXPECT_EQ(SQLNumericToString(returned_val), expected.expected_str);

        break;
      }
      default: {
        FAIL() << "case not handled!" << std::endl;
      }
    }
    row_count++;
  }
}
EXPECT_EQ(row_count, kFromNumericTestData.size());
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
  // TODO(b/357794952): Simba Driver For Windows, SQLGetDiagField API for
  // SQL_DIAG_RETURNCODE not returning values.
  TestTranslationsFromString(conn, query);
#endif  // _WIN32
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This test should follow translations according to
// https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-to-c-numeric?view=sql-server-ver16
TEST(DataTranslationTest, From_NUMERIC_to_all) {
  std::vector<std::string> data_type_names;
  data_type_names.push_back("NUMERIC");
  data_type_names.push_back("DECIMAL");
  for (std::string t_name : data_type_names) {
    auto const table_name =
        kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_NUMERIC" + t_name;
    Table table(table_name);
    // Create Table
    auto conn = std::make_shared<ODBCHandles>();
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    table.CreateWithPrepare(conn, "(index INT64, NumericField " + t_name + ")");
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  
    // Insert data to read
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    std::vector<std::string> numeric_data_to_insert;
    for (auto elem : kConversionFromNumericTestData) {
      numeric_data_to_insert.push_back(elem.value);
    }
    table.InsertNumericData(conn, numeric_data_to_insert, true);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  
    // Execute a read query and check whether the results returned are as
    // expected
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    std::string query =
        "SELECT NumericField FROM " + table_name + " ORDER BY index";
    TestTranslationsFromNumeric(conn, query, kConversionFromNumericTestData);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  
    // Delete table
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    table.DropWithPrepare(conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }
  }
  
  TEST(DataTranslationTest, From_BIGNUMERIC_All) {
  std::vector<std::string> type_names;
  type_names.push_back("BIGNUMERIC");
  type_names.push_back("BIGDECIMAL");
  for (std::string t_name : type_names) {
    auto const table_name =
        kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_SQL_NUMERIC" + t_name;
    Table table(table_name);
    // Create Table
    auto conn = std::make_shared<ODBCHandles>();
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    table.CreateWithPrepare(conn, "(index INT64, NumericField " + t_name + ")");
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
    // Insert data to read
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    std::vector<std::string> numeric_data_to_insert;
    for (auto elem : kConversionFromNumericTestData_bignumeric) {
      numeric_data_to_insert.push_back(elem.value);
    }
    table.InsertNumericData(conn, numeric_data_to_insert, true);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
    // Execute a read query and check whether the results returned are as
    // expected
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    std::string query =
        "SELECT NumericField FROM " + table_name + " ORDER BY index";
    TestTranslationsFromNumeric(conn, query,
                                kConversionFromNumericTestData_bignumeric);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
    // Delete table
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    table.DropWithPrepare(conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }
  }
  
  TEST(DataTranslationTest, BIGNUMERIC_Invalid_Data_Test) {
  std::string data_big_numeric = "BIGNUMERIC";
  std::string data_numeric = "NUMERIC";
  std::vector<double> invalid_big_numeric = {
      -5.7896044618658097711785492504343953926634992332820282019728792003956564819968E+40,
      -5.7896044618658097711785492504343953926634992332820282019728792003956564819968E+39,
      5.7896044618658097711785492504343953926634992332820282019728792003956564819968E+39,
      5.7896044618658097711785492504343953926634992332820282019728792003956564819968E+45};
  std::vector<double> invalid_numeric_decimal = {
      9.9999999999999999999999999999999999999E+29,
      9.9999999999999999999999999999999999999E+32,
      -9.9999999999999999999999999999999999999E+29,
      -9.9999999999999999999999999999999999999E+40};
  std::map<std::string, std::vector<double>> data_type_with_data;
  data_type_with_data.insert(make_pair(data_big_numeric, invalid_big_numeric));
  data_type_with_data.insert(make_pair(data_numeric, invalid_numeric_decimal));
  int index = 0;
  for (auto data : data_type_with_data) {
    std::string field_type = data.first;
    auto const table_name = kDatasetWithTablePrefix +
                            "ODBC_DATA_TRANSLATION_SQL_NUMERIC_INVALID_DATA" +
                            field_type;
    Table table(table_name);
    // Create Table
    auto conn = std::make_shared<ODBCHandles>();
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    table.CreateWithPrepare(conn,
                            "(index INT64, NumericField " + field_type + ")");
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
    // Insert data to read
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  
    auto all_invalid_data = data.second;
    for (auto each_invalid_data : all_invalid_data) {
      auto insert_stmt = "INSERT INTO " + table_name + " VALUES ";
      std::string row_str = "( ";
      row_str.append(std::to_string(index) + ", ");
      row_str.append(std::to_string(each_invalid_data));
      row_str.append(")");
      insert_stmt.append(row_str);
      SQLRETURN status =
          SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
      status = SQLExecute(conn->hstmt);
      EXPECT_EQ(status, SQL_ERROR);
    }
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    table.DropWithPrepare(conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }
  }
  #endif  // BQ_DRIVER_INTEGRATION_TESTS

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
  // TODO(b/357794952): Simba Driver For Windows, SQLGetDiagField API for
  // SQL_DIAG_RETURNCODE not
  //  returning
  // values..
  TestTranslationsFromArithmetic<Int64BasicTestStruct>(
      conn, query, kConversionFromInt64TestData);
#endif  // _WIN32
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

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

struct BooleanBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQLCHAR value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<BooleanBasicTestStruct> const kConversionFromBooleanTestData{
    {SQL_C_CHAR, '1', SQL_SUCCESS},  {SQL_C_BIT, 0, SQL_SUCCESS},
    {SQL_C_BINARY, 1, SQL_SUCCESS},  {SQL_C_WCHAR, L'1', SQL_SUCCESS},
    {SQL_C_DOUBLE, 0, SQL_SUCCESS},  {SQL_C_LONG, 1, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, 0, SQL_ERROR},
};

void TestTranslationsFromBoolean(std::shared_ptr<ODBCHandles> conn,
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

  for (auto const& expected : kConversionFromBooleanTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);

    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      ++row_count;
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      ++row_count;
      break;
    }
    EXPECT_EQ(SQL_SUCCESS, expected.status);

    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        std::string expected_val(1, expected.value);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_BIT: {
        SQLCHAR returned_val = *reinterpret_cast<SQLCHAR*>(data);
        EXPECT_EQ(returned_val, expected.value);
        break;
      }
      case SQL_C_BINARY: {
        if (strlen_or_ind == sizeof(SQLCHAR)) {
          SQLCHAR* binary_value = reinterpret_cast<SQLCHAR*>(data);
          EXPECT_EQ(*binary_value, expected.value);
        }
        break;
      }
      case SQL_C_WCHAR: {
        std::wstring wstr = reinterpret_cast<wchar_t*>(data);
        std::string returned_val_utf8 =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), 1);
        std::string expected_val(1, expected.value);
        EXPECT_STREQ(returned_val_utf8.data(), expected_val.data());
        break;
      }
      case SQL_C_DOUBLE: {
        SQLDOUBLE returned_val = *reinterpret_cast<SQLDOUBLE*>(data);
        SQLDOUBLE expected_val = static_cast<SQLDOUBLE>(expected.value);
        EXPECT_DOUBLE_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_LONG: {
        SQLINTEGER returned_val = *reinterpret_cast<SQLINTEGER*>(data);
        SQLINTEGER expected_val = static_cast<SQLINTEGER>(expected.value);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      default:
        break;
    }
    ++row_count;
  }
  EXPECT_EQ(row_count, kConversionFromBooleanTestData.size());
}

TEST(DataTranslationTest, From_SQL_Boolean_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_BOOLEAN";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, BoolField BOOLEAN)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<SQLCHAR> boolean_data;
  for (auto const& test_case : kConversionFromBooleanTestData) {
    boolean_data.push_back(test_case.value);
  }
  table.InsertBooleanData(conn, boolean_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query = "SELECT BoolField FROM " + table_name + " Order by index";
  TestTranslationsFromBoolean(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

std::vector<ArrayBasicTestStruct> const kConversionFromArrayTestData{
    {SQL_C_CHAR,
     {1, 2, 3, 4, 5},
     {1.1, 2.1, 3.1, 4.1, 5.1},
     {"This", "Is", "Array", "Test", "Data"},
     {{1, 1.1, "data1"}, {2, 2.2, "data2"}},
     SQL_SUCCESS},
    {SQL_C_CHAR,
     {12, 21, 32, 33},
     {12.2, 21.4, 32.22, 33.21},
     {"Apple", "Banana", "Mango", "Pear"},
     {{12, 12.1, "data12"}, {21, 21.2, "data22"}},
     SQL_SUCCESS},

    {SQL_C_WCHAR,
     {121, 123, 1212},
     {121.211, 123.1, 1.21},
     {"Apple", "Orange", "Cherry"},
     {{13, 13.1, "data13"}, {31, 31.2, "data32"}},
     SQL_SUCCESS},
};

void TestArraySQLBindColData(std::shared_ptr<ODBCHandles> conn,
                             std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  SQLCHAR data_int[kBufferLength];
  SQLCHAR data_double[kBufferLength];
  SQLCHAR data_string[kBufferLength];
  SQLLEN strlen_or_ind;
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  int row_count = 0;
  for (auto const& expected : kConversionFromArrayTestData) {
    std::vector<std::string> ret_int_values;
    std::vector<std::string> ret_double_values;
    std::vector<std::string> ret_string_values;
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data_int,
                        kBufferLength, &strlen_or_ind);

    CheckError(status, "SQLBindColInt", conn);

    status = SQLBindCol(conn->hstmt, 2, expected.target_c_type, data_double,
                        kBufferLength, &strlen_or_ind);

    CheckError(status, "SQLBindColDouble", conn);

    status = SQLBindCol(conn->hstmt, 3, expected.target_c_type, data_string,
                        kBufferLength, &strlen_or_ind);

    CheckError(status, "SQLBindColString", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      break;
    }

    EXPECT_EQ(SQL_SUCCESS, expected.status);
    std::string str_int(reinterpret_cast<char*>(data_int));
    if (expected.target_c_type == SQL_C_WCHAR) {
      str_int = ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data_int),
                                        SQL_NTS);
    }
    try {
      // Parse JSON
      nlohmann::json json_object_int = nlohmann::json::parse(str_int);

      if (json_object_int["v"].is_array()) {
        for (auto const& element : json_object_int["v"]) {
          ret_int_values.emplace_back(element["v"]);
        }
      }

    } catch (nlohmann::json::exception& e) {
      std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }

    std::string str_double(reinterpret_cast<char*>(data_double));
    if (expected.target_c_type == SQL_C_WCHAR) {
      str_double = ConvertSQLWCHARToString(
          reinterpret_cast<SQLWCHAR*>(data_double), SQL_NTS);
    }
    try {
      // Parse JSON
      nlohmann::json json_object_double = nlohmann::json::parse(str_double);

      if (json_object_double["v"].is_array()) {
        for (auto const& element : json_object_double["v"]) {
          ret_double_values.emplace_back(element["v"]);
        }
      }

    } catch (nlohmann::json::exception& e) {
      std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }

    std::string str_string(reinterpret_cast<char*>(data_string));
    if (expected.target_c_type == SQL_C_WCHAR) {
      str_string = ConvertSQLWCHARToString(
          reinterpret_cast<SQLWCHAR*>(data_string), SQL_NTS);
    }
    try {
      // Parse JSON
      nlohmann::json json_object_string = nlohmann::json::parse(str_string);

      if (json_object_string["v"].is_array()) {
        for (auto const& element : json_object_string["v"]) {
          ret_string_values.emplace_back(element["v"]);
        }
      }

    } catch (nlohmann::json::exception& e) {
      std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }

    EXPECT_EQ(ret_int_values.size(), expected.int_value.size());
    EXPECT_EQ(ret_double_values.size(), expected.double_value.size());
    EXPECT_EQ(ret_string_values.size(), expected.string_value.size());
    for (int i = 0; i < expected.int_value.size(); i++) {
      EXPECT_EQ(std::stoi(ret_int_values[i]), expected.int_value[i]);
    }

    for (int i = 0; i < expected.double_value.size(); i++) {
      EXPECT_EQ(std::stod(ret_double_values[i]), expected.double_value[i]);
    }

    for (int i = 0; i < expected.string_value.size(); i++) {
      EXPECT_EQ(ret_string_values[i], expected.string_value[i]);
    }
    row_count++;
  }
  EXPECT_EQ(kConversionFromArrayTestData.size(), row_count);
}

TEST(DataTranslationTest, From_SQL_Array_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_ARRAY";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn,
      "(index INTEGER, IntArrayField ARRAY<INT64>, DoubleArrayField "
      "ARRAY<FLOAT64>, StringArrayField ARRAY<STRING>, StructData "
      "ARRAY<STRUCT<int_value INT64, "
      "float_value FLOAT64, string_value STRING>>)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertArrayData(conn, kConversionFromArrayTestData, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as
  // expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT IntArrayField, DoubleArrayField, StringArrayField FROM " +
      table_name + " Order by index";
  TestArraySQLBindColData(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void TestArraySQLStatement(std::shared_ptr<ODBCHandles> conn,
                           std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  SQLCHAR data_int[kBufferLength];
  SQLLEN strlen_or_ind;
  int count = 5;
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  std::vector<std::string> ret_int_values;
  status = SQLBindCol(conn->hstmt, 1, SQL_C_CHAR, data_int, kBufferLength,
                      &strlen_or_ind);

  CheckError(status, "SQLBindColInt", conn);

  status = SQLFetch(conn->hstmt);
  CheckError(status, "SQLFetch", conn);

  std::string str_int(reinterpret_cast<char*>(data_int));
  try {
    // Parse JSON
    nlohmann::json json_object_int = nlohmann::json::parse(str_int);

    if (json_object_int["v"].is_array()) {
      for (auto const& element : json_object_int["v"]) {
        ret_int_values.emplace_back(element["v"]);
      }
    }

  } catch (nlohmann::json::exception& e) {
    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
  }

  EXPECT_EQ(ret_int_values.size(), count);
  for (int i = 1; i <= count; i++) {
    EXPECT_EQ(std::stoi(ret_int_values[i - 1]), i);
  }
}

TEST(DataTranslationTest, From_SQL_Array_SQL_Statement) {
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query = "SELECT [1, 2, 3,4,5] AS numbers";
  TestArraySQLStatement(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void TestArrayStructData(std::shared_ptr<ODBCHandles> conn, std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  for (auto const& expected : kConversionFromArrayTestData) {
    std::vector<std::string> ret_values;
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);

    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      break;
    }

    EXPECT_EQ(SQL_SUCCESS, expected.status);
    std::string str(reinterpret_cast<char*>(data));
    if (expected.target_c_type == SQL_C_WCHAR) {
      str = ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), SQL_NTS);
    }
    try {
      // Parse JSON
      nlohmann::json json_object = nlohmann::json::parse(str);

      if (json_object["v"].is_array()) {
        int row_count = 0;
        for (auto const& element : json_object["v"]) {
          nlohmann::json json_object_inner = element["v"];
          int index = 0;
          for (auto const& element_inner : json_object_inner["f"]) {
            std::string ret_val = element_inner["v"];
            if (index == 0) {
              EXPECT_EQ(std::stoi(ret_val),
                        expected.struct_value[row_count].int_value);
            } else if (index == 1) {
              EXPECT_EQ(std::stod(ret_val),
                        expected.struct_value[row_count].double_value);
            } else if (index == 2) {
              EXPECT_EQ(ret_val, expected.struct_value[row_count].string_value);
            }
            index++;
          }
          row_count++;
        }
      }

    } catch (nlohmann::json::exception& e) {
      std::cerr << "Error parsing JSON: " << e.what() << std::endl;
    }
  }
}

TEST(DataTranslationTest, From_SQL_Array_Struct) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_ARRAY_STRUCT";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn,
      "(index INTEGER, IntArrayField ARRAY<INT64>, DoubleArrayField "
      "ARRAY<FLOAT64>, StringArrayField ARRAY<STRING>, StructData "
      "ARRAY<STRUCT<int_value INT64, "
      "float_value FLOAT64, string_value STRING>>)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertArrayData(conn, kConversionFromArrayTestData, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as
  // expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT StructData FROM " + table_name + " Order by index";
  TestArrayStructData(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// TODO(b/394015883): Add more cases of Struct into StructBasicTestStruct
struct StructTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The expected values for the STRUCT fields
  StructBasicTestStruct values;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<StructTestStruct> const kConversionFromStructTestData{
    {SQL_C_CHAR,
     {12345, 3.14, "Test String 1", std::vector<int>{1, 2, 3}},
     SQL_SUCCESS},
    {SQL_C_WCHAR,
     {-99999, 42.42, "Negative Test", std::vector<int>{5, 2, 8}},
     SQL_SUCCESS},
    {SQL_C_BINARY, {0, 0.0, "Empty String", std::nullopt}, SQL_SUCCESS},
    {SQL_C_DOUBLE, {99999, 100.1, "Positive Test", std::nullopt}, SQL_ERROR},
    {SQL_C_SHORT, {29, 29.29, "Test String 2", std::nullopt}, SQL_ERROR},
    {SQL_C_LONG, {6290, 2.71, "Another String 2", std::nullopt}, SQL_ERROR},
    {SQL_C_TYPE_DATE, {78, 89.9, "Error", std::nullopt}, SQL_ERROR},
    {SQL_C_BIT, {67890, -2.71, "Another String", std::nullopt}, SQL_ERROR},
};

void TestTranslationsFromStruct(std::shared_ptr<ODBCHandles> conn,
                                std::string query) {
  SQLRETURN status = ExecWithPrepare(conn, query);
  CheckError(status, "Execute with Prepare", conn);

  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  int row_count = 0;

  for (auto const& expected : kConversionFromStructTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) break;
    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      ++row_count;
      continue;
    }
    EXPECT_EQ(SQL_SUCCESS, expected.status);

    std::string returned_val;
    if (expected.target_c_type == SQL_C_WCHAR) {
      returned_val = ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data),
                                             strlen_or_ind / sizeof(SQLWCHAR));
    } else {
      returned_val = std::string(reinterpret_cast<char*>(data), strlen_or_ind);
    }

    try {
      nlohmann::json json_object = nlohmann::json::parse(returned_val);
      if (json_object["v"].is_array()) {
        for (auto const& element : json_object["v"]) {
          int index = 0;
          for (auto const& field : element["v"]["f"]) {
            std::string ret_val = field["v"];
            if (index == 0)
              EXPECT_EQ(std::stoi(ret_val), expected.values.int_value);
            else if (index == 1)
              EXPECT_EQ(std::stod(ret_val), expected.values.double_value);
            else if (index == 2)
              EXPECT_EQ(ret_val, expected.values.string_value);
            else if (index == 3) {
              if (expected.values.int_array) {
                EXPECT_TRUE(field["v"].is_array());

                std::vector<int> actual_array;
                for (auto const& array_elem : field["v"]) {
                  actual_array.push_back(
                      std::stoi(array_elem.get<std::string>()));
                }

                EXPECT_EQ(actual_array.size(),
                          expected.values.int_array->size());
                for (size_t i = 0; i < actual_array.size(); ++i) {
                  EXPECT_EQ(actual_array[i], (*expected.values.int_array)[i]);
                }
              } else {
                EXPECT_TRUE(field["v"].is_null());
              }
            }
            index++;
          }
        }
      }
    } catch (std::exception const& e) {
      std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }
    row_count++;
  }
  EXPECT_EQ(row_count, kConversionFromStructTestData.size());
}

TEST(DataTranslationTest, From_SQL_Struct_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_STRUCT";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  table.CreateWithPrepare(
      conn,
      "(index INTEGER, StructField STRUCT<int_value BIGINT, double_value "
      "FLOAT64, string_value STRING, array_value ARRAY<INT64>>)");

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<StructBasicTestStruct> struct_data;
  for (auto const& test_case : kConversionFromStructTestData) {
    struct_data.emplace_back(test_case.values);
  }
  table.InsertStructData(conn, struct_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT StructField FROM " + table_name + " ORDER BY index";
  TestTranslationsFromStruct(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

struct BytesBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  std::vector<SQLCHAR> value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<BytesBasicTestStruct> const kConversionFromBytesTestData{
    {SQL_C_BINARY, {0x01, 0x02}, SQL_SUCCESS},
    {SQL_C_BINARY, {0xDE, 0xAD, 0xBE, 0xEF}, SQL_SUCCESS},
    {SQL_C_CHAR, {'a', 'b', '\0'}, SQL_SUCCESS},
    {SQL_C_WCHAR, {'\0', 'a', '\0', 'b', '\0', '\0'}, SQL_SUCCESS},
    {SQL_C_LONG, {1}, SQL_ERROR},
    {SQL_C_DOUBLE, {1}, SQL_ERROR},
};

void TestTranslationsFromBytes(std::shared_ptr<ODBCHandles> conn,
                               std::string query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;

  int row_count = 0;

  status = ExecWithPrepare(conn, query);
  CheckError(status, "Execute with Prepare", conn);

  for (auto const& expected : kConversionFromBytesTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      ++row_count;
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      ++row_count;
      continue;
    }
    EXPECT_EQ(SQL_SUCCESS, expected.status);

    switch (expected.target_c_type) {
      case SQL_C_BINARY: {
        std::vector<SQLCHAR> returned_val(data, data + strlen_or_ind);
        std::vector<SQLCHAR> expected_val(expected.value.begin(),
                                          expected.value.end());
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_CHAR: {
        std::string returned_val(reinterpret_cast<char*>(data), strlen_or_ind);
        returned_val = ConvertHexToChar(returned_val);
        std::string expected_val(expected.value.begin(), expected.value.end());
        EXPECT_EQ(returned_val, expected_val);
        break;
      }

      case SQL_C_WCHAR: {
        std::string returned_val_utf8 =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data),
                                    strlen_or_ind / sizeof(SQLWCHAR));
        std::wstring returned_val = ConvertHexToWchar(returned_val_utf8);
        returned_val.erase(returned_val.find_last_not_of(L'\0') + 1);
        std::wstring expected_val(expected.value.begin(), expected.value.end());
        expected_val.erase(expected_val.find_last_not_of(L'\0') + 1);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      default:
        break;
    }
    ++row_count;
  }
  EXPECT_EQ(row_count, kConversionFromBytesTestData.size());
}

TEST(DataTranslationTest, From_SQL_Bytes_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_BYTES";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, BytesField BYTES)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::vector<SQLCHAR>> bytes_data;
  for (auto const& test_case : kConversionFromBytesTestData) {
    bytes_data.push_back(test_case.value);
  }
  table.InsertBytesData(conn, bytes_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT BytesField FROM " + table_name + " Order by index";
  TestTranslationsFromBytes(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

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

  // Execute a read query and check whether the results returned are as
  // expected
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
    {SQL_C_BINARY, {{"age", 20}, {"name", "Anaya"}}, SQL_SUCCESS},
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
      case SQL_C_BINARY: {
        std::string expected_str = expected.value.dump();
        ASSERT_EQ(strlen_or_ind, static_cast<SQLLEN>(expected_str.size()));
        std::string returned_str(reinterpret_cast<char*>(data), strlen_or_ind);
        EXPECT_EQ(returned_str, expected_str);
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
    {SQL_C_WCHAR, {2024, 02, 20, 10, 20, 30, 123112}, SQL_SUCCESS},
    {SQL_C_BINARY, {2024, 03, 20, 00, 00, 00, 000000}, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {2024, 04, 20, 10, 20, 30, 123112}, SQL_SUCCESS},
    {SQL_C_TYPE_TIME, {2024, 05, 20, 10, 2, 30, 123112}, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2024, 06, 20, 11, 2, 30, 12311}, SQL_SUCCESS},
    {SQL_C_SLONG, {2024, 01, 20, 10, 20, 30, 123112}, SQL_ERROR},
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

  // Execute a read query and check whether the results returned are as
  // expected
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

std::vector<std::string> GetInputValuesToString(std::string column_name,
                                                StdAllTypesRows input_data) {
  std::vector<std::string> input_values;

  if (!column_name.compare("StringField")) {
    for (auto data : input_data) {
      input_values.emplace_back(data.str_field);
    }

  } else if (!column_name.compare("IntegerField")) {
    for (auto data : input_data) {
      if (data.int_field)
        input_values.emplace_back(std::to_string(data.int_field));
      else
        input_values.emplace_back("");
    }

  } else if (!column_name.compare("FloatField")) {
    for (auto data : input_data) {
      if (data.float_field)
        input_values.emplace_back(std::to_string(data.float_field));
      else
        input_values.emplace_back("");
    }

  } else if (!column_name.compare("TimestampField")) {
    for (auto data : input_data) {
      if (data.timestamp.year != 0) {
        std::string expected_val = FormatTimeStamp(data.timestamp);
        input_values.emplace_back(expected_val);
      } else
        input_values.emplace_back("");
    }

  } else if (!column_name.compare("DateField")) {
    for (auto data : input_data) {
      if (data.date.year != 0) {
        std::string expected_val = FormatDate(data.date);
        input_values.emplace_back(expected_val);
      } else
        input_values.emplace_back("");
    }

  } else if (!column_name.compare("TimeField")) {
    for (auto data : input_data) {
      std::string expected_val = FormatTimetoString(data.time);
      expected_val.append(".000000");
      input_values.emplace_back(expected_val);
    }
  } else if (!column_name.compare("JsonField")) {
    for (auto data : input_data) {
      std::string expected_val = to_string(data.json_field);
      input_values.emplace_back(expected_val);
    }
  }
  return input_values;
}

void VerifyColumnWiseResultsForDifferentTypes(StdAllTypesRows input_data,
                                              Results col_wise_data) {
  std::vector<std::string> all_col_names;
  for (auto it = col_wise_data.begin(); it != col_wise_data.end(); it++) {
    all_col_names.emplace_back(it->first);
  }

  for (auto col_name : all_col_names) {
    auto ret_col_values = col_wise_data[col_name];
    // We have to sort inserted and returned values because we haven't
    // specified the ordering
    sort(ret_col_values.begin(), ret_col_values.end(), str_comparison);

    std::vector<std::string> input_col_values =
        GetInputValuesToString(col_name, input_data);

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

TEST(DataTranslationTest, SQLGetData_AllTypes) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_SQL_GET_DATA_TEST_All";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(
      conn,
      "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64, "
      "TimestampField TIMESTAMP, DateField DATE, TimeField TIME, JsonField "
      "JSON)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.InsertAllData(conn, kConversionFromDifferentTestData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query = "SELECT * FROM " + table_name;

  auto results = *FetchResultsWithSqlGetData(conn, query);

  VerifyColumnWiseResultsForDifferentTypes(kConversionFromDifferentTestData,
                                           results);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void TestPartialDataFromSQLGetData(std::shared_ptr<ODBCHandles> conn,
                                   std::string query,
                                   std::vector<std::string> input_values) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  SQLSMALLINT buffer_len = 3;
  std::vector<std::string> ret_values;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  Results results;
  // Read all the rows using SQLFetch
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }

    SQLSMALLINT resp_status, resp_status_len;
    std::string returned_value;
    while (1) {
      status = SQLGetData(conn->hstmt, 1, SQL_CHAR, data, buffer_len,
                          &strlen_or_ind);
      CheckError(status, "SQLGetData", conn);
      if (status == SQL_SUCCESS_WITH_INFO) {
        returned_value.append((char*)data);
      }
      if (SQL_SUCCEEDED(status)) {
        status =
            SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, SQL_DIAG_SQLSTATE,
                            &resp_status, 0, &resp_status_len);
        if (status == SQL_NO_DATA) {
          returned_value.append((char*)data);
          ret_values.emplace_back(returned_value);
          break;
        }
        CheckError(status, "SQLGetDiagField", conn);
      } else {
        break;
      }
    }
  }

  for (int i = 0; i < ret_values.size(); i++) {
    EXPECT_EQ(ret_values[i], input_values[i]) << " at index: " << i;
  }
}

TEST(DataTranslationTest, SQLGetData_PartialData) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_GET_PARTIAL_DATA_TEST";
  Table table(table_name);

  // Create Table
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INT64, StringField STRING)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::string> string_data_to_insert;
  for (int i = 0; i < 5; i++) {
    string_data_to_insert.emplace_back(GetRandomString(10));
  }
  table.InsertStrData(conn, string_data_to_insert, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Execute a read query and check whether the results returned are as
  // expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT StringField FROM " + table_name + " ORDER BY index";

  TestPartialDataFromSQLGetData(conn, query, string_data_to_insert);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
// TODO(Kanchan): Add testcase for SQL_ARD_TYPE and SQL_APD_TYPE in SQLGetData
// PR Part 2.

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

struct GeographyBasicStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The geography parser to apply (e.g., ST_GEOGFROMTEXT, ST_GEOGFROMWKB)
  std::string geo_parser;
  // The value that should be returned by SQLGetData if it succeeds
  std::string value;
  // The value that should be expected.
  std::optional<std::string> expected_value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<GeographyBasicStruct> const kConversionFromGeographyTestData{
    {SQL_C_CHAR, "ST_GEOGFROMTEXT", "POINT(120.987 14.599)", std::nullopt,
     SQL_SUCCESS},
    {SQL_C_CHAR, "ST_GEOGFROMTEXT", "LINESTRING(121.1 14.5, 122.1 15.5)",
     std::nullopt, SQL_SUCCESS},
    {SQL_C_BINARY, "ST_GEOGFROMWKB",
     "01010000008B1B85EB51B81E40CDCCCCCCCCCC2840",
     "POINT(7.67999999999928 12.4)", SQL_SUCCESS},
    {SQL_C_WCHAR, "ST_GEOGFROMTEXT",
     "POLYGON((120 14, 121 14, 121 15, 120 15, 120 14))", std::nullopt,
     SQL_SUCCESS},
    {SQL_C_CHAR, "ST_GEOGPOINTFROMGEOHASH", "wwgqkpz2x",
     "POINT(117.256371974945 39.110062122345)", SQL_SUCCESS},
    {SQL_C_WCHAR, "ST_GEOGFROMGEOJSON",
     "{ \"type\": \"LineString\", \"coordinates\": [ [120.97, 14.529], [121.1, "
     "14.6], [121.3, 14.7], [121.5, 14.8] ] } ",
     "LINESTRING(120.97 14.529, 121.1 14.6, 121.3 14.7, 121.5 14.8)",
     SQL_SUCCESS},
    {SQL_C_DOUBLE, "ST_GEOGFROMTEXT", "LINESTRING(11.1 1.5, 120.1 12.5)",
     std::nullopt, SQL_ERROR},
    {SQL_C_SSHORT, "ST_GEOGFROMTEXT", "POINT(120.987 14.599)", std::nullopt,
     SQL_ERROR},
};

void TestTranslationFromGeography(
    std::shared_ptr<ODBCHandles> conn, std::string const& table_name,
    std::vector<GeographyBasicStruct> const& test_data) {
  SQLRETURN status;
  SQLPOINTER data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];

  std::string query = "SELECT GeoField FROM " + table_name + " ORDER BY Index";
  status = ExecWithPrepare(conn, query);
  CheckError(status, "ExecWithPrepare", conn);

  for (auto const& expected : test_data) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        auto expected_val = expected.expected_value.value_or(expected.value);
        EXPECT_EQ(returned_val.data(), expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        auto returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), SQL_NTS);
        auto expected_val = expected.expected_value.value_or(expected.value);
        EXPECT_EQ(returned_val.data(), expected_val);
        break;
      }
      case SQL_C_BINARY: {
        std::string returned_val(reinterpret_cast<char*>(data), strlen_or_ind);
        EXPECT_EQ(returned_val, expected.expected_value);
        break;
      }
      case SQL_C_DOUBLE: {
        auto returned_val = reinterpret_cast<double*>(data);
        EXPECT_EQ(status, expected.status);
        break;
      }
      case SQL_C_SSHORT: {
        SQLSMALLINT* returned_val = reinterpret_cast<SQLSMALLINT*>(data);
        EXPECT_EQ(status, expected.status);
        break;
      }
      default:
        break;
    }
  }
}

TEST(DataTranslationTest, From_Geography_To_All) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_GEOGRAPHY";
  Table table(table_name);
  auto conn = std::make_shared<ODBCHandles>();

  // Create Table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(Index INTEGER, GeoField GEOGRAPHY)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::pair<std::string, std::string>> geo_data_to_insert;
  for (auto elem : kConversionFromGeographyTestData) {
    geo_data_to_insert.push_back({elem.geo_parser, elem.value});
  }
  table.InsertGeographyData(conn, geo_data_to_insert, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Read Data
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  TestTranslationFromGeography(conn, table_name,
                               kConversionFromGeographyTestData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

std::vector<RangeTimeStampStruct> const kConversionFromRangeTimeStampTestData{
    {SQL_C_CHAR,
     {{2024, 2, 20, 12, 30, 45, 0}, {2024, 3, 20, 14, 15, 30, 425}},
     SQL_SUCCESS},
    {SQL_C_TYPE_DATE,
     {{2024, 4, 20, 10, 0, 0, 0}, {2024, 5, 20, 11, 45, 0, 250}},
     SQL_ERROR},
    {SQL_C_TYPE_TIMESTAMP,
     {{2024, 6, 20, 8, 20, 15, 750}, {2024, 7, 20, 9, 10, 5, 125}},
     SQL_ERROR},
    {SQL_C_WCHAR,
     {{2024, 8, 20, 16, 55, 30, 0}, {2024, 9, 20, 18, 40, 20, 375}},
     SQL_SUCCESS},
    {SQL_C_BINARY,
     {{2024, 10, 20, 20, 10, 5, 612}, {2024, 11, 20, 21, 30, 45, 0}},
     SQL_SUCCESS},
    {SQL_C_USHORT,
     {{2024, 12, 20, 22, 25, 35, 900}, {2025, 1, 20, 23, 50, 55, 100}},
     SQL_ERROR},
    {SQL_C_DOUBLE,
     {{2025, 2, 20, 13, 15, 10, 200}, {2025, 3, 20, 15, 5, 40, 300}},
     SQL_ERROR},
};

void TestTranslationsFromRangeTimestamp(std::shared_ptr<ODBCHandles> conn,
                                        std::string query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];

  int row_count = 0;

  status = ExecWithPrepare(conn, query);
  CheckError(status, "ExecWithPrepare", conn);

  for (auto const& expected : kConversionFromRangeTimeStampTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }

    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      row_count++;
      continue;
    }

    std::string expected_val =
        "[" + FormatRangeTimeStamp(expected.value.first) + ", " +
        FormatRangeTimeStamp(expected.value.second) + ")";
    std::string returned_val;
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        returned_val = reinterpret_cast<char const*>(data);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        returned_val.erase(returned_val.find_last_not_of('\0') + 1);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_BINARY: {
        returned_val = reinterpret_cast<char const*>(data);
// Existing Driver returns timestamp range in case of binary conversion in the
// format "[value, value) " on windows
#ifdef _WIN32
        expected_val.append(" ");
#else
        // whereas on linux it returns "[value, value):"
        expected_val.append(":");
#endif  //_WIN32
        EXPECT_EQ(returned_val, expected_val);
      }
      default:
        break;
    }
    EXPECT_EQ(returned_val, expected_val);
    ++row_count;
  }
  EXPECT_EQ(row_count, kConversionFromRangeTimeStampTestData.size());
}

TEST(DataTranslationTest, From_SQL_RangeTimeStamp_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_RANGE_TIMESTAMP";
  Table table(table_name);

  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, RangeField RANGE<TIMESTAMP>)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::pair<SQL_TIMESTAMP_STRUCT, SQL_TIMESTAMP_STRUCT>> range_data;
  for (auto const& test_case : kConversionFromRangeTimeStampTestData) {
    range_data.push_back(test_case.value);
  }
  table.InsertRangeTimeStampData(conn, range_data, true, "TIMESTAMP");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT RangeField FROM " + table_name + " Order by index";
  TestTranslationsFromRangeTimestamp(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DataTranslationTest, From_SQL_RangeDatetime_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_RANGE_DATETIME";
  Table table(table_name);

  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, RangeField RANGE<DATETIME>)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::pair<SQL_TIMESTAMP_STRUCT, SQL_TIMESTAMP_STRUCT>> range_data;
  for (auto const& test_case : kConversionFromRangeTimeStampTestData) {
    range_data.push_back(test_case.value);
  }
  table.InsertRangeTimeStampData(conn, range_data, true, "DATETIME");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT RangeField FROM " + table_name + " Order by index";
  TestTranslationsFromRangeTimestamp(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

std::vector<RangeDateStruct> const kConversionFromRangeDateTestData{
    {SQL_C_CHAR, {{2024, 2, 20}, {2024, 3, 20}}, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {{2024, 4, 20}, {2024, 5, 20}}, SQL_ERROR},
    {SQL_C_TYPE_TIMESTAMP, {{2024, 6, 20}, {2024, 7, 20}}, SQL_ERROR},
    {SQL_C_WCHAR, {{2024, 8, 20}, {2024, 9, 20}}, SQL_SUCCESS},
    {SQL_C_BINARY, {{2024, 10, 20}, {2024, 11, 20}}, SQL_SUCCESS},
    {SQL_C_USHORT, {{2024, 12, 20}, {2025, 1, 20}}, SQL_ERROR},
    {SQL_C_DOUBLE, {{2025, 2, 20}, {2025, 3, 20}}, SQL_ERROR},
};

void TestTranslationsFromRangeDate(std::shared_ptr<ODBCHandles> conn,
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

  for (auto const& expected : kConversionFromRangeDateTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      EXPECT_EQ(SQL_ERROR, expected.status);
      ++row_count;
      continue;
    }
    EXPECT_EQ(SQL_SUCCESS, expected.status);
    std::string expected_val = "[" + FormatDate(expected.value.first) + ", " +
                               FormatDate(expected.value.second) + ")";
    std::string returned_val;
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        returned_val = reinterpret_cast<char*>(data);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_WCHAR: {
        std::string returned_val_utf8 =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), 24);
        EXPECT_STREQ(returned_val_utf8.data(), expected_val.data());
        break;
      }
      case SQL_C_BINARY: {
        if (strlen_or_ind == sizeof(SQL_DATE_STRUCT) * 2) {
          auto* range =
              reinterpret_cast<std::pair<SQL_DATE_STRUCT, SQL_DATE_STRUCT>*>(
                  data);

          returned_val = "[" + FormatDate(range->first) + ", " +
                         FormatDate(range->second) + ")";

          EXPECT_EQ(returned_val, expected_val);
        }
        break;
      }
      default:
        break;
    }
    ++row_count;
  }
  EXPECT_EQ(row_count, kConversionFromRangeDateTestData.size());
}

TEST(DataTranslationTest, From_SQL_RangeDate_to_all) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_RANGE_DATE";
  Table table(table_name);

  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, RangeField RANGE<DATE>)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::vector<std::pair<SQL_DATE_STRUCT, SQL_DATE_STRUCT>> range_data;
  for (auto const& test_case : kConversionFromRangeDateTestData) {
    range_data.push_back(test_case.value);
  }
  table.InsertRangeDateData(conn, range_data, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  std::string query =
      "SELECT RangeField FROM " + table_name + " Order by index";
  TestTranslationsFromRangeDate(conn, query);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
