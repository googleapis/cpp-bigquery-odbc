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

namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

#ifdef DEFAULT_BIGQUERY_CLIENT_ID
  #define DEFAULT_CLIENT_ID DEFAULT_BIGQUERY_CLIENT_ID
#else
  #define DEFAULT_CLIENT_ID "provide DEFAULT_BIGQUERY_CLIENT_ID flag please"
#endif
#ifdef DEFAULT_BIGQUERY_CLIENT_SECRET
#define DEFAULT_CLIENT_SECRET DEFAULT_BIGQUERY_CLIENT_SECRET
#else
#define DEFAULT_CLIENT_SECRET "provide DEFAULT_BIGQUERY_CLIENT_SECRET flag please"
#endif

inline constexpr absl::string_view kAuthorizedUserFileName = "authorized_user.json";

enum class OauthMechanism {
    kAuthorizedUser,
    kExternalUser,
    kServiceAccount
};

/// Creates an object of UnifiedCredentials depending on the input arguments.
/// It creates a file in temp dir for OauthMechanism::kAuthorizedUser.
StatusOr<std::shared_ptr<Credentials>> CreateCredentials(
    OauthMechanism const& auth_mechanism,
    std::string const& credentials_file_path,
    std::string const& refresh_token);

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google

#endif //GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_AUTHORIZATION_H
