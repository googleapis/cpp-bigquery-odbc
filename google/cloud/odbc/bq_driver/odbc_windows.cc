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

#include "google/cloud/odbc/bq_driver/odbc_windows.h"
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {
using ::google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver_internal::AddDSNToRegistry;
using google::cloud::odbc_bq_driver_internal::AddLogTraceToRegistry;
using google::cloud::odbc_bq_driver_internal::ConvertLPCSTRToString;
using google::cloud::odbc_bq_driver_internal::DriverForm;
using google::cloud::odbc_bq_driver_internal::EditDSNInRegistry;
using google::cloud::odbc_bq_driver_internal::EditLogTraceInRegistry;
using google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni;
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
using google::cloud::odbc_bq_driver_internal::GetTraceLogRegistryPath;
using google::cloud::odbc_bq_driver_internal::GetUpperStr;
using google::cloud::odbc_bq_driver_internal::LogLevel;
using google::cloud::odbc_bq_driver_internal::LogTraceDialog;
using google::cloud::odbc_bq_driver_internal::ParseConnectionString;
using google::cloud::odbc_bq_driver_internal::RemoveDSNFromRegistry;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_bq_driver_internal::Trim;
using google::cloud::odbc_internal::StatusRecordOr;

std::string ConvertOAuthMechanism(std::string o_auth_mechanism) {
  std::string o_auth_value;
  if (o_auth_mechanism == "Service Authentication") {
    o_auth_value = std::to_string(
        static_cast<int>(OauthMechanism::kServiceAndUserAccount));
  } else if (o_auth_mechanism == "Application Default Credentials") {
    o_auth_value =
        std::to_string(static_cast<int>(OauthMechanism::kApplicationDefault));
  } else
    o_auth_value = "";
  return o_auth_value;
}

// TODO(b/b/391859145): Customization and Support For Logging and Driver
// Parameters
std::string ConvertLogLevel(std::string log_level) {
  std::string log_level_val;

  if (log_level == "LOG_TRACE") {
    log_level_val = std::to_string(static_cast<int>(LogLevel::kLogTrace));
  } else if (log_level == "LOG_OFF") {
    log_level_val = std::to_string(static_cast<int>(LogLevel::kLogOff));
  } else {
    log_level_val = "";
  }
  return log_level_val;
}

bool ConfigDSNInternal(HWND hwnd_parent, WORD f_request, LPCSTR lpsz_driver,
                       LPCSTR lpsz_attributes) {
  if (!lpsz_driver) {
    return FALSE;
  }
  std::string attribute = ConvertLPCSTRToString(lpsz_attributes);
  StatusRecordOr<Section> status_or_section = ParseConnectionString(attribute);
  Section section = *status_or_section;
  std::string dsn_value =
      section.count("DSN") > 0 ? section.at("DSN") : "Default DSN";
  std::string dsn_name;
  std::string email = section.count("Email") > 0 ? section.at("Email") : "";
  std::string key_file_path =
      section.count("KeyFilePath") > 0 ? section.at("KeyFilePath") : "";
  std::string o_auth_mechanism = ConvertOAuthMechanism(
      section.count("OAuthMechanism") > 0 ? section.at("OAuthMechanism") : "");
  std::string catalog =
      section.count("Catalog") > 0 ? section.at("Catalog") : "";
  std::string dataset_name =
      section.count("Dataset") > 0 ? section.at("Dataset") : "";
  std::string encrypt_data =
      section.count("EncryptData") > 0 ? section.at("EncryptData") : "";
  std::string trusted_certs =
      section.count("TrustedCerts") > 0 ? section.at("TrustedCerts") : "";
  std::string min_tls_version =
      section.count("Min_TLS") > 0 ? section.at("Min_TLS") : "";
  std::string description =
      section.count("Description") > 0 ? section.at("Description") : "";
  std::string log_level = ConvertLogLevel(
      section.count("LogLevel") > 0 ? section.at("LogLevel") : "");
  std::string log_file =
      section.count("LogFile") > 0 ? section.at("LogFile") : "";

  DriverForm form;
  auto CreateSectionFromForm = [&]() -> Section {
    return {{"Email", email},
            {"KeyFilePath", key_file_path},
            {"OAuthMechanism", o_auth_mechanism},
            {"Catalog", catalog},
            {"Dataset", dataset_name},
            {"EncryptData", encrypt_data},
            {"TrustedCerts", trusted_certs},
            {"Min_TLS", min_tls_version},
            {"Description", description}};
  };

  auto CreateSectionFromLogForm = [&]() -> Section {
    return {{"LogLevel", log_level}, {"LogFile", log_file}};
  };

  auto ShowFormAndReturnValues = [&]() -> std::string {
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
    o_auth_mechanism = ConvertOAuthMechanism(form.GetOAuthMechanism());
    catalog = form.GetCatalogName();
    dataset_name = form.GetDatasetName();
    encrypt_data = form.GetEncryptData();
    trusted_certs = form.GetTrustedCerts();
    min_tls_version = form.GetMinTls();
    description = form.GetDescription();
    log_level = ConvertLogLevel(form.GetLogLevel());
    log_file = form.GetLogFilePath();
    return dsn_name;
  };

  switch (f_request) {
    case ODBC_ADD_DSN: {
      if (hwnd_parent == NULL) {
        Section section = CreateSectionFromForm();
        Section trace_section = CreateSectionFromLogForm();
        AddDSNToRegistry(dsn_value, lpsz_driver, section);
        AddLogTraceToRegistry(trace_section);
        return true;
      }

      dsn_name = ShowFormAndReturnValues();
      Section section = CreateSectionFromForm();
      Section trace_section = CreateSectionFromLogForm();
      AddDSNToRegistry(dsn_name, lpsz_driver, section);
      AddLogTraceToRegistry(trace_section);
      return TRUE;
    }

    case ODBC_CONFIG_DSN: {
      if (hwnd_parent == NULL) {
        Section section_config = CreateSectionFromForm();
        EditDSNInRegistry(dsn_value, section_config);
        return true;
      }

      std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_value;
      std::string driver_registry_key = GetTraceLogRegistryPath() + "\\Driver";

      auto res = GetSectionWin(registry_key);
      auto trace_result = GetSectionWin(driver_registry_key);
      auto section = res.GetValue();
      auto trace_section = trace_result.GetValue();

      (*section)["DSN"] = dsn_value;

      form.SetValues(*section);
      form.SetLogTraceValues(*trace_section);
      dsn_name = ShowFormAndReturnValues();

      Section section_config = CreateSectionFromForm();
      Section trace_config_section = CreateSectionFromLogForm();
      EditDSNInRegistry(dsn_value, section_config);
      EditLogTraceInRegistry(trace_config_section);
      return TRUE;
    }
    case ODBC_REMOVE_DSN:
      RemoveDSNFromRegistry(dsn_value);
      return TRUE;

    default:
      return FALSE;
  }
}

}  // namespace google::cloud::odbc_bq_driver
