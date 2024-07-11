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

#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

TEST(SQLFreeHandleInternal, InvalidType) {
  int val = 10;

  SQLRETURN status = SQLFreeHandleInternal(55, &val);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
}

TEST(SQLFreeHandleInternal, ConnectionHandle_Basic) {
  EnvironmentHandle env_handle;
  auto* conn_handle = new ConnectionHandle(&env_handle);
  env_handle.GetConnectionHandles().emplace(conn_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DBC, conn_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_TRUE(env_handle.GetConnectionHandles().empty());
}

TEST(SQLFreeHandleInternal, ConnectionHandle_IncorrectHandleType) {
  auto* conn_handle = new ConnectionHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_ENV, conn_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete conn_handle;
}

TEST(SQLFreeHandleInternal, EnvironmentHandle_Basic) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, EnvironmentHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DBC, env_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
}

TEST(SQLFreeHandleInternal, StatementHandle_Basic) {
  ConnectionHandle conn_handle;
  auto* stmt_handle = new StatementHandle(&conn_handle);
  conn_handle.GetStatementHandles().emplace(stmt_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_STMT, stmt_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_TRUE(conn_handle.GetStatementHandles().empty());
}

TEST(SQLFreeHandleInternal, StatementHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_STMT, env_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
}

TEST(SQLFreeHandleInternal, DescriptorHandle_Basic) {
  auto* desc_handle = new DescriptorHandle();
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  desc_handle->SetConnectionHandle(&conn_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, desc_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, DescriptorHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, env_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
}

TEST(SQLFreeHandleInternal, DissociateDescriptorHandle) {
  StatementHandle stmt_handle = CreateStatementHandle();
  auto* desc_handle =
      new DescriptorHandle(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);
  stmt_handle.SetDescriptorHandle(DescriptorType::kAPD, desc_handle);
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  conn_handle.GetDescriptorHandles().emplace(desc_handle);
  desc_handle->SetConnectionHandle(&conn_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, desc_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(SQL_DESC_ALLOC_AUTO,
            stmt_handle.GetDescriptorHandle(DescriptorType::kAPD)
                .GetHeaderRecord()
                .GetAllocType());
  EXPECT_TRUE(conn_handle.GetDescriptorHandles().empty());
}

}  // namespace google::cloud::odbc_bq_driver
