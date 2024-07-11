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

#include "google/cloud/odbc/bq_driver/odbc_lock.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/testing/bq_driver_utils/status_utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

TEST(OdbcHandleLock, Success_Acquire_Release_Lock) {
  EnvironmentHandle env_handle;
  SQLRETURN status;
  status = AcquireHandleMutex(&env_handle, SQL_HANDLE_ENV);
  EXPECT_EQ(status, SQL_SUCCESS);
  status = ReleaseHandleMutex(&env_handle, SQL_HANDLE_ENV);
  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(OdbcHandleLock, Invalid_Handle_Acquire_Lock) {
  EnvironmentHandle env_handle;
  SQLRETURN status;
  status = AcquireHandleMutex(&env_handle, SQL_HANDLE_DBC);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
}

TEST(OdbcHandleLock, NULL_SQLHandle_Acquire_Lock) {
  SQLHDBC conn_handle = nullptr;
  SQLRETURN status = AcquireHandleMutex(conn_handle, SQL_HANDLE_DBC);
  EXPECT_EQ(status, SQL_NULL_HANDLE);
}

TEST(OdbcHandleLock, NULL_SQLHandle_Release_Lock) {
  SQLHDBC conn_handle = nullptr;
  SQLRETURN status = ReleaseHandleMutex(conn_handle, SQL_HANDLE_DBC);
  EXPECT_EQ(status, SQL_NULL_HANDLE);
}

}  // namespace google::cloud::odbc_bq_driver
