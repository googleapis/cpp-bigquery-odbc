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
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/status_or.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include <fstream>
#include <iterator>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::odbc_bigquery_client_interface::SetEnv;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

auto const kSelfSignedJwtEnvVar =
    "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT";

namespace {

std::shared_ptr<Credentials> UpgradeToAccessTokenCredentialsIfAvailable(
    std::shared_ptr<Credentials> creds, Options const& options) {
  if (!creds) return creds;
  auto generator = ::google::cloud::oauth2::MakeAccessTokenGenerator(*creds);
  if (generator) {
    auto token = GetOAuth2Token(generator);
    if (token.Ok()) {
      return ::google::cloud::MakeAccessTokenCredentials(
          token->token, token->expiration, options);
    }
  }
  return creds;
}

std::string NormalizeWorkforceAudience(std::string const& audience) {
  if (absl::StartsWith(audience, "locations/")) {
    return "//iam.googleapis.com/" + audience;
  }
  return audience;
}

StatusRecord ResolveClientCredentials(ConnProps const& conn_props,
                                      bool is_workforce,
                                      std::string& client_id_out,
                                      std::string& client_secret_out) {
  if (!conn_props.use_default_client_credentials) {
    if (conn_props.client_id.empty() || conn_props.client_secret.empty()) {
      return StatusRecord{SQLStates::k_HY000(),
                          "ClientId and ClientSecret must be provided when "
                          "UseDefaultClientCredentials is disabled"};
    }
    client_id_out = conn_props.client_id;
    client_secret_out = conn_props.client_secret;
    return StatusRecord::Ok();
  }
  if (!conn_props.client_id.empty() && !conn_props.client_secret.empty()) {
    client_id_out = conn_props.client_id;
    client_secret_out = conn_props.client_secret;
    return StatusRecord::Ok();
  }
  if (is_workforce) {
    client_id_out = kDefaultWifOAuthClientId;
    client_secret_out = kDefaultWifOAuthClientSecret;
  } else {
    client_id_out = kDefaultOAuthClientId;
    client_secret_out = kDefaultOAuthClientSecret;
  }
  return StatusRecord::Ok();
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateWorkforceRefreshTokenCredentials(
    ConnProps const& conn_props, Options const& options,
    nlohmann::json const* login_cfg = nullptr) {
  std::string universe_domain = !conn_props.gcd.universe_domain.empty()
                                    ? conn_props.gcd.universe_domain
                                    : kDefaultUniverseDomain;
  std::string audience = NormalizeWorkforceAudience(conn_props.byoid_aud_url);
  std::string token_url;
  if (login_cfg != nullptr && login_cfg->is_object()) {
    std::string ud_json = login_cfg->value("universe_domain", "");
    if (!ud_json.empty()) universe_domain = ud_json;
    std::string aud_json = login_cfg->value("audience", "");
    if (!aud_json.empty()) audience = NormalizeWorkforceAudience(aud_json);
    token_url = login_cfg->value("token_url", "");
  }
  if (!audience.empty()) {
    auto provider_or = ExtractWorkforceProviderName(audience);
    if (!provider_or) return provider_or.GetStatusRecord();
  }
  if (token_url.empty()) {
    if (!conn_props.byoid_token_url.empty() &&
        conn_props.byoid_token_url != kDefaultTokenUrl) {
      token_url = conn_props.byoid_token_url;
    } else {
      token_url =
          absl::StrCat("https://sts.", universe_domain, "/v1/oauthtoken");
    }
  }
  std::string client_id;
  std::string client_secret;
  auto status = ResolveClientCredentials(conn_props, /*is_workforce=*/true,
                                         client_id, client_secret);
  if (!status.ok()) return status;

  nlohmann::json json = {
      {"type", "external_account_authorized_user"},
      {"client_id", client_id},
      {"client_secret", client_secret},
      {"refresh_token", conn_props.refresh_token},
      {"token_url", token_url},
  };
  if (!audience.empty()) {
    json["audience"] = audience;
  }
  if (!universe_domain.empty()) {
    json["universe_domain"] = universe_domain;
  }
  return UpgradeToAccessTokenCredentialsIfAvailable(
      ::google::cloud::MakeExternalAccountCredentials(json.dump(), options),
      options);
}

}  // namespace

StatusRecordOr<std::string> ExtractWorkforceProviderName(
    std::string const& audience) {
  std::string provider_name;
  std::size_t const pos = audience.find("/locations/");
  if (pos != std::string::npos) {
    provider_name = audience.substr(pos + 1);
  } else if (absl::StartsWith(audience, "locations/")) {
    provider_name = audience;
  }
  if (provider_name.empty() || !IsWorkforcePoolAudience(provider_name)) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid Workforce Identity Federation audience URI: " + audience};
  }
  return provider_name;
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateServiceCredentials(
    std::string const& credentials_file_path, Options const& options) {
  if (credentials_file_path.empty()) {
    LOG(ERROR)
        << "CreateServiceCredentials:: The path to the file can't be empty";
    return StatusRecord{SQLStates::k_HY000(),
                        "The path to the file can't be empty"};
  }
  std::ifstream creds_stream(credentials_file_path);
  if (!creds_stream) {
    LOG(ERROR) << "CreateServiceCredentials:: Could not open Service Account "
                  "key file: "
               << credentials_file_path;
    return StatusRecord{
        SQLStates::k_HY000(),
        "Could not open Service Account key file: " + credentials_file_path};
  }

  std::string contents((std::istreambuf_iterator<char>(creds_stream)),
                       std::istreambuf_iterator<char>());

  if (contents.empty()) {
    LOG(ERROR) << "CreateServiceCredentials:: Service Account key file is "
                  "empty or could not be read: "
               << credentials_file_path;
    return StatusRecord{
        SQLStates::k_HY000(),
        "Service Account key file is empty or could not be read: " +
            credentials_file_path};
  }

  return ::google::cloud::MakeServiceAccountCredentials(contents, options);
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateUserCredentials(
    ConnProps const& conn_props, Options const& options) {
  bool const has_wif_audience =
      IsWorkforcePoolAudience(conn_props.byoid_aud_url);
  if (!conn_props.refresh_token.empty()) {
    if (!conn_props.credentials_file_path.empty()) {
      std::ifstream cfg_stream(conn_props.credentials_file_path);
      if (cfg_stream) {
        std::string cfg_contents((std::istreambuf_iterator<char>(cfg_stream)),
                                 std::istreambuf_iterator<char>());
        auto cfg_json = nlohmann::json::parse(cfg_contents, nullptr, false);
        if (cfg_json.is_object() &&
            cfg_json.value("type", "") ==
                "external_account_authorized_user_login_config") {
          return CreateWorkforceRefreshTokenCredentials(conn_props, options,
                                                        &cfg_json);
        }
      }
    }
    if (has_wif_audience ||
        (!conn_props.gcd.universe_domain.empty() &&
         conn_props.gcd.universe_domain != kDefaultUniverseDomain)) {
      return CreateWorkforceRefreshTokenCredentials(conn_props, options);
    }
    std::string client_id;
    std::string client_secret;
    auto client_status = ResolveClientCredentials(
        conn_props, /*is_workforce=*/false, client_id, client_secret);
    if (!client_status.ok()) return client_status;
    nlohmann::json json = {
        {"type", "authorized_user"},
        {"client_id", client_id},
        {"client_secret", client_secret},
        {"refresh_token", conn_props.refresh_token},
    };
    return UpgradeToAccessTokenCredentialsIfAvailable(
        ::google::cloud::internal::MakeUserAccountCredentials(json.dump(),
                                                              options),
        options);
  }

  std::string const& credentials_file_path = conn_props.credentials_file_path;
  if (credentials_file_path.empty()) {
    LOG(ERROR) << "CreateUserCredentials:: The path to the file can't be empty";
    return StatusRecord{SQLStates::k_HY000(),
                        "The path to the file can't be empty"};
  }

  std::ifstream creds_stream(credentials_file_path);
  if (!creds_stream) {
    LOG(ERROR) << "CreateUserCredentials:: Could not open User Account "
                  "key file: "
               << credentials_file_path;
    return StatusRecord{
        SQLStates::k_HY000(),
        "Could not open User Account key file: " + credentials_file_path};
  }

  std::string contents((std::istreambuf_iterator<char>(creds_stream)),
                       std::istreambuf_iterator<char>());

  if (contents.empty()) {
    LOG(ERROR) << "CreateUserCredentials:: User Account key file is "
                  "empty or could not be read: "
               << credentials_file_path;
    return StatusRecord{
        SQLStates::k_HY000(),
        "User Account key file is empty or could not be read: " +
            credentials_file_path};
  }

  auto json = nlohmann::json::parse(contents, nullptr, false);
  if (json.is_object()) {
    std::string const cred_type = json.value("type", "");
    if (cred_type == "external_account_authorized_user") {
      return UpgradeToAccessTokenCredentialsIfAvailable(
          ::google::cloud::MakeExternalAccountCredentials(contents, options),
          options);
    }
    if (cred_type == "external_account") {
      return ::google::cloud::MakeExternalAccountCredentials(contents, options);
    }
  }

  return UpgradeToAccessTokenCredentialsIfAvailable(
      ::google::cloud::internal::MakeUserAccountCredentials(contents, options),
      options);
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateApplicationDefaultCredentials(Options const& options) {
  // C++ client library in google-cloud-cpp first checks
  // GOOGLE_APPLICATION_CREDENTIALS env var and use it if it's present. Then it
  // looks for a 'default' location of the file with credentials.
  return ::google::cloud::MakeGoogleDefaultCredentials(options);
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateExternalAuthCredentialsJSON(
    ConnProps const& conn_props, Options const& options) {
  std::string const& credentials_file_path = conn_props.credentials_file_path;
  if (credentials_file_path.empty()) {
    LOG(ERROR) << "CreateExternalAuthCredentialsJSON:: The path to the "
                  "external auth JSON file can't be empty";
    return StatusRecord{
        SQLStates::k_HY000(),
        "The path to the external auth JSON file can't be empty"};
  }

  std::ifstream creds_stream(credentials_file_path);
  if (!creds_stream) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Could not open External Account key file: " + credentials_file_path};
  }

  std::string contents((std::istreambuf_iterator<char>(creds_stream)),
                       std::istreambuf_iterator<char>());

  if (contents.empty()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "External Account key file is empty or could not be read: " +
            credentials_file_path};
  }

  auto json = nlohmann::json::parse(contents, nullptr, false);
  if (json.is_object()) {
    std::string const cred_type = json.value("type", "");
    if (cred_type == "external_account_authorized_user_login_config" &&
        !conn_props.refresh_token.empty()) {
      return CreateWorkforceRefreshTokenCredentials(conn_props, options, &json);
    }
    if (cred_type == "external_account_authorized_user") {
      return UpgradeToAccessTokenCredentialsIfAvailable(
          ::google::cloud::MakeExternalAccountCredentials(contents, options),
          options);
    }
  }

  return ::google::cloud::MakeExternalAccountCredentials(contents, options);
}

StatusRecordOr<nlohmann::json> CreateJsonCredsObject(
    std::string const& byoid_aud_url, std::string const& byoid_creds_source,
    std::string const& byoid_pool_user_project,
    std::string const& byoid_sub_token_type,
    std::string const& byoid_token_url) {
  nlohmann::json json;
  json["type"] = "external_account";
  json["audience"] = byoid_aud_url;
  try {
    json["credential_source"] = nlohmann::json::parse(byoid_creds_source);
  } catch (nlohmann::json::parse_error const&) {
    return StatusRecord{SQLStates::k_HY000(),
                        "BYOID_CREDENTIALSOURCE is not a valid JSON string"};
  }
  json["subject_token_type"] = byoid_sub_token_type;
  json["token_url"] = byoid_token_url;
  if (!byoid_pool_user_project.empty()) {
    json["workforce_pool_user_project"] = byoid_pool_user_project;
  }
  return json;
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateExternalAccountAuthenticationBYOID(ConnProps const& conn_props,
                                         Options const& options) {
  if (!IsBYOIDPropsSet(conn_props)) {
    LOG(ERROR)
        << "CreateExternalAccountAuthenticationBYOID:: Unable to create "
           "external auth credentials: Required BYOID Properties are not set ";
    return StatusRecord{SQLStates::k_HY000(),
                        "Unable to create external auth credentials: Required "
                        "BYOID Properties are not set "};
  }
  StatusRecordOr<nlohmann::json> json_creds = CreateJsonCredsObject(
      conn_props.byoid_aud_url, conn_props.byoid_creds_src,
      conn_props.byoid_pool_user_project, conn_props.byoid_subj_token_type,
      conn_props.byoid_token_url);
  if (!json_creds) {
    LOG(ERROR)
        << "CreateExternalAccountAuthenticationBYOID::CreateJsonCredsObject:: "
        << json_creds.GetStatusRecord().message;
    return json_creds.GetStatusRecord();
  }
  return ::google::cloud::MakeExternalAccountCredentials((*json_creds).dump(),
                                                         options);
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateCredentials(
    ConnProps const& conn_props, Options const& options) {
  switch (conn_props.auth_mechanism) {
    case OauthMechanism::kServiceAccount:
      return CreateServiceCredentials(conn_props.credentials_file_path,
                                      options);
    case OauthMechanism::kUserAccount:
      return CreateUserCredentials(conn_props, options);
    case OauthMechanism::kApplicationDefault:
      return CreateApplicationDefaultCredentials(options);
    case OauthMechanism::kExternalUser: {
      if (!conn_props.credentials_file_path.empty()) {
        return CreateExternalAuthCredentialsJSON(conn_props, options);
      }
      if (IsBYOIDPropsSet(conn_props)) {
        return CreateExternalAccountAuthenticationBYOID(conn_props, options);
      }
      if (!conn_props.refresh_token.empty() &&
          IsWorkforcePoolAudience(conn_props.byoid_aud_url)) {
        return CreateWorkforceRefreshTokenCredentials(conn_props, options);
      }
      LOG(ERROR) << "CreateCredentials:: The path to the external auth JSON "
                    "file can't be empty";
      return StatusRecord{
          SQLStates::k_HY000(),
          "The path to the external auth JSON file can't be empty"};
    }
  }
  LOG(ERROR) << "CreateCredentials:: OauthMechanism enum is invalid";
  return StatusRecord{SQLStates::k_HY000(), "OauthMechanism enum is invalid"};
}

StatusRecordOr<AccessToken> GetOAuth2Token(
    std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> const&
        generator) {
  // We need to set env var for service account to force it to make a request
  // to Google Cloud. Then we return the value of this env var to the previous
  // state. If the env var is unset, token will be created locally, without
  // any request to Google Cloud.
  auto self_signed_jwt_disabled = GetEnv(kSelfSignedJwtEnvVar);
  SetEnv(kSelfSignedJwtEnvVar, "true");
  StatusOr<AccessToken> access_token = generator->GetToken();
  SetEnv(kSelfSignedJwtEnvVar, self_signed_jwt_disabled);
  return StatusRecordOr<AccessToken>::ConvertFromStatusOr(access_token);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
