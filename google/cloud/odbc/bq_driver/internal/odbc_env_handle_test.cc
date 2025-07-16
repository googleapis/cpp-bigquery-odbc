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
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/testing/bq_driver_utils/status_utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_testing_bq_driver_utils::GetLastStatusRecord;
using ::google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(EnvAttrConnectionPool, ConnectionPoolDefault) {
  EnvAttrConnectionPool default_val;
  EXPECT_EQ(default_val.Name(), "SQL_CP_OFF");
  EXPECT_EQ(default_val.Value(), SQL_CP_OFF);
}

TEST(EnvAttrConnectionPool, ConnectionPoolCpoff) {
  EnvAttrConnectionPool val(EnvAttrConnectionPoolVal::kCpOff);
  EXPECT_EQ(val.Name(), "SQL_CP_OFF");
  EXPECT_EQ(val.Value(), SQL_CP_OFF);
}

TEST(EnvAttrConnectionPool, ConnectionPoolOneperdriver) {
  EnvAttrConnectionPool val(EnvAttrConnectionPoolVal::kOnePerDriver);
  EXPECT_EQ(val.Name(), "SQL_CP_ONE_PER_DRIVER");
  EXPECT_EQ(val.Value(), SQL_CP_ONE_PER_DRIVER);
}

TEST(EnvAttrConnectionPool, ConnectionPoolOneperhenv) {
  EnvAttrConnectionPool val(EnvAttrConnectionPoolVal::kOnePerHenv);
  EXPECT_EQ(val.Name(), "SQL_CP_ONE_PER_HENV");
  EXPECT_EQ(val.Value(), SQL_CP_ONE_PER_HENV);
}

TEST(EnvAttrConnectionPool, ParseValDefault) {
  SQLUINTEGER val = SQL_CP_DEFAULT;
  auto status = EnvAttrConnectionPool::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::kCpOff);
}

TEST(EnvAttrConnectionPool, ParseValCpoff) {
  SQLUINTEGER val = SQL_CP_OFF;
  auto status = EnvAttrConnectionPool::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::kCpOff);
}

TEST(EnvAttrConnectionPool, ParseValOneperdriver) {
  SQLUINTEGER val = SQL_CP_ONE_PER_DRIVER;
  auto status = EnvAttrConnectionPool::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::kOnePerDriver);
}

TEST(EnvAttrConnectionPool, ParseValOneperhenv) {
  SQLUINTEGER val = SQL_CP_ONE_PER_HENV;
  auto status = EnvAttrConnectionPool::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrConnectionPoolVal::kOnePerHenv);
}

TEST(EnvAttrConnectionPool, ParseValUnsupportedval) {
  SQLUINTEGER val = 12345;
  auto status = EnvAttrConnectionPool::ParseVal((SQLPOINTER)val);
  EXPECT_THAT(
      status,
      StatusRecordIs(
          SQLStates::k_HY024(),
          HasSubstr("Unsupported attribute value for EnvAttrConnectionPool")));
}

TEST(EnvAttrConnectionPoolMatch, ConnectionPoolMatchDefault) {
  EnvAttrConnectionPoolMatch default_val;
  EXPECT_EQ(default_val.Name(), "SQL_CP_STRICT_MATCH");
  EXPECT_EQ(default_val.Value(), SQL_CP_STRICT_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ConnectionPoolStrictMatch) {
  EnvAttrConnectionPoolMatch val(EnvAttrCPMatchVal::kStrictMatch);
  EXPECT_EQ(val.Name(), "SQL_CP_STRICT_MATCH");
  EXPECT_EQ(val.Value(), SQL_CP_STRICT_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ConnectionPoolRelaxedMatch) {
  EnvAttrConnectionPoolMatch val(EnvAttrCPMatchVal::kRelaxedMatch);
  EXPECT_EQ(val.Name(), "SQL_CP_RELAXED_MATCH");
  EXPECT_EQ(val.Value(), SQL_CP_RELAXED_MATCH);
}

TEST(EnvAttrConnectionPoolMatch, ParseValDefault) {
  SQLUINTEGER val = SQL_CP_MATCH_DEFAULT;
  auto status = EnvAttrConnectionPoolMatch::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrCPMatchVal::kStrictMatch);
}

TEST(EnvAttrConnectionPoolMatch, ParseValStrictmatch) {
  SQLUINTEGER val = SQL_CP_STRICT_MATCH;
  auto status = EnvAttrConnectionPoolMatch::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrCPMatchVal::kStrictMatch);
}

TEST(EnvAttrConnectionPoolMatch, ParseValRelaxedmatch) {
  SQLUINTEGER val = SQL_CP_RELAXED_MATCH;
  auto status = EnvAttrConnectionPoolMatch::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrCPMatchVal::kRelaxedMatch);
}

TEST(EnvAttrConnectionPoolMatch, ParseValUnsupportedval) {
  SQLUINTEGER val = 12345;
  auto status = EnvAttrConnectionPoolMatch::ParseVal((SQLPOINTER)val);
  EXPECT_THAT(
      status,
      StatusRecordIs(
          SQLStates::k_HY024(),
          HasSubstr(
              "Unsupported attribute value for EnvAttrConnectionPoolMatch")));
}

TEST(EnvAttrOdbcVersion, OdbcVersDefault) {
  EnvAttrOdbcVersion default_val;
  EXPECT_EQ(default_val.Name(), "SQL_OV_ODBC3");
  EXPECT_EQ(default_val.Value(), SQL_OV_ODBC3);
}

TEST(EnvAttrOdbcVersion, OdbcVers2) {
  EnvAttrOdbcVersion val(EnvAttrOdbcVersVal::kOdbc2);
  EXPECT_EQ(val.Name(), "SQL_OV_ODBC2");
  EXPECT_EQ(val.Value(), SQL_OV_ODBC2);
}

TEST(EnvAttrOdbcVersion, OdbcVers3) {
  EnvAttrOdbcVersion val(EnvAttrOdbcVersVal::kOdbc3);
  EXPECT_EQ(val.Name(), "SQL_OV_ODBC3");
  EXPECT_EQ(val.Value(), SQL_OV_ODBC3);
}

TEST(EnvAttrOdbcVersion, ParseValOdbc2) {
  SQLINTEGER val = SQL_OV_ODBC2;
  auto status = EnvAttrOdbcVersion::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrOdbcVersVal::kOdbc2);
}

TEST(EnvAttrOdbcVersion, ParseValOdbc3) {
  SQLINTEGER val = SQL_OV_ODBC3;
  auto status = EnvAttrOdbcVersion::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(*status, EnvAttrOdbcVersVal::kOdbc3);
}

TEST(EnvAttrOdbcVersion, ParseValUnsupportedval) {
  SQLINTEGER val = -1;
  auto status = EnvAttrOdbcVersion::ParseVal((SQLPOINTER)val);
  EXPECT_THAT(
      status,
      StatusRecordIs(
          SQLStates::k_HY024(),
          HasSubstr("Unsupported attribute value for EnvAttrOdbcVersion")));
}

TEST(EnvAttrOutputNTS, OutputNTSDefault) {
  EnvAttrOutputNTS default_val;
  EXPECT_EQ(default_val.Name(), "SQL_TRUE");
  EXPECT_EQ(default_val.Value(), SQL_TRUE);
}

TEST(EnvAttrOutputNTS, ParseValTrue) {
  SQLINTEGER val = SQL_TRUE;
  auto status = EnvAttrOutputNTS::ParseVal((SQLPOINTER)val);
  ASSERT_STATUS_RECORD_OK(status);
}

TEST(EnvAttrOutputNTS, ParseValUnsupportedval) {
  SQLINTEGER val = SQL_FALSE;
  auto status = EnvAttrOutputNTS::ParseVal((SQLPOINTER)val);
  EXPECT_THAT(
      status,
      StatusRecordIs(
          SQLStates::k_HY024(),
          HasSubstr("Unsupported attribute value for EnvAttrOutputNTS")));
}

TEST(GetSetAttribute, ConnectionPoolDefault) {
  SQLUINTEGER set_val = SQL_CP_DEFAULT;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_OFF);
}

TEST(GetSetAttribute, ConnectionPoolCpoff) {
  SQLUINTEGER set_val = SQL_CP_OFF;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_OFF);
}

TEST(GetSetAttribute, ConnectionPoolOneperdriver) {
  SQLUINTEGER set_val = SQL_CP_ONE_PER_DRIVER;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_ONE_PER_DRIVER);
}

TEST(GetSetAttribute, ConnectionPoolOneperhenv) {
  SQLUINTEGER set_val = SQL_CP_ONE_PER_HENV;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS, handle.GetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                             &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_ONE_PER_HENV);
}

TEST(GetSetAttribute, ConnectionPoolUnsupportedval) {
  SQLUINTEGER val = 12345;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR, handle.SetAttribute(SQL_ATTR_CONNECTION_POOLING,
                                           (SQLPOINTER)val, nullptr));
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
}

TEST(GetSetAttribute, ConnectionPoolDefaultMatch) {
  SQLUINTEGER set_val = SQL_CP_MATCH_DEFAULT;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CP_MATCH,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_STRICT_MATCH);
}

TEST(GetSetAttribute, ConnectionPoolStrictMatch) {
  SQLUINTEGER set_val = SQL_CP_STRICT_MATCH;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CP_MATCH,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_STRICT_MATCH);
}

TEST(GetSetAttribute, ConnectionPoolRelaxedMatch) {
  SQLUINTEGER set_val = SQL_CP_RELAXED_MATCH;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_CP_MATCH,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_CP_MATCH, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_CP_RELAXED_MATCH);
}

TEST(GetSetAttribute, ConnectionPoolMatchUnsupportedVal) {
  SQLUINTEGER val = 12345;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_CP_MATCH, (SQLPOINTER)val, nullptr));
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
}

TEST(GetSetAttribute, ODBCVersionODBC2) {
  SQLINTEGER set_val = SQL_OV_ODBC2;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_ODBC_VERSION,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_ODBC_VERSION, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_OV_ODBC2);
}

TEST(GetSetAttribute, ODBCVersionODBC3) {
  SQLINTEGER set_val = SQL_OV_ODBC3;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_ODBC_VERSION,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_ODBC_VERSION, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_OV_ODBC3);
}

TEST(GetSetAttribute, ODBCVersionInvalidValue) {
  SQLINTEGER val = -1;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR, handle.SetAttribute(SQL_ATTR_ODBC_VERSION,
                                           (SQLPOINTER)val, nullptr));
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
}

TEST(GetSetAttribute, OutputNTSTrue) {
  SQLINTEGER set_val = SQL_TRUE;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_SUCCESS, handle.SetAttribute(SQL_ATTR_OUTPUT_NTS,
                                             (SQLPOINTER)set_val, nullptr));
  SQLUINTEGER get_val;
  EXPECT_EQ(SQL_SUCCESS,
            handle.GetAttribute(SQL_ATTR_OUTPUT_NTS, &get_val, nullptr));
  EXPECT_EQ(get_val, SQL_TRUE);
}

TEST(GetSetAttribute, OutputNTSInvalid) {
  SQLINTEGER val = SQL_FALSE;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR,
            handle.SetAttribute(SQL_ATTR_OUTPUT_NTS, (SQLPOINTER)val, nullptr));
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
}

TEST(GetSetAttribute, InvalidEnvironmentAttribute) {
  SQLINTEGER val = SQL_FALSE;
  EnvironmentHandle handle;
  EXPECT_EQ(SQL_ERROR, handle.SetAttribute(SQL_ATTR_ACCESS_MODE,
                                           (SQLPOINTER)val, nullptr));
  EXPECT_EQ(SQL_ERROR, handle.GetAttribute(SQL_ATTR_ACCESS_MODE,
                                           (SQLPOINTER)val, nullptr));
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
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
