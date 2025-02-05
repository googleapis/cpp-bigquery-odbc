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
std::string const kDsnListProjectsParent = "TestListProjectsParent";
std::string const kEmail = "a@b.com";
std::string const kRefreshToken = "test-token";
// BYOID Properties

std::string const kAudienceUrl = "test-aud";
std::string const kCredsSource = "~/workload/tkn.txt";
std::string const kTokenUrl = "https://test-token-url";
std::string const kSubTokenType = kSubTokenTypeIdToken;
std::string const kUserPoolProject = "test-project";

TEST(ConnectionHandle, ConnectWithInvalidFile) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path = test_data_path + "random_file.json";

  Authentication auth = {
      {OauthMechanism::kServiceAndUserAccount, credentials_file_path}};
  ConnectionHandle conn_handle;
  StatusRecord status = conn_handle.Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_FALSE(conn_handle.IsConnected());
}

TEST(ConnectionHandle, ConnectWithUnImplementedAuth) {
  Authentication auth = {{OauthMechanism::kExternalUser, "path-to-the-file"}};
  ConnectionHandle conn_handle;
  StatusRecord status = conn_handle.Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_FALSE(conn_handle.IsConnected());
}

TEST(ConnectionHandle, ConnectWithInvalidAuth) {
  Authentication auth = {{static_cast<OauthMechanism>(7), "path-to-the-file"}};
  ConnectionHandle conn_handle;
  StatusRecord status = conn_handle.Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_FALSE(conn_handle.IsConnected());
}

TEST(ConnectionHandle, DsnSetup) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["DESCRIPTION"] = kDsnDescription;
  dsn_section["DRIVER"] = kDsnDriver;
  dsn_section["CATALOG"] = kDsnCatalog;
  dsn_section["LISTPROJECTSPARENT"] = kDsnListProjectsParent;
  dsn_section["DEFAULTDATASET"] = kDsnDefaultDataset;
  dsn_section["SQLDIALECT"] = "0";
  dsn_section["EMAIL"] = kEmail;
  dsn_section["REFRESHTOKEN"] = kRefreshToken;

  conn_handle.SetUp(dsn_section, kDsnName);
  Dsn actual = conn_handle.GetDsn();

  EXPECT_EQ(actual.catalog, kDsnCatalog);
  EXPECT_EQ(actual.default_dataset, kDsnDefaultDataset);
  EXPECT_EQ(actual.driver, kDsnDriver);
  EXPECT_EQ(actual.description, kDsnDescription);
  EXPECT_EQ(actual.list_projects_parent, kDsnListProjectsParent);
  EXPECT_EQ(actual.dsn_name, kDsnName);
  EXPECT_EQ(actual.email, kEmail);
  EXPECT_EQ(actual.refresh_token, kRefreshToken);
  EXPECT_TRUE(actual.is_bq_legacy_sql);
  // `is_job_creation_required` is supposed to be false by default
  EXPECT_FALSE(actual.is_job_creation_required);
  EXPECT_FALSE(actual.sessions_enabled);
  EXPECT_FALSE(conn_handle.IsConnected());
  // Aseert BYOID Default Property values.
  EXPECT_EQ(actual.byoid_subj_token_type, kSubTokenTypeDefault);
  EXPECT_EQ(actual.byoid_token_url, kDefaultTokenUrl);
}

TEST(ConnectionHandle, DsnSetup_BYOID) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AudienceUrl"] = kAudienceUrl;
  dsn_section["BYOID_CredentialSource"] = kCredsSource;
  dsn_section["BYOID_PoolUserProject"] = kUserPoolProject;
  dsn_section["BYOID_SubjectTokenType"] = kSubTokenType;
  dsn_section["BYOID_TokenUrl"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  Dsn actual = conn_handle.GetDsn();

  // `is_job_creation_required` is supposed to be false by default
  EXPECT_FALSE(actual.is_job_creation_required);
  EXPECT_FALSE(actual.sessions_enabled);
  EXPECT_FALSE(conn_handle.IsConnected());
  // BYOID Properties
  EXPECT_EQ(actual.byoid_aud_url, kAudienceUrl);
  EXPECT_EQ(actual.byoid_creds_src, kCredsSource);
  EXPECT_EQ(actual.byoid_pool_user_project, kUserPoolProject);
  EXPECT_EQ(actual.byoid_subj_token_type, kSubTokenType);
  EXPECT_EQ(actual.byoid_token_url, kTokenUrl);
}

TEST(ConnectionHandle, IsBYOIDPropertiesSet_True_AllPropertiesSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AudienceUrl"] = kAudienceUrl;
  dsn_section["BYOID_CredentialSource"] = kCredsSource;
  dsn_section["BYOID_SubjectTokenType"] = kSubTokenType;

  conn_handle.SetUp(dsn_section, kDsnName);

  EXPECT_TRUE(conn_handle.IsBYOIDPropertiesSet());
}

TEST(ConnectionHandle, IsBYOIDPropertiesSet_True_NoPropertiesSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  conn_handle.SetUp(dsn_section, kDsnName);
  EXPECT_TRUE(
      conn_handle
          .IsBYOIDPropertiesSet());  // default value for subject token type.
}

TEST(ConnectionHandle, IsBYOIDPropertiesSet_True_PartialPropertiesSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AudienceUrl"] = kAudienceUrl;
  dsn_section["BYOID_CredentialSource"] = kCredsSource;
  dsn_section["BYOID_PoolUserProject"] = kUserPoolProject;
  dsn_section["BYOID_TokenUrl"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  EXPECT_TRUE(conn_handle.IsBYOIDPropertiesSet());
}

TEST(ConnectionHandle, ValidateBYOIDProperties_Success) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AudienceUrl"] = kAudienceUrl;
  dsn_section["BYOID_CredentialSource"] = kCredsSource;
  dsn_section["BYOID_PoolUserProject"] = kUserPoolProject;
  dsn_section["BYOID_SubjectTokenType"] = kSubTokenType;
  dsn_section["BYOID_TokenUrl"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  EXPECT_TRUE(conn_handle.ValidateBYOIDProperties().ok());
}

TEST(ConnectionHandle, ValidateBYOIDProperties_Fail_AudienceNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_CredentialSource"] = kCredsSource;
  dsn_section["BYOID_PoolUserProject"] = kUserPoolProject;
  dsn_section["BYOID_SubjectTokenType"] = kSubTokenType;
  dsn_section["BYOID_TokenUrl"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateBYOIDProperties();

  EXPECT_EQ(result.message, "Required BYOID properties not set");
}

TEST(ConnectionHandle, ValidateBYOIDProperties_Fail_CredSrcNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AudienceUrl"] = kAudienceUrl;
  dsn_section["BYOID_PoolUserProject"] = kUserPoolProject;
  dsn_section["BYOID_SubjectTokenType"] = kSubTokenType;
  dsn_section["BYOID_TokenUrl"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateBYOIDProperties();

  EXPECT_EQ(result.message, "Required BYOID properties not set");
}

TEST(ConnectionHandle, ValidateBYOIDProperties_Success_SubTokenTypeNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AudienceUrl"] = kAudienceUrl;
  dsn_section["BYOID_CredentialSource"] = kCredsSource;
  dsn_section["BYOID_PoolUserProject"] = kUserPoolProject;
  dsn_section["BYOID_TokenUrl"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateBYOIDProperties();
  EXPECT_TRUE(result.ok());
}

TEST(ConnectionHandle, ValidateBYOIDProperties_Fail_InvalidSubTokenType) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AudienceUrl"] = kAudienceUrl;
  dsn_section["BYOID_CredentialSource"] = kCredsSource;
  dsn_section["BYOID_PoolUserProject"] = kUserPoolProject;
  dsn_section["BYOID_SubjectTokenType"] = "invalid";
  dsn_section["BYOID_TokenUrl"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateBYOIDProperties();
  EXPECT_EQ(result.message, "Invalid subject token type");
}

TEST(ConnectionHandle, DsnSetup_JobCreationRequired) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JOBCREATIONMODE"] = "1";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.is_job_creation_required);
}

TEST(ConnectionHandle, DsnSetup_JobCreationDefault) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JOBCREATIONMODE"] = "9";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.is_job_creation_required);
}

TEST(ConnectionHandle, DsnSetup_SessionsEnabled_AnyString) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["ENABLESESSION"] = "aaaaaa";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetup_SessionsEnabled_True) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["ENABLESESSION"] = "1";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetup_SessionsEnabled_False) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["ENABLESESSION"] = "0";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetup_SetCurrentCatalog) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["CATALOG"] = kDsnCatalog;

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
  dsn_section["CATALOG"] = kDsnCatalog;

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

TEST(ConnectionHandle, DsnSetup_ListProjectsParent_NotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.list_projects_parent.empty());
}

TEST(ConnectionHandle, DsnSetup_ListProjectsParent_Set) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  dsn_section["LISTPROJECTSPARENT"] = kDsnListProjectsParent;
  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_EQ(actual.list_projects_parent, kDsnListProjectsParent);
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

}  // namespace google::cloud::odbc_bq_driver_internal
