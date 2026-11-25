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
#include "google/cloud/universe_domain_options.h"
#include "absl/log/log.h"
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::odbc_bigquery_client_interface::SetEnv;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

auto const kSelfSignedJwtEnvVar =
    "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT";

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
    std::string const& credentials_file_path, Options const& options) {
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

  return ::google::cloud::internal::MakeUserAccountCredentials(contents,
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
    std::string const& credentials_file_path, Options const& options) {
  if (credentials_file_path.empty()) {
    LOG(ERROR) << "CreateExternalAuthCredentialsJSON:: The path to the "
                  "external auth JSON file can't be empty";
    return StatusRecord{
        SQLStates::k_HY000(),
        "The path to the external auth JSON file can't be empty"};
  }
  // Client libraries MakeGoogleDefaultCredentials() processes the
  // GOOGLE_APPLICATION_CREDENTIALS env var to get to the file with
  // credentials. It parses the file and works for different authentication
  // types including external authentication. For more details see the
  // link below:
  // https://github.com/googleapis/google-cloud-cpp/blob/d3104eff1632bc3793a29572315ec7e80b143746/google/cloud/internal/unified_rest_credentials.cc#L97

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

  return ::google::cloud::MakeExternalAccountCredentials(contents, options);
}

namespace {

constexpr absl::string_view kHttpsPrefix = "https://";
constexpr absl::string_view kHttpPrefix = "http://";
constexpr absl::string_view kStsDefaultHost = "sts.googleapis.com";
constexpr absl::string_view kStsPscPrefix = "sts-";
constexpr absl::string_view kStsPscSuffix = ".p.googleapis.com";

bool IsAsciiAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsAsciiDigit(char c) { return c >= '0' && c <= '9'; }

bool IsAsciiAlnum(char c) { return IsAsciiAlpha(c) || IsAsciiDigit(c); }

std::string AsciiToLower(absl::string_view s) {
  std::string out(s.data(), s.size());
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

bool StartsWithIgnoreCase(absl::string_view s, absl::string_view prefix) {
  if (s.size() < prefix.size()) {
    return false;
  }
  return AsciiToLower(s.substr(0, prefix.size())) == AsciiToLower(prefix);
}

bool HasInvalidUrlChars(absl::string_view url) {
  for (char c : url) {
    auto uc = static_cast<unsigned char>(c);
    if (uc <= 0x20 || uc >= 0x7F) {
      return true;
    }
  }
  return false;
}

bool IsValidDnsLabel(absl::string_view label) {
  if (label.empty() || label.size() > 63) {
    return false;
  }
  if (!IsAsciiAlnum(label.front()) || !IsAsciiAlnum(label.back())) {
    return false;
  }
  for (char c : label) {
    if (!IsAsciiAlnum(c) && c != '-') {
      return false;
    }
  }
  return true;
}

bool IsValidDnsHostname(absl::string_view host) {
  if (host.empty() || host.size() > 253) {
    return false;
  }
  size_t label_count = 0;
  absl::string_view last_label;
  size_t start = 0;
  while (start <= host.size()) {
    size_t dot = host.find('.', start);
    absl::string_view label = (dot == absl::string_view::npos)
                                  ? host.substr(start)
                                  : host.substr(start, dot - start);
    if (!IsValidDnsLabel(label)) {
      return false;
    }
    last_label = label;
    ++label_count;
    if (dot == absl::string_view::npos) {
      break;
    }
    start = dot + 1;
  }
  return label_count >= 2 && IsAsciiAlpha(last_label.front());
}

bool IsValidPort(absl::string_view port_str) {
  if (port_str.empty() || port_str.size() > 5) {
    return false;
  }
  int port = 0;
  for (char c : port_str) {
    if (!IsAsciiDigit(c)) {
      return false;
    }
    port = port * 10 + (c - '0');
  }
  return port >= 1 && port <= 65535;
}

bool IsAllowedStsHost(absl::string_view lower_host,
                      absl::string_view universe_domain) {
  if (lower_host == kStsDefaultHost) {
    return true;
  }
  if (lower_host.size() > kStsPscPrefix.size() + kStsPscSuffix.size() &&
      lower_host.substr(0, kStsPscPrefix.size()) == kStsPscPrefix &&
      lower_host.substr(lower_host.size() - kStsPscSuffix.size()) ==
          kStsPscSuffix) {
    absl::string_view endpoint = lower_host.substr(
        kStsPscPrefix.size(),
        lower_host.size() - kStsPscPrefix.size() - kStsPscSuffix.size());
    if (IsValidDnsLabel(endpoint) &&
        (kStsPscPrefix.size() + endpoint.size()) <= 63) {
      return true;
    }
  }
  if (!universe_domain.empty() && IsValidDnsHostname(universe_domain)) {
    std::string expected_sts_host = "sts." + AsciiToLower(universe_domain);
    if (lower_host == expected_sts_host) {
      return true;
    }
  }
  return false;
}

bool IsValidHttpOrHttpsCredentialUrl(absl::string_view url) {
  if (url.empty() || HasInvalidUrlChars(url)) {
    return false;
  }
  absl::string_view rest;
  if (StartsWithIgnoreCase(url, kHttpsPrefix)) {
    rest = url.substr(kHttpsPrefix.size());
  } else if (StartsWithIgnoreCase(url, kHttpPrefix)) {
    rest = url.substr(kHttpPrefix.size());
  } else {
    return false;
  }
  size_t authority_end = rest.find_first_of("/?#");
  absl::string_view authority = (authority_end == absl::string_view::npos)
                                    ? rest
                                    : rest.substr(0, authority_end);
  if (authority.empty() || authority.front() == ':' ||
      authority.find('\\') != absl::string_view::npos) {
    return false;
  }
  return true;
}

}  // namespace

StatusRecord ValidateBYOIDTokenUrl(absl::string_view token_url,
                                   absl::string_view universe_domain) {
  if (token_url.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "BYOID_TOKENURL cannot be empty"};
  }
  if (HasInvalidUrlChars(token_url) ||
      token_url.find('\\') != absl::string_view::npos ||
      token_url.find('%') != absl::string_view::npos) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid BYOID_TOKENURL: contains disallowed characters"};
  }
  if (!StartsWithIgnoreCase(token_url, kHttpsPrefix)) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid BYOID_TOKENURL: scheme must be https"};
  }
  absl::string_view rest = token_url.substr(kHttpsPrefix.size());
  size_t authority_end = rest.find_first_of("/?#");
  absl::string_view authority = (authority_end == absl::string_view::npos)
                                    ? rest
                                    : rest.substr(0, authority_end);
  absl::string_view tail = (authority_end == absl::string_view::npos)
                               ? absl::string_view{}
                               : rest.substr(authority_end);
  if (!tail.empty() && tail.front() != '/') {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid BYOID_TOKENURL: malformed URL path"};
  }
  if (authority.empty() || authority.find('@') != absl::string_view::npos) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid BYOID_TOKENURL: missing host or contains userinfo"};
  }

  absl::string_view host = authority;
  size_t colon_pos = authority.find(':');
  if (colon_pos != absl::string_view::npos) {
    host = authority.substr(0, colon_pos);
    absl::string_view port_str = authority.substr(colon_pos + 1);
    if (!IsValidPort(port_str)) {
      return StatusRecord{SQLStates::k_HY000(),
                          "Invalid BYOID_TOKENURL: invalid port"};
    }
  }

  if (!IsValidDnsHostname(host)) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid BYOID_TOKENURL: invalid host"};
  }

  std::string lower_host = AsciiToLower(host);
  if (!IsAllowedStsHost(lower_host, universe_domain)) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid BYOID_TOKENURL: host is not an allowed STS endpoint"};
  }

  return StatusRecord::Ok();
}

StatusRecordOr<nlohmann::json> CreateJsonCredsObject(
    std::string const& byoid_aud_url, std::string const& byoid_creds_source,
    std::string const& byoid_pool_user_project,
    std::string const& byoid_sub_token_type,
    std::string const& byoid_token_url) {
  nlohmann::json creds_source;
  try {
    creds_source = nlohmann::json::parse(byoid_creds_source);
  } catch (nlohmann::json::parse_error const&) {
    return StatusRecord{SQLStates::k_HY000(),
                        "BYOID_CREDENTIALSOURCE is not a valid JSON string"};
  }
  if (!creds_source.is_object()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "BYOID_CREDENTIALSOURCE must be a JSON object"};
  }
  if (auto it = creds_source.find("url"); it != creds_source.end()) {
    if (!it->is_string() ||
        !IsValidHttpOrHttpsCredentialUrl(it->get_ref<std::string const&>())) {
      return StatusRecord{
          SQLStates::k_HY000(),
          "BYOID_CREDENTIALSOURCE url must be a valid http or https URL"};
    }
  }

  nlohmann::json json;
  json["type"] = "external_account";
  json["audience"] = byoid_aud_url;
  json["credential_source"] = std::move(creds_source);
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
  std::string universe_domain;
  if (options.has<google::cloud::internal::UniverseDomainOption>()) {
    universe_domain =
        options.get<google::cloud::internal::UniverseDomainOption>();
  } else if (conn_props.gcd.enable_gcd) {
    universe_domain = conn_props.gcd.universe_domain;
  }
  StatusRecord token_url_status =
      ValidateBYOIDTokenUrl(conn_props.byoid_token_url, universe_domain);
  if (!token_url_status.ok()) {
    LOG(ERROR)
        << "CreateExternalAccountAuthenticationBYOID::ValidateBYOIDTokenUrl:: "
        << token_url_status.message;
    return token_url_status;
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
      return CreateUserCredentials(conn_props.credentials_file_path, options);
    case OauthMechanism::kApplicationDefault:
      return CreateApplicationDefaultCredentials(options);
    case OauthMechanism::kExternalUser: {
      if (conn_props.credentials_file_path.empty() &&
          !IsBYOIDPropsSet(conn_props)) {
        LOG(ERROR) << "CreateCredentials:: The path to the external auth JSON "
                      "file can't be empty";
        return StatusRecord{
            SQLStates::k_HY000(),
            "The path to the external auth JSON file can't be empty"};
      }
      if (!conn_props.credentials_file_path.empty()) {
        // Call creation of external auth via JSON file
        return CreateExternalAuthCredentialsJSON(
            conn_props.credentials_file_path, options);
      }
      // Call creation of external auth via BYOID properties.
      return CreateExternalAccountAuthenticationBYOID(conn_props, options);
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
