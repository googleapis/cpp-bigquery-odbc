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
#include <fstream>

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
  auto is = std::ifstream(credentials_file_path);
  if (!is.is_open()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "There was an error while opening the file: " + credentials_file_path};
  }
  auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  if (is.bad()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "There was an error while reading the file: " + credentials_file_path};
  }
  std::size_t found = contents.find(R"("type": "service_account")");
  if (found != std::string::npos) {
    return ::google::cloud::MakeServiceAccountCredentials(contents);
  }
  SetEnv("GOOGLE_APPLICATION_CREDENTIALS", credentials_file_path.c_str());
  return ::google::cloud::MakeGoogleDefaultCredentials();
}

StatusRecordOr<std::shared_ptr<Credentials>> CreateCredentials(
    Oauth const& oauth) {
  switch (oauth.auth_mechanism) {
    case OauthMechanism::kServiceAccount:
      return CreateServiceCredentials(oauth.credentials_file_path);
    case OauthMechanism::kExternalUser:
      return StatusRecord{SQLStates::k_HY000(), "Currently not implemented"};
  }
  return StatusRecord{SQLStates::k_HY000(), "OauthMechanism enum is invalid"};
}

StatusRecordOr<AccessToken> GetOAuth2Token(
    std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> const&
        generator) {
  // We need to set env var for service account to force it to make a request to
  // Google Cloud. Then we return the value of this env var to the previous
  // state. If the env var is unset, token will be created locally, without any
  // request to Google Cloud.
  auto self_signed_jwt_disabled = GetEnv(kSelfSignedJwtEnvVar);
  SetEnv(kSelfSignedJwtEnvVar, "true");
  StatusOr<AccessToken> access_token = generator->GetToken();
  SetEnv(kSelfSignedJwtEnvVar, self_signed_jwt_disabled);
  return StatusRecordOr<AccessToken>::ConvertFromStatusOr(access_token);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
