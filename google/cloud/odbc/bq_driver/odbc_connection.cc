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

#include "google/cloud/odbc/bq_driver/odbc_connection.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/internal/getenv.h"

// NOLINTBEGIN(misc-unused-parameters, readability-non-const-parameter)
namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::Section;

/////////////////////////////
// Internal Helper Functions
/////////////////////////////

Authentication CreateAuth(Section& dsn_section) {
  Authentication auth;
  int auth_int;
  try {
    auth_int = stoi(dsn_section["OAuthMechanism"]);
  } catch (std::exception const& ex) {
    // TODO(#170): Add error tracing call here
    // TODO(#158): Add logging here
    auth_int = 0;
  }
  auth.auth_mechanism = static_cast<OauthMechanism>(auth_int);
  auth.email = dsn_section["Email"];
  auth.key_file_path = dsn_section["KeyFilePath"];
  auth.refresh_token = dsn_section["RefreshToken"];
  return auth;
}

void OverrideDsnSectionFromEnv(Section& dsn_section,
                               std::string const& dsn_name) {
  // TODO(#159): this has to handle windows too
  std::string odbcini_path =
      google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni();
  if (!odbcini_path.empty()) {
    auto sections =
        google::cloud::odbc_bq_driver_internal::ParseConfig(odbcini_path);
    if (!sections.ok()) {
      // The file path pointed by ODBCINI env is invalid
      // TODO(#170): Add error tracing call here
      return;
    }
    dsn_section = (*sections.value())[dsn_name];
  }
}

//////////////////////
// Public Functions
//////////////////////

SQLRETURN SQLAllocConnHandle(SQLHENV in_handle, SQLHANDLE* out_conn_handle) {
  EnvironmentHandle* env_handle = ValidateEnvironmentHandle(in_handle);
  if (env_handle == nullptr) {
    return SQL_INVALID_HANDLE;
  }
  env_handle->GetDiagnostics().ClearDiagnostics();

  auto* conn_handle = new ConnectionHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  *out_conn_handle = wrapped_handle;
  return SQL_SUCCESS;
}

SQLRETURN SQLDriverConnectInternal(SQLHDBC connection_handle,
                                   SQLHWND window_handle, SQLCHAR* in_conn_str,
                                   SQLSMALLINT in_conn_str_len,
                                   SQLCHAR* out_conn_str,
                                   SQLSMALLINT out_conn_str_buflen,
                                   SQLSMALLINT* out_conn_str_len,
                                   SQLUSMALLINT driver_completion) {
  ConnectionHandle* conn_handle = ValidateConnectionHandle(connection_handle);
  if (conn_handle == nullptr) {
    return SQL_INVALID_HANDLE;
  }
  conn_handle->GetDiagnostics().ClearDiagnostics();

  std::string conn_string = reinterpret_cast<char*>(in_conn_str);
  StatusOr<Section> connection_params_resp =
      google::cloud::odbc_bq_driver_internal::ParseConnectionString(
          conn_string);
  if (!connection_params_resp.ok()) {
    // The connection string is invalid
    // TODO(#170): Add error tracing call here
    // TODO(#158): SQLGetDiagRec should handle this
    return SQL_ERROR;
  }

  Section dsn_section;
  for (auto const& it : connection_params_resp.value()) {
    std::string property = it.first;
    std::string value = it.second;
    dsn_section[property] = value;
  }

  // Any parameters defined in the env should
  //  override the DSN section properties.
  std::string dsn_name = connection_params_resp.value()["DSN"];
  if (!dsn_name.empty()) {
    OverrideDsnSectionFromEnv(dsn_section, dsn_name);
  }

  // Populate the DSN info inside the handle.
  // This wasn't being called before.
  conn_handle->SetUp(dsn_section, dsn_name);

  Authentication auth = CreateAuth(dsn_section);
  Status status = conn_handle->Connect(auth);
  if (!status.ok()) {
    // Creating the connection failed
    // TODO(#170): Add error tracing call here
    // TODO(#158): SQLGetDiagRec should handle this
    return SQL_ERROR;
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
// NOLINTEND(misc-unused-parameters, readability-non-const-parameter)
