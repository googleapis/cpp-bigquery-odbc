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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_CLIENT_LIBRARY_UTILS_AUTHENTICATION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_CLIENT_LIBRARY_UTILS_AUTHENTICATION_H

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/options.h"
#include "google/cloud/status_or.h"

namespace google::cloud::odbc_testing_client_library_utils {
// TODO(b/383592620): Populate this after WORKLOAD credentials are generated.
std::string const kWorkLoadAudUrl = "<TODO>";
std::string const kWorkLoadCredsSource = "<TODO>";
std::string const kWorkLoadSubTokenType = "<TODO>";
std::string const kWorkLoadTokenUrl = "<TODO>";
// TODO(b/383592620): Populate this after WORKFORCE credentials are generated.
std::string const kWorkForceAudUrl = "<TODO>";
std::string const kWorkForceCredsSource = "<TODO>";
std::string const kWorkForcePoolUserProject = "<TODO>";
std::string const kWorkForceSubTokenType = "<TODO>";
std::string const kWorkForceTokenUrl = "<TODO>";

// Creates Options object which has credentials for User Account Authentication.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateUserAccountAuthentication();

// Creates Options object which has credentials for Service Account
// Authentication.
StatusOr<Options> CreateServiceAccountAuthentication();

// Creates Options object which has credentials for Service Account With Client
// ID Authentication. Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateServiceAccountAuthWithClientIdAuthentication();

// Creates WRONG Options object with the path to not existing file.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateWrongPathToAuthFileAuthentication();

// Creates WRONG Options object has not existing credentials.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateWrongAuthentication();

// Creates WRONG Options object of the user with 0 projects to access.
// Updates GOOGLE_APPLICATION_CREDENTIALS env var.
StatusOr<Options> CreateNoAccessAccountAuthentication();

// Creates Options object which has credentials for Application Default
// Authentication Authentication.
StatusOr<Options> CreateApplicationDefaultAuthentication();

// Creates Options object which has credentials for External Account
// Authentication via JSON file.
StatusOr<Options> CreateExternalAuthenticationJSONFile();

// Creates Options object which has credentials for External Account
// Authentication via BYOID properties.
StatusOr<Options> CreateExternalAuthenticationBYOID(
    std::string const& byoid_aud_url, std::string const& byoid_creds_source,
    std::string const& byoid_pool_user_project,
    std::string const& byoid_sub_token_type,
    std::string const& byoid_token_url);
StatusOr<Options> CreateExternalAuthenticationBYOIDWorkload();
StatusOr<Options> CreateExternalAuthenticationBYOIDWorkforce();

// Create  Oauth struct for External Authentication
::google::cloud::odbc_bigquery_client_interface::Oauth
CreateExternalUserOauthBYOIDWorkload();
::google::cloud::odbc_bigquery_client_interface::Oauth
CreateExternalUserOauthBYOIDWorkforce();

}  // namespace google::cloud::odbc_testing_client_library_utils

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_CLIENT_LIBRARY_UTILS_AUTHENTICATION_H
