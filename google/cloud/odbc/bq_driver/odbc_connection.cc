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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/internal/getenv.h"

// NOLINTBEGIN(misc-unused-parameters, readability-non-const-parameter)
namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using google::cloud::odbc_bq_driver_internal::Section;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

/////////////////////////////
// Internal Helper Functions
/////////////////////////////

Authentication CreateAuth(Section& dsn_section) {
  Authentication auth;
  int auth_int;
  try {
    auth_int = stoi(dsn_section["OAuthMechanism"]);
  } catch (std::exception const& ex) {
    auto& opts = *(*kTraceOptsConsole);
    TracePrintInternal(opts, ex.what());
    auth_int = 0;
  }
  auth.auth_mechanism = static_cast<OauthMechanism>(auth_int);
  auth.email = dsn_section["Email"];
  auth.key_file_path = dsn_section["KeyFilePath"];
  auth.refresh_token = dsn_section["RefreshToken"];
  return auth;
}

StatusRecord OverrideDsnSectionFromEnv(Section& dsn_section,
                                       std::string const& dsn_name) {
  // TODO(#159): this has to handle windows too
  std::string odbcini_path =
      google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni();
  if (!odbcini_path.empty()) {
    auto sections_status =
        google::cloud::odbc_bq_driver_internal::ParseConfig(odbcini_path);
    if (!sections_status) {
      return sections_status.GetStatusRecord();
    }
    auto sections = *sections_status;
    dsn_section = (*sections)[dsn_name];
  }
  return StatusRecord::Ok();
}

//////////////////////
// Public Functions
//////////////////////

SQLRETURN SQLAllocConnHandle(SQLHDBC in_handle, SQLHANDLE* out_conn_handle) {
  StatusRecordOr<EnvironmentHandle*> handle_result =
      ValidateEnvironmentHandle(in_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOptsConsole),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  auto* conn_handle = new ConnectionHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kConnHandle, conn_handle);
  *out_conn_handle = wrapped_handle;
  return SQL_SUCCESS;
}

SQLRETURN SQLDriverConnectInternal(SQLHDBC conn_handle, SQLHWND window_handle,
                                   SQLCHAR* in_conn_str,
                                   SQLSMALLINT in_conn_str_len,
                                   SQLCHAR* out_conn_str,
                                   SQLSMALLINT out_conn_str_buflen,
                                   SQLSMALLINT* out_conn_str_len,
                                   SQLUSMALLINT driver_completion) {
  auto conn_handle_ptr_status =
      CastToHandle<ConnectionHandle>(HandleType::kConnHandle, conn_handle);
  if (!conn_handle_ptr_status) {
    TracePrintInternal(*(*kTraceOptsConsole),
                       conn_handle_ptr_status.GetStatusRecord().message);
    return SQL_INVALID_HANDLE;
  }
  auto* handle_ref = *conn_handle_ptr_status;
  handle_ref->GetDiagnostics().ClearDiagnostics();

  std::string conn_string = reinterpret_cast<char*>(in_conn_str);
  StatusRecordOr<Section> connection_params_resp_status =
      google::cloud::odbc_bq_driver_internal::ParseConnectionString(
          conn_string);
  if (!connection_params_resp_status) {
    handle_ref->GetDiagnostics().AddStatusRecord(
        connection_params_resp_status.GetStatusRecord());
    return connection_params_resp_status.GetCalculatedReturnCode();
  }

  auto connection_params_resp = *connection_params_resp_status;
  Section dsn_section;
  for (auto const& it : connection_params_resp) {
    std::string property = it.first;
    std::string value = it.second;
    dsn_section[property] = value;
  }

  // Any parameters defined in the env should
  //  override the DSN section properties.
  std::string dsn_name = connection_params_resp["DSN"];
  if (!dsn_name.empty()) {
    OverrideDsnSectionFromEnv(dsn_section, dsn_name);
  }

  // Populate the DSN info inside the handle.
  // This wasn't being called before.
  handle_ref->SetUp(dsn_section, dsn_name);

  Authentication auth = CreateAuth(dsn_section);
  StatusRecord status = handle_ref->Connect(auth);
  if (!status.ok()) {
    // Creating the connection failed
    // TODO(#170): Add error tracing call here
    // TODO(#158): SQLGetDiagRec should handle this
    return SQL_ERROR;
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLGetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER buf_len, SQLINTEGER* str_len) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, false);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  auto* conn_handle = *handle_result;
  auto status_record =
      conn_handle->GetAttribute(attribute, value, buf_len, str_len);
  if (!status_record.ok()) {
    conn_handle->GetDiagnostics().AddStatusRecord(status_record);
    TracePrintInternal(opts, status_record.message);
  }

  return status_record.CalculateReturnCode();
}

SQLRETURN SQLSetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER str_len) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, false);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  auto* conn_handle = *handle_result;
  auto status_record = conn_handle->SetAttribute(attribute, value, str_len);
  if (!status_record.ok()) {
    conn_handle->GetDiagnostics().AddStatusRecord(status_record);
    TracePrintInternal(opts, status_record.message);
  }

  return status_record.CalculateReturnCode();
}

}  // namespace google::cloud::odbc_bq_driver
// NOLINTEND(misc-unused-parameters, readability-non-const-parameter)
