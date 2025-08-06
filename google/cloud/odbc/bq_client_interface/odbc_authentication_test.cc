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

using google::cloud::odbc_internal::StatusRecordOr;

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

TEST(ServiceAuthentication, InvalidPathFileDoesNotExist) {
  std::string invalid_path = "non_existing_key.json";

  auto credentials =
      CreateCredentials({OauthMechanism::kServiceAndUserAccount, invalid_path});

  EXPECT_THAT(credentials,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             testing::HasSubstr(
                                 "Could not open Service Account key file")));
}

TEST(DefaultApplicationAuthentication, DefaultApplicationAuthentication) {
  auto credentials =
      CreateCredentials({OauthMechanism::kApplicationDefault, ""});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(ServiceAuthentication, EmptyPath) {
  auto credentials =
      CreateCredentials({OauthMechanism::kServiceAndUserAccount, ""});

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

TEST(ExternalAuthentication, InvalidPathFileDoesNotExist) {
  std::string invalid_path = "non_existing_key.json";
  auto credentials =
      CreateCredentials({OauthMechanism::kExternalUser, invalid_path});

  EXPECT_THAT(credentials,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             testing::HasSubstr(
                                 "Could not open External Account key file")));
}

TEST(ExternalAuthentication, FailEmptyJsonPathNoReqdByoidPropsSet) {
  auto credentials = CreateCredentials({OauthMechanism::kExternalUser, ""});

  EXPECT_THAT(
      credentials,
      StatusRecordIs(
          odbc_internal::SQLStates::k_HY000(),
          HasSubstr("The path to the external auth JSON file can't be empty")));
}

TEST(ExternalAuthentication, FailEmptyjsonpathPartialReqdByoidPropsSet) {
  auto credentials = CreateCredentials(
      {OauthMechanism::kExternalUser, "", "test-aud-url", "test-creds-src"});

  EXPECT_THAT(
      credentials,
      StatusRecordIs(
          odbc_internal::SQLStates::k_HY000(),
          HasSubstr("The path to the external auth JSON file can't be empty")));
}

TEST(CreateJsonCredsObject, WithPoolUser) {
  StatusRecordOr<nlohmann::json> result =
      CreateJsonCredsObject("test-aud", "test-creds", "test-pool-user_project",
                            "test-subj-token", "test-token-url");
  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_EQ(result->value("type", ""), "external_account");
  EXPECT_EQ(result->value("audience", ""), "test-aud");
  EXPECT_EQ(result->value("credential_source", ""), "test-creds");
  EXPECT_EQ(result->value("subject_token_type", ""), "test-subj-token");
  EXPECT_EQ(result->value("token_url", ""), "test-token-url");
  EXPECT_EQ(result->value("workforce_pool_user_project", ""),
            "test-pool-user_project");
}

TEST(CreateJsonCredsObject, WithoutPoolUser) {
  StatusRecordOr<nlohmann::json> result = CreateJsonCredsObject(
      "test-aud", "test-creds", "", "test-subj-token", "test-token-url");
  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_EQ(result->value("type", ""), "external_account");
  EXPECT_EQ(result->value("audience", ""), "test-aud");
  EXPECT_EQ(result->value("credential_source", ""), "test-creds");
  EXPECT_EQ(result->value("subject_token_type", ""), "test-subj-token");
  EXPECT_EQ(result->value("token_url", ""), "test-token-url");
  EXPECT_EQ(result->value("workforce_pool_user_project", "NotSet"), "NotSet");
}

TEST(ExternalAuthentication, SuccessByoidPropsSetWithPoolUser) {
  auto credentials = CreateCredentials(
      {OauthMechanism::kExternalUser, "", "test-aud-url", "test-creds-src",
       "test-pool-user", "test-sub-token-type", "test-token-url"});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(ExternalAuthentication, SuccessByoidPropsSetWithoutPoolUser) {
  auto credentials = CreateCredentials(
      {OauthMechanism::kExternalUser, "", "test-aud-url", "test-creds-src", "",
       "test-sub-token-type", "test-token-url"});

  ASSERT_STATUS_RECORD_OK(credentials);
}
}  // namespace google::cloud::odbc_bigquery_client_interface
