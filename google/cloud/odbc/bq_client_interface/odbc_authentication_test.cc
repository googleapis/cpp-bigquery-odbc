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
      CreateCredentials({OauthMechanism::kServiceAccount, invalid_path});

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
  auto credentials = CreateCredentials({OauthMechanism::kServiceAccount, ""});

  EXPECT_THAT(credentials,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("The path to the file can't be empty")));
}

TEST(UserAuthentication, InvalidPathFileDoesNotExist) {
  std::string invalid_path = "non_existing_key.json";

  auto credentials =
      CreateCredentials({OauthMechanism::kUserAccount, invalid_path});

  EXPECT_THAT(credentials,
              StatusRecordIs(
                  odbc_internal::SQLStates::k_HY000(),
                  testing::HasSubstr("Could not open User Account key file")));
}

TEST(UserAuthentication, EmptyPath) {
  auto credentials = CreateCredentials({OauthMechanism::kUserAccount, ""});

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

TEST(ExternalAuthentication, FailInvalidCredentialSourceJson) {
  auto credentials = CreateCredentials(
      {OauthMechanism::kExternalUser, "", "test-aud-url", "test-creds-src"});

  EXPECT_THAT(
      credentials,
      StatusRecordIs(
          odbc_internal::SQLStates::k_HY000(),
          HasSubstr("BYOID_CREDENTIALSOURCE is not a valid JSON string")));
}

TEST(CreateJsonCredsObject, WithPoolUser) {
  StatusRecordOr<nlohmann::json> result = CreateJsonCredsObject(
      "test-aud", R"({"file": "test-creds"})", "test-pool-user_project",
      "test-subj-token", "test-token-url");
  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_EQ(result->value("type", ""), "external_account");
  EXPECT_EQ(result->value("audience", ""), "test-aud");
  EXPECT_EQ(result->value("credential_source", nlohmann::json::object()),
            nlohmann::json::parse(R"({"file": "test-creds"})"));
  EXPECT_EQ(result->value("subject_token_type", ""), "test-subj-token");
  EXPECT_EQ(result->value("token_url", ""), "test-token-url");
  EXPECT_EQ(result->value("workforce_pool_user_project", ""),
            "test-pool-user_project");
}

TEST(CreateJsonCredsObject, WithoutPoolUser) {
  StatusRecordOr<nlohmann::json> result =
      CreateJsonCredsObject("test-aud", R"({"file": "test-creds"})", "",
                            "test-subj-token", "test-token-url");
  ASSERT_STATUS_RECORD_OK(result);
  EXPECT_EQ(result->value("type", ""), "external_account");
  EXPECT_EQ(result->value("audience", ""), "test-aud");
  EXPECT_EQ(result->value("credential_source", nlohmann::json::object()),
            nlohmann::json::parse(R"({"file": "test-creds"})"));
  EXPECT_EQ(result->value("subject_token_type", ""), "test-subj-token");
  EXPECT_EQ(result->value("token_url", ""), "test-token-url");
  EXPECT_EQ(result->value("workforce_pool_user_project", "NotSet"), "NotSet");
}

TEST(ExternalAuthentication, SuccessByoidPropsSetWithPoolUser) {
  auto credentials =
      CreateCredentials({OauthMechanism::kExternalUser, "", "test-aud-url",
                         R"({"file": "test-creds-src"})", "test-pool-user",
                         "test-sub-token-type", "test-token-url"});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(ExternalAuthentication, SuccessByoidPropsSetWithoutPoolUser) {
  auto credentials =
      CreateCredentials({OauthMechanism::kExternalUser, "", "test-aud-url",
                         R"({"file": "test-creds-src"})", "",
                         "test-sub-token-type", "test-token-url"});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(UserAuthentication, RefreshTokenWithDefaultAndCustomClientCredentials) {
  ConnProps default_props{};
  default_props.auth_mechanism = OauthMechanism::kUserAccount;
  default_props.refresh_token = "mock-refresh-token";
  default_props.use_default_client_credentials = true;
  auto creds_default = CreateCredentials(default_props);
  ASSERT_STATUS_RECORD_OK(creds_default);

  ConnProps custom_props{};
  custom_props.auth_mechanism = OauthMechanism::kUserAccount;
  custom_props.refresh_token = "mock-refresh-token";
  custom_props.use_default_client_credentials = false;
  custom_props.client_id = "custom-client-id";
  custom_props.client_secret = "custom-client-secret";
  auto creds_custom = CreateCredentials(custom_props);
  ASSERT_STATUS_RECORD_OK(creds_custom);

  ConnProps missing_secret = custom_props;
  missing_secret.client_secret.clear();
  EXPECT_THAT(CreateCredentials(missing_secret),
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("ClientId and ClientSecret")));
}

TEST(WorkforceAuthentication, ExtractWorkforceProviderName) {
  auto p1 = ExtractWorkforceProviderName(
      "//iam.googleapis.com/locations/global/workforcePools/my-pool/providers/"
      "my-provider");
  ASSERT_STATUS_RECORD_OK(p1);
  EXPECT_EQ(*p1,
            "locations/global/workforcePools/my-pool/providers/my-provider");

  auto p2 = ExtractWorkforceProviderName(
      "locations/global/workforcePools/my-pool/providers/my-provider");
  ASSERT_STATUS_RECORD_OK(p2);
  EXPECT_EQ(*p2,
            "locations/global/workforcePools/my-pool/providers/my-provider");

  auto invalid = ExtractWorkforceProviderName(
      "//iam.googleapis.com/projects/123/locations/global/"
      "workloadIdentityPools/p/providers/pr");
  EXPECT_THAT(invalid, StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                                      HasSubstr("Invalid Workforce")));
}

TEST(WorkforceAuthentication, ExternalAccountAuthorizedUserAndRefreshToken) {
  std::string const temp_file = "test_external_account_authorized_user.json";
  {
    std::ofstream ofs(temp_file);
    ofs << R"({
      "type": "external_account_authorized_user",
      "audience": "//iam.googleapis.com/locations/global/workforcePools/pool-1/providers/prov-1",
      "client_id": "test-cid",
      "client_secret": "test-csec",
      "refresh_token": "test-rtok",
      "token_url": "https://sts.my-universe.net/v1/oauthtoken",
      "universe_domain": "my-universe.net",
      "quota_project_id": "my-quota-proj"
    })";
  }

  ConnProps ext_props{};
  ext_props.auth_mechanism = OauthMechanism::kExternalUser;
  ext_props.credentials_file_path = temp_file;
  auto creds = CreateCredentials(ext_props);
  ASSERT_STATUS_RECORD_OK(creds);

  ConnProps wif_rtok_props{};
  wif_rtok_props.auth_mechanism = OauthMechanism::kExternalUser;
  wif_rtok_props.byoid_aud_url =
      "//iam.googleapis.com/locations/global/workforcePools/pool-1/providers/"
      "prov-1";
  wif_rtok_props.refresh_token = "test-rtok";
  auto wif_creds = CreateCredentials(wif_rtok_props);
  ASSERT_STATUS_RECORD_OK(wif_creds);

  std::remove(temp_file.c_str());
}

}  // namespace google::cloud::odbc_bigquery_client_interface
