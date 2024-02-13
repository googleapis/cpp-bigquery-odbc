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

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::StrEq;

class ConnectionHandleTest : public ConnectionHandle {
 public:
  explicit ConnectionHandleTest() = default;
  void SetConnected() { is_connected_ = true; }
};

TEST(ValidateConnectionHandle, Success) {
  auto* conn_handle = new ConnectionHandleTest();
  conn_handle->SetConnected();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  SQLHDBC handle = wrapped_handle;

  auto result = ValidateConnectionHandle(handle);
  ASSERT_STATUS_OK(result);

  delete conn_handle;
  delete wrapped_handle;
}

TEST(ValidateConnectionHandle, InvalidNullPtr) {
  auto result = ValidateConnectionHandle(nullptr);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Null connection handle")));
}

TEST(ValidateConnectionHandle, InvalidHandleType) {
  auto* conn_handle = new ConnectionHandleTest();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, conn_handle);
  SQLHDBC handle = wrapped_handle;

  auto result = ValidateConnectionHandle(handle);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid connection handle type")));

  delete conn_handle;
  delete wrapped_handle;
}

TEST(ValidateConnectionHandle, InvalidHandleNotConnected) {
  auto* conn_handle = new ConnectionHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  SQLHDBC handle = wrapped_handle;

  auto result = ValidateConnectionHandle(handle);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid connection handle")));

  delete conn_handle;
  delete wrapped_handle;
}

}  // namespace google::cloud::odbc_bq_driver
