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

using fuzztest::Domain;
using fuzztest::ElementOf;
using fuzztest::InRange;
using fuzztest::StructOf;

struct DateFuzzInput {
  SQLSMALLINT target_c_type;
  SQL_DATE_STRUCT value;
};

Domain<DateFuzzInput> DateFuzzDomain() {
  return StructOf<DateFuzzInput>(
      ElementOf<SQLSMALLINT>({
          SQL_C_CHAR,
          SQL_C_WCHAR,
          SQL_C_BINARY,
          SQL_C_TYPE_DATE,
          SQL_C_TYPE_TIMESTAMP,
          SQL_C_USHORT,
          SQL_C_DOUBLE,
      }),
      StructOf<SQL_DATE_STRUCT>(
          InRange<SQLSMALLINT>(2000, 2100),  // year
          InRange<SQLUSMALLINT>(1, 12),      // month
          InRange<SQLUSMALLINT>(1, 28)       // day (avoid invalid dates)
          ));
}

void FuzzTranslationFromDate(DateFuzzInput const& input) {
  auto const table_name =
      kDatasetWithTablePrefix + "ODBC_DATA_TRANSLATION_DATE";

  Table table(table_name);
  auto conn = std::make_shared<ODBCHandles>();

  std::string connection_string =
      kDefaultConnectionString +
      ";ProxyHost=34.94.167.18;ProxyPort=3128;ProxyUid=fahmz;ProxyPwd=fahmz;";

  // Create table
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS);
  table.CreateWithPrepare(conn, "(index INTEGER, DateField DATE)");
  ASSERT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Insert fuzzed date
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS);
  table.InsertDateData(conn, {input.value}, true);
  ASSERT_EQ(Disconnect(conn), SQL_SUCCESS);

  // Query
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS);

  SQLRETURN status;
  SQLCHAR data[kBufferLength] = {0};
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];

  std::string query = "SELECT DateField FROM " + table_name + " ORDER BY index";
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  ASSERT_TRUE(SQL_SUCCEEDED(status));

  status = SQLExecute(conn->hstmt);
  ASSERT_TRUE(SQL_SUCCEEDED(status));

  status = SQLBindCol(conn->hstmt, 1, input.target_c_type, data, kBufferLength,
                      &strlen_or_ind);

  if (!SQL_SUCCEEDED(status)) {
    // Some C types are expected to fail binding
    Disconnect(conn);
    return;
  }

  status = SQLFetch(conn->hstmt);

  if (!SQL_SUCCEEDED(status)) {
    Disconnect(conn);
    return;
  }

  std::string expected_val = FormatDate(input.value);

  switch (input.target_c_type) {
    case SQL_C_CHAR: {
      std::string returned_val = reinterpret_cast<char*>(data);
      EXPECT_EQ(returned_val, expected_val);
      break;
    }
    case SQL_C_WCHAR: {
      std::string returned_val_utf8 =
          ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), 10);
      EXPECT_EQ(returned_val_utf8, expected_val);
      break;
    }
    case SQL_C_BINARY: {
      if (strlen_or_ind == sizeof(SQL_DATE_STRUCT)) {
        auto* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
        EXPECT_EQ(FormatDate(*date), expected_val);
      }
      break;
    }
    case SQL_C_TYPE_DATE: {
      auto* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
      EXPECT_EQ(date->year, input.value.year);
      EXPECT_EQ(date->month, input.value.month);
      EXPECT_EQ(date->day, input.value.day);
      break;
    }
    case SQL_C_TYPE_TIMESTAMP: {
      auto* ts = reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
      EXPECT_EQ(ts->year, input.value.year);
      EXPECT_EQ(ts->month, input.value.month);
      EXPECT_EQ(ts->day, input.value.day);
      break;
    }
    default:
      break;
  }

  Disconnect(conn);

  // Cleanup
  ASSERT_EQ(Connect(connection_string, conn), SQL_SUCCESS);
  table.DropWithPrepare(conn);
  ASSERT_EQ(Disconnect(conn), SQL_SUCCESS);
}

FUZZ_TEST(DataTranslationFuzz, FuzzTranslationFromDate)
    .WithDomains(DateFuzzDomain());

void RunArraySQLStatement(std::shared_ptr<ODBCHandles> conn,
                          std::string const& query) {
  SQLRETURN status;
  char read_stmt[kBufferLength] = {};
  SQLCHAR data_int[kBufferLength] = {};
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

}  // namespace google::cloud::odbc_tests
