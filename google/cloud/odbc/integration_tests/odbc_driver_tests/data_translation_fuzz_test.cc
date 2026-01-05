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
  SQLCHAR data_int[kBufferLength] = {0};
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

  std::string str_int(reinterpret_cast<char*>(data_int));

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
  } catch (nlohmann::json::exception& e) {
    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
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

// -------------------
// Table schema helper
// -------------------

template <typename SQLStruct>
std::string insert_table_schema();

template <>
std::string insert_table_schema<SQL_DATE_STRUCT>() {
  return "(idx INTEGER, col DATE)";
}

template <>
std::string insert_table_schema<SQL_TIMESTAMP_STRUCT>() {
  return "(idx INTEGER, col TIMESTAMP)";
}

template <>
std::string insert_table_schema<SQL_TIME_STRUCT>() {
  return "(idx INTEGER, col TIME)";
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
    TranslationFuzzInput<SQLStruct> const& input, std::string const& table_name,
    std::function<void(Table&, std::shared_ptr<ODBCHandles> const&,
                       std::vector<SQLStruct> const&, bool)>
        InsertFunc,
    std::function<void(SQLSMALLINT, SQLStruct const&, SQLPOINTER, SQLLEN)>
        CheckFunc) {
  Table table(table_name);
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str = kDefaultConnectionString;

  // Create table
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, insert_table_schema<SQLStruct>());
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
  std::string expected = FormatDate(input);

  switch (target_c_type) {
    case SQL_C_CHAR: {
      EXPECT_EQ(reinterpret_cast<char const*>(data), expected);
      break;
    }
    case SQL_C_WCHAR: {
      SQLINTEGER length = len / sizeof(SQLWCHAR);
      EXPECT_EQ(
          ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length),
          expected);
      break;
    }
    case SQL_C_BINARY: {
      auto* d = reinterpret_cast<const SQL_DATE_STRUCT*>(data);
      EXPECT_EQ(FormatDate(*d), expected);
      break;
    }
    case SQL_C_TYPE_DATE: {
      auto* d = reinterpret_cast<const SQL_DATE_STRUCT*>(data);
      EXPECT_EQ(d->year, input.year);
      EXPECT_EQ(d->month, input.month);
      EXPECT_EQ(d->day, input.day);
      break;
    }
    case SQL_C_TYPE_TIMESTAMP: {
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
      EXPECT_EQ(
          ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length),
          FormatTimeStamp(input));
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

// void FuzzTimestamp(const TimestampInput& input) {
//     FuzzTranslation<SQL_TIMESTAMP_STRUCT>(
//         input,
//         kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_TIMESTAMP",
//         [](Table& t, const std::shared_ptr<ODBCHandles>& conn,
//            const std::vector<SQL_TIMESTAMP_STRUCT>& rows, bool idx) {
//             t.InsertTimestampData(conn, rows, idx);
//         },
//         CheckTimestampTranslation
//     );
// }

// FUZZ_TEST(DataTranslationFuzz, FuzzTimestamp)
//     .WithDomains(TimestampFuzzDomain());

// void FuzzDate(const DateInput& input) {
//     FuzzTranslation<SQL_DATE_STRUCT>(
//         input,
//         kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATE",
//         [](Table& t, const std::shared_ptr<ODBCHandles>& conn,
//            const std::vector<SQL_DATE_STRUCT>& rows, bool idx) {
//             t.InsertDateData(conn, rows, idx);
//         },
//         CheckDateTranslation
//     );
// }

// FUZZ_TEST(DataTranslationFuzz, FuzzDate)
//     .WithDomains(DateFuzzDomain());

//     void FuzzTime(const TimeInput& input) {
//     FuzzTranslation<SQL_TIME_STRUCT>(
//         input,
//         kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_TIME",
//         [](Table& t, const std::shared_ptr<ODBCHandles>& conn,
//            const std::vector<SQL_TIME_STRUCT>& rows, bool idx) {
//             t.InsertTimeData(conn, rows, idx);
//         },
//         CheckTimeTranslation
//     );
// }

// FUZZ_TEST(DataTranslationFuzz, FuzzTime)
//     .WithDomains(TimeFuzzDomain());

// -------------------
// Fuzz functions
// -------------------

void FuzzDate(DateInput const& input) {
  FuzzTranslation<SQL_DATE_STRUCT>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATE",
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<SQL_DATE_STRUCT> const& rows,
         bool idx) { t.InsertDateData(conn, rows, idx); },
      CheckDateTranslation);
}

void FuzzTimestamp(TimestampInput const& input) {
  FuzzTranslation<SQL_TIMESTAMP_STRUCT>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_TIMESTAMP",
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<SQL_TIMESTAMP_STRUCT> const& rows,
         bool idx) { t.InsertTimestampData(conn, rows, idx); },
      CheckTimestampTranslation);
}

void FuzzTime(TimeInput const& input) {
  FuzzTranslation<SQL_TIME_STRUCT>(
      input, kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_TIME",
      [](Table& t, std::shared_ptr<ODBCHandles> const& conn,
         std::vector<SQL_TIME_STRUCT> const& rows,
         bool idx) { t.InsertTimeData(conn, rows, idx); },
      CheckTimeTranslation);
}

// -------------------
// FUZZ_TESTS
// -------------------

// IMPORTANT: functions must be declared **before** FUZZ_TEST macros
FUZZ_TEST(DataTranslationFuzz, FuzzDate).WithDomains(DateFuzzDomain());

FUZZ_TEST(DataTranslationFuzz, FuzzTimestamp)
    .WithDomains(TimestampFuzzDomain());

FUZZ_TEST(DataTranslationFuzz, FuzzTime).WithDomains(TimeFuzzDomain());

}  // namespace google::cloud::odbc_tests
