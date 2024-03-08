// Copyright 2023 Google LLC
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

#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::AllocateHandles;
using google::cloud::odbc_testing_bq_driver_utils::FreeHandles;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::StrEq;

// Helper class and functions specific to odbc utils unit tests.
namespace {
class OdbcUtilsConnectionHandleTest : public ConnectionHandle {
 public:
  explicit OdbcUtilsConnectionHandleTest() = default;
  void SetConnected() { is_connected_ = true; }
};
}  // namespace

///////////////////////////////////////
// Connection Handle Validation Tests
///////////////////////////////////////

TEST(ValidateConnectionHandle, Success) {
  auto conn_handle = new OdbcUtilsConnectionHandleTest();
  conn_handle->SetConnected();
  auto wrapped_handle = new HandleWrapped(HandleType::kConnHandle, conn_handle);

  auto result = ValidateConnectionHandle(wrapped_handle);
  ASSERT_STATUS_RECORD_OK(result);

  delete conn_handle;
  delete wrapped_handle;
}

TEST(ValidateConnectionHandle, InvalidNullPtr) {
  auto result = ValidateConnectionHandle(nullptr);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Handle is null pointer")));
}

TEST(ValidateConnectionHandle, InvalidHandleType) {
  SQLHENV env_handle;
  SQLHDBC conn_handle;
  ASSERT_EQ(SQL_SUCCESS, AllocateHandles(&env_handle, &conn_handle));
  auto result = ValidateConnectionHandle(env_handle);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Invalid handle type")));
  EXPECT_EQ(SQL_SUCCESS, FreeHandles(env_handle, conn_handle));
}

TEST(ValidateConnectionHandle, InvalidHandleNotConnected) {
  auto conn_handle = new OdbcUtilsConnectionHandleTest();
  auto wrapped_handle = new HandleWrapped(HandleType::kConnHandle, conn_handle);
  auto result = ValidateConnectionHandle(wrapped_handle);

  EXPECT_THAT(
      result,
      StatusRecordIs(SQLStates::k_08003(),
                     StrEq("Connection handle not connected to data source")));

  delete conn_handle;
  delete wrapped_handle;
}

///////////////////////////////////////
// Environment Handle Validation Tests
///////////////////////////////////////

TEST(ValidateEnvironmentHandle, Success) {
  SQLHENV env_handle;
  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  auto result = ValidateEnvironmentHandle(env_handle);
  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(ValidateEnvironmentHandle, InvalidNullPtr) {
  auto result = ValidateEnvironmentHandle(nullptr);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Handle is null pointer")));
}

TEST(ValidateEnvironmentHandle, InvalidHandleType) {
  SQLHENV env_handle;
  SQLHDBC conn_handle;
  ASSERT_EQ(SQL_SUCCESS, AllocateHandles(&env_handle, &conn_handle));
  auto result = ValidateEnvironmentHandle(conn_handle);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Invalid handle type")));

  EXPECT_EQ(SQL_SUCCESS, FreeHandles(env_handle, conn_handle));
}

///////////////////////////////////////
// Statement Handle Validation Tests
///////////////////////////////////////

TEST(ValidateStatementHandle, Success) {
  auto* stmt_handle = new StatementHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kStatementHandle, stmt_handle);

  auto result = ValidateStatementHandle(wrapped_handle);

  ASSERT_STATUS_RECORD_OK(result);
  delete stmt_handle;
  delete wrapped_handle;
}

TEST(ValidateStatementHandle, InvalidNullPtr) {
  auto result = ValidateStatementHandle(nullptr);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Handle is null pointer")));
}

TEST(ValidateStatementHandle, InvalidHandleType) {
  SQLHENV env_handle;
  SQLHDBC conn_handle;
  ASSERT_EQ(SQL_SUCCESS, AllocateHandles(&env_handle, &conn_handle));

  auto result = ValidateStatementHandle(conn_handle);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Invalid handle type")));

  EXPECT_EQ(SQL_SUCCESS, FreeHandles(env_handle, conn_handle));
}

}  // namespace google::cloud::odbc_bq_driver
