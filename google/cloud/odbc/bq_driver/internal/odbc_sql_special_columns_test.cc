// Copyright 2026 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_special_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {
namespace {

using ::google::cloud::odbc_internal::SQLStates;

TEST(FetchSpecialColumnsResultSetFromTableMetaData,
     SuccessSQLBestRowIdReturnsEmptyRows) {
  StatementHandle handle;
  auto result = FetchSpecialColumnsResultSetFromTableMetaData(
      handle, SQL_BEST_ROWID, "catalog", SQL_NTS, "schema", SQL_NTS, "table",
      SQL_NTS, SQL_SCOPE_SESSION, SQL_NULLABLE);

  ASSERT_TRUE(result.Ok());
  EXPECT_EQ(result->rows.size(), 0);
  EXPECT_EQ(result->row_schema.size(), 8);
}

TEST(FetchSpecialColumnsResultSetFromTableMetaData,
     SuccessSQLRowVerReturnsEmptyRows) {
  StatementHandle handle;
  auto result = FetchSpecialColumnsResultSetFromTableMetaData(
      handle, SQL_ROWVER, "catalog", SQL_NTS, "schema", SQL_NTS, "table",
      SQL_NTS, SQL_SCOPE_SESSION, SQL_NULLABLE);

  ASSERT_TRUE(result.Ok());
  EXPECT_EQ(result->rows.size(), 0);
  EXPECT_EQ(result->row_schema.size(), 8);
}

TEST(FetchSpecialColumnsResultSetFromTableMetaData, FailureEmptyTableName) {
  StatementHandle handle;
  // Use a dummy identifier type to bypass the early return
  auto result = FetchSpecialColumnsResultSetFromTableMetaData(
      handle, 999, "catalog", SQL_NTS, "schema", SQL_NTS, "", SQL_NTS,
      SQL_SCOPE_SESSION, SQL_NULLABLE);

  ASSERT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY009());
  EXPECT_EQ(result.GetStatusRecord().message,
            "Parameter table_name cannot be empty");
}

}  // namespace
}  // namespace google::cloud::odbc_bq_driver_internal
