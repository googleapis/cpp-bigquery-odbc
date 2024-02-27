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
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
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

SQLHDBC GetConnectionHandle(HandleType const& type, bool connected = true) {
  auto conn_handle = std::make_shared<OdbcUtilsConnectionHandleTest>();
  if (connected) {
    conn_handle->SetConnected();
  }
  auto wrapped_handle =
      std::make_shared<HandleWrapped>(type, conn_handle.get());
  return wrapped_handle.get();
}

void AllocateHandles(SQLHENV* env_handle_ref, SQLHDBC* conn_handle_ref) {
  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(env_handle_ref));
  EXPECT_EQ(SQL_SUCCESS, SQLAllocConnHandle(*env_handle_ref, conn_handle_ref));
}

void AllocateHandles(SQLHENV* env_handle_ref, SQLHDBC* conn_handle_ref,
                     SQLHSTMT* stmt_handle_ref) {
  AllocateHandles(env_handle_ref, conn_handle_ref);
  EXPECT_EQ(SQL_SUCCESS, SQLAllocStmtHandle(*conn_handle_ref, stmt_handle_ref));
}

void FreeHandles(SQLHENV env_handle, SQLHDBC conn_handle) {
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_DBC, conn_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

void FreeHandles(SQLHENV env_handle, SQLHDBC conn_handle,
                 SQLHSTMT stmt_handle) {
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_STMT, stmt_handle));
  FreeHandles(env_handle, conn_handle);
}

}  // namespace

///////////////////////////////////////
// Connection Handle Validation Tests
///////////////////////////////////////

TEST(ValidateConnectionHandle, Success) {
  auto result =
      ValidateConnectionHandle(GetConnectionHandle(HandleType::kConnHandle));
  ASSERT_STATUS_OK(result);
}

TEST(ValidateConnectionHandle, InvalidNullPtr) {
  auto result = ValidateConnectionHandle(nullptr);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Null connection handle")));
}

// TEST(ValidateConnectionHandle, InvalidHandleType) {
//   auto result =
//       ValidateConnectionHandle(GetConnectionHandle(HandleType::kEnvHandle));

//   EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
//                                StrEq("Invalid handle type")));
// }

TEST(ValidateConnectionHandle, InvalidHandleNotConnected) {
  auto result = ValidateConnectionHandle(
      GetConnectionHandle(HandleType::kConnHandle, /* connected */ false));

  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kInvalidArgument,
               StrEq("Connection handle not connected to data source")));
}

///////////////////////////////////////
// Environment Handle Validation Tests
///////////////////////////////////////

TEST(ValidateEnvironmentHandle, Success) {
  SQLHENV env_handle;
  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  auto result = ValidateEnvironmentHandle(env_handle);
  ASSERT_STATUS_OK(result);
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(ValidateEnvironmentHandle, InvalidNullPtr) {
  auto result = ValidateEnvironmentHandle(nullptr);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Null environment handle")));
}

TEST(ValidateEnvironmentHandle, InvalidHandleType) {
  SQLHENV env_handle;
  SQLHDBC conn_handle;
  AllocateHandles(&env_handle, &conn_handle);
  auto result = ValidateEnvironmentHandle(conn_handle);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid handle type")));

  FreeHandles(env_handle, conn_handle);
}

TEST(ValidateEnvironmentHandle, InvalidInternalEnvironmentHandle) {
  SQLHENV env_handle;
  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));

  auto result = ValidateEnvironmentHandle(env_handle);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid handle type")));
}

///////////////////////////////////////
// Statement Handle Validation Tests
///////////////////////////////////////

TEST(ValidateStatementHandle, Success) {
  SQLHENV env_handle;
  SQLHDBC conn_handle;
  SQLHSTMT stmt_handle;
  AllocateHandles(&env_handle, &conn_handle, &stmt_handle);
  auto result = ValidateStatementHandle(stmt_handle);
  ASSERT_STATUS_OK(result);
  FreeHandles(env_handle, conn_handle, stmt_handle);
}

TEST(ValidateStatementHandle, InvalidNullPtr) {
  auto result = ValidateStatementHandle(nullptr);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Null statement handle")));
}

TEST(ValidateStatementHandle, InvalidHandleType) {
  SQLHENV env_handle;
  SQLHDBC conn_handle;
  AllocateHandles(&env_handle, &conn_handle);

  auto result = ValidateStatementHandle(conn_handle);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid handle type")));

  FreeHandles(env_handle, conn_handle);
}

TEST(ValidateStatementHandle, InvalidInternalStatementHandle) {
  SQLHENV env_handle;
  SQLHDBC conn_handle;
  SQLHSTMT stmt_handle;
  AllocateHandles(&env_handle, &conn_handle, &stmt_handle);
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_STMT, stmt_handle));

  auto result = ValidateStatementHandle(stmt_handle);

  EXPECT_THAT(result, StatusIs(StatusCode::kInvalidArgument,
                               StrEq("Invalid handle type")));

  FreeHandles(env_handle, conn_handle);
}

}  // namespace google::cloud::odbc_bq_driver
