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
#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

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

namespace {

class ScopedEnvVar {
 public:
  ScopedEnvVar(char const* name, std::optional<std::string> const& value)
      : name_(name), previous_(GetEnv(name)) {
    SetEnv(name_, value);
  }
  ~ScopedEnvVar() { SetEnv(name_, previous_); }
  ScopedEnvVar(ScopedEnvVar const&) = delete;
  ScopedEnvVar& operator=(ScopedEnvVar const&) = delete;

 private:
  char const* name_;
  std::optional<std::string> previous_;
};

class ScopedTempFile {
 public:
  explicit ScopedTempFile(std::string const& contents) {
    static std::atomic<int> counter{0};
    auto filename =
        "odbc_auth_test_" + std::to_string(counter.fetch_add(1)) + ".json";
    path_ = (std::filesystem::temp_directory_path() / filename).string();
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    out << contents;
  }
  ~ScopedTempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }
  ScopedTempFile(ScopedTempFile const&) = delete;
  ScopedTempFile& operator=(ScopedTempFile const&) = delete;

  [[nodiscard]] std::string const& path() const { return path_; }

 private:
  std::string path_;
};

std::string MakeServiceAccountJson(std::string const& universe_domain = "") {
  nlohmann::json j = {
      {"type", "service_account"},
      {"project_id", "test-project"},
      {"private_key_id", "key-id"},
      {"private_key",
       "-----BEGIN PRIVATE KEY-----\nMIIE\n-----END PRIVATE KEY-----\n"},
      {"client_email", "sa@test-project.iam.gserviceaccount.com"},
      {"client_id", "123456789"},
      {"token_uri", "https://oauth2.googleapis.com/token"},
  };
  if (!universe_domain.empty()) {
    j["universe_domain"] = universe_domain;
  }
  return j.dump();
}

std::string MakeUserAccountJson(std::string const& universe_domain = "") {
  nlohmann::json j = {
      {"type", "authorized_user"},
      {"client_id", "test-client-id.apps.googleusercontent.com"},
      {"client_secret", "test-client-secret"},
      {"refresh_token", "test-refresh-token"},
  };
  if (!universe_domain.empty()) {
    j["universe_domain"] = universe_domain;
  }
  return j.dump();
}

std::string MakeExternalAccountJson(std::string const& universe_domain = "") {
  nlohmann::json j = {
      {"type", "external_account"},
      {"audience",
       "//iam.googleapis.com/projects/123/locations/global/"
       "workloadIdentityPools/p/providers/pr"},
      {"subject_token_type", "urn:ietf:params:oauth:token-type:jwt"},
      {"token_url", "https://sts.googleapis.com/v1/token"},
      {"credential_source", {{"file", "/tmp/token"}}},
  };
  if (!universe_domain.empty()) {
    j["universe_domain"] = universe_domain;
  }
  return j.dump();
}

class NoRetryUniverseDomainPolicy
    : public ::google::cloud::internal::UniverseDomainRetryPolicy {
 public:
  bool OnFailure(Status const&) override { return false; }
  [[nodiscard]] bool IsExhausted() const override { return true; }
  [[nodiscard]] bool IsPermanentFailure(Status const&) const override {
    return true;
  }
  [[nodiscard]] std::unique_ptr<
      ::google::cloud::internal::UniverseDomainRetryPolicy>
  clone() const override {
    return std::make_unique<NoRetryUniverseDomainPolicy>();
  }
};

}  // namespace

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
  ScopedEnvVar clear_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", std::nullopt);
  auto credentials =
      CreateCredentials({OauthMechanism::kExternalUser, "", "test-aud-url",
                         R"({"file": "test-creds-src"})", "",
                         "test-sub-token-type", "test-token-url"});

  ASSERT_STATUS_RECORD_OK(credentials);
}

TEST(UniverseDomainValidation, AcceptsValidHostnames) {
  EXPECT_TRUE(ValidateUniverseDomain("googleapis.com").ok());
  EXPECT_TRUE(ValidateUniverseDomain("s3nsapis.fr").ok());
  EXPECT_TRUE(ValidateUniverseDomain("apis-tpclp.goog").ok());
  EXPECT_TRUE(ValidateUniverseDomain("localhost.test").ok());
  EXPECT_TRUE(ValidateUniverseDomain("sub-1.domain2.example.co.uk").ok());
  EXPECT_TRUE(ValidateUniverseDomain("xn--bcher-kva.example").ok());
}

TEST(UniverseDomainValidation, RejectsMalformedHostnames) {
  std::vector<std::string> invalid_domains = {
      "",
      "   ",
      " googleapis.com",
      "googleapis.com ",
      "googleapis.com\n",
      "https://googleapis.com",
      "http://attacker.com",
      "googleapis.com:443",
      "googleapis.com/bigquery/v2",
      "googleapis.com?foo=bar",
      "googleapis.com#frag",
      "googleapis.com@attacker.com",
      "user@googleapis.com",
      ".googleapis.com",
      "googleapis.com.",
      "google..apis.com",
      "-bad.com",
      "bad-.com",
      "bad.-com",
      "bad_underscore.com",
      "localhost",
      "127.0.0.1",
      "[::1]",
      std::string(64, 'a') + ".com",
      std::string(60, 'a') + "." + std::string(60, 'b') + "." +
          std::string(60, 'c') + "." + std::string(60, 'd') + ".example.com",
  };

  for (auto const& domain : invalid_domains) {
    auto status = ValidateUniverseDomain(domain);
    EXPECT_FALSE(status.ok()) << "Expected failure for domain: " << domain;
    EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_HY024())
        << "Domain: " << domain;
  }
}

TEST(EffectiveUniverseDomain, ResolutionAndValidation) {
  ScopedEnvVar clear_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", std::nullopt);

  // Default empty resolves to googleapis.com.
  ConnProps props{};
  auto default_ud = GetEffectiveUniverseDomain(props);
  ASSERT_STATUS_RECORD_OK(default_ud);
  EXPECT_EQ(*default_ud, "googleapis.com");

  // Non-empty UniverseDomain is honored even when EnableGCD is false (per
  // public ODBC documentation).
  props.gcd.enable_gcd = false;
  props.gcd.universe_domain = "s3nsapis.fr";
  auto tpc_ud = GetEffectiveUniverseDomain(props);
  ASSERT_STATUS_RECORD_OK(tpc_ud);
  EXPECT_EQ(*tpc_ud, "s3nsapis.fr");

  // EnableGCD=true with empty UniverseDomain and no PSC is rejected (HY024).
  ConnProps empty_gcd_props{};
  empty_gcd_props.gcd.enable_gcd = true;
  EXPECT_THAT(GetEffectiveUniverseDomain(empty_gcd_props),
              StatusRecordIs(odbc_internal::SQLStates::k_HY024(),
                             HasSubstr("Invalid UniverseDomain")));

  // EnableGCD=true with PSC configured and empty UniverseDomain (Windows UI PSC
  // setup on GDU) defaults to googleapis.com.
  ConnProps psc_only_props{};
  psc_only_props.gcd.enable_gcd = true;
  psc_only_props.psc = "BIGQUERY=https://bigquery-psc.p.googleapis.com";
  auto psc_ud = GetEffectiveUniverseDomain(psc_only_props);
  ASSERT_STATUS_RECORD_OK(psc_ud);
  EXPECT_EQ(*psc_ud, "googleapis.com");

  // GOOGLE_CLOUD_UNIVERSE_DOMAIN overrides and is validated.
  {
    ScopedEnvVar set_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                            std::string("apis-tpclp.goog"));
    ConnProps env_props{};
    auto env_ud = GetEffectiveUniverseDomain(env_props);
    ASSERT_STATUS_RECORD_OK(env_ud);
    EXPECT_EQ(*env_ud, "apis-tpclp.goog");
  }
  {
    ScopedEnvVar bad_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN",
                            std::string("https://bad.example.com"));
    ConnProps env_props{};
    EXPECT_THAT(GetEffectiveUniverseDomain(env_props),
                StatusRecordIs(odbc_internal::SQLStates::k_HY024(),
                               HasSubstr("Invalid UniverseDomain")));
  }
}

TEST(CredentialsUniverseDomainJson, ExtractsAndValidatesFields) {
  auto default_ud =
      GetUniverseDomainFromCredentialsJson(nlohmann::json::object());
  ASSERT_STATUS_RECORD_OK(default_ud);
  EXPECT_EQ(*default_ud, "googleapis.com");

  auto explicit_ud = GetUniverseDomainFromCredentialsJson(
      nlohmann::json{{"universe_domain", "s3nsapis.fr"}});
  ASSERT_STATUS_RECORD_OK(explicit_ud);
  EXPECT_EQ(*explicit_ud, "s3nsapis.fr");

  EXPECT_THAT(
      GetUniverseDomainFromCredentialsJson(
          nlohmann::json{{"universe_domain", ""}}),
      StatusRecordIs(odbc_internal::SQLStates::k_HY000(), HasSubstr("empty")));

  EXPECT_THAT(GetUniverseDomainFromCredentialsJson(
                  nlohmann::json{{"universe_domain", 42}}),
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("expected string")));

  EXPECT_THAT(GetUniverseDomainFromCredentialsJson(
                  nlohmann::json{{"universe_domain", nlohmann::json()}}),
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("expected string")));

  EXPECT_THAT(GetUniverseDomainFromCredentialsJson(nlohmann::json{
                  {"universe_domain", "https://attacker.example"}}),
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("Invalid universe_domain")));

  // Case-insensitive normalization on credentials JSON.
  auto upper_ud = GetUniverseDomainFromCredentialsJson(
      nlohmann::json{{"universe_domain", "S3NSAPIS.FR"}});
  ASSERT_STATUS_RECORD_OK(upper_ud);
  EXPECT_EQ(*upper_ud, "s3nsapis.fr");

  // Impersonated service account inherits source_credentials universe_domain
  // and rejects conflicting nested values.
  nlohmann::json impersonated = {
      {"type", "impersonated_service_account"},
      {"source_credentials",
       {{"type", "authorized_user"}, {"universe_domain", "s3nsapis.fr"}}},
  };
  auto imp_ud = GetUniverseDomainFromCredentialsJson(impersonated);
  ASSERT_STATUS_RECORD_OK(imp_ud);
  EXPECT_EQ(*imp_ud, "s3nsapis.fr");

  impersonated["universe_domain"] = "googleapis.com";
  EXPECT_THAT(GetUniverseDomainFromCredentialsJson(impersonated),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("Mismatched universe_domain")));
}

TEST(ServiceAuthentication, UniverseDomainMatchAndMismatch) {
  ScopedEnvVar clear_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", std::nullopt);
  ScopedTempFile gdu_sa(MakeServiceAccountJson());
  ScopedTempFile tpc_sa(MakeServiceAccountJson("s3nsapis.fr"));

  // GDU SA with default UniverseDomain succeeds.
  ConnProps props{OauthMechanism::kServiceAccount, gdu_sa.path()};
  ASSERT_STATUS_RECORD_OK(CreateCredentials(props));

  // TPC SA without UniverseDomain fails closed (28000) instead of leaking to
  // googleapis.com.
  ConnProps leak_props{OauthMechanism::kServiceAccount, tpc_sa.path()};
  EXPECT_THAT(CreateCredentials(leak_props),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("does not match the universe domain")));

  // TPC SA with matching UniverseDomain (even without EnableGCD=1, and
  // case-insensitively) succeeds.
  ConnProps tpc_props{OauthMechanism::kServiceAccount, tpc_sa.path()};
  tpc_props.gcd.universe_domain = "S3NSAPIS.FR";
  ASSERT_STATUS_RECORD_OK(CreateCredentials(tpc_props));

  // GDU SA with GOOGLEAPIS.COM (case-insensitive) succeeds.
  ConnProps gdu_upper_props{OauthMechanism::kServiceAccount, gdu_sa.path()};
  gdu_upper_props.gcd.universe_domain = "GoogleAPIs.com";
  ASSERT_STATUS_RECORD_OK(CreateCredentials(gdu_upper_props));

  // GDU SA with attacker UniverseDomain fails closed (28000).
  ConnProps exfil_props{OauthMechanism::kServiceAccount, gdu_sa.path()};
  exfil_props.gcd.enable_gcd = true;
  exfil_props.gcd.universe_domain = "attacker.example";
  EXPECT_THAT(CreateCredentials(exfil_props),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("does not match the universe domain")));
}

TEST(UserAuthentication, UniverseDomainMatchAndMismatch) {
  ScopedEnvVar clear_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", std::nullopt);
  ScopedTempFile gdu_user(MakeUserAccountJson());
  ScopedTempFile tpc_user(MakeUserAccountJson("s3nsapis.fr"));

  ConnProps ok_gdu{OauthMechanism::kUserAccount, gdu_user.path()};
  ASSERT_STATUS_RECORD_OK(CreateCredentials(ok_gdu));

  // AuthorizedUserCredentials in google-cloud-cpp v3.10.0 does not override
  // Credentials::universe_domain(); verify direct JSON validation catches it.
  ConnProps mismatch_tpc{OauthMechanism::kUserAccount, tpc_user.path()};
  EXPECT_THAT(CreateCredentials(mismatch_tpc),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("does not match the universe domain")));

  ConnProps ok_tpc{OauthMechanism::kUserAccount, tpc_user.path()};
  ok_tpc.gcd.universe_domain = "s3nsapis.fr";
  ASSERT_STATUS_RECORD_OK(CreateCredentials(ok_tpc));

  ConnProps mismatch_gdu{OauthMechanism::kUserAccount, gdu_user.path()};
  mismatch_gdu.gcd.enable_gcd = true;
  mismatch_gdu.gcd.universe_domain = "attacker.example";
  EXPECT_THAT(CreateCredentials(mismatch_gdu),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("does not match the universe domain")));
}

TEST(ExternalAuthentication, UniverseDomainMatchAndMismatch) {
  ScopedEnvVar clear_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", std::nullopt);
  ScopedTempFile gdu_ext(MakeExternalAccountJson());
  ScopedTempFile tpc_ext(MakeExternalAccountJson("s3nsapis.fr"));

  ConnProps mismatch_tpc{OauthMechanism::kExternalUser, tpc_ext.path()};
  EXPECT_THAT(CreateCredentials(mismatch_tpc),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("does not match the universe domain")));

  ConnProps ok_tpc{OauthMechanism::kExternalUser, tpc_ext.path()};
  ok_tpc.gcd.universe_domain = "s3nsapis.fr";
  ASSERT_STATUS_RECORD_OK(CreateCredentials(ok_tpc));

  ConnProps mismatch_gdu{OauthMechanism::kExternalUser, gdu_ext.path()};
  mismatch_gdu.gcd.enable_gcd = true;
  mismatch_gdu.gcd.universe_domain = "attacker.example";
  EXPECT_THAT(CreateCredentials(mismatch_gdu),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("does not match the universe domain")));

  // BYOID properties carry no universe_domain and are restricted to
  // googleapis.com.
  ConnProps byoid_non_gdu{OauthMechanism::kExternalUser,
                          "",
                          "test-aud-url",
                          R"({"file": "test-creds-src"})",
                          "",
                          "test-sub-token-type",
                          "test-token-url"};
  byoid_non_gdu.gcd.enable_gcd = true;
  byoid_non_gdu.gcd.universe_domain = "s3nsapis.fr";
  EXPECT_THAT(CreateCredentials(byoid_non_gdu),
              StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                             HasSubstr("does not match the universe domain")));
}

TEST(DefaultApplicationAuthentication, FileAndMetadataUniverseDomainChecks) {
  ScopedEnvVar clear_env_ud("GOOGLE_CLOUD_UNIVERSE_DOMAIN", std::nullopt);
  ScopedTempFile gdu_adc(MakeServiceAccountJson("googleapis.com"));
  ScopedTempFile tpc_adc(MakeServiceAccountJson("s3nsapis.fr"));

  // Multi-tenant ADC exfiltration scenario via GOOGLE_APPLICATION_CREDENTIALS.
  {
    ScopedEnvVar set_adc("GOOGLE_APPLICATION_CREDENTIALS", gdu_adc.path());
    ConnProps exfil_props{OauthMechanism::kApplicationDefault, ""};
    exfil_props.gcd.enable_gcd = true;
    exfil_props.gcd.universe_domain = "localhost.test";
    EXPECT_THAT(
        CreateCredentials(exfil_props),
        StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                       HasSubstr("does not match the universe domain")));
  }

  // TPC ADC file with omitted UniverseDomain fails closed; matching succeeds.
  {
    ScopedEnvVar set_adc("GOOGLE_APPLICATION_CREDENTIALS", tpc_adc.path());
    ConnProps default_props{OauthMechanism::kApplicationDefault, ""};
    EXPECT_THAT(
        CreateCredentials(default_props),
        StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                       HasSubstr("does not match the universe domain")));

    ConnProps matching_props{OauthMechanism::kApplicationDefault, ""};
    matching_props.gcd.universe_domain = "s3nsapis.fr";
    ASSERT_STATUS_RECORD_OK(CreateCredentials(matching_props));
  }

  // Well-known gcloud ADC path is also checked when env var is unset.
  {
    ScopedEnvVar unset_adc("GOOGLE_APPLICATION_CREDENTIALS", std::nullopt);
    ScopedEnvVar set_gcloud_adc("GOOGLE_GCLOUD_ADC_PATH_OVERRIDE",
                                gdu_adc.path());
    ConnProps exfil_props{OauthMechanism::kApplicationDefault, ""};
    exfil_props.gcd.universe_domain = "attacker.example";
    EXPECT_THAT(
        CreateCredentials(exfil_props),
        StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                       HasSubstr("does not match the universe domain")));
  }

  // Metadata-server ADC fallback fails closed when non-default UniverseDomain
  // cannot be verified against MDS.
  {
    ScopedEnvVar unset_adc("GOOGLE_APPLICATION_CREDENTIALS", std::nullopt);
    ScopedEnvVar missing_gcloud_adc(
        "GOOGLE_GCLOUD_ADC_PATH_OVERRIDE",
        std::string("/nonexistent/gcloud_adc.json"));
    ScopedEnvVar unreachable_mds("GCE_METADATA_HOST",
                                 std::string("127.0.0.1:1"));
    Options opts;
    opts.set<::google::cloud::internal::UniverseDomainRetryPolicyOption>(
        std::make_shared<NoRetryUniverseDomainPolicy>());
    ConnProps mds_props{OauthMechanism::kApplicationDefault, ""};
    mds_props.gcd.enable_gcd = true;
    mds_props.gcd.universe_domain = "s3nsapis.fr";
    EXPECT_THAT(
        CreateCredentials(mds_props, opts),
        StatusRecordIs(
            odbc_internal::SQLStates::k_28000(),
            HasSubstr("Failed to verify metadata server universe domain")));
  }

  // Corrupt non-JSON ADC file fails closed instead of falling through.
  {
    ScopedTempFile corrupt_adc("not-json-or-p12");
    ScopedEnvVar set_adc("GOOGLE_APPLICATION_CREDENTIALS", corrupt_adc.path());
    ConnProps default_props{OauthMechanism::kApplicationDefault, ""};
    EXPECT_THAT(CreateCredentials(default_props),
                StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                               HasSubstr("Cannot open credentials file")));
  }
}

}  // namespace google::cloud::odbc_bigquery_client_interface
