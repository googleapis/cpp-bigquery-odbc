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
#include "google/cloud/universe_domain_options.h"
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
                         "test-sub-token-type", kDefaultTokenUrl});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(ExternalAuthentication, SuccessByoidPropsSetWithoutPoolUser) {
  auto credentials =
      CreateCredentials({OauthMechanism::kExternalUser, "", "test-aud-url",
                         R"({"file": "test-creds-src"})", "",
                         "test-sub-token-type", kDefaultTokenUrl});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(ValidateBYOIDTokenUrl, AcceptsAllowedStsHosts) {
  EXPECT_TRUE(ValidateBYOIDTokenUrl(kDefaultTokenUrl).ok());
  EXPECT_TRUE(
      ValidateBYOIDTokenUrl("HTTPS://STS.GOOGLEAPIS.COM/v1/token").ok());
  EXPECT_TRUE(
      ValidateBYOIDTokenUrl("https://sts.googleapis.com:443/v1/token").ok());
  EXPECT_TRUE(
      ValidateBYOIDTokenUrl("https://sts-bqpsc.p.googleapis.com/v1/token")
          .ok());
  EXPECT_TRUE(ValidateBYOIDTokenUrl(
                  "https://sts-my-endpoint-1.p.googleapis.com:8443/v1/token")
                  .ok());
  EXPECT_TRUE(ValidateBYOIDTokenUrl("https://sts.apis-tpclp.goog/v1/token",
                                    "apis-tpclp.goog")
                  .ok());
  EXPECT_TRUE(
      ValidateBYOIDTokenUrl("https://STS.S3NSAPIS.FR/v1/token", "s3nsapis.fr")
          .ok());
}

TEST(ValidateBYOIDTokenUrl, RejectsDisallowedSchemesAndMalformedUrls) {
  std::vector<std::string> const invalid_urls = {
      "",
      "test-token-url",
      "http://sts.googleapis.com/v1/token",
      "file:///etc/passwd",
      "https://",
      "https:///v1/token",
      "https://user:pass@sts.googleapis.com/v1/token",
      "https://sts.googleapis.com@evil.com/v1/token",
      "https://sts.googleapis.com:443@evil.com/v1/token",
      "https://sts%2egoogleapis.com/v1/token",
      "https://sts.googleapis.com/v1/token%00",
      " https://sts.googleapis.com/v1/token",
      "https://sts.googleapis.com/v1/token ",
      "https://sts.googleapis.com/v1/token\r\n",
      "https://sts.googleapis.com\\@evil.com/v1/token",
      "https://sts.googleapis.com?@evil.com",
      "https://sts.googleapis.com#@evil.com",
      "https://sts.googleapis.com:/v1/token",
      "https://sts.googleapis.com:0/v1/token",
      "https://sts.googleapis.com:65536/v1/token",
      "https://sts.googleapis.com:abc/v1/token",
      "https://sts.googleapis.com.:443/v1/token",
  };

  for (auto const& url : invalid_urls) {
    auto status = ValidateBYOIDTokenUrl(url);
    EXPECT_FALSE(status.ok()) << "Expected failure for URL: " << url;
    EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_HY000());
  }
}

TEST(ValidateBYOIDTokenUrl, RejectsIpLiteralsAndNonStsHosts) {
  std::vector<std::string> const disallowed_hosts = {
      "https://127.0.0.1:8443/collect",
      "https://169.254.169.254/collect",
      "https://2130706433/collect",
      "https://0x7f.0.0.0x1/collect",
      "https://[::1]:8443/collect",
      "https://evil.com/v1/token",
      "https://storage.googleapis.com/v1/token",
      "https://sts.googleapis.com.evil.com/v1/token",
      "https://evil-sts.googleapis.com/v1/token",
      "https://sts.p.googleapis.com/v1/token",
      "https://sts-.p.googleapis.com/v1/token",
      "https://sts--bad.p.googleapis.com/v1/token",
      "https://sts-bad-.p.googleapis.com/v1/token",
      "https://sts-sub.domain.p.googleapis.com/v1/token",
  };

  for (auto const& url : disallowed_hosts) {
    auto status = ValidateBYOIDTokenUrl(url);
    EXPECT_FALSE(status.ok()) << "Expected failure for URL: " << url;
    EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_HY000());
  }
}

TEST(ValidateBYOIDTokenUrl, UniverseDomainHandling) {
  // Custom universe STS host is rejected when universe_domain is unset.
  auto without_universe =
      ValidateBYOIDTokenUrl("https://sts.apis-tpclp.goog/v1/token");
  EXPECT_FALSE(without_universe.ok());
  EXPECT_EQ(without_universe.sql_state, odbc_internal::SQLStates::k_HY000());

  // Mismatched universe domain is rejected.
  auto mismatched =
      ValidateBYOIDTokenUrl("https://sts.evil.com/v1/token", "apis-tpclp.goog");
  EXPECT_FALSE(mismatched.ok());

  // Malformed or IP-literal universe domains are not accepted.
  EXPECT_FALSE(
      ValidateBYOIDTokenUrl("https://sts.localhost/v1/token", "localhost")
          .ok());
  EXPECT_FALSE(
      ValidateBYOIDTokenUrl("https://sts.127.0.0.1/v1/token", "127.0.0.1")
          .ok());
  EXPECT_FALSE(
      ValidateBYOIDTokenUrl("https://sts.evil.com/v1/token", "evil.com/path")
          .ok());
}

TEST(CreateJsonCredsObject, RejectsNonObjectCredentialSource) {
  for (std::string const& src :
       {R"(["file"])", R"("file")", "123", "true", "null"}) {
    auto result = CreateJsonCredsObject("test-aud", src, "", "test-subj-token",
                                        kDefaultTokenUrl);
    EXPECT_THAT(result,
                StatusRecordIs(
                    odbc_internal::SQLStates::k_HY000(),
                    HasSubstr("BYOID_CREDENTIALSOURCE must be a JSON object")))
        << "Input: " << src;
  }
}

TEST(CreateJsonCredsObject, ValidatesCredentialSourceUrlScheme) {
  // Valid http and https credential_source URLs succeed.
  ASSERT_STATUS_RECORD_OK(CreateJsonCredsObject(
      "test-aud",
      R"({"url": "https://idp.example.com/oauth2/token", "headers": {"Authorization": "Bearer x"}})",
      "", "test-subj-token", kDefaultTokenUrl));
  ASSERT_STATUS_RECORD_OK(CreateJsonCredsObject(
      "test-aud",
      R"({"url": "http://169.254.169.254/computeMetadata/v1/instance/service-accounts/default/token"})",
      "", "test-subj-token", kDefaultTokenUrl));

  // Non-http(s), non-string, empty, or malformed URLs are rejected.
  std::vector<std::string> const bad_sources = {
      R"({"url": ""})",
      R"({"url": 123})",
      R"({"url": null})",
      R"({"url": "file:///etc/passwd"})",
      R"({"url": "gopher://127.0.0.1:6379/_INFO"})",
      R"({"url": "ftp://example.com/token"})",
      R"({"url": "dict://127.0.0.1:11211/stat"})",
      R"({"url": "http://"})",
      R"({"url": "https:///path"})",
      R"({"url": "https://:443/path"})",
      R"({"url": "https://idp.example.com/token with space"})",
  };
  for (auto const& src : bad_sources) {
    auto result = CreateJsonCredsObject("test-aud", src, "", "test-subj-token",
                                        kDefaultTokenUrl);
    EXPECT_THAT(result,
                StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                               HasSubstr("BYOID_CREDENTIALSOURCE url must be a "
                                         "valid http or https URL")))
        << "Input: " << src;
  }
}

TEST(ExternalAuthentication, RejectsUntrustedByoidTokenUrlInCreateCredentials) {
  auto attacker_creds = CreateCredentials(
      {OauthMechanism::kExternalUser, "",
       "//iam.googleapis.com/locations/global/workforcePools/p/providers/pr",
       R"({"file": "/etc/hostname"})", "", kSubTokenTypeDefault,
       "https://127.0.0.1:8443/collect"});
  EXPECT_THAT(attacker_creds,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("Invalid BYOID_TOKENURL")));

  // UniverseDomain without EnableGCD does not allow custom STS host.
  ConnProps props_gcd_disabled{
      OauthMechanism::kExternalUser,         "", "test-aud-url",
      R"({"file": "test-creds-src"})",       "", kSubTokenTypeDefault,
      "https://sts.apis-tpclp.goog/v1/token"};
  props_gcd_disabled.gcd = {false, "apis-tpclp.goog"};
  EXPECT_THAT(CreateCredentials(props_gcd_disabled),
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("Invalid BYOID_TOKENURL")));
}

TEST(ExternalAuthentication, AcceptsCustomUniverseByoidTokenUrlWhenEnabled) {
  ConnProps props{
      OauthMechanism::kExternalUser,         "", "test-aud-url",
      R"({"file": "test-creds-src"})",       "", kSubTokenTypeDefault,
      "https://sts.apis-tpclp.goog/v1/token"};

  Options opts;
  opts.set<google::cloud::internal::UniverseDomainOption>("apis-tpclp.goog");
  ASSERT_STATUS_RECORD_OK(CreateCredentials(props, opts));

  props.gcd = {true, "apis-tpclp.goog"};
  ASSERT_STATUS_RECORD_OK(CreateCredentials(props));
}
}  // namespace google::cloud::odbc_bigquery_client_interface
