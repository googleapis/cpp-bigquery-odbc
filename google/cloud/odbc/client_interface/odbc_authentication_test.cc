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

#include <fstream>
#include <gmock/gmock.h>

#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/bq_driver/internal/setenv.h"
#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"
#include "google/cloud/odbc/client_interface/odbc_authentication.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bq_driver::UnsetEnv;
using google::cloud::odbc_testing_util_internal::StatusIs;
using ::testing::HasSubstr;

void CleanEnvVars() {
  UnsetEnv("GOOGLE_APPLICATION_CREDENTIALS");
}

TEST(UserAuthentication, HappyPath) {
  std::string refresh_token = "my-token";

  auto credentials = CreateCredentials(OauthMechanism::kAuthorizedUser, "", refresh_token);

  ASSERT_STATUS_OK(credentials);
  auto file_path = GetEnv("GOOGLE_APPLICATION_CREDENTIALS");
  EXPECT_TRUE(file_path);
  auto is = std::ifstream(AuthorizedUserFilePath());
  is.exceptions(std::ios::badbit);  // Minimal error handling
  auto content = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  EXPECT_TRUE(content.find(refresh_token) != std::string::npos);

  CleanEnvVars();
}

TEST(ServiceAuthentication, ServiceAccountAuthWithClientIdAuthentication) {
  std::string test_data_path = google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH").value_or("");
  std::string credentials_file_path = test_data_path + "user_account_auth_keys.json";

  auto credentials = CreateCredentials(OauthMechanism::kServiceAccount, credentials_file_path, "");

  ASSERT_STATUS_OK(credentials);
  auto file_path = GetEnv("GOOGLE_APPLICATION_CREDENTIALS");
  EXPECT_TRUE(file_path);

  CleanEnvVars();
}

TEST(ServiceAuthentication, FileNotExist) {
  auto credentials = CreateCredentials(OauthMechanism::kServiceAccount, "not_existing_file.json", "");

  EXPECT_THAT(credentials, StatusIs(StatusCode::kInvalidArgument, HasSubstr("File content is not valid json")));
}

TEST(ServiceAuthentication, TypeIsNotRecognized) {
  std::string test_data_path = google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH").value_or("");
  std::string credentials_file_path = test_data_path + "not_recognized_auth_keys.json";

  auto credentials = CreateCredentials(OauthMechanism::kServiceAccount, credentials_file_path, "");

  EXPECT_THAT(credentials, StatusIs(StatusCode::kInvalidArgument, HasSubstr("File content is not recognized")));
}

TEST(ServiceAuthentication, ServiceAccountAuthentication) {
  std::string test_data_path = google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH").value_or("");
  std::string credentials_file_path = test_data_path + "service_account_auth_keys.json";

  auto credentials = CreateCredentials(OauthMechanism::kServiceAccount, credentials_file_path, "");

  ASSERT_STATUS_OK(credentials);
  auto file_path = GetEnv("GOOGLE_APPLICATION_CREDENTIALS");
  EXPECT_FALSE(file_path);
}

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
