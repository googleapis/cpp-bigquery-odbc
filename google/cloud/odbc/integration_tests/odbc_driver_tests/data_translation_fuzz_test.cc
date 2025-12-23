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
