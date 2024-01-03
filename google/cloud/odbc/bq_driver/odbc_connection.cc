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

#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/bq_driver/odbc_connection.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bq_driver {

std::shared_ptr<Authentication> CreateAuth(Section& dsn_section) {
  Authentication auth;
  int auth_int;
  try {
    auth_int = stoi(dsn_section["OAuthMechanism"]);
  } catch(std::exception const& ex) {
    // TODO: Add logging here
    auth_int = 0;
  }
  auth.auth_mechanism = static_cast<AuthMechanism>(auth_int);
  auth.email = dsn_section["Email"];
  auth.key_file_path = dsn_section["KeyFilePath"];
  auth.refresh_token = dsn_section["RefreshToken"];
  return std::make_shared<Authentication>(auth);
}

std::shared_ptr<Dsn> CreateDsnObj(Section& dsn_section) {
  Dsn dsn;
  dsn.description = dsn_section["Description"];
  dsn.driver = dsn_section["Driver"];
  dsn.catalog = dsn_section["Catalog"];
  return std::make_shared<Dsn>(dsn);
}

SQLRETURN SQLDriverConnectInternal(
    SQLHDBC connectionHandle, SQLHWND windowHandle, SQLCHAR *inConnectionString,
    SQLSMALLINT inConnectionStringLen, SQLCHAR *outConnectionString,
    SQLSMALLINT outConnectionStringBufferLen, SQLSMALLINT *outConnectionStringLen,
    SQLUSMALLINT driverCompletion)
{
  // Validate the handle
  HandleWrapped * handle_wrapped = reinterpret_cast<HandleWrapped *>(connectionHandle);
  if (handle_wrapped->handle_type != HandleType::kConnHandle) {
    // TODO: SQLGetDiagRec should handle this
    return SQL_INVALID_HANDLE;
  }

  ConnectionHandle conn_handle = *reinterpret_cast<ConnectionHandle *>(handle_wrapped);
  // TODO: Is this the right way to convert?
  std::string conn_string = (char *)inConnectionString;
  Section connection_params = ParseConnectionString(conn_string);

  std::string dsn_name = connection_params["DSN"];
  if (dsn_name.empty()) {
    // There is no DSN name in the connection string
    // TODO: SQLGetDiagRec should handle this
    return SQL_ERROR;
  }


  Section dsn_section;
  // TODO: this has to handle windows too
  std::string odbcini_path = google::cloud::internal::GetEnv("ODBCINI").value_or("");
  if (!odbcini_path.empty()) {
    auto sections = ParseConfig(odbcini_path);
    if(!sections.ok()) {
      // The file path pointed by ODBCINI env is invalid
      // TODO: SQLGetDiagRec should handle this
      return SQL_ERROR;
    }
    dsn_section = (*sections.value())[dsn_name];
  }

  // Any parameters defined in the connection string should
  //  override the DSN section properties.
  for (const auto & it : connection_params) {
    std::string property = it.first;
    std::string value = it.second;
    dsn_section[property] = value;
  }
  Dsn dsn = *CreateDsnObj(dsn_section);
  Authentication auth = *CreateAuth(dsn_section);

  conn_handle.SetDsn(dsn);
  Status status = conn_handle.Connect(auth);
  if (!status.ok()) {
    // Creating the connection failed
    // TODO: SQLGetDiagRec should handle this
    return SQL_ERROR;
  }
  return SQL_SUCCESS;
}

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
