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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_HANDLES_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_HANDLES_H

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"

#include "google/cloud/odbc/bq_driver/internal/odbc_includes.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;

enum AuthMechanism {
     kUserAuth = 0,
     kServiceAuth = 1,
     kApplicationDefaultAuth = 2,
     kExternalAuth = 4
};

// Details of authentication provided in the odbc.ini/Windows Registry
struct Authentication {
     AuthMechanism auth_mechanism;
     std::string email;
     std::string key_file_path;
     // TODO: This should be removed if we decide that we will not support refresh tokens
     std::string refresh_token;	
};

// This is populated by SQL*Connect APIs after parsing the DSN section from odbc.ini/Windows Registry
struct Dsn {
     std::string description;
     std::string driver;
     std::string catalog;
     bool is_bq_legacy_sql;
};

class ConnectionHandle {
public:

  explicit ConnectionHandle();
  ~ConnectionHandle();

  ConnectionHandle(ConnectionHandle const&) = default;
  ConnectionHandle& operator=(ConnectionHandle const&) = default;
  ConnectionHandle(ConnectionHandle&&) = default;
  ConnectionHandle& operator=(ConnectionHandle&&) = default;

  Status Connect(Authentication& auth);

  void SetDsn(Dsn& dsn);

  std::shared_ptr<ODBCBQClient> GetClient();

  SQLRETURN GetAttribute(SQLINTEGER  attribute, void* value, void* length);

  SQLRETURN SetAttribute(SQLINTEGER  attribute, void* value, void* length);

private:

  Dsn dsn_;
  Authentication auth_;
  // The ODBCBQClient we will use for APIs interacting with BigQuery
  ODBCBQClient client_;

};

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_HANDLES_H
