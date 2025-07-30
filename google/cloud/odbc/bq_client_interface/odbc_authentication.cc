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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/status_or.h"
#include <nlohmann/json.hpp>
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

StatusRecordOr<std::shared_ptr<Credentials>> CreateServiceCredentials(
    std::string const& credentials_file_path) {
  if (credentials_file_path.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "The path to the file can't be empty"};
  }
  // Client libraries don't have a special function for user authentication.
  // We use MakeGoogleDefaultCredentials() and override
  // GOOGLE_APPLICATION_CREDENTIALS env var to point to the file with
  // credentials. It works for both: user authentication and service
  // authentication.
  // https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/credentials.h#L113
  // Read the contents of the key file directly instead of setting an
  // environment variable.
  std::ifstream is(credentials_file_path);
  if (!is) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Could not open Service Account key file: " + credentials_file_path};
  }

  std::string contents((std::istreambuf_iterator<char>(is)),
                       std::istreambuf_iterator<char>());

  if (contents.empty()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Service Account key file is empty or could not be read: " +
            credentials_file_path};
  }

  return ::google::cloud::MakeServiceAccountCredentials(contents);
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateApplicationDefaultCredentials() {
  // C++ client library in google-cloud-cpp first checks
  // GOOGLE_APPLICATION_CREDENTIALS env var and use it if it's present. Then it
  // looks for a 'default' location of the file with credentials.
  return ::google::cloud::MakeGoogleDefaultCredentials();
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateExternalAuthCredentialsJSON(
    std::string const& credentials_file_path) {
  if (credentials_file_path.empty()) {
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

  std::ifstream is(credentials_file_path);
  if (!is) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Could not open External Account key file: " + credentials_file_path};
  }

  std::string contents((std::istreambuf_iterator<char>(is)),
                       std::istreambuf_iterator<char>());

  if (contents.empty()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "External Account key file is empty or could not be read: " +
            credentials_file_path};
  }

  nlohmann::json parsed_json;
  try {
    parsed_json = nlohmann::json::parse(contents);
  } catch (nlohmann::json::parse_error const& e) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid JSON format in credential file: " +
                            credentials_file_path + ". Details: " + e.what()};
  }

  if (!parsed_json.contains("type") ||
      parsed_json["type"] != "external_account") {
    return StatusRecord{SQLStates::k_HY000(),
                        "The provided credential file is not a valid External "
                        "Account credential. "
                        "Expected 'type' field to be 'external_account'."};
  }

  return ::google::cloud::MakeExternalAccountCredentials(contents);
}

StatusRecordOr<nlohmann::json> CreateJsonCredsObject(
    std::string const& byoid_aud_url, std::string const& byoid_creds_source,
    std::string const& byoid_pool_user_project,
    std::string const& byoid_sub_token_type,
    std::string const& byoid_token_url) {
  nlohmann::json json;
  json["type"] = "external_account";
  json["audience"] = byoid_aud_url;
  json["credential_source"] = byoid_creds_source;
  json["subject_token_type"] = byoid_sub_token_type;
  json["token_url"] = byoid_token_url;
  if (!byoid_pool_user_project.empty()) {
    json["workforce_pool_user_project"] = byoid_pool_user_project;
  } else {
    json.erase("workforce_pool_user_project");
  }
  return json;
}

StatusRecordOr<std::shared_ptr<Credentials>>
CreateExternalAccountAuthenticationBYOID(Oauth const& oauth) {
  if (!IsBYOIDPropsSet(oauth)) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Unable to create external auth credentials: Required "
                        "BYOID Properties are not set "};
  }
  StatusRecordOr<nlohmann::json> json_creds = CreateJsonCredsObject(
      oauth.byoid_aud_url, oauth.byoid_creds_src, oauth.byoid_pool_user_project,
      oauth.byoid_subj_token_type, oauth.byoid_token_url);
  if (!json_creds) {
    return json_creds.GetStatusRecord();
  }
  return ::google::cloud::MakeExternalAccountCredentials((*json_creds).dump());
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateCredentials(
    Oauth const& oauth) {
  switch (oauth.auth_mechanism) {
    case OauthMechanism::kServiceAndUserAccount:
      return CreateServiceCredentials(oauth.credentials_file_path);
    case OauthMechanism::kApplicationDefault:
      return CreateApplicationDefaultCredentials();
    case OauthMechanism::kExternalUser: {
      if (!IsBYOIDPropsSet(oauth)) {
        // Call creation of external auth via JSON file
        return CreateExternalAuthCredentialsJSON(oauth.credentials_file_path);
      }
      // Call creation of external auth via BYOID properties.
      return CreateExternalAccountAuthenticationBYOID(oauth);
    }
  }
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
