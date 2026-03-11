// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"
#include <gtest/gtest.h>
#include <fstream>

namespace google::cloud::odbc_testing_client_library_utils {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_client_interface::CreateCredentials;
using google::cloud::odbc_bigquery_client_interface::kDefaultTokenUrl;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeJWT;
using ::google::cloud::odbc_bigquery_client_interface::Oauth;
using ::google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

StatusOr<Options> CreateUserAccountAuthentication() {
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY environment "
                  "variable is not set");
  }
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kServiceAccount;
  oauth.credentials_file_path = path_to_file_with_credentials;
  StatusRecordOr<std::shared_ptr<Credentials>> creds = CreateCredentials(oauth);
  if (!creds) {
    return Status(StatusCode::kInternal, "Unable to create User credentials");
  }
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      *creds);
}

StatusOr<Options> CreateServiceAccountAuthentication() {
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY environment "
                  "variable is not set");
  }
  auto is = std::ifstream(path_to_file_with_credentials);
  is.exceptions(std::ios::badbit);  // Minimal error handling
  auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeServiceAccountCredentials(contents));
}

StatusOr<Options> CreateApplicationDefaultAuthentication() {
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kApplicationDefault;
  StatusRecordOr<std::shared_ptr<Credentials>> creds = CreateCredentials(oauth);
  if (!creds) {
    return Status(StatusCode::kInternal, "Unable to create ADC credentials");
  }

  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      *creds);
}

// TODO(b/333011414) Enable tests which use this function it or remove it
StatusOr<Options> CreateServiceAccountAuthWithClientIdAuthentication() {
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_AUTH_KEY environment "
                  "variable is not set");
  }
  setenv("GOOGLE_APPLICATION_CREDENTIALS",
         path_to_file_with_credentials.c_str(), 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateWrongPathToAuthFileAuthentication() {
  setenv("GOOGLE_APPLICATION_CREDENTIALS", "path-to-non-existing-file.json", 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateWrongAuthentication() {
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "CPP_BIGQUERY_ODBC_TEST_WRONG_AUTH_KEY environment variable "
                  "is not set");
  }
  setenv("GOOGLE_APPLICATION_CREDENTIALS",
         path_to_file_with_credentials.c_str(), 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateNoAccessAccountAuthentication() {
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "CPP_BIGQUERY_ODBC_TEST_NO_ACCESS_ACCOUNT_AUTH_KEY "
                  "environment variable is not set");
  }
  setenv("GOOGLE_APPLICATION_CREDENTIALS",
         path_to_file_with_credentials.c_str(), 1);
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      google::cloud::MakeGoogleDefaultCredentials());
}

StatusOr<Options> CreateExternalAuthenticationJSONFile() {
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY").value_or("");
  if (path_to_file_with_credentials.empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY environment "
        "variable is not set");
  }
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kExternalUser;
  oauth.credentials_file_path = path_to_file_with_credentials;
  StatusRecordOr<std::shared_ptr<Credentials>> creds = CreateCredentials(oauth);
  if (!creds) {
    return Status(StatusCode::kInternal,
                  "Unable to create external credentials from JSON file");
  }
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      *creds);
}

StatusOr<Options> CreateExternalAuthenticationBYOID(
    std::string const& byoid_aud_url, std::string const& byoid_creds_source,
    std::string const& byoid_pool_user_project,
    std::string const& byoid_sub_token_type,
    std::string const& byoid_token_url) {
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kExternalUser;
  oauth.byoid_aud_url = byoid_aud_url;
  oauth.byoid_creds_src = byoid_creds_source;
  oauth.byoid_pool_user_project = byoid_pool_user_project;
  oauth.byoid_subj_token_type = byoid_sub_token_type;
  oauth.byoid_token_url = byoid_token_url;
  StatusRecordOr<std::shared_ptr<Credentials>> creds = CreateCredentials(oauth);
  if (!creds) {
    return Status(StatusCode::kInternal,
                  "Unable to create external credentials from JSON file");
  }
  return google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
      *creds);
}

StatusOr<Options> CreateExternalAuthenticationBYOIDWorkload() {
  return CreateExternalAuthenticationBYOID(
      kWorkLoadAudUrl, kWorkLoadCredsSource,
      "" /* pool_user_project empty for workload */, kWorkLoadSubTokenType,
      kWorkLoadTokenUrl);
}

StatusOr<Options> CreateExternalAuthenticationBYOIDWorkforce() {
  return CreateExternalAuthenticationBYOID(
      kWorkForceAudUrl, kWorkForceCredsSource, kWorkForcePoolUserProject,
      kWorkForceSubTokenType, kWorkForceTokenUrl);
}

Oauth CreateExternalUserOauthBYOIDWorkload() {
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kExternalUser;
  oauth.byoid_aud_url = kWorkLoadAudUrl;
  oauth.byoid_creds_src = kWorkLoadCredsSource;
  oauth.byoid_subj_token_type = kWorkLoadSubTokenType;
  oauth.byoid_token_url = kWorkLoadTokenUrl;
  return oauth;
}

Oauth CreateExternalUserOauthBYOIDWorkforce() {
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kExternalUser;
  oauth.byoid_aud_url = kWorkForceAudUrl;
  oauth.byoid_creds_src = kWorkForceCredsSource;
  oauth.byoid_subj_token_type = kWorkForceSubTokenType;
  oauth.byoid_token_url = kWorkForceTokenUrl;
  oauth.byoid_pool_user_project = kWorkForcePoolUserProject;
  return oauth;
}

}  // namespace google::cloud::odbc_testing_client_library_utils
