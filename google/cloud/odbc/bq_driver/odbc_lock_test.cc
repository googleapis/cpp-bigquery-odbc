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

TEST(OdbcHandleLock, SuccessAcquireReleaseLock) {
  EnvironmentHandle env_handle;
  SQLRETURN status;
  status = AcquireHandleMutex(&env_handle, SQL_HANDLE_ENV);
  EXPECT_EQ(status, SQL_SUCCESS);
  status = ReleaseHandleMutex(&env_handle, SQL_HANDLE_ENV);
  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(OdbcHandleLock, InvalidHandleAcquireLock) {
  EnvironmentHandle env_handle;
  SQLRETURN status;
  status = AcquireHandleMutex(&env_handle, SQL_HANDLE_DBC);
  EXPECT_EQ(status, SQL_INVALID_HANDLE);
}

TEST(OdbcHandleLock, NULLSqlhandleAcquireLock) {
  SQLHDBC conn_handle = nullptr;
  SQLRETURN status = AcquireHandleMutex(conn_handle, SQL_HANDLE_DBC);
  EXPECT_EQ(status, SQL_NULL_HANDLE);
}

TEST(OdbcHandleLock, NULLSqlhandleReleaseLock) {
  SQLHDBC conn_handle = nullptr;
  SQLRETURN status = ReleaseHandleMutex(conn_handle, SQL_HANDLE_DBC);
  EXPECT_EQ(status, SQL_NULL_HANDLE);
}

#ifdef BQ_DRIVER_INTEGRATION_TESTS
TEST(OdbcHandleLock, HandleLockRaii) {
  SQLHENV env_handle = SQL_NULL_HANDLE;
  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_handle);
  {
    HandleLock lock(env_handle, SQL_HANDLE_ENV, false);
    // The mutex should be locked here
    EXPECT_TRUE(AcquireHandleMutex(env_handle, SQL_HANDLE_ENV, false) ==
                SQL_INVALID_HANDLE);
  }
  // The mutex should be unlocked here
  EXPECT_TRUE(AcquireHandleMutex(env_handle, SQL_HANDLE_ENV, false) ==
              SQL_SUCCESS);
  ReleaseHandleMutex(env_handle, SQL_HANDLE_ENV, false);
  SQLFreeHandle(SQL_HANDLE_ENV, env_handle);
}

TEST(OdbcHandleLock, HandleLockRaiiWithparentlock) {
  SQLHENV env_handle = SQL_NULL_HANDLE;
  SQLHDBC dbc_handle = SQL_NULL_HANDLE;
  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_handle);
  SQLAllocHandle(SQL_HANDLE_DBC, env_handle, &dbc_handle);
  {
    HandleLock lock(dbc_handle, SQL_HANDLE_DBC, true);
    // Parent environment mutex should be locked
    EXPECT_TRUE(AcquireHandleMutex(env_handle, SQL_HANDLE_ENV, false) ==
                SQL_INVALID_HANDLE);
  }
  // Parent environment mutex should be unlocked here
  EXPECT_TRUE(AcquireHandleMutex(env_handle, SQL_HANDLE_ENV, false) ==
              SQL_SUCCESS);
  ReleaseHandleMutex(env_handle, SQL_HANDLE_ENV, false);
  SQLFreeHandle(SQL_HANDLE_DBC, dbc_handle);
  SQLFreeHandle(SQL_HANDLE_ENV, env_handle);
}

TEST(OdbcHandleLock, GetParentHandlesConnection) {
  SQLHENV env_handle = SQL_NULL_HANDLE;
  SQLHDBC dbc_handle = SQL_NULL_HANDLE;
  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_handle);
  SQLAllocHandle(SQL_HANDLE_DBC, env_handle, &dbc_handle);

  SQLHANDLE handle = dbc_handle;
  SQLSMALLINT handle_type = SQL_HANDLE_DBC;
  bool is_global = false;

  SQLRETURN result = GetParentHandles(handle, handle_type, is_global);
  EXPECT_EQ(result, SQL_SUCCESS);
  EXPECT_EQ(handle, env_handle);
  EXPECT_EQ(handle_type, SQL_HANDLE_ENV);
  EXPECT_TRUE(is_global);

  SQLFreeHandle(SQL_HANDLE_DBC, dbc_handle);
  SQLFreeHandle(SQL_HANDLE_ENV, env_handle);
}

TEST(OdbcHandleLock, GetParentHandlesStatement) {
  SQLHENV env_handle = SQL_NULL_HANDLE;
  SQLHDBC dbc_handle = SQL_NULL_HANDLE;
  SQLHSTMT stmt_handle = SQL_NULL_HANDLE;
  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_handle);
  SQLAllocHandle(SQL_HANDLE_DBC, env_handle, &dbc_handle);
  SQLAllocHandle(SQL_HANDLE_STMT, dbc_handle, &stmt_handle);

  SQLHANDLE handle = stmt_handle;
  SQLSMALLINT handle_type = SQL_HANDLE_STMT;
  bool is_global = false;

  SQLRETURN result = GetParentHandles(handle, handle_type, is_global);
  EXPECT_EQ(result, SQL_SUCCESS);
  EXPECT_EQ(handle, dbc_handle);
  EXPECT_EQ(handle_type, SQL_HANDLE_DBC);
  EXPECT_FALSE(is_global);

  SQLFreeHandle(SQL_HANDLE_STMT, stmt_handle);
  SQLFreeHandle(SQL_HANDLE_DBC, dbc_handle);
  SQLFreeHandle(SQL_HANDLE_ENV, env_handle);
}

TEST(OdbcHandleLock, GetParentHandlesDescriptor) {
  SQLHENV env_handle = SQL_NULL_HANDLE;
  SQLHDBC dbc_handle = SQL_NULL_HANDLE;
  SQLHDESC desc_handle = SQL_NULL_HANDLE;
  SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_handle);
  SQLAllocHandle(SQL_HANDLE_DBC, env_handle, &dbc_handle);
  SQLAllocHandle(SQL_HANDLE_DESC, dbc_handle, &desc_handle);

  SQLHANDLE handle = desc_handle;
  SQLSMALLINT handle_type = SQL_HANDLE_DESC;
  bool is_global = false;

  SQLRETURN result = GetParentHandles(handle, handle_type, is_global);
  EXPECT_EQ(result, SQL_SUCCESS);
  EXPECT_EQ(handle, dbc_handle);
  EXPECT_EQ(handle_type, SQL_HANDLE_DBC);
  EXPECT_FALSE(is_global);

  SQLFreeHandle(SQL_HANDLE_DESC, desc_handle);
  SQLFreeHandle(SQL_HANDLE_DBC, dbc_handle);
  SQLFreeHandle(SQL_HANDLE_ENV, env_handle);
}

TEST(OdbcHandleLock, GetParentHandlesInvalidhandle) {
  SQLHANDLE handle = reinterpret_cast<SQLHANDLE>(0xDEADBEEF);
  SQLSMALLINT handle_type = SQL_HANDLE_DBC;
  bool is_global = false;

  SQLRETURN result = GetParentHandles(handle, handle_type, is_global);
  EXPECT_EQ(result, SQL_INVALID_HANDLE);
}
#endif  // BQ_DRIVER_INTEGRATION_TESTS
}  // namespace google::cloud::odbc_bq_driver
