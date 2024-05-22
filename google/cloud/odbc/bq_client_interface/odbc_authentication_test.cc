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
#include "google/cloud/odbc/bq_client_interface/setenv.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include <gmock/gmock.h>
#include <fstream>

namespace google::cloud::odbc_bigquery_client_interface {

class MockAccessTokenGenerator
    : public ::google::cloud::oauth2::AccessTokenGenerator {
 public:
  MOCK_METHOD(StatusOr<AccessToken>, GetToken, (), (override));
};

using google::cloud::internal::GetEnv;
using ::google::cloud::odbc_bigquery_client_interface::SetEnv;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::StrEq;

TEST(ServiceAuthentication, ServiceAccountAuthentication) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path =
      test_data_path + "service_account_auth_keys.json";

  auto credentials = CreateCredentials(
      {OauthMechanism::kServiceAccount, credentials_file_path});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(ServiceAuthentication, UserAccountAuthentication) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path = test_data_path + "user_account.json";

  auto credentials = CreateCredentials(
      {OauthMechanism::kServiceAccount, credentials_file_path});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(DefaultApplicationAuthentication, DefaultApplicationAuthentication) {
  auto credentials =
      CreateCredentials({OauthMechanism::kApplicationDefault, ""});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(ServiceAuthentication, EmptyPath) {
  auto credentials = CreateCredentials({OauthMechanism::kServiceAccount, ""});

  EXPECT_THAT(credentials,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("The path to the file can't be empty")));
}

TEST(GetOAuth2Token, GetToken) {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::minutes(15);
  auto access_token = AccessToken{"test-token", expiration};
  auto mock_generator = std::make_shared<MockAccessTokenGenerator>();
  EXPECT_CALL(*mock_generator, GetToken())
      .Times(1)
      .WillOnce(Return(StatusOr(access_token)));
  std::string env_var = "something";
  SetEnv("GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", env_var);

  odbc_internal::StatusRecordOr<AccessToken> token =
      GetOAuth2Token(mock_generator);

  ASSERT_STATUS_RECORD_OK(token);
  EXPECT_EQ(GetEnv("GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT")
                .value_or(""),
            env_var);
}

TEST(GetOAuth2Token, Unauthenticated) {
  auto mock_generator = std::make_shared<MockAccessTokenGenerator>();
  EXPECT_CALL(*mock_generator, GetToken())
      .Times(1)
      .WillOnce(Return(Status(StatusCode::kUnauthenticated, "no access")));
  std::string env_var = "something";
  SetEnv("GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT", env_var);

  odbc_internal::StatusRecordOr<AccessToken> token =
      GetOAuth2Token(mock_generator);

  EXPECT_THAT(token, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                    HasSubstr("no access")));
  EXPECT_EQ(GetEnv("GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT")
                .value_or(""),
            env_var);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
