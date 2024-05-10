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

#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/testing/bq_driver_utils/status_utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::odbc_testing_bq_driver_utils::GetLastStatusRecord;

using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

TEST(SetEnvAttr, Success) {
  SQLHENV env_handle;
  SQLUINTEGER val = SQL_CP_ONE_PER_DRIVER;

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS,
            SQLSetEnvAttrInternal(env_handle, SQL_ATTR_CONNECTION_POOLING,
                                  (SQLPOINTER)val, 0));
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(SetEnvAttr, InvalidHandle) {
  SQLUINTEGER val = SQL_CP_ONE_PER_DRIVER;
  EXPECT_EQ(SQL_INVALID_HANDLE,
            SQLSetEnvAttrInternal(nullptr, SQL_ATTR_CONNECTION_POOLING,
                                  (SQLPOINTER)val, 0));
}

TEST(GetEnvAttr, Success) {
  SQLHENV env_handle;
  SQLUINTEGER set_val = SQL_CP_ONE_PER_DRIVER;

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  EXPECT_EQ(SQL_SUCCESS,
            SQLSetEnvAttrInternal(env_handle, SQL_ATTR_CONNECTION_POOLING,
                                  (SQLPOINTER)set_val, 0));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetEnvAttrInternal(env_handle, SQL_ATTR_CONNECTION_POOLING,
                                  &get_val, 0, nullptr));
  EXPECT_EQ(get_val, SQL_CP_ONE_PER_DRIVER);
  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(GetEnvAttr, NullAttributeValue) {
  SQLHENV env_handle;

  EXPECT_EQ(SQL_SUCCESS, SQLAllocEnvHandle(&env_handle));
  StatusRecordOr<EnvironmentHandle*> env_handle_status =
      ValidateEnvironmentHandle(env_handle);
  ASSERT_STATUS_RECORD_OK(env_handle_status);

  EXPECT_EQ(SQL_ERROR,
            SQLGetEnvAttrInternal(env_handle, SQL_ATTR_CONNECTION_POOLING,
                                  nullptr, 0, nullptr));

  StatusRecord status_record = GetLastStatusRecord(*(*env_handle_status));
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
  EXPECT_EQ(status_record.message, "Null attribute value");

  EXPECT_EQ(SQL_SUCCESS, SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle));
}

TEST(GetEnvAttr, InvalidHandle) {
  SQLUINTEGER val = SQL_CP_ONE_PER_DRIVER;
  EXPECT_EQ(SQL_INVALID_HANDLE,
            SQLGetEnvAttrInternal(nullptr, SQL_ATTR_CONNECTION_POOLING, &val, 0,
                                  nullptr));
}
}  // namespace google::cloud::odbc_bq_driver
