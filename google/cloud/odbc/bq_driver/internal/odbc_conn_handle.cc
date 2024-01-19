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
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"

namespace google::cloud::odbc_bq_driver_internal {

void ConnectionHandle::SetUp(Section& dsn_section) {
  dsn_.description = dsn_section["Description"];
  dsn_.driver = dsn_section["Driver"];
  dsn_.catalog = dsn_section["Catalog"];
}

Status ConnectionHandle::Connect(Authentication& auth) {
  Oauth oauth;
  oauth.auth_mechanism = auth.auth_mechanism;
  oauth.credentials_file_path = auth.key_file_path;

  StatusOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(oauth);
  if (!response.ok()) {
    return response.status();
  }
  client_ = response.value();

  // Verify the credentials by calling ODBCBQClient::GetOAuth2Token
  StatusOr<AccessToken> access_token_resp = client_->GetOAuth2Token();
  if (!access_token_resp.ok()) {
    return access_token_resp.status();
  }

  auth_ = auth;
  return Status(StatusCode::kOk, "");
}

}  // namespace google::cloud::odbc_bq_driver_internal
