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

void OverrideDsnSectionFromConnStr(Section& dsn_section,
                                   Section& conn_str_section) {
  for (auto const& it : conn_str_section) {
    std::string const& key = it.first;
    std::string const& val = it.second;

    if (dsn_section.find(key) != dsn_section.end()) {
      dsn_section[key] = val;
    }
  }
}

void PopulateOutConnStr(SQLCHAR* out_conn_str, SQLSMALLINT* out_conn_str_len,
                        SQLCHAR* in_conn_str) {
  if (out_conn_str != nullptr) {
    std::string out_tmp_str(ToCharStr(in_conn_str));
    strncpy(reinterpret_cast<char*>(out_conn_str), out_tmp_str.c_str(),
            out_tmp_str.length());
    *out_conn_str_len = out_tmp_str.length();
    out_conn_str[out_tmp_str.length()] = '\0';
  }
}

StatusRecord SetupConnAndConnect(ConnectionHandle* handle_ref,
                                 Section& dsn_section,
                                 std::string const& dsn_name) {
  handle_ref->SetUp(dsn_section, dsn_name);
  Authentication auth = CreateAuth(dsn_section);
  StatusRecord status = handle_ref->Connect(auth);
  return status;
}

StatusRecordOr<SQLRETURN> CheckConnAttribute(Section driver_section,
                                             SQLCHAR* out_conn_str,
                                             SQLSMALLINT* out_conn_str_len) {
  std::vector<std::string> required_keywords = {
      "Driver", "Catalog", "OAuthMechanism", "KeyFilePath"};
  std::ostringstream out_str;
  for (auto const& kv : driver_section) {
    std::string key = kv.first;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    auto is_req = std::find_if(
        required_keywords.begin(), required_keywords.end(),
        [&key](std::string const& req_keyword) {
          std::string lower_req_keyword = req_keyword;  // Copy required keyword
          std::transform(lower_req_keyword.begin(), lower_req_keyword.end(),
                         lower_req_keyword.begin(),
                         ::tolower);  // Convert required keyword to lowercase
          return key == lower_req_keyword;  // Compare lowercase versions
        });

    if (is_req == required_keywords.end()) {
      // Extra key found, return error
      return StatusRecord{
          SQLStates::k_HY000(),
          "Non Requested connection attribute " + key + " in ConnectionString"};
    }
  }

  for (auto const& key : required_keywords) {
    auto req_key = key;
    std::transform(req_key.begin(), req_key.end(), req_key.begin(), ::tolower);

    auto it = std::find_if(
        driver_section.begin(), driver_section.end(),
        [&req_key](std::pair<std::string const, std::string> const& dr_sec) {
          std::string lower_key = dr_sec.first;
          std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                         ::tolower);
          return req_key == lower_key;
        });

    if (it != driver_section.end()) {  // Key found
      // Check if the value is empty
      if (it->second.empty()) {
        out_str << key << ":" << key << "=?;";
      } else {
        out_str << "";
      }
    } else if (driver_section.count(req_key) > 1) {
      // Duplicate keyword found
      return StatusRecord{
          SQLStates::k_HY000(),
          "Duplicate connection string attribute found: " + key};
    } else {
      out_str << key << ":" << key << "=?;";
    }
  }

  std::string res_str = out_str.str();
  strncpy(reinterpret_cast<char*>(out_conn_str), res_str.c_str(),
          res_str.length());
  out_conn_str[res_str.length()] = '\0';
  *out_conn_str_len = res_str.length();

  if (!res_str.empty()) {
    return StatusRecordOr<SQLRETURN>(SQL_NEED_DATA);
  }
  return StatusRecordOr<SQLRETURN>(SQL_SUCCESS);
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
  for (auto k : *connection_params_resp_status) {
    std::cout << "key->> " << k.first << " Value--> " << k.second << std::endl;
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

  for (auto k : dsn_section) {
    std::cout << "key 2->> " << k.first << " Value 2--> " << k.second
              << std::endl;
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

SQLRETURN SQLBrowseConnectInternal(SQLHDBC conn_handle, SQLCHAR* in_conn_str,
                                   SQLSMALLINT in_conn_str_len,
                                   SQLCHAR* out_conn_str,
                                   SQLSMALLINT out_conn_str_bufflen,
                                   SQLSMALLINT* out_conn_str_len) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(conn_handle, false);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  static std::string static_conn_str;
  auto* handle_ref = *handle_result;
  if (in_conn_str_len < 0 && in_conn_str_len != SQL_NTS) {
    static_conn_str.clear();
    auto status_record =
        StatusRecord{SQLStates::k_HY090(), "Invalid string or buffer length"};
    return LogAndReturnCode(*handle_ref, status_record);
  }

  std::string conn_string = reinterpret_cast<char*>(in_conn_str);
  static_conn_str += conn_string;

  StatusRecordOr<Section> connection_params_resp_status =
      google::cloud::odbc_bq_driver_internal::ParseConnectionString(
          static_conn_str);

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

  std::string dsn_name = connection_params_resp["DSN"];
  std::string driver_name = connection_params_resp["DRIVER"];

  if (dsn_name.empty() && driver_name.empty()) {
    static_conn_str.clear();
    auto status_record =
        StatusRecord{SQLStates::k_IM002(),
                     "Data source not found and no default driver specified"};
    return LogAndReturnCode(*handle_ref, status_record);
  }

  if (!dsn_name.empty()) {
    OverrideDsnSectionFromEnv(dsn_section, dsn_name);
    OverrideDsnSectionFromConnStr(dsn_section, connection_params_resp);

    handle_ref->SetUp(dsn_section, dsn_name);

    Authentication auth = CreateAuth(dsn_section);
    StatusRecord status = handle_ref->Connect(auth);

    // auto status = SetupConnAndConnect(handle_ref, dsn_section, dsn_name);
    // if (!status.ok()) {
    //   return LogAndReturnCode(*handle_ref, status);
    // }

    // PopulateOutConnStr(out_conn_str, out_conn_str_len, in_conn_str);
    if (status.ok() && out_conn_str != nullptr) {
      // Populate the output parameters as per the spec.
      std::string out_tmp_str(ToCharStr(in_conn_str));
      strncpy(reinterpret_cast<char*>(out_conn_str), out_tmp_str.c_str(),
              out_tmp_str.length());
      *out_conn_str_len = out_tmp_str.length();
      out_conn_str[out_tmp_str.length()] = '\0';
    }
  }

  if (!driver_name.empty()) {
    StatusRecordOr<SQLRETURN> conn_att_resp =
        CheckConnAttribute(dsn_section, out_conn_str, out_conn_str_len);

    if (!conn_att_resp) {
      return LogAndReturnCode(*handle_ref, conn_att_resp);
    }

    auto status_check = *conn_att_resp;
    if (status_check != SQL_SUCCESS) {
      return status_check;
    }

    handle_ref->SetUp(dsn_section, dsn_name);

    Authentication auth = CreateAuth(dsn_section);
    StatusRecord status = handle_ref->Connect(auth);
    // auto status = SetupConnAndConnect(handle_ref, dsn_section, dsn_name);
    // if (!status.ok()) {
    //   return LogAndReturnCode(*handle_ref, status);
    // }

    // PopulateOutConnStr(out_conn_str, out_conn_str_len, in_conn_str);

    if (status.ok() && out_conn_str != nullptr) {
      // Populate the output parameters as per the spec.
      std::string out_tmp_str(ToCharStr(in_conn_str));
      strncpy(reinterpret_cast<char*>(out_conn_str), out_tmp_str.c_str(),
              out_tmp_str.length());
      *out_conn_str_len = out_tmp_str.length();
      out_conn_str[out_tmp_str.length()] = '\0';
    }
  }
  static_conn_str.clear();
  return SQL_SUCCESS;
}
}  // namespace google::cloud::odbc_bq_driver
// NOLINTEND(misc-unused-parameters, readability-non-const-parameter)
