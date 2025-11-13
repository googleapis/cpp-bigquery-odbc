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
#ifdef _WIN32
#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#endif  // WIN32

// NOLINTBEGIN(misc-unused-parameters, readability-non-const-parameter)
namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver::ToCharStr;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::Dsn;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::GetMissingAttributesStr;
using google::cloud::odbc_bq_driver_internal::GetUpperStr;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::PopulateOutputConnectionString;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::UpdateTraceOption;
using google::cloud::odbc_bq_driver_internal::ValidateAllowedAttributes;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

#ifdef _WIN32
using google::cloud::odbc_bq_driver_internal::DriverForm;
using google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni;
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
#endif  // WIN32

/////////////////////////////
// Internal Helper Functions
/////////////////////////////

Authentication CreateAuth(Dsn const& dsn) {
  Authentication auth;
  int auth_int;
  try {
    auth_int = stoi(dsn.o_auth_mechanism);
  } catch (std::exception const& ex) {
    LOG(ERROR) << "CreateAuth:: " << ex.what();
    auth_int = 0;
  }
  auth.oauth.auth_mechanism = static_cast<OauthMechanism>(auth_int);
  auth.oauth.credentials_file_path = dsn.key_file_path;
  auth.refresh_token = dsn.refresh_token;
  // Populate BYOID Properties from Dsn.
  auth.oauth.byoid_aud_url = dsn.byoid_aud_url;
  auth.oauth.byoid_creds_src = dsn.byoid_creds_src;
  auth.oauth.byoid_pool_user_project = dsn.byoid_pool_user_project;
  auth.oauth.byoid_subj_token_type = dsn.byoid_subj_token_type;
  auth.oauth.byoid_token_url = dsn.byoid_token_url;
  auth.oauth.ssl_credentials.pem_root_certs = dsn.pem_file;
  auth.oauth.proxy_options.hostname = dsn.proxy_options.hostname;
  auth.oauth.proxy_options.port = dsn.proxy_options.port;
  auth.oauth.proxy_options.username = dsn.proxy_options.username;
  auth.oauth.proxy_options.password = dsn.proxy_options.password;
  auth.oauth.kms_key_name = dsn.kms_key_name;
  auth.oauth.psc = dsn.psc;
  auth.oauth.tpc.enable_tpc = dsn.enable_tpc;
  auth.oauth.tpc.universe_domain = dsn.universe_domain;
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
      LOG(ERROR) << "OverrideDsnSectionFromEnv::ParseConfig:: "
                 << sections_status.GetStatusRecord().message;
      return sections_status.GetStatusRecord();
    }
    auto sections = *sections_status;

    for (auto const& [key, value] : (*sections)[dsn_name]) {
      std::string upper_key = key;
      GetUpperStr(upper_key);
      dsn_section[upper_key] = value;
      LOG(INFO) << "OverrideDsnSectionFromEnv:: " << upper_key << " : "
                << value;
    }
  }
  return StatusRecord::Ok();
}

StatusRecord ConfigTraceFromSection(Section const& section) {
  std::optional<std::string> log_level;
  std::optional<std::string> log_path;

  if (auto it = section.find("LOGLEVEL"); it != section.end()) {
    log_level = it->second;
  }

  if (auto it = section.find("LOGPATH"); it != section.end()) {
    log_path = it->second;
  }

  if (log_level || log_path) {
    UpdateTraceOption(log_level, log_path);
  }
  return StatusRecord::Ok();
}

#ifdef _WIN32
SQLRETURN HandleDriverPrompt(std::string& conn_string, SQLHWND window_handle,
                             SQLCHAR* out_conn_str,
                             SQLSMALLINT out_conn_str_buflen,
                             SQLSMALLINT* out_conn_str_len,
                             ConnectionHandle* handle_ref) {
  if (!window_handle) {
    LOG(ERROR) << "HandleDriverPrompt:: Dialog failed";
    return LogAndReturnCode(
        *handle_ref, StatusRecord{SQLStates::k_IM008(), "Dialog failed"});
  }

  auto connection_params_status =
      google::cloud::odbc_bq_driver_internal::ParseConnectionString(
          conn_string);
  if (!connection_params_status) {
    LOG(ERROR) << "HandleDriverPrompt::ParseConnectionString:: "
               << connection_params_status.GetStatusRecord().message;
    return LogAndReturnCode(*handle_ref, connection_params_status);
  }

  auto connection_params = *connection_params_status;
  DriverForm form(window_handle);

  std::string dsn_name = connection_params["DSN"];
  std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_name;
  auto section = GetSectionWin(registry_key).GetValue();
  (*section)["DSN"] = dsn_name;
  form.SetValues(*section);

  form.Show();

  // Event loop to handle form.
  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Retrieve user inputs and configure the connection.
  Section dsn_section = {
      {"DSN", form.GetDSN()},
      {"KEYFILEPATH", form.GetKeyFilePath()},
      {"OAUTHMECHANISM", form.GetOAuthMechanism()},
      {"CATALOG", form.GetCatalogName()},
      {"DATASET", form.GetDatasetName()},
      {"TRUSTEDCERTS", form.GetTrustedCerts()},
      {"PROXYHOST", form.GetProxyOptions()->GetProxyHost()},
      {"PROXYPORT", form.GetProxyOptions()->GetProxyPort()},
      {"PROXYUID", form.GetProxyOptions()->GetProxyUsername()},
      {"PROXYPWD_ENC", form.GetProxyOptions()->GetProxyPass()},
      {"KMSKEYNAME", form.GetAdvanceOptions()->GetEncryptionKey()},
      {"SESSIONLOCATION", form.GetAdvanceOptions()->GetSessionLocation()},
      {"ENABLESESSION", form.GetAdvanceOptions()->GetEnableSession()},
      {"ADDITIONALPROJECTS", form.GetAdvanceOptions()->GetAdditionalProjects()},
  };

  handle_ref->SetUp(dsn_section, form.GetDSN());
  Authentication auth = CreateAuth(handle_ref->GetDsn());
  auto status = handle_ref->Connect(auth);

  if (status.ok() && out_conn_str != nullptr) {
    auto status_record = PopulateOutputConnectionString(
        out_conn_str, out_conn_str_buflen, out_conn_str_len, conn_string);
    if (!status_record.ok()) {
      LOG(ERROR) << "HandleDriverPrompt::PopulateOutputConnectionString:: "
                 << status_record.message;
      return LogAndReturnCode(*handle_ref, status_record);
    }
  }
  LOG(ERROR) << "HandleDriverPrompt:: " << status.message << "\n";
  return LogAndReturnCode(*handle_ref, status);
}
#endif  //_WIN32

//////////////////////
// Public Functions
//////////////////////

SQLRETURN SQLAllocConnHandle(SQLHDBC in_handle, SQLHANDLE* out_conn_handle) {
  StatusRecordOr<EnvironmentHandle*> handle_result =
      ValidateEnvironmentHandle(in_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLAllocConnHandle::ValidateEnvironmentHandle:: "
               << handle_result.GetStatusRecord().message;
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
    LOG(ERROR) << "SQLDriverConnect::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  auto* handle_ref = *handle_result;
  std::string conn_string = reinterpret_cast<char*>(in_conn_str);

  switch (driver_completion) {
#ifdef _WIN32
    case SQL_DRIVER_PROMPT: {
      return HandleDriverPrompt(conn_string, window_handle, out_conn_str,
                                out_conn_str_buflen, out_conn_str_len,
                                handle_ref);
    }
#endif  // _WIN32
    case SQL_DRIVER_COMPLETE:
    case SQL_DRIVER_COMPLETE_REQUIRED: {
      if (conn_string.empty() && !window_handle) {
        LOG(ERROR) << "SQLDriverConnect:: Dialog failed";
        return LogAndReturnCode(
            *handle_ref, StatusRecord{SQLStates::k_IM008(), "Dialog failed"});
      }
      break;
    }
    default:
      break;
  }
  StatusRecordOr<Section> connection_params_resp_status =
      google::cloud::odbc_bq_driver_internal::ParseConnectionString(
          conn_string);

  if (!connection_params_resp_status) {
    LOG(ERROR) << "SQLDriverConnect::ParseConnectionString:: "
               << connection_params_resp_status.GetStatusRecord().message;
    return LogAndReturnCode(*handle_ref, connection_params_resp_status);
  }

  auto connection_params_resp = *connection_params_resp_status;

  Section dsn_section;
  for (auto const& it : connection_params_resp) {
    std::string property = it.first;
    std::string value = it.second;
    GetUpperStr(property);
    dsn_section[property] = value;
  }
  // Tracing specified via connection string takes priority
  auto config_res = ConfigTraceFromSection(dsn_section);

  // Any parameters defined in the env should
  //  override the DSN section properties.
  std::string dsn_name = dsn_section["DSN"];
  if (!dsn_name.empty()) {
    OverrideDsnSectionFromEnv(dsn_section, dsn_name);

    for (auto& it : connection_params_resp) {
      std::string property = it.first;
      GetUpperStr(property);
      dsn_section[property] = it.second;
    }
  }
  for (auto const& it : dsn_section) {
    LOG(INFO) << "SQLDriverConnect::DSN Configuration:: " << it.first << " : "
              << it.second;
  }
  // Populate the DSN info inside the handle.
  // This wasn't being called before.
  handle_ref->SetUp(dsn_section, dsn_name);
  Authentication auth = CreateAuth(handle_ref->GetDsn());
  StatusRecord status = handle_ref->Connect(auth);

  if (status.ok() && out_conn_str != nullptr) {
    // Populate the output parameters as per the spec.
    auto status_record = PopulateOutputConnectionString(
        out_conn_str, out_conn_str_buflen, out_conn_str_len, conn_string);
    if (!status_record.ok()) {
      LOG(ERROR) << "SQLDriverConnect::PopulateOutputConnectionString:: "
                 << status_record.message;
      return LogAndReturnCode(*handle_ref, status_record);
    }
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
    LOG(ERROR) << "SQLConnect::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  auto& handle_ref = *(*handle_result);
  if (server_name_len < 0 && server_name_len != SQL_NTS) {
    auto status_record =
        StatusRecord{SQLStates::k_HY090(), "Invalid server name length"};
    LOG(ERROR) << "SQLConnect:: " << status_record.message;
    return LogAndReturnCode(handle_ref, status_record);
  }
  if (user_name_len < 0 && user_name_len != SQL_NTS) {
    auto status_record =
        StatusRecord{SQLStates::k_HY090(), "Invalid user name length"};
    LOG(ERROR) << "SQLConnect:: " << status_record.message;
    return LogAndReturnCode(handle_ref, status_record);
  }
  if (auth_string_len < 0 && auth_string_len != SQL_NTS) {
    auto status_record =
        StatusRecord{SQLStates::k_HY090(), "Invalid auth string length"};
    LOG(ERROR) << "SQLConnect:: " << status_record.message;
    return LogAndReturnCode(handle_ref, status_record);
  }

  std::string dsn_name = ToCharStr(server_name);
  std::string user_name_str = ToCharStr(user_name);
  std::string auth_string_str = ToCharStr(auth_string);

  Section dsn_section;
  if (!dsn_name.empty()) {
    auto status_record = OverrideDsnSectionFromEnv(dsn_section, dsn_name);
    if (!status_record.ok()) {
      LOG(ERROR) << "SQLConnect::OverrideDsnSectionFromEnv:: "
                 << status_record.message;
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
      LOG(ERROR) << "SQLConnect:: " << status_record.message;
      return LogAndReturnCode(handle_ref, status_record);
    }
    if (auth_string_str.empty()) {
      auto status_record =
          StatusRecord{SQLStates::k_HY090(),
                       "Auth String cannot be empty for DSN-less usecase"};
      LOG(ERROR) << "SQLConnect:: " << status_record.message;
      return LogAndReturnCode(handle_ref, status_record);
    }
    if (!IsValidEmail(user_name_str)) {
      auto status_record = StatusRecord{
          SQLStates::k_HY090(), "Username needs to be an email address"};
      LOG(ERROR) << "SQLConnect:: " << status_record.message;
      return LogAndReturnCode(handle_ref, status_record);
    }
    dsn_section["OAUTHMECHANISM"] = std::to_string(
        static_cast<int>(OauthMechanism::kServiceAndUserAccount));
    dsn_section["KEYFILEPATH"] = auth_string_str;
  }
  // Populate the DSN info inside the handle.
  // This wasn't being called before.
  handle_ref.SetUp(dsn_section, dsn_name);

  Authentication auth = CreateAuth(handle_ref.GetDsn());
  StatusRecord status = handle_ref.Connect(auth);
  LOG(INFO) << "SQLConnect:: Driver connected with data source "
            << status.message;
  return LogAndReturnCode(handle_ref, status);
}

SQLRETURN SQLGetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER buf_len, SQLINTEGER* str_len) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, false);
  if (!handle_result) {
    LOG(ERROR) << "SQLGetConnectAttr::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }

  auto* conn_handle = *handle_result;
  auto status_record =
      conn_handle->GetAttribute(attribute, value, buf_len, str_len);
  LOG(INFO) << "SQLGetConnectAttr::GetAttribute:: " << status_record.message;
  return LogAndReturnCode(*conn_handle, status_record);
}

SQLRETURN SQLSetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER str_len) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, false);
  if (!handle_result) {
    LOG(ERROR) << "SQLSetConnectAttr::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }

  auto* conn_handle = *handle_result;
  auto status_record = conn_handle->SetAttribute(attribute, value, str_len);
  if (!status_record.ok()) {
    LOG(ERROR) << "SQLSetConnectAttr: " << status_record.message;
    return LogAndReturnCode(*conn_handle, status_record);
  }

  // Additionally set these attributes to all associated statement handles
  if (attribute == SQL_ATTR_METADATA_ID || attribute == SQL_ATTR_ASYNC_ENABLE) {
    for (auto* const stmt_handle : conn_handle->GetStatementHandles()) {
      status_record = stmt_handle->SetAttribute(
          attribute, reinterpret_cast<SQLULEN>(value));
      if (!status_record.ok()) {
        LOG(ERROR) << "SQLSetConnectAttr: " << status_record.message;
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
    LOG(ERROR) << "SQLDisconnect::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  ConnectionHandle* conn_handle = *handle_result;

  if (conn_handle->IsTransactionActive()) {
    StatusRecord record{SQLStates::k_25000(),
                        "Outstanding transactions during disconnect"};
    LOG(ERROR) << "SQLDisconnect:: " << record.message;
    return LogAndReturnCode(*conn_handle, record);
  }

  conn_handle->Disconnect();
  LOG(INFO) << "SQLDisconnect:: Connection handle disconnect";
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

SQLRETURN SQLBrowseConnectInternal(SQLHDBC conn_handle, SQLCHAR* in_conn_str,
                                   SQLSMALLINT in_conn_str_len,
                                   SQLCHAR* out_conn_str,
                                   SQLSMALLINT out_conn_str_bufflen,
                                   SQLSMALLINT* out_conn_str_len) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(conn_handle, false);
  if (!handle_result) {
    LOG(ERROR) << "SQLBrowseConnect::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  auto* handle_ref = *handle_result;

  std::string conn_string = reinterpret_cast<char*>(in_conn_str);
  StatusRecordOr<Section> connection_params_resp_status =
      google::cloud::odbc_bq_driver_internal::ParseConnectionString(
          conn_string);

  if (!connection_params_resp_status) {
    LOG(ERROR) << "SQLBrowseConnect::ParseConnectionString:: "
               << connection_params_resp_status.GetStatusRecord().message;
    return LogAndReturnCode(*handle_ref, connection_params_resp_status);
  }

  auto connection_params_resp = *connection_params_resp_status;

  Section dsn_section;
  for (auto const& it : connection_params_resp) {
    std::string property = it.first;
    std::string value = it.second;
    GetUpperStr(property);
    dsn_section[property] = value;
    LOG(INFO) << "SQLBrowseConnect:: Connection string params:: " << property
              << " : " << value;
  }

  StatusRecord validation_status =
      ValidateAllowedAttributes(handle_ref, dsn_section);
  if (!validation_status.ok()) {
    LOG(ERROR) << "SQLBrowseConnect::ValidateAllowedAttributes:: "
               << validation_status.message;
    return LogAndReturnCode(*handle_ref, validation_status);
  }

  std::string dsn_name = dsn_section["DSN"];
  if (!dsn_name.empty()) {
    OverrideDsnSectionFromEnv(dsn_section, dsn_name);

    for (auto& it : connection_params_resp) {
      std::string property = it.first;
      if (!dsn_section[property].empty()) {
        dsn_section[property] = it.second;
      }
    }
  }
  handle_ref->SetUp(dsn_section, dsn_name);
  auto missing_att_str = GetMissingAttributesStr(handle_ref);

  if (missing_att_str) {
    auto status_record = PopulateOutputConnectionString(
        out_conn_str, out_conn_str_bufflen, out_conn_str_len, *missing_att_str,
        false);
    if (!status_record.ok()) {
      LOG(ERROR) << "SQLBrowseConnect::PopulateOutputConnectionString:: "
                 << status_record.message;
    }
    return SQL_NEED_DATA;
  }
  Authentication auth = CreateAuth(handle_ref->GetDsn());
  StatusRecord status = handle_ref->Connect(auth);

  if (status.ok() && out_conn_str != nullptr) {
    // Populate the output parameters as per the spec.
    std::ostringstream str_stream;
    std::string temp_conn_str;

    if (dsn_name.empty()) {
      str_stream << "DRIVER={" << handle_ref->GetDsn().driver << "};";
    } else {
      str_stream << "DSN=" << handle_ref->GetDsn().dsn_name << ";";
    }
    str_stream << "Catalog=" << handle_ref->GetDsn().catalog << ";"
               << "KeyFilePath=" << handle_ref->GetDsn().key_file_path << ";"
               << "OAuthMechanism=" << handle_ref->GetDsn().o_auth_mechanism
               << ";";

    std::string constructed_str = str_stream.str();
    auto status_record = PopulateOutputConnectionString(
        out_conn_str, out_conn_str_bufflen, out_conn_str_len, constructed_str,
        false);
    if (!status_record.ok()) {
      LOG(ERROR) << "SQLBrowseConnect::PopulateOutputConnectionString:: "
                 << status_record.message;
      return SQL_NEED_DATA;
    }
  }
  return SQL_SUCCESS;
}
}  // namespace google::cloud::odbc_bq_driver
// NOLINTEND(misc-unused-parameters, readability-non-const-parameter)
