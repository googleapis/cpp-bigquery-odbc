// Copyright 2024 Google LLC
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

#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

#ifdef _WIN32
#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#endif  // _WIN32

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::HandleType;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_internal::StatusRecordOr;

#ifdef _WIN32
using google::cloud::odbc_bq_driver_internal::AddDSNToRegistry;
using google::cloud::odbc_bq_driver_internal::AddLogTraceToRegistry;
using google::cloud::odbc_bq_driver_internal::ConvertLPCSTRToString;
using google::cloud::odbc_bq_driver_internal::DriverForm;
using google::cloud::odbc_bq_driver_internal::EditDSNInRegistry;
using google::cloud::odbc_bq_driver_internal::EditLogTraceInRegistry;
using google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni;
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
using google::cloud::odbc_bq_driver_internal::LogTraceDialog;
using google::cloud::odbc_bq_driver_internal::ParseConnectionString;
using google::cloud::odbc_bq_driver_internal::RemoveDSNFromRegistry;
using google::cloud::odbc_bq_driver_internal::Section;
#endif  // _WIN32

SQLRETURN SQLFreeHandleInternal(SQLSMALLINT handle_type, SQLHANDLE in_handle) {
  switch (handle_type) {
    case SQL_HANDLE_ENV: {
      StatusRecordOr<EnvironmentHandle*> handle_result =
          ValidateEnvironmentHandle(in_handle);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOption),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      (*handle_result)->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    case SQL_HANDLE_DBC: {
      StatusRecordOr<ConnectionHandle*> handle_result =
          ValidateConnectionHandle(in_handle, false);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOption),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      ConnectionHandle* conn_handle = *handle_result;
      // Dissociate itself from an environment handle
      if (conn_handle->GetEnvironmentHandle()) {
        conn_handle->GetEnvironmentHandle()->GetConnectionHandles().erase(
            conn_handle);
      }
      (*handle_result)->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    case SQL_HANDLE_STMT: {
      StatusRecordOr<StatementHandle*> handle_result =
          ValidateStatementHandle(in_handle);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOption),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      StatementHandle* stmt_handle = *handle_result;
      // Dissociate itself from a connection handle
      if (stmt_handle->GetConnectionHandle()) {
        stmt_handle->GetConnectionHandle()->GetStatementHandles().erase(
            stmt_handle);
      }
      stmt_handle->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    case SQL_HANDLE_DESC: {
      StatusRecordOr<DescriptorHandle*> handle_result =
          ValidateDescriptorHandle(in_handle);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOption),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      DescriptorHandle* desc_handle = *handle_result;
      // Dissociate this handle from all statement handles it was associated
      // with
      std::set<std::pair<StatementHandle*, DescriptorType>> pairs =
          desc_handle->GetAssociatedStatementHandles();
      for (auto const& [stmt_handle, type] : pairs) {
        stmt_handle->SetDescriptorHandle(type, nullptr);
      }
      if (desc_handle->GetConnectionHandle()) {
        desc_handle->GetConnectionHandle()->GetDescriptorHandles().erase(
            desc_handle);
      }
      desc_handle->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    default:
      return SQL_INVALID_HANDLE;
  }
  return SQL_SUCCESS;
}

#ifdef _WIN32
bool ConfigDSNInternal(HWND hwnd_parent, WORD f_request, LPCSTR lpsz_driver,
                       LPCSTR lpsz_attributes) {
  if (!lpsz_driver) {
    return FALSE;
  }
  std::string attribute = ConvertLPCSTRToString(lpsz_attributes);
  StatusRecordOr<Section> statusOrSection = ParseConnectionString(attribute);
  Section section = *statusOrSection;
  std::string dsn_value =
      section.count("DSN") > 0 ? section.at("DSN") : "Default DSN";
  std::string dsn_name;
  std::string email = section.count("Email") > 0 ? section.at("Email") : "";
  std::string key_file_path =
      section.count("KeyFilePath") > 0 ? section.at("KeyFilePath") : "";
  std::string oAuth_mechanism =
      section.count("OAuthMechanism") > 0 ? section.at("OAuthMechanism") : "";
  std::string catalog =
      section.count("Catalog") > 0 ? section.at("Catalog") : "";
  std::string dataset_name =
      section.count("Dataset") > 0 ? section.at("Dataset") : "";
  std::string log_level =
      section.count("LogLevel") > 0 ? section.at("LogLevel") : "";
  std::string log_file =
      section.count("LogFile") > 0 ? section.at("LogFile") : "";

  DriverForm form;
  LogTraceDialog logForm;
  switch (f_request) {
    case ODBC_ADD_DSN: {
      if (hwnd_parent == NULL) {
        Section section = {{"Email", email},
                           {"KeyFilePath", key_file_path},
                           {"OAuthMechanism", oAuth_mechanism},
                           {"Catalog", catalog},
                           {"Dataset", dataset_name}};

        Section trace_section = {
            {"LogLevel", log_level},
            {"LogFile", log_file},
        };
        AddDSNToRegistry(dsn_value, lpsz_driver, section);
        AddLogTraceToRegistry(trace_section);
        return true;
      }
      form.Show();
      form.GetHwnd();
      MSG msg = {};
      while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      dsn_name = form.GetDSN();
      email = form.GetEmail();
      key_file_path = form.GetKeyFilePath();
      oAuth_mechanism = form.GetOAuthMechanism();
      catalog = form.GetCatalogName();
      dataset_name = form.GetDatasetName();
      log_level = logForm.GetLogLevel();
      log_file = logForm.GetLogFilePath();

      Section section = {{"Email", email},
                         {"KeyFilePath", key_file_path},
                         {"OAuthMechanism", oAuth_mechanism},
                         {"Catalog", catalog},
                         {"Dataset", dataset_name}};

      Section trace_section = {
          {"LogLevel", log_level},
          {"LogFile", log_file},
      };
      AddDSNToRegistry(dsn_name, lpsz_driver, section);
      AddLogTraceToRegistry(trace_section);
      return TRUE;
    }
    case ODBC_CONFIG_DSN: {
      if (hwnd_parent == NULL) {
        Section section2 = {{"Email", email},
                            {"KeyFilePath", key_file_path},
                            {"OAuthMechanism", oAuth_mechanism},
                            {"Catalog", catalog},
                            {"Dataset", dataset_name}};

        Section trace_section2 = {
            {"LogLevel", log_level},
            {"LogFile", log_file},
        };
        EditDSNInRegistry(dsn_value, section2);
        return true;
      }
      std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_value;
      std::string driver_registry_key =
          "SOFTWARE\\Google\\ODBC Driver for Google BigQuery\\Driver";
      auto res = GetSectionWin(registry_key);
      auto trace_res = GetSectionWin(driver_registry_key);
      auto section = res.GetValue();
      auto trace_section = trace_res.GetValue();
      (*section)["DSN"] = dsn_value;
      form.SetValues(*section);
      form.Show();
      form.GetHwnd();

      logForm.SetValues(*trace_section);
      MSG msg = {};
      while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      email = form.GetEmail();
      key_file_path = form.GetKeyFilePath();
      oAuth_mechanism = form.GetOAuthMechanism();
      catalog = form.GetCatalogName();
      dataset_name = form.GetDatasetName();
      log_level = logForm.GetLogLevel();
      log_file = logForm.GetLogFilePath();
      Section section2 = {{"Email", email},
                          {"KeyFilePath", key_file_path},
                          {"OAuthMechanism", oAuth_mechanism},
                          {"Catalog", catalog},
                          {"Dataset", dataset_name}};

      Section trace_section2 = {
          {"LogLevel", log_level},
          {"LogFile", log_file},
      };
      EditDSNInRegistry(dsn_value, section2);
      EditLogTraceInRegistry(trace_section2);
      return TRUE;
    }
    case ODBC_REMOVE_DSN:
      RemoveDSNFromRegistry(dsn_value);
      return TRUE;

    default:
      return FALSE;
  }
}

#endif  // _WIN32

}  // namespace google::cloud::odbc_bq_driver
