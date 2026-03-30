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
#include "google/cloud/odbc/bq_driver/internal/driver_adv_opt_form.h"
#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#include "google/cloud/odbc/bq_driver/internal/driver_form_proxy.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {
using ::google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver_internal::AddLogTraceToRegistry;
using google::cloud::odbc_bq_driver_internal::AdvanceOptions;
using google::cloud::odbc_bq_driver_internal::ConvertLPCSTRToString;
using google::cloud::odbc_bq_driver_internal::DriverForm;
using google::cloud::odbc_bq_driver_internal::EncryptPassword;
using google::cloud::odbc_bq_driver_internal::GetOdbcTraceConfigPath;
using google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni;
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
using google::cloud::odbc_bq_driver_internal::GetUpperStr;
using google::cloud::odbc_bq_driver_internal::GetValueOrDefault;
using google::cloud::odbc_bq_driver_internal::kLogFileCount;
using google::cloud::odbc_bq_driver_internal::kLogFileSize;
using google::cloud::odbc_bq_driver_internal::kLogLevel;
using google::cloud::odbc_bq_driver_internal::kLogPath;
using ::google::cloud::odbc_bq_driver_internal::LanguageDialect;
using google::cloud::odbc_bq_driver_internal::LogLevel;
using google::cloud::odbc_bq_driver_internal::LogTraceDialog;
using google::cloud::odbc_bq_driver_internal::ParseConnectionString;
using google::cloud::odbc_bq_driver_internal::ProxyOptions;
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
  } else if (o_auth_mechanism == "External Account Authentication") {
    o_auth_value =
        std::to_string(static_cast<int>(OauthMechanism::kExternalUser));
  } else
    o_auth_value = "";
  return o_auth_value;
}

std::string ConvertLanguageDialect(std::string language_dialect) {
  std::string language_dialect_value;
  if (language_dialect == "GoogleSQL") {
    language_dialect_value =
        std::to_string(static_cast<int>(LanguageDialect::kStandardSQL));
  } else if (language_dialect == "LegacySQL") {
    language_dialect_value =
        std::to_string(static_cast<int>(LanguageDialect::kLegacySQL));
  } else
    language_dialect_value = "";
  return language_dialect_value;
}

// TODO(b/b/391859145): Customization and Support For Logging and Driver
// Parameters
std::string ConvertLogLevel(std::string log_level) {
  std::string log_level_val;
  if (log_level == "LOG_INFO") {
    log_level_val = std::to_string(static_cast<int>(LogLevel::kLogInfo));
  } else if (log_level == "LOG_OFF") {
    log_level_val = std::to_string(static_cast<int>(LogLevel::kLogOff));
  } else if (log_level == "LOG_ERROR") {
    log_level_val = std::to_string(static_cast<int>(LogLevel::kLogError));
  } else if (log_level == "LOG_WARNING") {
    log_level_val = std::to_string(static_cast<int>(LogLevel::kLogWarning));
  } else {
    log_level_val = std::to_string(static_cast<int>(LogLevel::kLogInfo));
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

  std::string dsn_value = GetValueOrDefault(section, dsn_key);
  if (dsn_value.empty()) {
    dsn_value = "Default DSN";
  }

  std::string dsn_name;
  std::string key_file_path = GetValueOrDefault(section, key_file_path_key);
  std::string o_auth_mechanism =
      ConvertOAuthMechanism(GetValueOrDefault(section, oauth_mechanism_key));
  std::string catalog = GetValueOrDefault(section, catalog_key);
  std::string dataset_name = GetValueOrDefault(section, dataset_key);
  std::string encrypt_data = GetValueOrDefault(section, encrypt_data_key);
  std::string trusted_certs = GetValueOrDefault(section, trusted_certs_key);
  std::string min_tls_version = GetValueOrDefault(section, min_tls_key);
  std::string description = GetValueOrDefault(section, description_key);
  std::string log_level =
      ConvertLogLevel(GetValueOrDefault(section, kLogLevel).empty()
                          ? "0"
                          : GetValueOrDefault(section, kLogLevel));
  std::string log_file = GetValueOrDefault(section, kLogPath);
  std::string log_max_files = GetValueOrDefault(section, kLogFileCount);
  std::string log_max_size = GetValueOrDefault(section, kLogFileSize);
  std::string language_dialect =
      ConvertLanguageDialect(GetValueOrDefault(section, sql_dialect_key));
  std::string large_dataset_name =
      GetValueOrDefault(section, large_results_dataset_key);
  std::string encryption_key_value = GetValueOrDefault(section, encryption_key);
  std::string rows_per_block = GetValueOrDefault(section, rows_per_block_key);
  std::string default_string_length =
      GetValueOrDefault(section, default_string_length_key);
  std::string temp_expiration = GetValueOrDefault(section, temp_expiration_key);
  std::string session_location =
      GetValueOrDefault(section, session_location_key);
  std::string max_threads = GetValueOrDefault(section, max_threads_key);
  std::string additional_projects =
      GetValueOrDefault(section, additional_projects_key);
  std::string query_properties =
      GetValueOrDefault(section, query_properties_key);
  std::string activation_threshold =
      GetValueOrDefault(section, activation_threshold_key);
  // std::string use_wchar = GetValueOrDefault(section, use_wchar_key);
  std::string enable_session = GetValueOrDefault(section, enable_session_key);
  std::string htapi_activation_threshold_check =
      GetValueOrDefault(section, htapi_activation_threshold_check_key);
  std::string allow_large_results =
      GetValueOrDefault(section, allow_large_results_key);
  std::string use_default_large_results_dataset =
      GetValueOrDefault(section, use_default_large_results_dataset_key);
  std::string proxy_check = GetValueOrDefault(section, proxy_check_key);
  std::string proxy_host = GetValueOrDefault(section, proxy_host_key);
  std::string proxy_port = GetValueOrDefault(section, proxy_port_key);
  std::string proxy_username = GetValueOrDefault(section, proxy_username_key);
  std::string proxy_pwd = GetValueOrDefault(section, proxy_pwd_key);
  std::string proxy_pwd_enc =
      EncryptPassword(GetValueOrDefault(section, proxy_pwd_enc_key));
  std::string encryption_type_value =
      GetValueOrDefault(section, encryption_type);

  DriverForm form;
  AdvanceOptions advance_form;
  ProxyOptions proxy_form;

  auto CreateSectionFromForm = [&]() -> Section {
    return {
        {key_file_path_key, key_file_path},
        {oauth_mechanism_key, o_auth_mechanism},
        {catalog_key, catalog},
        {dataset_key, dataset_name},
        {encrypt_data_key, encrypt_data},
        {trusted_certs_key, trusted_certs},
        {min_tls_key, min_tls_version},
        {description_key, description},
        {sql_dialect_key, language_dialect},
        {large_results_dataset_key, large_dataset_name},
        {encryption_key, encryption_key_value},
        {rows_per_block_key, rows_per_block},
        {default_string_length_key, default_string_length},
        {temp_expiration_key, temp_expiration},
        {session_location_key, session_location},
        {additional_projects_key, additional_projects},
        {query_properties_key, query_properties},
        {activation_threshold_key, activation_threshold},
        // {use_wchar_key, use_wchar},
        {enable_session_key, enable_session},
        {max_threads_key, max_threads},
        {htapi_activation_threshold_check_key,
         htapi_activation_threshold_check},
        {allow_large_results_key, allow_large_results},
        {use_default_large_results_dataset_key,
         use_default_large_results_dataset},
        {encryption_type, encryption_type_value},
        {proxy_check_key, proxy_check},
        {proxy_host_key, proxy_host},
        {proxy_port_key, proxy_port},
        {proxy_username_key, proxy_username},
        {proxy_pwd_key, proxy_pwd},
        {proxy_pwd_enc_key, proxy_pwd_enc},
    };
  };
  auto CreateSectionFromLogForm = [&]() -> Section {
    return {{kLogLevel, log_level},
            {kLogPath, log_file},
            {kLogFileCount, log_max_files},
            {kLogFileSize, log_max_size}};
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
    log_max_files = form.GetLogMaxFiles();
    log_max_size = form.GetLogMaxSize();

    language_dialect =
        ConvertLanguageDialect(advance_form.GetLanguageDialect());
    large_dataset_name = advance_form.GetDatasetName();
    encryption_key_value = advance_form.GetEncryptionKey();
    rows_per_block = advance_form.GetRowsPerBlock();
    default_string_length = advance_form.GetDefaultStringLength();
    temp_expiration = advance_form.GetTempTableExpiration();
    session_location = advance_form.GetSessionLocation();
    additional_projects = advance_form.GetAdditionalProjects();
    query_properties = advance_form.GetQueryProperties();
    activation_threshold = advance_form.GetActivationThreshold();
    // use_wchar = advance_form.GetUseWchar();
    enable_session = advance_form.GetEnableSession();
    max_threads = advance_form.GetMaxThreads();
    htapi_activation_threshold_check =
        advance_form.GetActivationThresholdCheckbox();
    allow_large_results = advance_form.GetAllowLargeResults();
    use_default_large_results_dataset =
        advance_form.GetUseDefaultLargeResults();
    encryption_type_value = advance_form.GetEncryptionType();
    proxy_check = proxy_form.GetProxyCheck();
    proxy_host = proxy_form.GetProxyHost();
    proxy_port = proxy_form.GetProxyPort();
    proxy_username = proxy_form.GetProxyUsername();
    proxy_pwd_enc = EncryptPassword(proxy_form.GetProxyPass());
    return dsn_name;
  };

  switch (f_request) {
    case ODBC_ADD_DSN: {
      std::string dsn_name = dsn_value;
      if (hwnd_parent != NULL) {
        form.ResetToDefaults();
        dsn_name = ShowFormAndReturnValues();
        if (dsn_name.empty()) {
          return FALSE;
        }
      }
      Section section = CreateSectionFromForm();

      if (!SQLWriteDSNToIni(dsn_name.c_str(), lpsz_driver)) {
        return FALSE;
      }

      for (auto const& kv : section) {
        SQLWritePrivateProfileString(dsn_name.c_str(), kv.first.c_str(),
                                     kv.second.c_str(), "ODBC.INI");
      }
      Section trace_section = CreateSectionFromLogForm();
      AddLogTraceToRegistry(trace_section);

      return TRUE;
    }

    case ODBC_CONFIG_DSN: {
      std::string dsn_name = dsn_value;

      if (hwnd_parent != NULL) {
        int const BUFFER_SIZE = 1024;
        char buffer[BUFFER_SIZE];

        Section section = CreateSectionFromForm();
        for (auto& kv : section) {
          SQLGetPrivateProfileString(dsn_name.c_str(), kv.first.c_str(), "",
                                     buffer, BUFFER_SIZE, "ODBC.INI");
          section[kv.first.c_str()] = buffer;
        }
        section[dsn_key] = dsn_value;
        std::string driver_registry_key = GetOdbcTraceConfigPath() + "\\Driver";
        auto trace_result = GetSectionWin(driver_registry_key);
        auto trace_section = trace_result.GetValue();

        form.SetValues(section);
        advance_form.SetValues(section);
        proxy_form.SetValues(section);

        form.SetLogTraceValues(*trace_section);
        dsn_name = ShowFormAndReturnValues();
        Section trace_config_section = CreateSectionFromLogForm();
        AddLogTraceToRegistry(trace_config_section);
      }

      Section section_config = CreateSectionFromForm();

      for (auto const& kv : section_config) {
        SQLWritePrivateProfileString(dsn_name.c_str(), kv.first.c_str(),
                                     kv.second.c_str(), "ODBC.INI");
      }
      return TRUE;
    }

    case ODBC_REMOVE_DSN:
      if (!SQLRemoveDSNFromIni(dsn_value.c_str())) {
        return FALSE;
      }
      return TRUE;

    default:
      return FALSE;
  }
}

}  // namespace google::cloud::odbc_bq_driver
