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

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bigquery_client_interface::CreateCredentials;

Status ConnectionHandle::Connect(Authentication& auth) {
  auth_ = auth;
  Oauth oauth;
  switch (auth.auth_mechanism) {
    case AuthMechanism::kUserAuth:
      return Status(StatusCode::kUnimplemented, "Currently not implemented user auth");
    case AuthMechanism::kServiceAuth:
      oauth.auth_mechanism = OauthMechanism::kServiceAccount;
      break;
    case AuthMechanism::kApplicationDefaultAuth:
      return Status(StatusCode::kUnimplemented, "Currently not implemented application default auth");
    case AuthMechanism::kExternalAuth:
      oauth.auth_mechanism = OauthMechanism::kExternalUser;
      break;
    default:
      return Status(StatusCode::kInvalidArgument, "Invalid auth mechanism");
  }
  oauth.credentials_file_path = auth.key_file_path;

  auto response = ODBCBQClient::CreateBQClient(oauth);
  if (!response.ok()) {
    return Status(response.status().code(), response.status().message());
  }
  client_ = *response.value();

  return Status(StatusCode::kOk, "");
}

void ConnectionHandle::SetDsn(Dsn& dsn) {
  dsn_ = dsn;
}

std::shared_ptr<ODBCBQClient> ConnectionHandle::GetClient() {
  return std::make_shared<ODBCBQClient>(client_);
}

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
