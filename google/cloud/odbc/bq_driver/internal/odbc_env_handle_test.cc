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

#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

TEST(EnvAttrConnectionPool, ConnectionPoolDefault) {
  EnvAttrConnectionPool default_val;
  EXPECT_EQ(default_val.Name(), "SQL_CP_OFF");
  EXPECT_EQ(default_val.Value(), SQL_CP_OFF);
}

TEST(EnvAttrConnectionPool, ConnectionPool_CPOff) {
  EnvAttrConnectionPool val(EnvAttrConnectionPoolVal::CP_OFF);
  EXPECT_EQ(val.Name(), "SQL_CP_OFF");
  EXPECT_EQ(val.Value(), SQL_CP_OFF);
}

TEST(EnvAttrConnectionPool, ConnectionPool_OnePerDriver) {
  EnvAttrConnectionPool val(EnvAttrConnectionPoolVal::ONE_PER_DRIVER);
  EXPECT_EQ(val.Name(), "SQL_CP_ONE_PER_DRIVER");
  EXPECT_EQ(val.Value(), SQL_CP_ONE_PER_DRIVER);
}

TEST(EnvAttrConnectionPool, ConnectionPool_OnePerHEnv) {
  EnvAttrConnectionPool val(EnvAttrConnectionPoolVal::ONE_PER_HENV);
  EXPECT_EQ(val.Name(), "SQL_CP_ONE_PER_HENV");
  EXPECT_EQ(val.Value(), SQL_CP_ONE_PER_HENV);
}

TEST(EnvAttrConnectionPool, ParseVal_Default) {
  SQLUINTEGER val = SQL_CP_DEFAULT;
  auto status = EnvAttrConnectionPool::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::CP_OFF);
}

TEST(EnvAttrConnectionPool, ParseVal_CPOff) {
  SQLUINTEGER val = SQL_CP_OFF;
  auto status = EnvAttrConnectionPool::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::CP_OFF);
}

TEST(EnvAttrConnectionPool, ParseVal_OnePerDriver) {
  SQLUINTEGER val = SQL_CP_ONE_PER_DRIVER;
  auto status = EnvAttrConnectionPool::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::ONE_PER_DRIVER);
}

TEST(EnvAttrConnectionPool, ParseVal_OnePerHenv) {
  SQLUINTEGER val = SQL_CP_ONE_PER_HENV;
  auto status = EnvAttrConnectionPool::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::ONE_PER_HENV);
}

TEST(EnvAttrConnectionPool, ParseVal_NullVal) {
  auto status = EnvAttrConnectionPool::ParseVal(nullptr);
  EXPECT_THAT(
      status,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("Invalid null attribute value for EnvAttrConnectionPool")));
}

TEST(EnvAttrConnectionPool, ParseVal_UnsupportedVal) {
  SQLUINTEGER val = 12345;
  auto status = EnvAttrConnectionPool::ParseVal(&val);
  EXPECT_THAT(
      status,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("Unsupported attribute value for EnvAttrConnectionPool")));
}

TEST(EnvAttrConnectionPoolMatch, ConnectionPoolMatchDefault) {
  EnvAttrConnectionPoolMatch default_val;
  EXPECT_EQ(default_val.Name(), "SQL_CP_STRICT_MATCH");
  EXPECT_EQ(default_val.Value(), SQL_CP_STRICT_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ConnectionPoolStrictMatch) {
  EnvAttrConnectionPoolMatch val(EnvAttrCPMatchVal::STRICT_MATCH);
  EXPECT_EQ(val.Name(), "SQL_CP_STRICT_MATCH");
  EXPECT_EQ(val.Value(), SQL_CP_STRICT_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ConnectionPoolRelaxedMatch) {
  EnvAttrConnectionPoolMatch val(EnvAttrCPMatchVal::RELAXED_MATCH);
  EXPECT_EQ(val.Name(), "SQL_CP_RELAXED_MATCH");
  EXPECT_EQ(val.Value(), SQL_CP_RELAXED_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ParseVal_Default) {
  SQLUINTEGER val = SQL_CP_MATCH_DEFAULT;
  auto status = EnvAttrConnectionPoolMatch::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrCPMatchVal::STRICT_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ParseVal_StrictMatch) {
  SQLUINTEGER val = SQL_CP_STRICT_MATCH;
  auto status = EnvAttrConnectionPoolMatch::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrCPMatchVal::STRICT_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ParseVal_RelaxedMatch) {
  SQLUINTEGER val = SQL_CP_RELAXED_MATCH;
  auto status = EnvAttrConnectionPoolMatch::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrCPMatchVal::RELAXED_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ParseVal_NullVal) {
  auto status = EnvAttrConnectionPoolMatch::ParseVal(nullptr);
  EXPECT_THAT(
      status,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "Invalid null attribute value for EnvAttrConnectionPoolMatch")));
}

TEST(EnvAttrConnectionPoolMatch, ParseVal_UnsupportedVal) {
  SQLUINTEGER val = 12345;
  auto status = EnvAttrConnectionPoolMatch::ParseVal(&val);
  EXPECT_THAT(
      status,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "Unsupported attribute value for EnvAttrConnectionPoolMatch")));
}

TEST(EnvAttrOdbcVersion, OdbcVersDefault) {
  EnvAttrOdbcVersion default_val;
  EXPECT_EQ(default_val.Name(), "SQL_OV_ODBC3");
  EXPECT_EQ(default_val.Value(), SQL_OV_ODBC3);
}

TEST(EnvAttrOdbcVersion, OdbcVers2) {
  EnvAttrOdbcVersion val(EnvAttrOdbcVersVal::ODBC_2);
  EXPECT_EQ(val.Name(), "SQL_OV_ODBC2");
  EXPECT_EQ(val.Value(), SQL_OV_ODBC2);
}

TEST(EnvAttrOdbcVersion, OdbcVers3) {
  EnvAttrOdbcVersion val(EnvAttrOdbcVersVal::ODBC_3);
  EXPECT_EQ(val.Name(), "SQL_OV_ODBC3");
  EXPECT_EQ(val.Value(), SQL_OV_ODBC3);
}

TEST(EnvAttrOdbcVersion, ParseVal_ODBC2) {
  SQLINTEGER val = SQL_OV_ODBC2;
  auto status = EnvAttrOdbcVersion::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrOdbcVersVal::ODBC_2);
}

TEST(EnvAttrOdbcVersion, ParseVal_ODBC3) {
  SQLINTEGER val = SQL_OV_ODBC3;
  auto status = EnvAttrOdbcVersion::ParseVal(&val);
  ASSERT_STATUS_OK(status);
  EXPECT_EQ(*status, EnvAttrOdbcVersVal::ODBC_3);
}

TEST(EnvAttrOdbcVersion, ParseVal_NullVal) {
  auto status = EnvAttrOdbcVersion::ParseVal(nullptr);
  EXPECT_THAT(
      status,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("Invalid null attribute value for EnvAttrOdbcVersion")));
}

TEST(EnvAttrOdbcVersion, ParseVal_UnsupportedVal) {
  SQLINTEGER val = -1;
  auto status = EnvAttrOdbcVersion::ParseVal(&val);
  EXPECT_THAT(
      status,
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("Unsupported attribute value for EnvAttrOdbcVersion")));
}

TEST(EnvAttrOutputNTS, OutputNTSDefault) {
  EnvAttrOutputNTS default_val;
  EXPECT_EQ(default_val.Name(), "SQL_TRUE");
  EXPECT_EQ(default_val.Value(), SQL_TRUE);
}

TEST(EnvAttrOutputNTS, ParseVal_True) {
  SQLINTEGER val = SQL_TRUE;
  auto status = EnvAttrOutputNTS::ParseVal(&val);
  EXPECT_EQ(status.code(), StatusCode::kOk);
}

TEST(EnvAttrOutputNTS, ParseVal_NullVal) {
  auto status = EnvAttrOutputNTS::ParseVal(nullptr);
  EXPECT_EQ(status.code(), StatusCode::kInvalidArgument);
}

TEST(EnvAttrOutputNTS, ParseVal_UnsupportedVal) {
  SQLINTEGER val = SQL_FALSE;
  auto status = EnvAttrOutputNTS::ParseVal(&val);
  EXPECT_EQ(status.code(), StatusCode::kInvalidArgument);
}

TEST(GetSetAttribute, ConnectionPool_Default) {
  SQLUINTEGER set_val = SQL_CP_DEFAULT;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_OFF);
}

TEST(GetSetAttribute, ConnectionPool_CPOff) {
  SQLUINTEGER set_val = SQL_CP_OFF;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_OFF);
}

TEST(GetSetAttribute, ConnectionPool_OnePerDriver) {
  SQLUINTEGER set_val = SQL_CP_ONE_PER_DRIVER;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_ONE_PER_DRIVER);
}

TEST(GetSetAttribute, ConnectionPool_OnePerHenv) {
  SQLUINTEGER set_val = SQL_CP_ONE_PER_HENV;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_ONE_PER_HENV);
}

TEST(GetSetAttribute, ConnectionPool_Null) {
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING, nullptr, nullptr));
  EXPECT_EQ(SQL_ERROR,
            handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING, nullptr, nullptr));
}

TEST(GetSetAttribute, ConnectionPool_UnsupportedVal) {
  SQLUINTEGER val = 12345;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING, &val, nullptr));
}

TEST(GetSetAttribute, ConnectionPoolDefaultMatch) {
  SQLUINTEGER set_val = SQL_CP_MATCH_DEFAULT;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS,
            handle.SetAttribute(SQL_ATTR_CP_MATCH, &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_STRICT_MATCH);
}

TEST(GetSetAttribute, ConnectionPoolStrictMatch) {
  SQLUINTEGER set_val = SQL_CP_STRICT_MATCH;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS,
            handle.SetAttribute(SQL_ATTR_CP_MATCH, &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_STRICT_MATCH);
}

TEST(GetSetAttribute, ConnectionPoolRelaxedMatch) {
  SQLUINTEGER set_val = SQL_CP_RELAXED_MATCH;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS,
            handle.SetAttribute(SQL_ATTR_CP_MATCH, &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_RELAXED_MATCH);
}

TEST(GetSetAttribute, ConnectionPoolMatchNullPtr) {
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_CP_MATCH, nullptr, nullptr));
  EXPECT_EQ(SQL_ERROR,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, nullptr, nullptr));
}

TEST(GetSetAttribute, ConnectionPoolMatchUnsupportedVal) {
  SQLUINTEGER val = 12345;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR, handle.SetAttribute(SQL_ATTR_CP_MATCH, &val, nullptr));
}

TEST(GetSetAttribute, ODBCVersionODBC2) {
  SQLINTEGER set_val = SQL_OV_ODBC2;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS,
            handle.SetAttribute(SQL_ATTR_ODBC_VERSION, &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_ODBC_VERSION, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_OV_ODBC2);
}

TEST(GetSetAttribute, ODBCVersionODBC3) {
  SQLINTEGER set_val = SQL_OV_ODBC3;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS,
            handle.SetAttribute(SQL_ATTR_ODBC_VERSION, &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_ODBC_VERSION, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_OV_ODBC3);
}

TEST(GetSetAttribute, ODBCVersionNull) {
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_ODBC_VERSION, nullptr, nullptr));
  EXPECT_EQ(SQL_ERROR,
            handle.GetAttribute(SQL_ATTR_ODBC_VERSION, nullptr, nullptr));
}

TEST(GetSetAttribute, ODBCVersionInvalidValue) {
  SQLINTEGER val = -1;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_ODBC_VERSION, &val, nullptr));
}

TEST(GetSetAttribute, OutputNTSTrue) {
  SQLINTEGER set_val = SQL_TRUE;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS,
            handle.SetAttribute(SQL_ATTR_OUTPUT_NTS, &set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_OUTPUT_NTS, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_TRUE);
}

TEST(GetSetAttribute, OutputNTSNull) {
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_OUTPUT_NTS, nullptr, nullptr));
  EXPECT_EQ(SQL_ERROR,
            handle.GetAttribute(SQL_ATTR_OUTPUT_NTS, nullptr, nullptr));
}

TEST(GetSetAttribute, OutputNTSInvalid) {
  SQLINTEGER val = SQL_FALSE;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR, handle.SetAttribute(SQL_ATTR_OUTPUT_NTS, &val, nullptr));
}

TEST(GetSetAttribute, InvalidEnvironmentAttribute) {
  SQLINTEGER val = SQL_FALSE;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_ACCESS_MODE, &val, nullptr));
  EXPECT_EQ(SQL_ERROR,
            handle.GetAttribute(SQL_ATTR_ACCESS_MODE, &val, nullptr));
}

TEST(GetSetAttribute, DefaultValues) {
  EnvironmentHandle handle;
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_OFF);
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_STRICT_MATCH);
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_ODBC_VERSION, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_OV_ODBC3);
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_OUTPUT_NTS, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_TRUE);
}

}  // namespace google::cloud::odbc_bq_driver_internal
