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
#include "google/cloud/odbc/bq_driver/internal/setenv.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

    using google::cloud::odbc_bq_driver::SetEnv;

StatusOr<std::shared_ptr<Credentials>> CreateUserCredentials(
    std::string const& refresh_token) {
  std::string file_path = AuthorizedUserFilePath();
  std::ofstream os(file_path);
  if (!os.is_open()) {
    return Status(StatusCode::kInvalidArgument, "Can't open file with path: " + file_path);
  }

  std::string content =
    "{\n"
    "  \"type\": \"authorized_user\",\n"
    "  \"client_id\": \"" + kDefaultClientId + "\",\n"
    "  \"client_secret\":\"" + kDefaultClientSecret + "\",\n"
    "  \"refresh_token\":\"" + refresh_token + "\"\n"
    "}\n";
  os.write(content.c_str(), content.size());
  os.close();
  if (!os.good()) {
    return Status(StatusCode::kInvalidArgument, "Can't close file with path: " + file_path);
  }

  SetEnv("GOOGLE_APPLICATION_CREDENTIALS", file_path);
  return google::cloud::MakeGoogleDefaultCredentials();
}

StatusOr<std::shared_ptr<Credentials>> CreateCredentials(
    OauthMechanism const& auth_mechanism,
    std::string const& credentials_file_path,
    std::string const& refresh_token) {
  switch (auth_mechanism) {
    case OauthMechanism::kAuthorizedUser:
      return CreateUserCredentials(refresh_token);
      break;
    case OauthMechanism::kServiceAccount:
      return Status(StatusCode::kUnimplemented, "Currently not implemented. Will use file: " + credentials_file_path);
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
