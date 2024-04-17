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
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::HandleType;
using google::cloud::odbc_bq_driver_internal::HandleWrapped;
using google::cloud::odbc_bq_driver_internal::StatementHandle;

TEST(SQLFreeHandleInternal, InvalidType) {
  int val = 10;

  SQLRETURN status = SQLFreeHandleInternal(55, &val);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
}

TEST(SQLFreeHandleInternal, ConnectionHandle_Basic) {
  auto* conn_handle = new ConnectionHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DBC, wrapped_handle);
  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, ConnectionHandle_IncorrectHandleType) {
  auto* conn_handle = new ConnectionHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_ENV, wrapped_handle);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete conn_handle;
  delete wrapped_handle;
}

TEST(SQLFreeHandleInternal, EnvironmentHandle_Basic) {
  auto* env_handle = new EnvironmentHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, env_handle);
  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_ENV, wrapped_handle);
  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, EnvironmentHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, env_handle);
  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DBC, wrapped_handle);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
  delete wrapped_handle;
}

TEST(SQLFreeHandleInternal, StatementHandle_Basic) {
  auto* stmt_handle = new StatementHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kStatementHandle, stmt_handle);
  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_STMT, wrapped_handle);
  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, StatementHandle_IncorrectHandleType) {
  auto* stmt_handle = new StatementHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, stmt_handle);
  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_STMT, wrapped_handle);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete stmt_handle;
  delete wrapped_handle;
}

TEST(SQLFreeHandleInternal, DescriptorHandle_Basic) {
  auto* desc_handle = new DescriptorHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kDescriptorHandle, desc_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, wrapped_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, DescriptorHandle_IncorrectHandleType) {
  auto* desc_handle = new DescriptorHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, desc_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, wrapped_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete desc_handle;
  delete wrapped_handle;
}

}  // namespace google::cloud::odbc_bq_driver
