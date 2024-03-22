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
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
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
  OdbcUtilsConnectionHandleTest conn_handle;
  conn_handle.SetConnected();
  HandleWrapped wrapped_handle(HandleType::kConnHandle, &conn_handle);

  auto result = ValidateConnectionHandle(&wrapped_handle);

  ASSERT_STATUS_RECORD_OK(result);
}

TEST(ValidateConnectionHandle, InvalidNullPtr) {
  auto result = ValidateConnectionHandle(nullptr);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Handle is null pointer")));
}

TEST(ValidateConnectionHandle, InvalidHandleType) {
  EnvironmentHandle env_handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &env_handle);

  auto result = ValidateConnectionHandle(&wrapped_handle);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Invalid handle type")));
}

TEST(ValidateConnectionHandle, InvalidHandleNotConnected) {
  OdbcUtilsConnectionHandleTest conn_handle;
  HandleWrapped wrapped_handle(HandleType::kConnHandle, &conn_handle);

  auto result = ValidateConnectionHandle(&wrapped_handle);

  EXPECT_THAT(
      result,
      StatusRecordIs(SQLStates::k_08003(),
                     StrEq("Connection handle not connected to data source")));
}

///////////////////////////////////////
// Environment Handle Validation Tests
///////////////////////////////////////

TEST(ValidateEnvironmentHandle, Success) {
  EnvironmentHandle env_handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &env_handle);

  auto result = ValidateEnvironmentHandle(&wrapped_handle);

  ASSERT_STATUS_RECORD_OK(result);
}

TEST(ValidateEnvironmentHandle, InvalidNullPtr) {
  auto result = ValidateEnvironmentHandle(nullptr);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Handle is null pointer")));
}

TEST(ValidateEnvironmentHandle, InvalidHandleType) {
  ConnectionHandle conn_handle;
  HandleWrapped wrapped_handle(HandleType::kConnHandle, &conn_handle);

  auto result = ValidateEnvironmentHandle(&wrapped_handle);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Invalid handle type")));
}

///////////////////////////////////////
// Statement Handle Validation Tests
///////////////////////////////////////

TEST(ValidateStatementHandle, Success) {
  StatementHandle stmt_handle;
  HandleWrapped wrapped_handle(HandleType::kStatementHandle, &stmt_handle);

  auto result = ValidateStatementHandle(&wrapped_handle);

  ASSERT_STATUS_RECORD_OK(result);
}

TEST(ValidateStatementHandle, InvalidNullPtr) {
  auto result = ValidateStatementHandle(nullptr);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Handle is null pointer")));
}

TEST(ValidateStatementHandle, InvalidHandleType) {
  ConnectionHandle conn_handle;
  HandleWrapped wrapped_handle(HandleType::kConnHandle, &conn_handle);

  auto result = ValidateStatementHandle(&wrapped_handle);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Invalid handle type")));
}

///////////////////////////////////////
// Descriptor Handle Validation Tests
///////////////////////////////////////

TEST(ValidateDescriptorHandle, Success) {
  DescriptorHandle desc_handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &desc_handle);

  auto result = ValidateDescriptorHandle(&wrapped_handle);

  ASSERT_STATUS_RECORD_OK(result);
}

TEST(ValidateDescriptorHandle, InvalidNullPtr) {
  auto result = ValidateDescriptorHandle(nullptr);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Handle is null pointer")));
}

TEST(ValidateDescriptorHandle, InvalidHandleType) {
  StatementHandle stmt_handle;
  HandleWrapped wrapped_handle(HandleType::kStatementHandle, &stmt_handle);

  auto result = ValidateDescriptorHandle(&wrapped_handle);

  EXPECT_THAT(result, StatusRecordIs(SQLStates::k_HY000(),
                                     StrEq("Invalid handle type")));
}

}  // namespace google::cloud::odbc_bq_driver
