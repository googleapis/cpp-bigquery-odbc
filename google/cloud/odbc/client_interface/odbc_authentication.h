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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_AUTHORIZATION_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_AUTHORIZATION_H

#include "absl/strings/string_view.h"

#include "google/cloud/credentials.h"
#include "google/cloud/status_or.h"

/// Including <filesystem> or <experimental/filesystem>
/// depends on the specific compiler
#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#else
  #error "Missing the <filesystem> header."
#endif

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

/// Include secrets from GCP Secret Manager during compilation
#ifdef DEFAULT_BIGQUERY_CLIENT_ID
  std::string const kDefaultClientId = DEFAULT_BIGQUERY_CLIENT_ID;
#else
  std::string const kDefaultClientId = "provide DEFAULT_BIGQUERY_CLIENT_ID flag please";
#endif
#ifdef DEFAULT_BIGQUERY_CLIENT_SECRET
  std::string const kDefaultClientSecret = DEFAULT_BIGQUERY_CLIENT_SECRET;
#else
  std::string const kDefaultClientSecret = "provide DEFAULT_BIGQUERY_CLIENT_SECRET flag please";
#endif

inline std::string AuthorizedUserFilePath() {
  return (fs::temp_directory_path() / "authorized_user.json")
      .string(); // Additional conversions are needed for Windows platform
}

enum class OauthMechanism {
    kAuthorizedUser,
    kExternalUser,
    kServiceAccount
};

struct Auth {
    OauthMechanism auth_mechanism;
    std::string credentials_file_path;
    std::string refresh_token;
};

/// Creates an object of UnifiedCredentials depending on the input arguments.
/// It creates a file in temp dir for OauthMechanism::kAuthorizedUser.
StatusOr<std::shared_ptr<Credentials>> CreateCredentials(Auth const& auth);

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)

#endif //GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_AUTHORIZATION_H
