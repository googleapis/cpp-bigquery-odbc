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

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

enum class OauthMechanism {
    kServiceAccount,
    kExternalUser
};

struct Auth {
    OauthMechanism auth_mechanism;
    std::string credentials_file_path;
};

/// Creates an object of UnifiedCredentials depending on the input arguments.
StatusOr<std::shared_ptr<Credentials>> CreateCredentials(Auth const& auth);

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)

#endif //GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_AUTHORIZATION_H
