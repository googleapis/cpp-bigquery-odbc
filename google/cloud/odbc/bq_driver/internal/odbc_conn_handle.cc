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
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

void ConnectionHandle::SetUp(Section& dsn_section,
                             std::string const& dsn_name) {
  dsn_.description = dsn_section["Description"];
  dsn_.driver = dsn_section["Driver"];
  dsn_.catalog = dsn_section["Catalog"];
  dsn_.dsn_name = dsn_name;
}

StatusRecord ConnectionHandle::Connect(Authentication& auth) {
  Oauth oauth;
  oauth.auth_mechanism = auth.auth_mechanism;
  oauth.credentials_file_path = auth.key_file_path;

  StatusRecordOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(oauth);
  if (!response) {
    return response.GetStatusRecord();
  }
  client_ = *response;

  // Verify the credentials by calling ODBCBQClient::GetOAuth2Token
  StatusRecordOr<AccessToken> access_token_resp = client_->GetOAuth2Token();
  if (!access_token_resp) {
    return access_token_resp.GetStatusRecord();
  }

  auth_ = auth;
  is_connected_ = true;
  return {};
}

}  // namespace google::cloud::odbc_bq_driver_internal
