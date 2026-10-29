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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_statistics.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {
namespace {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;

TEST(OdbcSqlStatisticsTest, EmptyTableNameReturnsEmptyResultSet) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);

  DescriptorHandle impl_desc;

  auto stmt_handle = std::make_unique<StatementHandle>(
      &conn_handle, Descriptors{impl_desc, impl_desc, impl_desc, impl_desc});

  conn_handle.GetStatementHandles().emplace(stmt_handle.get());

  auto result = FetchStatisticsResultSet(*stmt_handle, "catalog", "schema", "",
                                         SQL_INDEX_ALL, SQL_QUICK);

  ASSERT_TRUE(result.Ok());
  EXPECT_TRUE(result->rows.empty());

  conn_handle.GetStatementHandles().erase(stmt_handle.get());
}

TEST(OdbcSqlStatisticsTest, InvalidUniqueOption) {
  StatementHandle handle;
  auto result = FetchStatisticsResultSet(handle, "catalog", "schema", "table",
                                         999, SQL_QUICK);
  ASSERT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY100());
}

TEST(OdbcSqlStatisticsTest, InvalidReservedOption) {
  StatementHandle handle;
  auto result = FetchStatisticsResultSet(handle, "catalog", "schema", "table",
                                         SQL_INDEX_ALL, 999);
  ASSERT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY101());
}

TEST(OdbcSqlStatisticsTest, NullConnectionHandle) {
  StatementHandle handle;
  auto result = FetchStatisticsResultSet(handle, "catalog", "schema", "table",
                                         SQL_INDEX_ALL, SQL_QUICK);
  ASSERT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY013());
}

}  // namespace
}  // namespace google::cloud::odbc_bq_driver_internal
