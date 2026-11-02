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

// clang-format off
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
// clang-format on

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/setenv.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include <gmock/gmock.h>
#include <cstdio>
#include <fstream>
#include <map>
#include <thread>

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

namespace {

std::map<std::string, std::string> ParseUrlQueryForTest(
    std::string const& url) {
  std::map<std::string, std::string> out;
  auto qpos = url.find('?');
  if (qpos == std::string::npos) return out;
  std::string query = url.substr(qpos + 1);
  std::size_t start = 0;
  while (start <= query.size()) {
    auto amp = query.find('&', start);
    std::string kv = (amp == std::string::npos)
                         ? query.substr(start)
                         : query.substr(start, amp - start);
    auto eq = kv.find('=');
    if (eq != std::string::npos) {
      std::string key = kv.substr(0, eq);
      std::string val = kv.substr(eq + 1);
      std::string decoded;
      for (std::size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '%' && i + 2 < val.size()) {
          decoded.push_back(
              static_cast<char>(std::stoi(val.substr(i + 1, 2), nullptr, 16)));
          i += 2;
        } else if (val[i] == '+') {
          decoded.push_back(' ');
        } else {
          decoded.push_back(val[i]);
        }
      }
      out[key] = decoded;
    }
    if (amp == std::string::npos) break;
    start = amp + 1;
  }
  return out;
}

int ExtractPortFromRedirectUri(std::string const& redirect_uri) {
  auto colon = redirect_uri.rfind(':');
  auto slash = redirect_uri.find('/', colon);
  return std::stoi(redirect_uri.substr(colon + 1, slash - colon - 1));
}

std::string SendLoopbackCallbackRequest(int port, std::string const& path,
                                        bool open_preconnect_socket = false) {
#ifdef _WIN32
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(static_cast<std::uint16_t>(port));

  if (open_preconnect_socket) {
    auto pre_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    connect(pre_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#ifdef _WIN32
    closesocket(pre_sock);
#else
    close(pre_sock);
#endif
  }

  auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  std::string req = "GET " + path +
                    " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
  send(sock, req.data(), static_cast<int>(req.size()), 0);
  char buf[4096] = {0};
  int n = recv(sock, buf, sizeof(buf) - 1, 0);
#ifdef _WIN32
  closesocket(sock);
  WSACleanup();
#else
  close(sock);
#endif
  return n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : std::string{};
}

}  // namespace

TEST(PkceHelpers, Rfc7636S256VectorAndVerifierLength) {
  EXPECT_EQ(
      ComputePkceCodeChallenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),
      "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");

  std::string verifier = GeneratePkceCodeVerifier();
  EXPECT_GE(verifier.size(), 43U);
  EXPECT_LE(verifier.size(), 128U);
  EXPECT_EQ(verifier.find('='), std::string::npos);
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

TEST(InteractiveOAuthSignIn, StandardGoogleAccountPkceFlow) {
  ClearInteractiveOAuthTokenCache();
  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kUserAccount;
  props.use_default_client_credentials = false;
  props.client_id = "my-installed-client-id";
  props.client_secret = "my-installed-client-secret";

  std::string captured_challenge;
  std::string captured_redirect_uri;
  std::string callback_http_response;
  std::thread client_thread;

  auto launcher = [&](std::string const& url) -> odbc_internal::StatusRecord {
    EXPECT_THAT(url,
                HasSubstr("https://accounts.google.com/o/oauth2/v2/auth?"));
    auto q = ParseUrlQueryForTest(url);
    EXPECT_EQ(q["client_id"], "my-installed-client-id");
    EXPECT_EQ(q["code_challenge_method"], "S256");
    EXPECT_EQ(q["access_type"], "offline");
    EXPECT_EQ(q["prompt"], "consent");
    captured_challenge = q["code_challenge"];
    captured_redirect_uri = q["redirect_uri"];
    int port = ExtractPortFromRedirectUri(captured_redirect_uri);
    std::string state = q["state"];
    client_thread = std::thread([port, state, &callback_http_response]() {
      callback_http_response = SendLoopbackCallbackRequest(
          port, "/?code=mock%2Fauth_code&state=" + state,
          /*open_preconnect_socket=*/true);
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto post_fn =
      [&](std::string const& token_url,
          std::vector<std::pair<std::string, std::string>> const& form_data)
      -> StatusOr<std::string> {
    EXPECT_EQ(token_url, "https://oauth2.googleapis.com/token");
    std::map<std::string, std::string> form(form_data.begin(), form_data.end());
    EXPECT_EQ(form["grant_type"], "authorization_code");
    EXPECT_EQ(form["code"], "mock/auth_code");
    EXPECT_EQ(form["redirect_uri"], captured_redirect_uri);
    EXPECT_EQ(form["client_id"], "my-installed-client-id");
    EXPECT_EQ(form["client_secret"], "my-installed-client-secret");
    EXPECT_EQ(ComputePkceCodeChallenge(form["code_verifier"]),
              captured_challenge);
    return std::string{
        R"({"access_token":"mock_access_tok","expires_in":3600,"refresh_token":"mock_new_refresh_token","token_type":"Bearer"})"};
  };

  auto res = RunInteractiveOAuthSignIn(props, Options{}, launcher, post_fn,
                                       std::chrono::seconds(5));
  if (client_thread.joinable()) client_thread.join();
  ASSERT_STATUS_RECORD_OK(res);
  EXPECT_EQ(res->refresh_token, "mock_new_refresh_token");
  EXPECT_THAT(callback_http_response, HasSubstr("200 OK"));

  ConnProps connect_props = props;
  connect_props.allow_interactive_browser = true;
  auto cached_creds = CreateCredentials(connect_props);
  ASSERT_STATUS_RECORD_OK(cached_creds);
}

TEST(InteractiveOAuthSignIn, RejectsStateMismatch) {
  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kUserAccount;

  std::thread client_thread;
  auto launcher = [&](std::string const& url) -> odbc_internal::StatusRecord {
    auto q = ParseUrlQueryForTest(url);
    int port = ExtractPortFromRedirectUri(q["redirect_uri"]);
    client_thread = std::thread([port]() {
      SendLoopbackCallbackRequest(port, "/?code=abc&state=tampered_state");
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto res = RunInteractiveOAuthSignIn(props, Options{}, launcher, nullptr,
                                       std::chrono::seconds(5));
  if (client_thread.joinable()) client_thread.join();
  EXPECT_THAT(res, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                  HasSubstr("state mismatch")));
}

TEST(InteractiveOAuthSignIn, CancelsWaitWhenRequested) {
  ConnProps props{};
  props.auth_mechanism = OauthMechanism::kUserAccount;

  std::thread cancel_thread;
  auto launcher = [&](std::string const&) -> odbc_internal::StatusRecord {
    cancel_thread = std::thread([]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      CancelInteractiveOAuthSignIn();
    });
    return odbc_internal::StatusRecord::Ok();
  };

  auto res = RunInteractiveOAuthSignIn(props, Options{}, launcher, nullptr,
                                       std::chrono::seconds(30));
  if (cancel_thread.joinable()) cancel_thread.join();
  EXPECT_THAT(res, StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                                  HasSubstr("cancelled")));
}
}  // namespace google::cloud::odbc_bigquery_client_interface
