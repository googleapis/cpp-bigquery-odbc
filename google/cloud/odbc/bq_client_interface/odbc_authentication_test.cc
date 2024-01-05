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

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google::cloud::odbc_bigquery_client_interface {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

TEST(ServiceAuthentication, ServiceAccountAuthentication) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path =
      test_data_path + "service_account_auth_keys.json";

  auto credentials = CreateCredentials(
      {OauthMechanism::kServiceAccount, credentials_file_path});

  ASSERT_STATUS_OK(credentials);
}

TEST(ServiceAuthentication, EmptyPath) {
  auto credentials = CreateCredentials({OauthMechanism::kServiceAccount, ""});

  EXPECT_THAT(credentials,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("The path to the file can't be empty.")));
}

TEST(ServiceAuthentication, FileNotExist) {
  auto credentials = CreateCredentials(
      {OauthMechanism::kServiceAccount, "not_existing_file.json"});

  EXPECT_THAT(
      credentials,
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("There was an error while opening the file:")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
