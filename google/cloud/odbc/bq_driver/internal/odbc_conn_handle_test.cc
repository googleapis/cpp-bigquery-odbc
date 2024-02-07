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
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;

std::string const kDsnDescription = "test-dsn";
std::string const kDsnCatalog = "bigquery-test";
std::string const kDsnDriver = "test-driver";
std::string const kDsnName = "SampleDSN";

TEST(ConnectionHandle, Connect) {
  std::string credentials_file_path =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  Authentication auth = {OauthMechanism::kServiceAccount,
                         credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  Status status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), true);
  delete conn_handle;
}

TEST(ConnectionHandle, ConnectWithInvalidFile) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path = test_data_path + "random_file.json";

  Authentication auth = {OauthMechanism::kServiceAccount,
                         credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  Status status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
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
  Status status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.code(), StatusCode::kUnimplemented);
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
  Status status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.code(), StatusCode::kInvalidArgument);
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

  delete conn_handle;
}

// TODO(171): Add tests which use refresh token

}  // namespace google::cloud::odbc_bq_driver_internal
