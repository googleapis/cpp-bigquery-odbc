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

using google::cloud::odbc_bigquery_client_interface::kDefaultTokenUrl;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeDefault;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeIdToken;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
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

TEST(ConnectionHandle, DsnSetupHtapiDefaultValues) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  conn_handle.SetUp(dsn_section, kDsnName);
  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.use_default_large_results_dataset);
  EXPECT_FALSE(actual.allow_htapi);
}

TEST(ConnectionHandle, DsnSetupHtapiBasic) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["USEDEFAULTLARGERESULTSDATASET"] = "0";
  dsn_section["LARGERESULTSDATASETID"] = "large_dataset";
  dsn_section["ALLOWHTAPIFORLARGERESULTS"] = "1";
  dsn_section["HTAPI_ACTIVATIONTHRESHOLD"] = "4";
  dsn_section["LARGERESULTSTEMPTABLEEXPIRATIONTIME"] = "36000";

  conn_handle.SetUp(dsn_section, kDsnName);
  Dsn actual = conn_handle.GetDsn();

  EXPECT_FALSE(actual.use_default_large_results_dataset);
  EXPECT_EQ(actual.large_results_dataset_id, "large_dataset");
  EXPECT_TRUE(actual.allow_htapi);
  EXPECT_EQ(actual.htapi_activation_threshold, "4");
  EXPECT_EQ(actual.large_table_expiration_time, "36000");
}

TEST(ConnectionHandle, DsnSetupByoid) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AUDIENCEURL"] = kAudienceUrl;
  dsn_section["BYOID_CREDENTIALSOURCE"] = kCredsSource;
  dsn_section["BYOID_POOLUSERPROJECT"] = kUserPoolProject;
  dsn_section["BYOID_SUBJECTTOKENTYPE"] = kSubTokenType;
  dsn_section["BYOID_TOKENURL"] = kTokenUrl;

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

TEST(ConnectionHandle, IsByoidPropertiesSetTrueAllPropertiesSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AUDIENCEURL"] = kAudienceUrl;
  dsn_section["BYOID_CREDENTIALSOURCE"] = kCredsSource;
  dsn_section["BYOID_SUBJECTTOKENTYPE"] = kSubTokenType;

  conn_handle.SetUp(dsn_section, kDsnName);

  EXPECT_TRUE(conn_handle.IsDsnBYOIDPropertiesSet());
}

TEST(ConnectionHandle, IsByoidPropertiesSetTrueNoPropertiesSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  conn_handle.SetUp(dsn_section, kDsnName);
  EXPECT_TRUE(
      conn_handle
          .IsDsnBYOIDPropertiesSet());  // default value for subject token type.
}

TEST(ConnectionHandle, IsByoidPropertiesSetTruePartialPropertiesSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AUDIENCEURL"] = kAudienceUrl;
  dsn_section["BYOID_CREDENTIALSOURCE"] = kCredsSource;
  dsn_section["BYOID_POOLUSERPROJECT"] = kUserPoolProject;
  dsn_section["BYOID_TOKENURL"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  EXPECT_TRUE(conn_handle.IsDsnBYOIDPropertiesSet());
}

TEST(ConnectionHandle, ValidateBYOIDPropertiesSuccess) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AUDIENCEURL"] = kAudienceUrl;
  dsn_section["BYOID_CREDENTIALSOURCE"] = kCredsSource;
  dsn_section["BYOID_POOLUSERPROJECT"] = kUserPoolProject;
  dsn_section["BYOID_SUBJECTTOKENTYPE"] = kSubTokenType;
  dsn_section["BYOID_TOKENURL"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  EXPECT_TRUE(conn_handle.ValidateDsnBYOIDProperties().ok());
}

TEST(ConnectionHandle, ValidateBYOIDPropertiesFailAudienceNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_CREDENTIALSOURCE"] = kCredsSource;
  dsn_section["BYOID_POOLUSERPROJECT"] = kUserPoolProject;
  dsn_section["BYOID_SUBJECTTOKENTYPE"] = kSubTokenType;
  dsn_section["BYOID_TOKENURL"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateDsnBYOIDProperties();

  EXPECT_EQ(result.message, "Required BYOID properties not set");
}

TEST(ConnectionHandle, ValidateBYOIDPropertiesFailCredSrcNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AUDIENCEURL"] = kAudienceUrl;
  dsn_section["BYOID_POOLUSERPROJECT"] = kUserPoolProject;
  dsn_section["BYOID_SUBJECTTOKENTYPE"] = kSubTokenType;
  dsn_section["BYOID_TOKENURL"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateDsnBYOIDProperties();

  EXPECT_EQ(result.message, "Required BYOID properties not set");
}

TEST(ConnectionHandle, ValidateBYOIDPropertiesSuccessSubTokenTypeNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AUDIENCEURL"] = kAudienceUrl;
  dsn_section["BYOID_CREDENTIALSOURCE"] = kCredsSource;
  dsn_section["BYOID_POOLUSERPROJECT"] = kUserPoolProject;
  dsn_section["BYOID_TOKENURL"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateDsnBYOIDProperties();
  EXPECT_TRUE(result.ok());
}

TEST(ConnectionHandle, ValidateBYOIDPropertiesFailInvalidSubTokenType) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  // BYOID Properties
  dsn_section["BYOID_AUDIENCEURL"] = kAudienceUrl;
  dsn_section["BYOID_CREDENTIALSOURCE"] = kCredsSource;
  dsn_section["BYOID_POOLUSERPROJECT"] = kUserPoolProject;
  dsn_section["BYOID_SUBJECTTOKENTYPE"] = "invalid";
  dsn_section["BYOID_TOKENURL"] = kTokenUrl;

  conn_handle.SetUp(dsn_section, kDsnName);
  auto result = conn_handle.ValidateDsnBYOIDProperties();
  EXPECT_EQ(result.message, "Invalid subject token type");
}

TEST(ConnectionHandle, DsnSetupJobCreationRequired) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JOBCREATIONMODE"] = "1";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.is_job_creation_required);
}

TEST(ConnectionHandle, DsnSetupJobCreationDefault) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JOBCREATIONMODE"] = "9";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.is_job_creation_required);
}

TEST(ConnectionHandle, DsnSetupSessionsEnabledAnyString) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["ENABLESESSION"] = "aaaaaa";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetupSessionsEnabledTrue) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["ENABLESESSION"] = "1";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetupSessionsEnabledFalse) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["ENABLESESSION"] = "0";

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.sessions_enabled);
}

TEST(ConnectionHandle, DsnSetupSetCurrentCatalog) {
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

TEST(ConnectionHandle, DsnSetupNotSetCurrentCatalogSetBefore) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["CATALOG"] = kDsnCatalog;

  SQLCHAR buf[256] = "test";
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(buf), 4);
  ASSERT_TRUE(status_record.ok());

  conn_handle.SetUp(dsn_section, kDsnName);

  SQLCHAR buf_out[256];
  auto status =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 256, nullptr);
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val, "test");
}

TEST(ConnectionHandle, DsnSetupSQLDialectNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_FALSE(actual.is_bq_legacy_sql);
}

TEST(ConnectionHandle, DsnSetupListProjectsParentNotSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_TRUE(actual.list_projects_parent.empty());
}

TEST(ConnectionHandle, DsnSetupListProjectsParentSet) {
  ConnectionHandle conn_handle;
  Section dsn_section;

  dsn_section["LISTPROJECTSPARENT"] = kDsnListProjectsParent;
  conn_handle.SetUp(dsn_section, kDsnName);

  Dsn actual = conn_handle.GetDsn();
  EXPECT_EQ(actual.list_projects_parent, kDsnListProjectsParent);
}

TEST(ConnectionHandle, SetAttributeSuccessSQLUInt) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ACCESS_MODE, reinterpret_cast<SQLPOINTER>(SQL_MODE_READ_ONLY),
      0);
  EXPECT_TRUE(status_record.ok());
}
TEST(ConnectionHandle, SetAttributeSuccessSQLChar) {
  ConnectionHandle conn_handle;

  SQLCHAR buf[256] = "test";
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(buf), 4);
  EXPECT_TRUE(status_record.ok());
}

TEST(ConnectionHandle, SetAttributeSuccessSQLULen) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ASYNC_ENABLE, reinterpret_cast<SQLPOINTER>(SQL_ASYNC_ENABLE_OFF),
      0);
  EXPECT_TRUE(status_record.ok());
}

TEST(ConnectionHandle, SetAttributeSuccessSQLIntBitmask) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_TXN_ISOLATION,
      reinterpret_cast<SQLPOINTER>(SQL_TRANSACTION_SERIALIZABLE), 0);
  EXPECT_TRUE(status_record.ok());
}

TEST(ConnectionHandle, SetAttributeFailUnsupportedSetAttribute) {
  ConnectionHandle conn_handle;
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_TRANSLATE_OPTION, reinterpret_cast<SQLPOINTER>(1), 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
}

TEST(ConnectionHandle, SetAttributeFailUnSupportedAttribute) {
  ConnectionHandle conn_handle;
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ODBC_CURSORS, reinterpret_cast<SQLPOINTER>(1), 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
}

TEST(ConnectionHandle, SetAttributeFailInvalidAttributeValue) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ACCESS_MODE, reinterpret_cast<SQLPOINTER>(2), 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY024());
}

TEST(ConnectionHandle, SetAttributeFailNegativeStringLen) {
  ConnectionHandle conn_handle;

  SQLCHAR catalog[256] = "test";
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(catalog), -1);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY090());
}

TEST(ConnectionHandle, SetAttributeFailInvalidStringLen) {
  ConnectionHandle conn_handle;

  SQLCHAR catalog[256] = "test";
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(catalog), 2);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY090());
}

TEST(ConnectionHandle, SetAttributeFailInvalidStringValue) {
  ConnectionHandle conn_handle;

  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, static_cast<SQLPOINTER>(nullptr), 0);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY009());
}

TEST(ConnectionHandle, GetAttributeFailUnsupportedGetAttribute) {
  ConnectionHandle conn_handle;
  SQLULEN val;
  SQLINTEGER str_len;
  auto status_record =
      conn_handle.GetAttribute(SQL_ATTR_ODBC_CURSORS, &val, 0, &str_len);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY092());
}

TEST(ConnectionHandle, GetAttributeFailInvalidConnectionBehavior) {
  ConnectionHandle conn_handle;
  SQLUINTEGER val;
  SQLINTEGER str_len;
  auto status_record =
      conn_handle.GetAttribute(SQL_ATTR_CONNECTION_DEAD, &val, 0, &str_len);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_08003());
}

TEST(ConnectionHandle, GetAttributeSuccessSQLUInteger) {
  ConnectionHandle conn_handle;
  SQLUINTEGER val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLUINTEGER);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ACCESS_MODE, reinterpret_cast<SQLPOINTER>(SQL_MODE_READ_ONLY),
      0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_ACCESS_MODE, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_MODE_READ_ONLY);
  EXPECT_EQ(str_len, expected_len);
}
TEST(ConnectionHandle, GetAttributeSuccessSQLULEN) {
  ConnectionHandle conn_handle;
  SQLULEN val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLULEN);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_ASYNC_ENABLE, reinterpret_cast<SQLPOINTER>(SQL_ASYNC_ENABLE_OFF),
      0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_ASYNC_ENABLE_OFF);
  EXPECT_EQ(str_len, expected_len);
}

TEST(ConnectionHandle, GetAttributeSuccessSQLIntBitmask) {
  ConnectionHandle conn_handle;
  SQLINTEGER val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLINTEGER);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_TXN_ISOLATION,
      reinterpret_cast<SQLPOINTER>(SQL_TRANSACTION_SERIALIZABLE), 0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_TXN_ISOLATION, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_TRANSACTION_SERIALIZABLE);
  EXPECT_EQ(str_len, expected_len);
}

TEST(ConnectionHandle, GetAttributeIsolationLevelGetOnlySupportedOne) {
  ConnectionHandle conn_handle;
  SQLINTEGER val;
  SQLINTEGER str_len;
  auto expected_len = sizeof(SQLINTEGER);
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_TXN_ISOLATION,
      reinterpret_cast<SQLPOINTER>(SQL_TRANSACTION_READ_COMMITTED), 0);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_TXN_ISOLATION, &val, 0, &str_len);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(val, (SQLUINTEGER)SQL_TRANSACTION_SERIALIZABLE);
  EXPECT_EQ(str_len, expected_len);
}

TEST(ConnectionHandle, GetAttributeSuccessSQLCharDestBufferGT) {
  ConnectionHandle conn_handle;
  SQLCHAR buf_in[256] = "test";
  SQLCHAR buf_out[256];
  SQLINTEGER str_len;
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(buf_in), 4);
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
  status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(buf_in), 4);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 5, &str_len);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val3(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val3, "aest");
  EXPECT_EQ(str_len, 4);
}

TEST(ConnectionHandle, GetAttributeSuccessSQLCharDestBufferSmaller) {
  ConnectionHandle conn_handle;
  SQLCHAR buf_in[256] = "test";
  SQLCHAR buf_out[256];
  SQLINTEGER str_len;
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(buf_in), 4);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 3, &str_len);
  EXPECT_FALSE(status_record.ok());
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(status_record.sql_state, SQLStates::k_01004());
  EXPECT_EQ(actual_val, "te");
}

TEST(ConnectionHandle, GetAttributeSuccessSQLCharDestBufferEQ) {
  ConnectionHandle conn_handle;
  SQLCHAR buf_in[256] = "test";
  SQLCHAR buf_out[256];
  SQLINTEGER str_len;
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(buf_in), 4);
  EXPECT_TRUE(status_record.ok());
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 4, &str_len);
  EXPECT_FALSE(status_record.ok());
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(status_record.sql_state, SQLStates::k_01004());
  EXPECT_EQ(actual_val, "tes");
}

TEST(ConnectionHandle, SetAttributeSetTwice) {
  ConnectionHandle conn_handle;

  SQLCHAR buf_in[256] = "test";
  auto status_record = conn_handle.SetAttribute(
      SQL_ATTR_CURRENT_CATALOG, reinterpret_cast<SQLPOINTER>(buf_in), SQL_NTS);
  EXPECT_TRUE(status_record.ok());

  SQLCHAR buf_out[256];
  status_record =
      conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out, 256, nullptr);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val(reinterpret_cast<char*>(buf_out));
  EXPECT_EQ(actual_val, "test");

  SQLCHAR buf_in_2[256] = "test_2";
  status_record =
      conn_handle.SetAttribute(SQL_ATTR_CURRENT_CATALOG,
                               reinterpret_cast<SQLPOINTER>(buf_in_2), SQL_NTS);
  EXPECT_TRUE(status_record.ok());

  SQLCHAR buf_out_2[256];
  status_record = conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, buf_out_2,
                                           256, nullptr);
  EXPECT_TRUE(status_record.ok());
  std::string actual_val_2(reinterpret_cast<char*>(buf_out_2));
  EXPECT_EQ(actual_val_2, "test_2");
}

TEST(ConnectionHandle, ValidateExternalUserSuccessByoidWithPoolUser) {
  Authentication auth;
  auth.oauth.auth_mechanism = OauthMechanism::kExternalUser;
  auth.oauth.byoid_aud_url = "test-aud";
  auth.oauth.byoid_creds_src = "test-creds";
  auth.oauth.byoid_subj_token_type = kSubTokenTypeDefault;
  auth.oauth.byoid_pool_user_project = "test-pool-user-project";
  auth.oauth.byoid_token_url = kDefaultTokenUrl;

  StatusRecord status = ConnectionHandle::ValidateExternalUser(auth);
  EXPECT_TRUE(status.ok());
}

TEST(ConnectionHandle, ValidateExternalUserSuccessByoidWithoutPoolUser) {
  Authentication auth;
  auth.oauth.auth_mechanism = OauthMechanism::kExternalUser;
  auth.oauth.byoid_aud_url = "test-aud";
  auth.oauth.byoid_creds_src = "test-creds";
  auth.oauth.byoid_subj_token_type = kSubTokenTypeDefault;
  auth.oauth.byoid_token_url = kDefaultTokenUrl;

  StatusRecord status = ConnectionHandle::ValidateExternalUser(auth);
  EXPECT_TRUE(status.ok());
}

TEST(ConnectionHandle, ValidateExternalUserSuccessJson) {
  Authentication auth;
  auth.oauth.auth_mechanism = OauthMechanism::kExternalUser;
  auth.oauth.credentials_file_path = "path-to-file";

  StatusRecord status = ConnectionHandle::ValidateExternalUser(auth);
  EXPECT_TRUE(status.ok());
}

TEST(ConnectionHandle, ValidateExternalUserSuccessNotExternalUser) {
  Authentication auth;
  auth.oauth.auth_mechanism = OauthMechanism::kServiceAndUserAccount;

  StatusRecord status = ConnectionHandle::ValidateExternalUser(auth);
  EXPECT_TRUE(status.ok());
}

TEST(ConnectionHandle, ValidateExternalUserFailByoid) {
  Authentication auth;
  auth.oauth.auth_mechanism = OauthMechanism::kExternalUser;
  auth.oauth.byoid_aud_url = "test-aud";
  auth.oauth.byoid_creds_src = "test-creds";
  auth.oauth.byoid_subj_token_type = "invalid";
  auth.oauth.byoid_token_url = kDefaultTokenUrl;

  StatusRecord status = ConnectionHandle::ValidateExternalUser(auth);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message, "Invalid subject token type");
}

TEST(ConnectionHandle, ValidateExternalUserFailJson) {
  Authentication auth;
  auth.oauth.auth_mechanism = OauthMechanism::kExternalUser;

  StatusRecord status = ConnectionHandle::ValidateExternalUser(auth);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message,
            "JSON Credentials File path is empty for external user");
}

TEST(ConnectionHandle, DsnSetupQueryPropertiesParsedCorrectly) {
  Section dsn_section;
  dsn_section["QUERYPROPERTIES"] = "key1=value1, key2=value2";

  ConnectionHandle handle;
  handle.SetUp(dsn_section, "TestDSN");

  std::vector<ConnectionProperty> const& props_vec =
      handle.GetDsn().connection_properties;
  ASSERT_EQ(props_vec.size(), 2);

  ConnectionProperty const& prop0 = props_vec[0];
  ConnectionProperty const& prop1 = props_vec[1];

  EXPECT_EQ(prop0.key, "key1");
  EXPECT_EQ(prop0.value, "value1");
  EXPECT_EQ(prop1.key, "key2");
  EXPECT_EQ(prop1.value, "value2");
}

TEST(ConnectionHandle, DsnSetupQueryPropertiesEmptyString) {
  Section dsn_section;
  dsn_section["QUERYPROPERTIES"] = "";

  ConnectionHandle handle;
  handle.SetUp(dsn_section, "TestDSN_EmptyQueryProps");

  EXPECT_TRUE(handle.GetDsn().connection_properties.empty());
}
}  // namespace google::cloud::odbc_bq_driver_internal
