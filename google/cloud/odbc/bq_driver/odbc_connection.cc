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
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/internal/getenv.h"

// NOLINTBEGIN(misc-unused-parameters, readability-non-const-parameter)
namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver::IsValidEmail;
using google::cloud::odbc_bq_driver::ToCharStr;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

#ifdef _WIN32
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
using google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni;
#endif

/////////////////////////////
// Internal Helper Functions
/////////////////////////////

Authentication CreateAuth(Section& dsn_section) {
  Authentication auth;
  int auth_int;
  try {
    auth_int = stoi(dsn_section["OAuthMechanism"]);
  } catch (std::exception const& ex) {
    auto& opts = *(*kTraceOption);
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
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  EnvironmentHandle* env_handle = *handle_result;

  auto* conn_handle = new ConnectionHandle(env_handle);
  env_handle->GetConnectionHandles().insert(conn_handle);
  *out_conn_handle = conn_handle;
  return SQL_SUCCESS;
}

SQLRETURN SQLDriverConnectInternal(SQLHDBC conn_handle, SQLHWND window_handle,
                                   SQLCHAR* in_conn_str,
                                   SQLSMALLINT in_conn_str_len,
                                   SQLCHAR* out_conn_str,
                                   SQLSMALLINT out_conn_str_buflen,
                                   SQLSMALLINT* out_conn_str_len,
                                   SQLUSMALLINT driver_completion) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(conn_handle, false);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  auto* handle_ref = *handle_result;

  std::string conn_string = reinterpret_cast<char*>(in_conn_str);
  StatusRecordOr<Section> connection_params_resp_status =
      google::cloud::odbc_bq_driver_internal::ParseConnectionString(
          conn_string);
  if (!connection_params_resp_status) {
    return LogAndReturnCode(*handle_ref, connection_params_resp_status);
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
  if (status.ok() && out_conn_str != nullptr) {
    // Populate the output parameters as per the spec.
    std::string out_tmp_str(ToCharStr(in_conn_str));
    out_tmp_str.append(";");
    strncpy(reinterpret_cast<char*>(out_conn_str), out_tmp_str.c_str(),
            out_tmp_str.length());
    *out_conn_str_len = out_tmp_str.length();
  }
  return LogAndReturnCode(*handle_ref, status);
}

SQLRETURN SQLConnectInternal(SQLHDBC conn_handle, SQLCHAR* server_name,
                             SQLSMALLINT server_name_len, SQLCHAR* user_name,
                             SQLSMALLINT user_name_len, SQLCHAR* auth_string,
                             SQLSMALLINT auth_string_len) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(conn_handle, false);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  auto& handle_ref = *(*handle_result);
  if (server_name_len < 0 && server_name_len != SQL_NTS) {
    auto status_record =
        StatusRecord{SQLStates::k_HY090(), "Invalid server name length"};
    return LogAndReturnCode(handle_ref, status_record);
  }
  if (user_name_len < 0 && user_name_len != SQL_NTS) {
    auto status_record =
        StatusRecord{SQLStates::k_HY090(), "Invalid user name length"};
    return LogAndReturnCode(handle_ref, status_record);
  }
  if (auth_string_len < 0 && auth_string_len != SQL_NTS) {
    auto status_record =
        StatusRecord{SQLStates::k_HY090(), "Invalid auth string length"};
    return LogAndReturnCode(handle_ref, status_record);
  }

  std::string dsn_name = ToCharStr(server_name);
  std::string user_name_str = ToCharStr(user_name);
  std::string auth_string_str = ToCharStr(auth_string);

  Section dsn_section;
  if (!dsn_name.empty()) {
    auto status_record = OverrideDsnSectionFromEnv(dsn_section, dsn_name);
    if (!status_record.ok()) {
      return LogAndReturnCode(handle_ref, status_record);
    }
  } else {
    // DSN is not provided. Use the optional username and auth string which in
    // our case is email and a credentials file path. For security reasons,
    // refresh_token as auth string is not supported.
    if (user_name_str.empty()) {
      auto status_record =
          StatusRecord{SQLStates::k_HY090(),
                       "Username cannot be empty for DSN-less usecase"};
      return LogAndReturnCode(handle_ref, status_record);
    }
    if (auth_string_str.empty()) {
      auto status_record =
          StatusRecord{SQLStates::k_HY090(),
                       "Auth String cannot be empty for DSN-less usecase"};
      return LogAndReturnCode(handle_ref, status_record);
    }
    if (!IsValidEmail(user_name_str)) {
      auto status_record = StatusRecord{
          SQLStates::k_HY090(), "Username needs to be an email address"};
      return LogAndReturnCode(handle_ref, status_record);
    }
    dsn_section["OAuthMechanism"] =
        std::to_string(static_cast<int>(OauthMechanism::kServiceAccount));
    dsn_section["Email"] = user_name_str;
    dsn_section["KeyFilePath"] = auth_string_str;
  }
  // Populate the DSN info inside the handle.
  // This wasn't being called before.
  handle_ref.SetUp(dsn_section, dsn_name);

  Authentication auth = CreateAuth(dsn_section);
  StatusRecord status = handle_ref.Connect(auth);
  return LogAndReturnCode(handle_ref, status);
}

SQLRETURN SQLGetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER buf_len, SQLINTEGER* str_len) {
  TraceOptions& opts = *(*kTraceOption);
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, false);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  auto* conn_handle = *handle_result;
  auto status_record =
      conn_handle->GetAttribute(attribute, value, buf_len, str_len);
  return LogAndReturnCode(*conn_handle, status_record);
}

SQLRETURN SQLSetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER str_len) {
  TraceOptions& opts = *(*kTraceOption);
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, false);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  auto* conn_handle = *handle_result;
  auto status_record = conn_handle->SetAttribute(attribute, value, str_len);
  if (!status_record.ok()) {
    return LogAndReturnCode(*conn_handle, status_record);
  }

  // Additionally set these attributes to all associated statement handles
  if (attribute == SQL_ATTR_METADATA_ID || attribute == SQL_ATTR_ASYNC_ENABLE) {
    for (auto* const stmt_handle : conn_handle->GetStatementHandles()) {
      status_record = stmt_handle->SetAttribute(
          attribute, reinterpret_cast<SQLULEN>(value));
      if (!status_record.ok()) {
        return LogAndReturnCode(*conn_handle, status_record);
      }
    }
  }

  return SQL_SUCCESS;
}

SQLRETURN SQLDisconnectInternal(SQLHDBC connection_handle) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  ConnectionHandle* conn_handle = *handle_result;

  if (conn_handle->IsTransactionActive()) {
    StatusRecord record{SQLStates::k_25000(),
                        "Outstanding transactions during disconnect"};
    return LogAndReturnCode(*conn_handle, record);
  }

  conn_handle->Disconnect();
  std::vector<DescriptorHandle*> desc_handles(
      conn_handle->GetDescriptorHandles().begin(),
      conn_handle->GetDescriptorHandles().end());
  for (auto* const desc_handle : desc_handles) {
    auto status = SQLFreeHandleInternal(SQL_HANDLE_DESC, desc_handle);
    if (status != SQL_SUCCESS) {
      return status;
    }
  }
  std::vector<StatementHandle*> stmt_handles(
      conn_handle->GetStatementHandles().begin(),
      conn_handle->GetStatementHandles().end());
  for (auto* const stmt_handle : stmt_handles) {
    auto status = SQLFreeHandleInternal(SQL_HANDLE_STMT, stmt_handle);
    if (status != SQL_SUCCESS) {
      return status;
    }
  }
  return SQL_SUCCESS;
}

#ifdef _WIN32
SQLRETURN ConnectUsingRegistryDsn(SQLHDBC conn_handle,Authentication auth) {
    StatusRecordOr<ConnectionHandle*> handle_result = ValidateConnectionHandle(conn_handle, false);
    if (!handle_result) {
        TracePrintInternal(*(*kTraceOption), handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
    }
    auto* handle_ref = *handle_result;

    StatusRecord status = handle_ref->Connect(auth);
    if (!status.ok()) {
        return LogAndReturnCode(*handle_ref, status);
    }

    return SQL_SUCCESS;
}

bool TestODBCConnection(const std::string& dsn) {
    std::string registry_key = GetPathToOdbcIni() + "\\" + dsn;
    auto section_result = GetSectionWin(registry_key);

    if (!section_result.Ok()) {
        return false;
    }

    std::shared_ptr<Section> section = section_result.GetValue();
    
    if (section->find("KeyFilePath") == section->end() || (*section)["KeyFilePath"].empty()) {
        return false;
    }
    if (section->find("OAuthMechanism") == section->end() || (*section)["OAuthMechanism"].empty()) {
        return false;
    }
    if (section->find("Catalog") == section->end() || (*section)["Catalog"].empty()) {
        return false;
    }
    std::string key_file_path = (*section)["KeyFilePath"]; 
    std::string key_file_path_up;
    for (char ch : key_file_path) {
        if (ch == '\\') {
            key_file_path_up += "\\\\"; 
        } else {
            key_file_path_up += ch; 
        }
    }

    SQLHENV h_env;
    SQLHDBC h_dbc;
    SQLRETURN ret;

    // Allocate environment and connection handles
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &h_env);
    SQLSetEnvAttr(h_env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, h_env, &h_dbc);

    //Attempt the connection
    Authentication auth = CreateAuth(*section);
    ret=ConnectUsingRegistryDsn(h_dbc,auth);
    bool success = SQL_SUCCEEDED(ret);

    // Disconnect and free handles
    SQLDisconnect(h_dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, h_dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, h_env);

    return success;
}

bool TestODBCConnectionAd(const std::shared_ptr<Section>& section) {
    if (!section) {
        return false;
    }

    if (section->find("KeyFilePath") == section->end() || (*section)["KeyFilePath"].empty()) {
        return false;
    }
    if (section->find("OAuthMechanism") == section->end() || (*section)["OAuthMechanism"].empty()) {
        return false;
    }
    if (section->find("Catalog") == section->end() || (*section)["Catalog"].empty()) {
        return false;
    }

    std::string key_file_path = (*section)["KeyFilePath"]; 
    std::string key_file_path_up;
    for (char ch : key_file_path) {
        if (ch == '\\') {
            key_file_path_up += "\\\\"; 
        } else {
            key_file_path_up += ch; 
        }
    }

    SQLHENV h_env;
    SQLHDBC h_dbc;
    SQLRETURN ret;

    // Allocate environment and connection handles
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &h_env);
    SQLSetEnvAttr(h_env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, h_env, &h_dbc);

    // Attempt the connection
    Authentication auth = CreateAuth(*section);
    ret = ConnectUsingRegistryDsn(h_dbc, auth);
    bool success = SQL_SUCCEEDED(ret);

    // Disconnect and free handles
    SQLDisconnect(h_dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, h_dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, h_env);

    return true;
}
#endif


}  // namespace google::cloud::odbc_bq_driver
// NOLINTEND(misc-unused-parameters, readability-non-const-parameter)
