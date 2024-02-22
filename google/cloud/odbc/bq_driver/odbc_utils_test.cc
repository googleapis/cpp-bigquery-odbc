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
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::StrEq;

// Helper class and functions specific to odbc utils unit tests.
namespace {
class OdbcUtilsConnectionHandleTest : public ConnectionHandle {
 public:
  explicit OdbcUtilsConnectionHandleTest() = default;
  void SetConnected() { is_connected_ = true; }
};

SQLHDBC GetConnHandle(HandleType const& type, bool connected = true) {
  auto conn_handle = std::make_shared<OdbcUtilsConnectionHandleTest>();
  if (connected) {
    conn_handle->SetConnected();
  }
  auto wrapped_handle =
      std::make_shared<HandleWrapped>(type, conn_handle.get());
  return wrapped_handle.get();
}

}  // namespace

///////////////////////////////////////
// Connection Handle Validation Tests
///////////////////////////////////////

TEST(ValidateConnectionHandle, Success) {
  auto result =
      ValidateConnectionHandle(GetConnHandle(HandleType::kConnHandle));
  ASSERT_STATUS_OK(result);
}

TEST(ValidateConnectionHandle, InvalidNullPtr) {
  auto result = ValidateConnectionHandle(nullptr);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Null connection handle")));
}

TEST(ValidateConnectionHandle, InvalidHandleType) {
  auto result = ValidateConnectionHandle(GetConnHandle(HandleType::kEnvHandle));

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid connection handle type")));
}

TEST(ValidateConnectionHandle, InvalidHandleNotConnected) {
  auto result = ValidateConnectionHandle(
      GetConnHandle(HandleType::kConnHandle, /* connected */ false));

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid connection handle")));
}

///////////////////////////////////////
// Statement Handle Validation Tests
///////////////////////////////////////

TEST(ValidateStatementHandle, Success) {
  auto stmt_handle = std::make_shared<StatementHandle>();
  auto wrapped_handle = std::make_shared<HandleWrapped>(
      HandleType::kStatementHandle, stmt_handle.get());
  auto result = ValidateStatementHandle(wrapped_handle.get());
  ASSERT_STATUS_OK(result);
}

TEST(ValidateStatementHandle, InvalidNullPtr) {
  auto result = ValidateStatementHandle(nullptr);
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Null statement handle")));
}

TEST(ValidateStatementHandle, InvalidHandleType) {
  auto stmt_handle = std::make_shared<StatementHandle>();
  auto wrapped_handle = std::make_shared<HandleWrapped>(HandleType::kEnvHandle,
                                                        stmt_handle.get());
  auto result = ValidateStatementHandle(wrapped_handle.get());
  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid statement handle type")));
}

}  // namespace google::cloud::odbc_bq_driver
