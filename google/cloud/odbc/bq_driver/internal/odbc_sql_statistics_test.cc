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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_statistics.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {
namespace {

using google::cloud::odbc_internal::SQLStates;

class OdbcSqlStatisticsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    env_handle_ = std::make_shared<EnvironmentHandle>();
    conn_handle_ = std::make_shared<ConnectionHandle>(env_handle_);
    stmt_handle_ = std::make_unique<StatementHandle>(conn_handle_);
  }

  std::shared_ptr<EnvironmentHandle> env_handle_;
  std::shared_ptr<ConnectionHandle> conn_handle_;
  std::unique_ptr<StatementHandle> stmt_handle_;
};

TEST_F(OdbcSqlStatisticsTest, MissingTableName) {
  auto result = FetchStatisticsResultSet(*stmt_handle_, "catalog", 7, "schema", 6, "", 0, SQL_INDEX_ALL, SQL_QUICK);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY009());
}

TEST_F(OdbcSqlStatisticsTest, InvalidUniqueOption) {
  auto result = FetchStatisticsResultSet(*stmt_handle_, "catalog", 7, "schema", 6, "table", 5, 999, SQL_QUICK);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY100());
}

TEST_F(OdbcSqlStatisticsTest, InvalidReservedOption) {
  auto result = FetchStatisticsResultSet(*stmt_handle_, "catalog", 7, "schema", 6, "table", 5, SQL_INDEX_ALL, 999);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY101());
}

TEST_F(OdbcSqlStatisticsTest, NullConnectionHandle) {
  StatementHandle empty_stmt_handle(nullptr);
  auto result = FetchStatisticsResultSet(empty_stmt_handle, "catalog", 7, "schema", 6, "table", 5, SQL_INDEX_ALL, SQL_QUICK);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY013());
}

}  // namespace
}  // namespace google::cloud::odbc_bq_driver_internal

