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

#include <fstream>

#include "google/cloud/credentials.h"
#include "google/cloud/status_or.h"

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

StatusOr<std::shared_ptr<Credentials>> CreateServiceCredentials(
    std::string const& credentials_file_path) {
  if (credentials_file_path.empty()) {
    return Status(StatusCode::kInvalidArgument, "The path to the file can't be empty.");
  }
  auto is = std::ifstream(credentials_file_path);
  if (!is.is_open()) {
    return Status(StatusCode::kInvalidArgument, "There was an error while opening the file: " + credentials_file_path);
  }
  auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  if (is.bad()) {
    return Status(StatusCode::kInternal, "There was an error while reading the file: " + credentials_file_path);
  }
  return google::cloud::MakeServiceAccountCredentials(contents);
}

StatusOr<std::shared_ptr<Credentials>> CreateCredentials(Auth const& auth) {
  switch (auth.auth_mechanism) {
    case OauthMechanism::kServiceAccount:
      return CreateServiceCredentials(auth.credentials_file_path);
      break;
    case OauthMechanism::kExternalUser:
      return Status(StatusCode::kUnimplemented, "Currently not implemented.");
      break;
  }
  return Status(StatusCode::kInvalidArgument, "OauthMechanism enum is invalid.");
}

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
