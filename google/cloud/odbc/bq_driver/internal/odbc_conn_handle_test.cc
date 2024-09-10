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
std::string const kDsnDefaultDataset = "bigquery-test-dataset";
std::string const kDsnDriver = "test-driver";
std::string const kDsnName = "SampleDSN";

TEST(ConnectionHandle, ConnectWithInvalidFile) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path = test_data_path + "random_file.json";

  Authentication auth = {OauthMechanism::kServiceAccount,
                         credentials_file_path};
  ConnectionHandle conn_handle;
  StatusRecord status = conn_handle.Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_FALSE(conn_handle.IsConnected());
}

TEST(ConnectionHandle, ConnectWithUnImplementedAuth) {
  Authentication auth = {OauthMechanism::kExternalUser, "path-to-the-file"};
  ConnectionHandle conn_handle;
  StatusRecord status = conn_handle.Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_FALSE(conn_handle.IsConnected());
}

TEST(ConnectionHandle, ConnectWithInvalidAuth) {
  Authentication auth = {static_cast<OauthMechanism>(7), "path-to-the-file"};
  ConnectionHandle conn_handle;
  StatusRecord status = conn_handle.Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_FALSE(conn_handle.IsConnected());
}

TEST(ConnectionHandle, DsnSetup) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["Description"] = kDsnDescription;
  dsn_section["Driver"] = kDsnDriver;
  dsn_section["Catalog"] = kDsnCatalog;
  dsn_section["DefaultDataset"] = kDsnDefaultDataset;
  dsn_section["SQLDialect"] = "0";

  conn_handle.SetUp(dsn_section, kDsnName);
  Dsn actual = conn_handle.GetDsn();

  EXPECT_EQ(actual.catalog, kDsnCatalog);
  EXPECT_EQ(actual.default_dataset, kDsnDefaultDataset);
  EXPECT_EQ(actual.driver, kDsnDriver);
  EXPECT_EQ(actual.description, kDsnDescription);
  EXPECT_EQ(actual.dsn_name, kDsnName);
  EXPECT_TRUE(actual.is_bq_legacy_sql);
  // `is_job_creation_required` is supposed to be false by default
  EXPECT_FALSE(actual.is_job_creation_required);
  EXPECT_FALSE(actual.sessions_enabled);
  EXPECT_FALSE(conn_handle.IsConnected());
}

TEST(ConnectionHandle, DsnSetup_JobCreationRequired) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JobCreationMode"] = "1";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.is_job_creation_required);
}

TEST(ConnectionHandle, DsnSetup_JobCreationDefault) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JobCreationMode"] = "9";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.is_job_creation_required);
}

TEST(ConnectionHandle, DsnSetup_SessionsEnabled_AnyString) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["EnableSession"] = "aaaaaa";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetup_SessionsEnabled_True) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["EnableSession"] = "1";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetup_SessionsEnabled_False) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["EnableSession"] = "0";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetup_SetCurrentCatalog) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["Catalog"] = kDsnCatalog;

  conn_handle.SetUp(dsn_section, kDsnName);

  SQLCHAR buf_out[256];
  auto status =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 256, nullptr);
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val, kDsnCatalog);
}

TEST(ConnectionHandle, DsnSetup_NotSetCurrentCatalog_SetBefore) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["Catalog"] = kDsnCatalog;

  SQLCHAR buf[256] = "test";
  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)buf, 4);
  ASSERT_TRUE(status_record.ok());

  conn_handle.SetUp(dsn_section, kDsnName);

  SQLCHAR buf_out[256];
  auto status =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 256, nullptr);
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val, "test");
}

TEST(ConnectionHandle, DsnSetup_SQLDialect_NotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.is_bq_legacy_sql);
}

TEST(ConnectionHandle, SetAttribute_Success_SQLUInt) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ACCESS_MODE, (SQLPOINTER)SQL_MODE_READ_ONLY, 0);
  EXPECT_TRUE(status_record.ok());
}

TEST(ConnectionHandle, SetAttribute_Success_SQLChar) {
  ConnectionHandle conn_handle;

  SQLCHAR buf[256] = "test";
  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)buf, 4);
  EXPECT_TRUE(status_record.ok());
}

TEST(ConnectionHandle, SetAttribute_Success_SQLULen) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)SQL_ASYNC_ENABLE_OFF, 0);
  EXPECT_TRUE(status_record.ok());
}

TEST(ConnectionHandle, SetAttribute_Success_SQLIntBitmask) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TRANSACTION_SERIALIZABLE, 0);
  EXPECT_TRUE(status_record.ok());
}

TEST(ConnectionHandle, SetAttribute_Fail_UnsupportedSetAttribute) {
  ConnectionHandle conn_handle;
  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_TRANSLATE_OPTION, (SQLPOINTER)1, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
}

TEST(ConnectionHandle, SetAttribute_Fail_UnSupportedAttribute) {
  ConnectionHandle conn_handle;
  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_ODBC_CURSORS, (SQLPOINTER)1, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
}

TEST(ConnectionHandle, SetAttribute_Fail_InvalidAttributeValue) {
  ConnectionHandle conn_handle;

  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_ACCESS_MODE, (SQLPOINTER)2, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
}

TEST(ConnectionHandle, SetAttribute_Fail_NegativeStringLen) {
  ConnectionHandle conn_handle;

  SQLCHAR catalog[256] = "test";
  auto status_record = conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                                                (SQLPOINTER)catalog, -1);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY090());
}

TEST(ConnectionHandle, SetAttribute_Fail_InvalidStringLen) {
  ConnectionHandle conn_handle;

  SQLCHAR catalog[256] = "test";
  auto status_record = conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                                                (SQLPOINTER)catalog, 2);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY090());
}

TEST(ConnectionHandle, SetAttribute_Fail_InvalidStringValue) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                                                (SQLPOINTER) nullptr, 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY009());
}

TEST(ConnectionHandle, GetAttribute_Fail_UnsupportedGetAttribute) {
  ConnectionHandle conn_handle;
  SQLULEN val;
  SQLINTEGER str_len;
  auto status_record =
      conn_handle.GetAttribute(SQL_ATTR_ODBC_CURSORS, &val, 0, &str_len);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
}

TEST(ConnectionHandle, GetAttribute_Fail_InvalidConnectionBehavior) {
  ConnectionHandle conn_handle;
  SQLUINTEGER val;
  SQLINTEGER str_len;
  auto status_record =
      conn_handle.GetAttribute(SQL_ATTR_CONNECTION_DEAD, &val, 0, &str_len);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_08003());
}

TEST(ConnectionHandle, GetAttribute_Success_SQLUInteger) {
  ConnectionHandle conn_handle;
  SQLUINTEGER val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLUINTEGER);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ACCESS_MODE, (SQLPOINTER)SQL_MODE_READ_ONLY, 0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_ACCESS_MODE, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_MODE_READ_ONLY);
  EXPECT_EQ(str_len, expected_len);
}

TEST(ConnectionHandle, GetAttribute_Success_SQLULEN) {
  ConnectionHandle conn_handle;
  SQLULEN val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLULEN);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)SQL_ASYNC_ENABLE_OFF, 0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_ASYNC_ENABLE_OFF);
  EXPECT_EQ(str_len, expected_len);
}

TEST(ConnectionHandle, GetAttribute_Success_SQLIntBitmask) {
  ConnectionHandle conn_handle;
  SQLINTEGER val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLINTEGER);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TRANSACTION_SERIALIZABLE, 0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_TXN_ISOLATION, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_TRANSACTION_SERIALIZABLE);
  EXPECT_EQ(str_len, expected_len);
}

TEST(ConnectionHandle, GetAttribute_IsolationLevel_GetOnlySupportedOne) {
  ConnectionHandle conn_handle;
  SQLINTEGER val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLINTEGER);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_TXN_ISOLATION, (SQLPOINTER)SQL_TRANSACTION_READ_COMMITTED, 0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_TXN_ISOLATION, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_TRANSACTION_SERIALIZABLE);
  EXPECT_EQ(str_len, expected_len);
}

TEST(ConnectionHandle, GetAttribute_Success_SQLChar_DestBufferGT) {
  ConnectionHandle conn_handle;
  SQLCHAR buf_in[256] = "test";
  SQLCHAR buf_out[256];
  SQLINTEGER str_len;
  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)buf_in, 4);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 5, &str_len);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val, "test");
  EXPECT_EQ(str_len, 4);

  // Parity with Simba Driver:
  // Modifying the input values should have no effect on the attribute stored.
  buf_in[0] = 'a';

  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 5, &str_len);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val2(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val2, "test");
  EXPECT_EQ(str_len, 4);

  buf_in[0] = 'a';
  status_record =
      conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)buf_in, 4);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 5, &str_len);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val3(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val3, "aest");
  EXPECT_EQ(str_len, 4);
}

TEST(ConnectionHandle, GetAttribute_Success_SQLChar_DestBufferSmaller) {
  ConnectionHandle conn_handle;
  SQLCHAR buf_in[256] = "test";
  SQLCHAR buf_out[256];
  SQLINTEGER str_len;
  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)buf_in, 4);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 3, &str_len);
  EXPECT_FALSE(status_record.ok());
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(status_record.sql_state, SQLStates::k_01004());
  EXPECT_EQ(actual_val, "te");
}

TEST(ConnectionHandle, GetAttribute_Success_SQLChar_DestBufferEQ) {
  ConnectionHandle conn_handle;
  SQLCHAR buf_in[256] = "test";
  SQLCHAR buf_out[256];
  SQLINTEGER str_len;
  auto status_record =
      conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG, (SQLPOINTER)buf_in, 4);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 4, &str_len);
  EXPECT_FALSE(status_record.ok());
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(status_record.sql_state, SQLStates::k_01004());
  EXPECT_EQ(actual_val, "tes");
}

TEST(ConnectionHandle, SetAttribute_SetTwice) {
  ConnectionHandle conn_handle;

  SQLCHAR buf_in[256] = "test";
  auto status_record = conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                                                (SQLPOINTER)buf_in, SQL_NTS);
  EXPECT_TRUE(status_record.ok());

  SQLCHAR buf_out[256];
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 256, nullptr);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val, "test");

  SQLCHAR buf_in_2[256] = "test_2";
  status_record = conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                                           (SQLPOINTER)buf_in_2, SQL_NTS);
  EXPECT_TRUE(status_record.ok());

  SQLCHAR buf_out_2[256];
  status_record = conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out_2,
                                           256, nullptr);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val_2(reinterpret_cast<char*>(buf_out_2));
  EXPECT_EQ(actual_val_2, "test_2");
}

// TODO(171): Add tests which use refresh token

}  // namespace google::cloud::odbc_bq_driver_internal
