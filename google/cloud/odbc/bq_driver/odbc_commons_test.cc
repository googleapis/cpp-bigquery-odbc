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
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_includes.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;

TEST(FreeHandle, ConnectionHandle_Basic) {
  auto* conn_handle = new ConnectionHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  SQLRETURN status =
      FreeHandle<ConnectionHandle>(HandleType::kConnHandle, wrapped_handle);
  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(FreeHandle, ConnectionHandle_IncorrectHandleType) {
  auto* conn_handle = new ConnectionHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  SQLRETURN status =
      FreeHandle<ConnectionHandle>(HandleType::kEnvHandle, wrapped_handle);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete conn_handle;
  delete wrapped_handle;
}

TEST(FreeHandle, EnvironmentHandle_Basic) {
  auto* env_handle = new EnvironmentHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, env_handle);
  SQLRETURN status =
      FreeHandle<EnvironmentHandle>(HandleType::kEnvHandle, wrapped_handle);
  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(FreeHandle, EnvironmentHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, env_handle);
  SQLRETURN status =
      FreeHandle<EnvironmentHandle>(HandleType::kConnHandle, wrapped_handle);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
  delete wrapped_handle;
}

}  // namespace google::cloud::odbc_bq_driver
