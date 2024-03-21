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

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

std::string const kDsnDescription = "test-dsn";
std::string const kDsnCatalog = "bigquery-test";
std::string const kDsnDriver = "test-driver";
std::string const kDsnName = "SampleDSN";

TEST(ConnectionHandle, ConnectWithInvalidFile) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path = test_data_path + "random_file.json";

  Authentication auth = {OauthMechanism::kServiceAccount,
                         credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  StatusRecord status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_FALSE(conn_handle->IsConnected());
  delete conn_handle;
}

TEST(ConnectionHandle, ConnectWithUnImplementedAuth) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path =
      test_data_path + "service_account_auth_keys.json";

  Authentication auth = {OauthMechanism::kExternalUser, credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  StatusRecord status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_FALSE(conn_handle->IsConnected());
  delete conn_handle;
}

TEST(ConnectionHandle, ConnectWithInvalidAuth) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path =
      test_data_path + "service_account_auth_keys.json";

  Authentication auth = {static_cast<OauthMechanism>(7), credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  StatusRecord status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_FALSE(conn_handle->IsConnected());
  delete conn_handle;
}

TEST(ConnectionHandle, DsnSetup) {
  auto* conn_handle = new ConnectionHandle();
  Section dsn_section;
  dsn_section["Description"] = kDsnDescription;
  dsn_section["Driver"] = kDsnDriver;
  dsn_section["Catalog"] = kDsnCatalog;

  conn_handle->SetUp(dsn_section, kDsnName);
  Dsn actual = conn_handle->GetDsn();

  EXPECT_EQ(actual.catalog, kDsnCatalog);
  EXPECT_EQ(actual.driver, kDsnDriver);
  EXPECT_EQ(actual.description, kDsnDescription);
  EXPECT_EQ(actual.dsn_name, kDsnName);
  EXPECT_FALSE(conn_handle->IsConnected());

  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Success_SQLUInt) {
  auto* conn_handle = new ConnectionHandle();

  auto status_record = conn_handle->SetAttribute(
      SQL_ATTR_ACCESS_MODE, (SQLPOINTER)SQL_MODE_READ_ONLY, 0);
  EXPECT_TRUE(status_record.ok());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Success_SQLChar) {
  auto* conn_handle = new ConnectionHandle();

  SQLCHAR buf[256] = "test";
  auto status_record =
      conn_handle->SetAttribute(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)buf, 256);
  EXPECT_TRUE(status_record.ok());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Success_SQLULen) {
  auto* conn_handle = new ConnectionHandle();

  auto status_record = conn_handle->SetAttribute(
      SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)SQL_ASYNC_ENABLE_OFF, 0);
  EXPECT_TRUE(status_record.ok());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Success_SQLIntBitmask) {
  auto* conn_handle = new ConnectionHandle();

  auto status_record = conn_handle->SetAttribute(
      SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TRANSACTION_SERIALIZABLE, 0);
  EXPECT_TRUE(status_record.ok());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Fail_UnsupportedSetAttribute) {
  auto* conn_handle = new ConnectionHandle();
  auto status_record =
      conn_handle->SetAttribute(SQL_ATTR_TRANSLATE_OPTION, (SQLPOINTER)1, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Fail_UnSupportedAttribute) {
  auto* conn_handle = new ConnectionHandle();
  auto status_record =
      conn_handle->SetAttribute(SQL_ATTR_ODBC_CURSORS, (SQLPOINTER)1, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Fail_InvalidAttributeValue) {
  auto* conn_handle = new ConnectionHandle();

  auto status_record =
      conn_handle->SetAttribute(SQL_ATTR_ACCESS_MODE, (SQLPOINTER)2, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Fail_InvalidStringLen) {
  auto* conn_handle = new ConnectionHandle();

  SQLCHAR catalog[256] = "test";
  auto status_record = conn_handle->SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                                                 (SQLPOINTER)catalog, -1);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY090());
  delete conn_handle;
}

TEST(ConnectionHandle, SetAttribute_Fail_InvalidStringValue) {
  auto* conn_handle = new ConnectionHandle();

  auto status_record = conn_handle->SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                                                 (SQLPOINTER) nullptr, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
  delete conn_handle;
}

// TODO(171): Add tests which use refresh token

}  // namespace google::cloud::odbc_bq_driver_internal
