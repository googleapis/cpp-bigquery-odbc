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
#include "google/cloud/internal/oauth2_google_application_default_credentials_file.h"
#include "google/cloud/internal/unified_rest_credentials.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/status_or.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <vector>
namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::odbc_bigquery_client_interface::SetEnv;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

auto const kSelfSignedJwtEnvVar =
    "GOOGLE_CLOUD_CPP_EXPERIMENTAL_DISABLE_SELF_SIGNED_JWT";
auto constexpr kUniverseDomainEnvVar = "GOOGLE_CLOUD_UNIVERSE_DOMAIN";
std::size_t constexpr kMaxDnsDomainLength = 253;
std::size_t constexpr kMaxDnsLabelLength = 63;
int constexpr kMaxImpersonationChainDepth = 16;

namespace {

bool IsAsciiAlpha(char ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool IsAsciiAlphaNum(char ch) {
  return IsAsciiAlpha(ch) || (ch >= '0' && ch <= '9');
}

StatusRecordOr<std::optional<std::string>> ExtractExplicitUniverseDomainField(
    nlohmann::json const& json_obj) {
  auto it = json_obj.find("universe_domain");
  if (it == json_obj.end()) {
    return std::optional<std::string>{std::nullopt};
  }
  if (!it->is_string()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid type for universe_domain field in credentials; expected "
        "string"};
  }
  auto universe_domain = it->get<std::string>();
  if (universe_domain.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "universe_domain field in credentials cannot be empty"};
  }
  auto syntax_status = ValidateUniverseDomain(universe_domain);
  if (!syntax_status.ok()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        absl::StrCat("Invalid universe_domain field in credentials: ",
                     universe_domain)};
  }
  return std::optional<std::string>{absl::AsciiStrToLower(universe_domain)};
}

StatusRecord ValidateCredentialsUniverseDomainMatch(
    absl::string_view credential_universe_domain,
    absl::string_view effective_universe_domain) {
  if (!absl::EqualsIgnoreCase(credential_universe_domain,
                              effective_universe_domain)) {
    auto message = absl::StrCat(
        "The configured universe domain (", effective_universe_domain,
        ") does not match the universe domain of the credentials (",
        credential_universe_domain, ")");
    LOG(ERROR) << "ValidateCredentialsUniverseDomainMatch:: " << message;
    return StatusRecord{SQLStates::k_28000(), std::move(message)};
  }
  return StatusRecord::Ok();
}

StatusRecordOr<std::string> ReadCredentialFileContents(
    std::string const& credentials_file_path, absl::string_view fn_name,
    absl::string_view credential_kind, absl::string_view empty_path_message) {
  if (credentials_file_path.empty()) {
    LOG(ERROR) << fn_name << ":: " << empty_path_message;
    return StatusRecord{SQLStates::k_HY000(), std::string(empty_path_message)};
  }

  std::ifstream creds_stream(credentials_file_path);
  if (!creds_stream) {
    auto message = absl::StrCat("Could not open ", credential_kind,
                                " key file: ", credentials_file_path);
    LOG(ERROR) << fn_name << ":: " << message;
    return StatusRecord{SQLStates::k_HY000(), std::move(message)};
  }

  std::string contents((std::istreambuf_iterator<char>(creds_stream)),
                       std::istreambuf_iterator<char>());
  if (contents.empty()) {
    auto message = absl::StrCat(
        credential_kind,
        " key file is empty or could not be read: ", credentials_file_path);
    LOG(ERROR) << fn_name << ":: " << message;
    return StatusRecord{SQLStates::k_HY000(), std::move(message)};
  }

  return contents;
}

StatusRecord ValidateCredentialsJsonContents(
    std::string const& contents, absl::string_view credential_kind,
    absl::string_view effective_universe_domain) {
  auto json_creds = nlohmann::json::parse(contents, nullptr, false);
  if (json_creds.is_discarded() || !json_creds.is_object()) {
    auto message =
        absl::StrCat("Invalid JSON in ", credential_kind, " credentials file");
    LOG(ERROR) << "ValidateCredentialsJsonContents:: " << message;
    return StatusRecord{SQLStates::k_HY000(), std::move(message)};
  }
  auto cred_universe_domain = GetUniverseDomainFromCredentialsJson(json_creds);
  if (!cred_universe_domain) {
    LOG(ERROR) << "ValidateCredentialsJsonContents:: "
               << cred_universe_domain.GetStatusRecord().message;
    return cred_universe_domain.GetStatusRecord();
  }
  return ValidateCredentialsUniverseDomainMatch(*cred_universe_domain,
                                                effective_universe_domain);
}

bool UsesAuthorizedUserSourceCredentials(nlohmann::json const& json_creds) {
  nlohmann::json const* current = &json_creds;
  for (int depth = 0;
       depth < kMaxImpersonationChainDepth &&
       current->value("type", "") == "impersonated_service_account";
       ++depth) {
    auto src_it = current->find("source_credentials");
    if (src_it == current->end() || !src_it->is_object()) {
      break;
    }
    current = &(*src_it);
  }
  return current->value("type", "") == "authorized_user";
}

StatusRecord RejectUnsupportedNonDefaultUniverseImpersonation(
    absl::string_view effective_universe_domain) {
  auto message = absl::StrCat(
      "Base credentials do not support service account impersonation in "
      "universe domain (",
      effective_universe_domain, ")");
  LOG(ERROR) << "RejectUnsupportedNonDefaultUniverseImpersonation:: "
             << message;
  return StatusRecord{SQLStates::k_28000(), std::move(message)};
}

}  // namespace

StatusRecord ValidateUniverseDomain(absl::string_view universe_domain) {
  if (universe_domain.empty()) {
    return StatusRecord{SQLStates::k_HY024(),
                        "Invalid UniverseDomain: value cannot be empty"};
  }
  if (universe_domain.size() > kMaxDnsDomainLength ||
      universe_domain.front() == '.' || universe_domain.back() == '.') {
    return StatusRecord{
        SQLStates::k_HY024(),
        absl::StrCat("Invalid UniverseDomain value: ", universe_domain)};
  }

  std::vector<absl::string_view> labels = absl::StrSplit(universe_domain, '.');
  if (labels.size() < 2) {
    return StatusRecord{
        SQLStates::k_HY024(),
        absl::StrCat("Invalid UniverseDomain value: ", universe_domain)};
  }

  for (auto const& label : labels) {
    if (label.empty() || label.size() > kMaxDnsLabelLength ||
        label.front() == '-' || label.back() == '-') {
      return StatusRecord{
          SQLStates::k_HY024(),
          absl::StrCat("Invalid UniverseDomain value: ", universe_domain)};
    }
    for (char ch : label) {
      if (!IsAsciiAlphaNum(ch) && ch != '-') {
        return StatusRecord{
            SQLStates::k_HY024(),
            absl::StrCat("Invalid UniverseDomain value: ", universe_domain)};
      }
    }
  }

  bool tld_has_alpha = false;
  for (char ch : labels.back()) {
    if (IsAsciiAlpha(ch)) {
      tld_has_alpha = true;
      break;
    }
  }
  if (!tld_has_alpha) {
    return StatusRecord{
        SQLStates::k_HY024(),
        absl::StrCat("Invalid UniverseDomain value: ", universe_domain)};
  }

  return StatusRecord::Ok();
}

StatusRecordOr<std::string> GetEffectiveUniverseDomain(
    ConnProps const& conn_props, Options const& options) {
  if (conn_props.gcd.enable_gcd && conn_props.gcd.universe_domain.empty() &&
      conn_props.psc.empty()) {
    return StatusRecord{
        SQLStates::k_HY024(),
        "Invalid UniverseDomain: value cannot be empty when EnableGCD is "
        "enabled"};
  }

  if (!conn_props.gcd.universe_domain.empty()) {
    auto status = ValidateUniverseDomain(conn_props.gcd.universe_domain);
    if (!status.ok()) {
      return status;
    }
  }

  if (options.has<::google::cloud::internal::UniverseDomainOption>()) {
    auto const& opt_ud =
        options.get<::google::cloud::internal::UniverseDomainOption>();
    if (!opt_ud.empty()) {
      auto status = ValidateUniverseDomain(opt_ud);
      if (!status.ok()) {
        return status;
      }
    }
  }

  auto env_ud = GetEnv(kUniverseDomainEnvVar);
  if (env_ud && !env_ud->empty()) {
    auto status = ValidateUniverseDomain(*env_ud);
    if (!status.ok()) {
      return status;
    }
    if (!conn_props.gcd.universe_domain.empty() &&
        !absl::EqualsIgnoreCase(*env_ud, conn_props.gcd.universe_domain)) {
      LOG(WARNING) << "GetEffectiveUniverseDomain:: " << kUniverseDomainEnvVar
                   << " (" << *env_ud
                   << ") overrides configured UniverseDomain ("
                   << conn_props.gcd.universe_domain << ")";
    }
    return absl::AsciiStrToLower(*env_ud);
  }

  if (!conn_props.gcd.universe_domain.empty()) {
    return absl::AsciiStrToLower(conn_props.gcd.universe_domain);
  }

  if (options.has<::google::cloud::internal::UniverseDomainOption>()) {
    auto const& opt_ud =
        options.get<::google::cloud::internal::UniverseDomainOption>();
    if (!opt_ud.empty()) {
      return absl::AsciiStrToLower(opt_ud);
    }
  }

  return kDefaultUniverseDomain;
}

StatusRecordOr<std::string> GetUniverseDomainFromCredentialsJson(
    nlohmann::json const& json_creds) {
  if (!json_creds.is_object()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid credentials JSON: expected a JSON object"};
  }

  auto resolved_ud = ExtractExplicitUniverseDomainField(json_creds);
  if (!resolved_ud) {
    return resolved_ud.GetStatusRecord();
  }

  nlohmann::json const* current = &json_creds;
  for (int depth = 0;
       depth < kMaxImpersonationChainDepth &&
       current->value("type", "") == "impersonated_service_account";
       ++depth) {
    auto src_it = current->find("source_credentials");
    if (src_it == current->end() || !src_it->is_object()) {
      break;
    }
    auto src_ud = ExtractExplicitUniverseDomainField(*src_it);
    if (!src_ud) {
      return src_ud.GetStatusRecord();
    }
    if (resolved_ud->has_value() && src_ud->has_value() &&
        !absl::EqualsIgnoreCase(**resolved_ud, **src_ud)) {
      return StatusRecord{
          SQLStates::k_28000(),
          "Mismatched universe_domain between impersonated_service_account and "
          "source_credentials"};
    }
    if (!resolved_ud->has_value() && src_ud->has_value()) {
      *resolved_ud = *src_ud;
    }
    current = &(*src_it);
  }

  return resolved_ud->value_or(kDefaultUniverseDomain);
}

static StatusRecordOr<std::shared_ptr<Credentials>> CreateServiceCredentials(
    std::string const& credentials_file_path,
    absl::string_view effective_universe_domain, Options const& options) {
  auto contents = ReadCredentialFileContents(
      credentials_file_path, "CreateServiceCredentials", "Service Account",
      "The path to the file can't be empty");
  if (!contents) {
    return contents.GetStatusRecord();
  }

  auto ud_status = ValidateCredentialsJsonContents(*contents, "Service Account",
                                                   effective_universe_domain);
  if (!ud_status.ok()) {
    return ud_status;
  }

  return ::google::cloud::MakeServiceAccountCredentials(*contents, options);
}

static StatusRecordOr<std::shared_ptr<Credentials>> CreateUserCredentials(
    std::string const& credentials_file_path,
    absl::string_view effective_universe_domain,
    absl::string_view impersonated_email, Options const& options) {
  auto contents = ReadCredentialFileContents(
      credentials_file_path, "CreateUserCredentials", "User Account",
      "The path to the file can't be empty");
  if (!contents) {
    return contents.GetStatusRecord();
  }

  auto ud_status = ValidateCredentialsJsonContents(*contents, "User Account",
                                                   effective_universe_domain);
  if (!ud_status.ok()) {
    return ud_status;
  }

  // AuthorizedUserCredentials in google-cloud-cpp v3.10.0 does not override
  // Credentials::universe_domain(), causing REST MinimalIamCredentialsRestStub
  // to route impersonation requests to iamcredentials.googleapis.com.
  if (!impersonated_email.empty() &&
      !absl::EqualsIgnoreCase(effective_universe_domain,
                              kDefaultUniverseDomain)) {
    return RejectUnsupportedNonDefaultUniverseImpersonation(
        effective_universe_domain);
  }

  return ::google::cloud::internal::MakeUserAccountCredentials(*contents,
                                                               options);
}

static StatusRecordOr<std::shared_ptr<Credentials>>
CreateApplicationDefaultCredentials(absl::string_view effective_universe_domain,
                                    absl::string_view impersonated_email,
                                    Options const& options) {
  // Mirror google-cloud-cpp ADC resolution order:
  // 1) GOOGLE_APPLICATION_CREDENTIALS env var, if set.
  // 2) Well-known gcloud ADC file path, if it exists.
  // 3) Metadata server (ComputeEngineCredentials).
  auto adc_path =
      ::google::cloud::oauth2_internal::GoogleAdcFilePathFromEnvVarOrEmpty();
  if (adc_path.empty()) {
    auto well_known = ::google::cloud::oauth2_internal::
        GoogleAdcFilePathFromWellKnownPathOrEmpty();
    if (!well_known.empty()) {
      std::error_code ec;
      if (std::filesystem::exists(well_known, ec) && !ec) {
        adc_path = std::move(well_known);
      }
    }
  }

  if (!adc_path.empty()) {
    std::ifstream creds_stream(adc_path);
    if (!creds_stream) {
      LOG(ERROR) << "CreateApplicationDefaultCredentials:: Could not open "
                    "Application Default Credentials file: "
                 << adc_path;
      return StatusRecord{
          SQLStates::k_HY000(),
          absl::StrCat("Could not open Application Default Credentials file: ",
                       adc_path)};
    }
    std::string contents((std::istreambuf_iterator<char>(creds_stream)),
                         std::istreambuf_iterator<char>());
    if (contents.empty()) {
      LOG(ERROR) << "CreateApplicationDefaultCredentials:: Application Default "
                    "Credentials file is empty or could not be read: "
                 << adc_path;
      return StatusRecord{
          SQLStates::k_HY000(),
          absl::StrCat("Application Default Credentials file is empty or could "
                       "not be read: ",
                       adc_path)};
    }
    auto json_creds = nlohmann::json::parse(contents, nullptr, false);
    if (!json_creds.is_discarded() && json_creds.is_object()) {
      auto cred_ud = GetUniverseDomainFromCredentialsJson(json_creds);
      if (!cred_ud) {
        LOG(ERROR) << "CreateApplicationDefaultCredentials:: "
                   << cred_ud.GetStatusRecord().message;
        return cred_ud.GetStatusRecord();
      }
      auto match_status = ValidateCredentialsUniverseDomainMatch(
          *cred_ud, effective_universe_domain);
      if (!match_status.ok()) {
        return match_status;
      }
      if (!absl::EqualsIgnoreCase(effective_universe_domain,
                                  kDefaultUniverseDomain) &&
          UsesAuthorizedUserSourceCredentials(json_creds) &&
          (!impersonated_email.empty() ||
           json_creds.value("type", "") == "impersonated_service_account")) {
        return RejectUnsupportedNonDefaultUniverseImpersonation(
            effective_universe_domain);
      }
      return ::google::cloud::MakeGoogleDefaultCredentials(options);
    }

    // Non-JSON ADC file (e.g. PKCS#12 service account key or malformed file):
    // let google-cloud-cpp parse it and verify its universe domain, failing
    // closed on parse errors or mismatches.
    auto creds = ::google::cloud::MakeGoogleDefaultCredentials(options);
    auto cred_ud =
        ::google::cloud::rest_internal::MapCredentials(*creds)->universe_domain(
            options);
    if (!cred_ud) {
      if (cred_ud.status().code() == StatusCode::kNotFound) {
        auto match_status = ValidateCredentialsUniverseDomainMatch(
            kDefaultUniverseDomain, effective_universe_domain);
        if (!match_status.ok()) {
          return match_status;
        }
        return creds;
      }
      LOG(ERROR) << "CreateApplicationDefaultCredentials:: "
                 << cred_ud.status().message();
      return StatusRecord{SQLStates::k_HY000(), cred_ud.status().message()};
    }
    auto syntax_status = ValidateUniverseDomain(*cred_ud);
    if (!syntax_status.ok()) {
      return StatusRecord{
          SQLStates::k_HY000(),
          absl::StrCat("Invalid universe_domain in ADC credentials: ",
                       *cred_ud)};
    }
    auto match_status = ValidateCredentialsUniverseDomainMatch(
        *cred_ud, effective_universe_domain);
    if (!match_status.ok()) {
      return match_status;
    }
    return creds;
  }

  auto creds = ::google::cloud::MakeGoogleDefaultCredentials(options);
  if (!absl::EqualsIgnoreCase(effective_universe_domain,
                              kDefaultUniverseDomain)) {
    auto mds_ud =
        ::google::cloud::rest_internal::MapCredentials(*creds)->universe_domain(
            options);
    if (!mds_ud) {
      auto message = absl::StrCat(
          "Failed to verify metadata server universe domain for configured "
          "UniverseDomain (",
          effective_universe_domain, "): ", mds_ud.status().message());
      LOG(ERROR) << "CreateApplicationDefaultCredentials:: " << message;
      return StatusRecord{SQLStates::k_28000(), std::move(message)};
    }
    auto syntax_status = ValidateUniverseDomain(*mds_ud);
    if (!syntax_status.ok()) {
      auto message = absl::StrCat(
          "Invalid universe domain returned by metadata server: ", *mds_ud);
      LOG(ERROR) << "CreateApplicationDefaultCredentials:: " << message;
      return StatusRecord{SQLStates::k_28000(), std::move(message)};
    }
    auto match_status = ValidateCredentialsUniverseDomainMatch(
        *mds_ud, effective_universe_domain);
    if (!match_status.ok()) {
      return match_status;
    }
  }

  return creds;
}

static StatusRecordOr<std::shared_ptr<Credentials>>
CreateExternalAuthCredentialsJSON(std::string const& credentials_file_path,
                                  absl::string_view effective_universe_domain,
                                  Options const& options) {
  auto contents = ReadCredentialFileContents(
      credentials_file_path, "CreateExternalAuthCredentialsJSON",
      "External Account",
      "The path to the external auth JSON file can't be empty");
  if (!contents) {
    return contents.GetStatusRecord();
  }

  auto ud_status = ValidateCredentialsJsonContents(
      *contents, "External Account", effective_universe_domain);
  if (!ud_status.ok()) {
    return ud_status;
  }

  return ::google::cloud::MakeExternalAccountCredentials(*contents, options);
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

static StatusRecordOr<std::shared_ptr<Credentials>>
CreateExternalAccountAuthenticationBYOID(
    ConnProps const& conn_props, absl::string_view effective_universe_domain,
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
  auto cred_ud = GetUniverseDomainFromCredentialsJson(*json_creds);
  if (!cred_ud) {
    return cred_ud.GetStatusRecord();
  }
  auto match_status = ValidateCredentialsUniverseDomainMatch(
      *cred_ud, effective_universe_domain);
  if (!match_status.ok()) {
    return match_status;
  }
  return ::google::cloud::MakeExternalAccountCredentials((*json_creds).dump(),
                                                         options);
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateCredentials(
    ConnProps const& conn_props, Options const& options) {
  auto effective_universe_domain =
      GetEffectiveUniverseDomain(conn_props, options);
  if (!effective_universe_domain) {
    LOG(ERROR) << "CreateCredentials::GetEffectiveUniverseDomain:: "
               << effective_universe_domain.GetStatusRecord().message;
    return effective_universe_domain.GetStatusRecord();
  }

  Options effective_options = options;
  if (*effective_universe_domain != kDefaultUniverseDomain) {
    effective_options.set<::google::cloud::internal::UniverseDomainOption>(
        *effective_universe_domain);
  } else {
    effective_options.unset<::google::cloud::internal::UniverseDomainOption>();
  }

  switch (conn_props.auth_mechanism) {
    case OauthMechanism::kServiceAccount:
      return CreateServiceCredentials(conn_props.credentials_file_path,
                                      *effective_universe_domain,
                                      effective_options);
    case OauthMechanism::kUserAccount:
      return CreateUserCredentials(
          conn_props.credentials_file_path, *effective_universe_domain,
          conn_props.impersonated_email, effective_options);
    case OauthMechanism::kApplicationDefault:
      return CreateApplicationDefaultCredentials(*effective_universe_domain,
                                                 conn_props.impersonated_email,
                                                 effective_options);
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
            conn_props.credentials_file_path, *effective_universe_domain,
            effective_options);
      }
      // Call creation of external auth via BYOID properties.
      return CreateExternalAccountAuthenticationBYOID(
          conn_props, *effective_universe_domain, effective_options);
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
