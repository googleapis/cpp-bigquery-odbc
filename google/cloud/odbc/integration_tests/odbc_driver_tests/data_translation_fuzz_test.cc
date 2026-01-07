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
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <iostream>

namespace google::cloud::odbc_tests {
using ::testing::HasSubstr;

using fuzztest::Arbitrary;
using fuzztest::Domain;
using fuzztest::ElementOf;
using fuzztest::InRange;
using fuzztest::OneOf;
using fuzztest::StructOf;

void RunArraySQLStatement(std::shared_ptr<ODBCHandles> conn,
                          std::string const& query) {
  SQLRETURN status;
  char read_stmt[kBufferLength] = {0};
  SQLPOINTER data_int[kBufferLength];
  SQLLEN strlen_or_ind = 0;
  int count = 5;

  StrToChar(read_stmt, query.c_str());

  status =
      SQLPrepare(conn->hstmt, reinterpret_cast<SQLCHAR*>(read_stmt), SQL_NTS);
  if (!SQL_SUCCEEDED(status)) return;

  status = SQLExecute(conn->hstmt);
  if (!SQL_SUCCEEDED(status)) return;

  status = SQLBindCol(conn->hstmt, 1, SQL_C_CHAR, data_int, kBufferLength,
                      &strlen_or_ind);
  if (!SQL_SUCCEEDED(status)) return;

  status = SQLFetch(conn->hstmt);
  if (!SQL_SUCCEEDED(status)) return;

  if (strlen_or_ind <= 0 || strlen_or_ind > kBufferLength) {
  return;  // nothing valid to parse
}

std::string str_int(reinterpret_cast<char*>(data_int),
                    static_cast<size_t>(strlen_or_ind));

if (!nlohmann::json::accept(str_int)) {
  return;  // invalid UTF-8 or not JSON → expected in fuzzing
}

  std::vector<std::string> ret_int_values;
  try {
    auto json_object_int = nlohmann::json::parse(str_int);
    auto normalized_array =
        json_object_int.is_array() ? json_object_int : json_object_int["v"];

    for (auto const& element : normalized_array) {
      ret_int_values.emplace_back(element.contains("v")
                                      ? element["v"].get<std::string>()
                                      : element.get<std::string>());
    }
  } catch (const nlohmann::json::exception&) {
    return;  // expected under fuzzing
  }

  // Invariants you want to enforce
  EXPECT_LE(ret_int_values.size(), count);

  for (size_t i = 0; i < ret_int_values.size(); ++i) {
    EXPECT_NO_THROW(std::stoi(ret_int_values[i]));
  }
}

void TestArraySQLStatementFuzz(std::string const& query) {
  auto conn = std::make_shared<ODBCHandles>();

  if (Connect(kDefaultConnectionString, conn) != SQL_SUCCESS) {
    return;
  }

  RunArraySQLStatement(conn, query);

  Disconnect(conn);
}

FUZZ_TEST(DataTranslationFuzz, TestArraySQLStatementFuzz)
    .WithSeeds({
        "SELECT [1, 2, 3, 4, 5] AS numbers",
    });

// Generic fuzz input
template <typename SQLStruct>
struct TranslationFuzzInput {
  SQLSMALLINT target_c_type;
  SQLStruct value;
};

using DateInput = TranslationFuzzInput<SQL_DATE_STRUCT>;
using TimestampInput = TranslationFuzzInput<SQL_TIMESTAMP_STRUCT>;
using TimeInput = TranslationFuzzInput<SQL_TIME_STRUCT>;
using BooleanInput = TranslationFuzzInput<std::string>;
using StringInput = TranslationFuzzInput<std::string>;

// -------------------
// Fuzz domains
// -------------------

Domain<DateInput> DateFuzzDomain() {
  return StructOf<DateInput>(
      ElementOf<SQLSMALLINT>({SQL_C_CHAR, SQL_C_WCHAR, SQL_C_BINARY,
                              SQL_C_TYPE_DATE, SQL_C_TYPE_TIMESTAMP,
                              SQL_C_USHORT, SQL_C_DOUBLE}),
      StructOf<SQL_DATE_STRUCT>(
          InRange<SQLSMALLINT>(2000, 2100),  // year
          InRange<SQLUSMALLINT>(1, 12),      // month
          InRange<SQLUSMALLINT>(1, 28)       // day (avoid invalid)
          ));
}

Domain<TimestampInput> TimestampFuzzDomain() {
  return StructOf<TimestampInput>(
      ElementOf<SQLSMALLINT>({
          SQL_C_CHAR, SQL_C_WCHAR, SQL_C_BINARY, SQL_C_TYPE_DATE,
          SQL_C_TYPE_TIME, SQL_C_TYPE_TIMESTAMP,
          SQL_C_SLONG  // expected failure
      }),
      StructOf<SQL_TIMESTAMP_STRUCT>(
          InRange<SQLSMALLINT>(1900, 2100),  // year
          InRange<SQLUSMALLINT>(1, 12),      // month
          InRange<SQLUSMALLINT>(1, 28),      // day
          InRange<SQLUSMALLINT>(0, 23),      // hour
          InRange<SQLUSMALLINT>(0, 59),      // minute
          InRange<SQLUSMALLINT>(0, 59),      // second
          InRange<SQLUINTEGER>(0, 999999)    // fraction
          ));
}

Domain<TimeInput> TimeFuzzDomain() {
  return StructOf<TimeInput>(
      ElementOf<SQLSMALLINT>({SQL_C_CHAR, SQL_C_WCHAR, SQL_C_BINARY,
                              SQL_C_TYPE_TIME, SQL_C_TYPE_TIMESTAMP}),
      StructOf<SQL_TIME_STRUCT>(InRange<SQLUSMALLINT>(0, 23),  // hour
                                InRange<SQLUSMALLINT>(0, 59),  // minute
                                InRange<SQLUSMALLINT>(0, 59)   // second
                                ));
}

Domain<BooleanInput> BooleanFuzzDomain() {
  return StructOf<BooleanInput>(
      ElementOf<SQLSMALLINT>({
          SQL_C_CHAR,
          SQL_C_BIT,
          SQL_C_BINARY,
          SQL_C_WCHAR,
          SQL_C_DOUBLE,
          SQL_C_LONG,
          SQL_C_STINYINT,
          SQL_C_UTINYINT,
          SQL_C_TINYINT,
          SQL_C_SBIGINT,
          SQL_C_UBIGINT,
          SQL_C_SSHORT,
          SQL_C_USHORT,
          SQL_C_SHORT,
          SQL_C_SLONG,
          SQL_C_ULONG,
          SQL_C_FLOAT,
          SQL_C_NUMERIC,
          SQL_C_TYPE_DATE,  // expected failure
      }),
      ElementOf<std::string>({
          "0",
          "1",
          "true",
          "false",
      })
  );
}

Domain<StringInput> StringFuzzDomain() {
  return StructOf<StringInput>(
      ElementOf<SQLSMALLINT>({
          SQL_C_CHAR,
          SQL_C_FLOAT,
          SQL_C_DOUBLE,
          SQL_C_SSHORT,
          SQL_C_USHORT,
          SQL_C_SLONG,
          SQL_C_ULONG,
          SQL_C_BIT,
          SQL_C_SBIGINT,
          SQL_C_UBIGINT,
          SQL_C_STINYINT,
          SQL_C_UTINYINT,
          SQL_C_BINARY,
          SQL_C_TYPE_DATE,
          SQL_C_TYPE_TIME,
          SQL_C_TYPE_TIMESTAMP,
          SQL_C_INTERVAL_DAY,
          SQL_C_INTERVAL_DAY_TO_HOUR,
          SQL_C_INTERVAL_DAY_TO_MINUTE,
          SQL_C_INTERVAL_DAY_TO_SECOND,
          SQL_C_INTERVAL_HOUR,
          SQL_C_INTERVAL_HOUR_TO_MINUTE,
          SQL_C_INTERVAL_HOUR_TO_SECOND,
          SQL_C_INTERVAL_MINUTE,
          SQL_C_INTERVAL_MINUTE_TO_SECOND,
          SQL_C_INTERVAL_SECOND,
          SQL_C_INTERVAL_MONTH,
          SQL_C_INTERVAL_YEAR,
          SQL_C_INTERVAL_YEAR_TO_MONTH,
          SQL_C_TINYINT,
          SQL_C_NUMERIC,
          SQL_C_LONG,
          SQL_C_SHORT,
      }),
      ElementOf<std::string>({
          "Test String 1",
          "19.1",
          "2a",
          "-38.3",
          "a3",
          "31",
          "9223372036854775807",
          "89",
          "-9",
          "65537",
          "-934934934",
          "1.1",
          "b1",
          "934934934",
          "0",
          "1",
          "2",
          "18446744073709551615",
          "127",
          "-128",
          "255",
          "2024-01-01",
          "1999-12-31",
          "14:30:15",
          "00:00:00",
          "2024-01-01 12:34:56",
          "2000-01-01 00:00:00",
          "20",
          "5 12",
          "3 10:25",
          "2 04:12:35",
          "12",
          "08:45",
          "06:30:15",
          "55",
          "45:20",
          "30",
          "7",
          "2",
          "3-5",
          "-123456789",
          "123456789",
          "-2147483647",
          "32767",
      })
  );
}



// -------------------
// Table schema helper
// -------------------
enum class DateTimeColumnKind {
  Timestamp,
  Datetime,
  Date,
  Time,
  Boolean,
  String
};

template <typename SQLStruct>
std::string insert_table_schema(DateTimeColumnKind kind) {
  switch (kind) {
    case DateTimeColumnKind::Timestamp:
      return "(idx INTEGER, col TIMESTAMP)";
    case DateTimeColumnKind::Datetime:
      return "(idx INTEGER, col DATETIME)";
    case DateTimeColumnKind::Time:
      return "(idx INTEGER, col TIME)";
    case DateTimeColumnKind::Date:
      return "(idx INTEGER, col DATE)";
      case DateTimeColumnKind::Boolean:
      return "(idx INTEGER, col Boolean)";
      case DateTimeColumnKind::String:
      return "(idx INTEGER, col STRING)";
  }
  return {};
}


// -------------------
// RunSelectTranslation
// -------------------

template <typename Checker>
void RunSelectTranslation(std::string const& connection_string,
                          std::string const& query, SQLSMALLINT target_c_type,
                          Checker&& checker) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS);

  SQLPOINTER data[kBufferLength];
  SQLLEN strlen_or_ind = 0;
  char stmt[kBufferLength];

  StrToChar(stmt, query.c_str());

  ASSERT_TRUE(SQL_SUCCEEDED(
      SQLPrepare(conn->hstmt, reinterpret_cast<SQLCHAR*>(stmt), SQL_NTS)));
  ASSERT_TRUE(SQL_SUCCEEDED(SQLExecute(conn->hstmt)));

  SQLRETURN status = SQLBindCol(conn->hstmt, 1, target_c_type, data,
                                kBufferLength, &strlen_or_ind);
  if (!SQL_SUCCEEDED(status)) {
    Disconnect(conn);  // expected bind failure
    return;
  }

  status = SQLFetch(conn->hstmt);
  if (!SQL_SUCCEEDED(status)) {
    Disconnect(conn);
    return;
  }

  // Pass const buffer
  checker(target_c_type, data, strlen_or_ind);

  Disconnect(conn);
}

// -------------------
// Generic fuzz translation
// -------------------

template <typename SQLStruct>
void FuzzTranslation(
    TranslationFuzzInput<SQLStruct> const& input,
    std::string const& table_name,
    DateTimeColumnKind kind,
    std::function<void(Table&, std::shared_ptr<ODBCHandles> const&,
                       std::vector<SQLStruct> const&, bool)>
        InsertFunc,
    std::function<void(SQLSMALLINT, SQLStruct const&, SQLPOINTER, SQLLEN)>
        CheckFunc)
{
  Table table(table_name);
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str = kDefaultConnectionString;

  // Create table
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, insert_table_schema<SQLStruct>(kind));
  ASSERT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert value
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);
  InsertFunc(table, conn, {input.value}, true);
  ASSERT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Query + check
  RunSelectTranslation(conn_str,
                       "SELECT col FROM " + table_name + " ORDER BY idx",
                       input.target_c_type,
                       [&](SQLSMALLINT c_type, SQLPOINTER data, SQLLEN len) {
                         CheckFunc(c_type, input.value, data, len);
                       });

  // Cleanup
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  ASSERT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// -------------------
// Check functions
// -------------------

void CheckDateTranslation(SQLSMALLINT target_c_type,
                          const SQL_DATE_STRUCT& input, SQLPOINTER data,
                          SQLLEN len) {
                            std::cout<<"Inside date check function"<<target_c_type<<std::endl;
  std::string expected = FormatDate(input);

  switch (target_c_type) {
    case SQL_C_CHAR: {
      std::cout<<"Inside char check"<<std::endl;
      EXPECT_EQ(reinterpret_cast<char const*>(data), expected);
      break;
    }
    case SQL_C_WCHAR: {
      std::cout<<"Inside wchar check"<<std::endl;
      SQLINTEGER length = len / sizeof(SQLWCHAR);
      EXPECT_EQ(
          ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length),
          expected);
      break;
    }
    case SQL_C_BINARY: {
      std::cout<<"Inside binary check"<<std::endl;
      auto* d = reinterpret_cast<const SQL_DATE_STRUCT*>(data);
      EXPECT_EQ(FormatDate(*d), expected);
      break;
    }
    case SQL_C_TYPE_DATE: {
      std::cout<<"Inside date struct check"<<std::endl;
      auto* d = reinterpret_cast<const SQL_DATE_STRUCT*>(data);
      EXPECT_EQ(d->year, input.year);
      EXPECT_EQ(d->month, input.month);
      EXPECT_EQ(d->day, input.day);
      break;
    }
    case SQL_C_TYPE_TIMESTAMP: {
      std::cout<<"Inside timestamp struct check"<<std::endl;
      auto* ts = reinterpret_cast<const SQL_TIMESTAMP_STRUCT*>(data);
      EXPECT_EQ(ts->year, input.year);
      EXPECT_EQ(ts->month, input.month);
      EXPECT_EQ(ts->day, input.day);
      break;
    }
    default:
      break;
  }
}

void CheckTimeTranslation(SQLSMALLINT target_c_type,
                          const SQL_TIME_STRUCT& input, SQLPOINTER data,
                          SQLLEN len) {
  std::string expected = FormatTimetoString(input);
  switch (target_c_type) {
    case SQL_C_CHAR: {
      if (!kIsBqDriver) {
        expected.append(".000000");
      }
      EXPECT_EQ(reinterpret_cast<char const*>(data), expected);
      break;
    }
    case SQL_C_WCHAR: {
      SQLINTEGER length = len / sizeof(SQLWCHAR);
      if (!kIsBqDriver) {
        expected.append(".000000");
      }
      EXPECT_EQ(
          ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length),
          expected);
      break;
    }
    case SQL_C_BINARY: {
      if (len == sizeof(SQL_TIME_STRUCT)) {
        SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
        std::string returned_val = FormatTimetoString(*time);
        EXPECT_EQ(returned_val, expected);
      }
      break;
    }
    case SQL_C_TYPE_TIME: {
      auto* t = reinterpret_cast<const SQL_TIME_STRUCT*>(data);
      EXPECT_EQ(t->hour, input.hour);
      EXPECT_EQ(t->minute, input.minute);
      EXPECT_EQ(t->second, input.second);
      break;
    }
    case SQL_C_TYPE_TIMESTAMP: {
      auto* ts = reinterpret_cast<const SQL_TIMESTAMP_STRUCT*>(data);
      EXPECT_EQ(ts->hour, input.hour);
      EXPECT_EQ(ts->minute, input.minute);
      EXPECT_EQ(ts->second, input.second);
      break;
    }
    default:
      break;
  }
}

void CheckTimestampTranslation(SQLSMALLINT target_c_type,
                               const SQL_TIMESTAMP_STRUCT& input,
                               SQLPOINTER data, SQLLEN len) {
  switch (target_c_type) {
    case SQL_C_CHAR: {
      EXPECT_EQ(reinterpret_cast<char const*>(data), FormatTimeStamp(input));
      break;
    }
    case SQL_C_WCHAR: {
      SQLINTEGER length = len / sizeof(SQLWCHAR);

        std::string returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);

        std::string expected_val = FormatTimeStamp(input);
        EXPECT_STREQ(returned_val.data(), expected_val.data());
        break;
      break;
    }
    case SQL_C_BINARY: {
      auto* ts = reinterpret_cast<const SQL_TIMESTAMP_STRUCT*>(data);
      EXPECT_EQ(ts->year, input.year);
      EXPECT_EQ(ts->month, input.month);
      EXPECT_EQ(ts->day, input.day);
      EXPECT_EQ(ts->hour, input.hour);
      EXPECT_EQ(ts->minute, input.minute);
      EXPECT_EQ(ts->second, input.second);
      break;
    }
    case SQL_C_TYPE_DATE: {
      auto* d = reinterpret_cast<const SQL_DATE_STRUCT*>(data);
      EXPECT_EQ(d->year, input.year);
      EXPECT_EQ(d->month, input.month);
      EXPECT_EQ(d->day, input.day);
      break;
    }
    case SQL_C_TYPE_TIME: {
      auto* t = reinterpret_cast<const SQL_TIME_STRUCT*>(data);
      EXPECT_EQ(t->hour, input.hour);
      EXPECT_EQ(t->minute, input.minute);
      EXPECT_EQ(t->second, input.second);
      break;
    }
    case SQL_C_TYPE_TIMESTAMP: {
      auto* ts = reinterpret_cast<const SQL_TIMESTAMP_STRUCT*>(data);
      EXPECT_EQ(ts->year, input.year);
      EXPECT_EQ(ts->month, input.month);
      EXPECT_EQ(ts->day, input.day);
      EXPECT_EQ(ts->hour, input.hour);
      EXPECT_EQ(ts->minute, input.minute);
      EXPECT_EQ(ts->second, input.second);
      break;
    }
    case SQL_C_SLONG: {
      FAIL() << "Expected failure but got success";
      break;
    }
    default:
      break;
  }
}

void CheckDatetimeTranslation(SQLSMALLINT target_c_type,
                               const SQL_TIMESTAMP_STRUCT& input,
                               SQLPOINTER data, SQLLEN len) {
                                std::string expected_val = FormatTimeStamp(input);
  switch (target_c_type) {
    
    case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        if (kIsBqDriver) {
          expected_val = FormatToGoogleDatetimeStr(expected_val);
        }
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
    case SQL_C_WCHAR: {
        SQLINTEGER length = len / sizeof(SQLWCHAR);
        std::string returned_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        if (kIsBqDriver) {
          expected_val = FormatToGoogleDatetimeStr(expected_val);
        }
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
      auto* d = reinterpret_cast<const SQL_DATE_STRUCT*>(data);
      EXPECT_EQ(d->year, input.year);
      EXPECT_EQ(d->month, input.month);
      EXPECT_EQ(d->day, input.day);
      break;
    }
    case SQL_C_TYPE_TIME: {
      auto* t = reinterpret_cast<const SQL_TIME_STRUCT*>(data);
      EXPECT_EQ(t->hour, input.hour);
      EXPECT_EQ(t->minute, input.minute);
      EXPECT_EQ(t->second, input.second);
      break;
    }
    case SQL_C_TYPE_TIMESTAMP: {
      auto* ts = reinterpret_cast<const SQL_TIMESTAMP_STRUCT*>(data);
      EXPECT_EQ(ts->year, input.year);
      EXPECT_EQ(ts->month, input.month);
      EXPECT_EQ(ts->day, input.day);
      EXPECT_EQ(ts->hour, input.hour);
      EXPECT_EQ(ts->minute, input.minute);
      EXPECT_EQ(ts->second, input.second);
      break;
    }
    case SQL_C_SLONG: {
      FAIL() << "Expected failure but got success";
      break;
    }
    default:
      break;
  }
}

void CheckBooleanTranslation(SQLSMALLINT target_c_type,
                               const std::string& input,
                               SQLPOINTER data, SQLLEN len) {
   switch (target_c_type) {
      case SQL_C_CHAR: {
        std::string returned_val = reinterpret_cast<char*>(data);
        EXPECT_EQ(returned_val, input);
        break;
      }
      case SQL_C_BIT: {
        SQLCHAR returned_val = *reinterpret_cast<SQLCHAR*>(data);
        EXPECT_EQ(returned_val,
                  static_cast<SQLCHAR>(std::stoi(input)));
        break;
      }
      case SQL_C_BINARY: {
        if (len == sizeof(SQLCHAR)) {
          SQLCHAR* binary_value = reinterpret_cast<SQLCHAR*>(data);
          EXPECT_EQ(*binary_value,
                    static_cast<SQLCHAR>(std::stoi(input)));
        }
        break;
      }
      case SQL_C_WCHAR: {
        std::wstring wstr = reinterpret_cast<wchar_t*>(data);
        std::wstring expected_wstr(input.begin(), input.end());
        EXPECT_STREQ(wstr.c_str(), expected_wstr.c_str());
        break;
      }
      case SQL_C_DOUBLE: {
        SQLDOUBLE returned_val = *reinterpret_cast<SQLDOUBLE*>(data);
        SQLDOUBLE expected_val = std::stod(input);
        EXPECT_DOUBLE_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_LONG:
      case SQL_C_SLONG: {
        SQLINTEGER returned_val = *reinterpret_cast<SQLINTEGER*>(data);
        SQLINTEGER expected_val = std::stoi(input);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_ULONG: {
        SQLUINTEGER returned_val = *reinterpret_cast<SQLUINTEGER*>(data);
        SQLUINTEGER expected_val =
            static_cast<SQLUINTEGER>(std::stoul(input));
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_STINYINT:
      case SQL_C_TINYINT: {
        SQLSCHAR returned_val = *reinterpret_cast<SQLSCHAR*>(data);
        SQLSCHAR expected_val =
            static_cast<SQLSCHAR>(std::stoi(input));
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_UTINYINT: {
        SQLCHAR returned_val = *reinterpret_cast<SQLCHAR*>(data);
        SQLCHAR expected_val = static_cast<SQLCHAR>(std::stoul(input));
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_SBIGINT: {
        SQLBIGINT returned_val = *reinterpret_cast<SQLBIGINT*>(data);
        SQLBIGINT expected_val = std::stoll(input);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_UBIGINT: {
        SQLUBIGINT returned_val = *reinterpret_cast<SQLUBIGINT*>(data);
        SQLUBIGINT expected_val = std::stoull(input);
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_SSHORT:
      case SQL_C_SHORT: {
        SQLSMALLINT returned_val = *reinterpret_cast<SQLSMALLINT*>(data);
        SQLSMALLINT expected_val =
            static_cast<SQLSMALLINT>(std::stoi(input));
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_USHORT: {
        SQLUSMALLINT returned_val = *reinterpret_cast<SQLUSMALLINT*>(data);
        SQLUSMALLINT expected_val =
            static_cast<SQLUSMALLINT>(std::stoul(input));
        EXPECT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_FLOAT: {
        SQLREAL returned_val = *reinterpret_cast<SQLREAL*>(data);
        SQLREAL expected_val = static_cast<SQLREAL>(std::stof(input));
        EXPECT_FLOAT_EQ(returned_val, expected_val);
        break;
      }
      case SQL_C_NUMERIC: {
        SQL_NUMERIC_STRUCT* returned_val =
            reinterpret_cast<SQL_NUMERIC_STRUCT*>(data);
        SQL_NUMERIC_STRUCT expected_val{};
        expected_val.precision = 1;
        expected_val.scale = 0;
        int numeric_value = std::stoi(input);
        expected_val.sign = numeric_value == 0 ? 0 : 1;
        expected_val.val[0] = static_cast<uint8_t>(numeric_value);

        EXPECT_EQ(returned_val->precision, expected_val.precision);
        EXPECT_EQ(returned_val->scale, expected_val.scale);
        EXPECT_EQ(returned_val->sign, expected_val.sign);
        EXPECT_EQ(returned_val->val[0], expected_val.val[0]);
        break;
      }
      default:
        break;
    }
   
}

void CheckStringTranslation(SQLSMALLINT target_c_type,
                               const std::string& input,
                               SQLPOINTER data, SQLLEN len) {
                                std::cout<<"Inside string check function"<<target_c_type<<std::endl;
   if (target_c_type == SQL_C_CHAR) {
              std::string returned_val = (char*)data;
              EXPECT_EQ(returned_val, input);
            } else if (target_c_type == SQL_C_FLOAT) {
              std::cout<<"inside float check"<<std::endl;
              SQLREAL* returned_val = (SQLREAL*)data;
              EXPECT_EQ(*returned_val, std::stof(input));
            } else if (target_c_type == SQL_C_DOUBLE) {
              SQLDOUBLE* returned_val = (SQLDOUBLE*)data;
              EXPECT_EQ(*returned_val, std::stod(input));
            } else if (target_c_type == SQL_C_SSHORT) {
              SQLSMALLINT* returned_val = (SQLSMALLINT*)data;
              EXPECT_EQ(*returned_val, std::stoi(input));
            } else if (target_c_type == SQL_C_USHORT) {
              SQLUSMALLINT* returned_val = (SQLUSMALLINT*)data;
              EXPECT_EQ(*returned_val, std::stoi(input));
            } else if (target_c_type == SQL_C_SLONG) {
              SQLINTEGER* returned_val = (SQLINTEGER*)data;
              EXPECT_EQ(*returned_val, std::stoi(input));
            } else if (target_c_type == SQL_C_ULONG) {
              SQLUINTEGER* returned_val = (SQLUINTEGER*)data;
              EXPECT_EQ(*returned_val, std::stoi(input));
            } else if (target_c_type == SQL_C_BIT) {
              SQLCHAR* returned_val = (SQLCHAR*)data;
              EXPECT_EQ(*returned_val, std::stod(input));
            } else if (target_c_type == SQL_C_BINARY) {
              std::vector<uint8_t> expected_binary(input.begin(), input.end());
              std::vector<uint8_t> returned_binary(
                  (uint8_t*)data, (uint8_t*)data + expected_binary.size());
              EXPECT_EQ(returned_binary, expected_binary);
            } else if (target_c_type == SQL_C_TYPE_DATE) {
              DATE_STRUCT* returned_val = (DATE_STRUCT*)data;
              std::tm expected_tm = {};
              std::istringstream ss(input);
              ss >> std::get_time(&expected_tm, "%Y-%m-%d");
              EXPECT_EQ(returned_val->year, expected_tm.tm_year + 1900);
              EXPECT_EQ(returned_val->month, expected_tm.tm_mon + 1);
              EXPECT_EQ(returned_val->day, expected_tm.tm_mday);
            } else if (target_c_type == SQL_C_TYPE_TIME) {
              TIME_STRUCT* returned_val = (TIME_STRUCT*)data;
              std::tm expected_tm = {};
              std::istringstream ss(input);
              ss >> std::get_time(&expected_tm, "%H:%M:%S");
              EXPECT_EQ(returned_val->hour, expected_tm.tm_hour);
              EXPECT_EQ(returned_val->minute, expected_tm.tm_min);
              EXPECT_EQ(returned_val->second, expected_tm.tm_sec);
            } else if (target_c_type == SQL_C_TYPE_TIMESTAMP) {
              TIMESTAMP_STRUCT* returned_val = (TIMESTAMP_STRUCT*)data;
              std::tm expected_tm = {};
              std::istringstream ss(input);
              ss >> std::get_time(&expected_tm, "%Y-%m-%d %H:%M:%S");
              EXPECT_EQ(returned_val->year, expected_tm.tm_year + 1900);
              EXPECT_EQ(returned_val->month, expected_tm.tm_mon + 1);
              EXPECT_EQ(returned_val->day, expected_tm.tm_mday);
              EXPECT_EQ(returned_val->hour, expected_tm.tm_hour);
              EXPECT_EQ(returned_val->minute, expected_tm.tm_min);
              EXPECT_EQ(returned_val->second, expected_tm.tm_sec);
            } else if (target_c_type == SQL_C_STINYINT) {
              int8_t* returned_val = (int8_t*)data;
              EXPECT_EQ(*returned_val,
                        static_cast<int8_t>(std::stoi(input)));
            } else if (target_c_type == SQL_C_UTINYINT) {
              uint8_t* returned_val = (uint8_t*)data;
              EXPECT_EQ(*returned_val,
                        static_cast<uint8_t>(std::stoul(input)));
            } else if (target_c_type == SQL_C_SBIGINT) {
              SQLBIGINT* returned_val = (SQLBIGINT*)data;
              EXPECT_EQ(*returned_val, std::stoll(input));
            } else if (target_c_type == SQL_C_UBIGINT) {
              SQLUBIGINT* returned_val = (SQLUBIGINT*)data;
              EXPECT_EQ(*returned_val, std::stoull(input));
            } else if (target_c_type == SQL_C_INTERVAL_YEAR ||
                       target_c_type == SQL_C_INTERVAL_MONTH ||
                       target_c_type == SQL_C_INTERVAL_YEAR_TO_MONTH) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int years = 0, months = 0;
              if (target_c_type == SQL_C_INTERVAL_YEAR_TO_MONTH) {
                sscanf(input.c_str(), "%d-%d", &years, &months);
              } else if (target_c_type == SQL_C_INTERVAL_YEAR) {
                years = std::stoi(input);
              } else if (target_c_type == SQL_C_INTERVAL_MONTH) {
                months = std::stoi(input);
              }
              EXPECT_EQ(returned_val->intval.year_month.year, years);
              EXPECT_EQ(returned_val->intval.year_month.month, months);
            } else if (target_c_type == SQL_C_INTERVAL_DAY) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int days = std::stoi(input);
              EXPECT_EQ(returned_val->intval.day_second.day, days);
            } else if (target_c_type == SQL_C_INTERVAL_HOUR) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int hours = std::stoi(input);
              EXPECT_EQ(returned_val->intval.day_second.hour, hours);
            } else if (target_c_type == SQL_C_INTERVAL_MINUTE) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int minutes = std::stoi(input);
              EXPECT_EQ(returned_val->intval.day_second.minute, minutes);
            } else if (target_c_type == SQL_C_INTERVAL_SECOND) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int seconds = std::stoi(input);
              EXPECT_EQ(returned_val->intval.day_second.second, seconds);
            }

            else if (target_c_type == SQL_C_INTERVAL_DAY_TO_HOUR) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int d, h;
              sscanf(input.c_str(), "%d %d", &d, &h);
              EXPECT_EQ(returned_val->intval.day_second.day, d);
              EXPECT_EQ(returned_val->intval.day_second.hour, h);

            } else if (target_c_type == SQL_C_INTERVAL_DAY_TO_MINUTE) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int d, h, m;
              sscanf(input.c_str(), "%d %d:%d", &d, &h, &m);
              EXPECT_EQ(returned_val->intval.day_second.day, d);
              EXPECT_EQ(returned_val->intval.day_second.hour, h);
              EXPECT_EQ(returned_val->intval.day_second.minute, m);

            } else if (target_c_type == SQL_C_INTERVAL_DAY_TO_SECOND) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int d, h, m, s;
              sscanf(input.c_str(), "%d %d:%d:%d", &d, &h, &m, &s);
              EXPECT_EQ(returned_val->intval.day_second.day, d);
              EXPECT_EQ(returned_val->intval.day_second.hour, h);
              EXPECT_EQ(returned_val->intval.day_second.minute, m);
              EXPECT_EQ(returned_val->intval.day_second.second, s);

            } else if (target_c_type ==
                       SQL_C_INTERVAL_HOUR_TO_MINUTE) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int h, m;
              sscanf(input.c_str(), "%d:%d", &h, &m);
              EXPECT_EQ(returned_val->intval.day_second.hour, h);
              EXPECT_EQ(returned_val->intval.day_second.minute, m);

            } else if (target_c_type ==
                       SQL_C_INTERVAL_HOUR_TO_SECOND) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int h, m, s;
              sscanf(input.c_str(), "%d:%d:%d", &h, &m, &s);
              EXPECT_EQ(returned_val->intval.day_second.hour, h);
              EXPECT_EQ(returned_val->intval.day_second.minute, m);
              EXPECT_EQ(returned_val->intval.day_second.second, s);

            } else if (target_c_type ==
                       SQL_C_INTERVAL_MINUTE_TO_SECOND) {
              SQL_INTERVAL_STRUCT* returned_val = (SQL_INTERVAL_STRUCT*)data;
              int m, s;
              sscanf(input.c_str(), "%d:%d", &m, &s);
              EXPECT_EQ(returned_val->intval.day_second.minute, m);
              EXPECT_EQ(returned_val->intval.day_second.second, s);
            } else if (target_c_type == SQL_C_NUMERIC) {
              SQL_NUMERIC_STRUCT returned_val = *(SQL_NUMERIC_STRUCT*)data;
              EXPECT_EQ(SQLNumericToString(returned_val), input);
            } else if (target_c_type == SQL_C_LONG) {
              SQLINTEGER returned_val = *reinterpret_cast<SQLINTEGER*>(data);
              EXPECT_EQ(std::to_string(returned_val), input);
            } else if (target_c_type == SQL_C_SHORT) {
              SQLSMALLINT returned_val = *reinterpret_cast<SQLSMALLINT*>(data);
              EXPECT_EQ(std::to_string(returned_val), input);
            } else if (target_c_type == SQL_C_TINYINT) {
              SQLCHAR returned_val = *reinterpret_cast<SQLCHAR*>(data);
              EXPECT_EQ(std::to_string(returned_val), input);
            }
}

// -------------------
// Fuzz functions
// -------------------

void FuzzDate(DateInput const& input) {
  FuzzTranslation<SQL_DATE_STRUCT>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATE",
      DateTimeColumnKind::Date,
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<SQL_DATE_STRUCT> const& rows,
         bool idx) { t.InsertDateData(conn, rows, idx); },
      CheckDateTranslation);
}

void FuzzTimestamp(TimestampInput const& input) {
  FuzzTranslation<SQL_TIMESTAMP_STRUCT>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_TIMESTAMP",
      DateTimeColumnKind::Timestamp,
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<SQL_TIMESTAMP_STRUCT> const& rows,
         bool idx) { t.InsertTimestampData(conn, rows, idx); },
      CheckTimestampTranslation);
}

void FuzzTime(TimeInput const& input) {
  FuzzTranslation<SQL_TIME_STRUCT>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_TIME",
      DateTimeColumnKind::Time,
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<SQL_TIME_STRUCT> const& rows,
         bool idx) { t.InsertTimeData(conn, rows, idx); },
      CheckTimeTranslation);
}

void FuzzDatetime(TimestampInput const& input) {
  FuzzTranslation<SQL_TIMESTAMP_STRUCT>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATETIME",
      DateTimeColumnKind::Datetime,
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<SQL_TIMESTAMP_STRUCT> const& rows,
         bool idx) { t.InsertTimestampData(conn, rows, idx); },
      CheckDatetimeTranslation);
}

void FuzzBoolean(BooleanInput const& input) {
  FuzzTranslation<std::string>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_BOOLEAN",
      DateTimeColumnKind::Boolean,
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<std::string> const& rows,
         bool idx) { t.InsertBooleanData(conn, rows, idx); },
      CheckBooleanTranslation);
}

void FuzzString(StringInput const& input) {
  FuzzTranslation<std::string>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_STRING",
      DateTimeColumnKind::String,
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<std::string> const& rows,
         bool idx) { t.InsertStrData(conn, rows, idx); },
      CheckStringTranslation);
}
// -------------------
// FUZZ_TESTS
// -------------------

// IMPORTANT: functions must be declared **before** FUZZ_TEST macros
FUZZ_TEST(DataTranslationFuzz, FuzzDate).WithDomains(DateFuzzDomain());

FUZZ_TEST(DataTranslationFuzz, FuzzTimestamp)
    .WithDomains(TimestampFuzzDomain());

FUZZ_TEST(DataTranslationFuzz, FuzzTime).WithDomains(TimeFuzzDomain());

FUZZ_TEST(DataTranslationFuzz, FuzzDatetime)
    .WithDomains(TimestampFuzzDomain());

    FUZZ_TEST(DataTranslationFuzz, FuzzBoolean)
    .WithDomains(BooleanFuzzDomain());

     FUZZ_TEST(DataTranslationFuzz, FuzzString)
    .WithDomains(StringFuzzDomain());
}  // namespace google::cloud::odbc_tests
