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
#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {
using google::cloud::odbc_bq_driver_internal::AddDSNToRegistry;
using google::cloud::odbc_bq_driver_internal::AdvanceOptions;
using google::cloud::odbc_bq_driver_internal::ConvertLPCSTRToString;
using google::cloud::odbc_bq_driver_internal::DriverForm;
using google::cloud::odbc_bq_driver_internal::EditDSNInRegistry;
using google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni;
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
using google::cloud::odbc_bq_driver_internal::ParseConnectionString;
using google::cloud::odbc_bq_driver_internal::RemoveDSNFromRegistry;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_internal::StatusRecordOr;

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
  std::string o_auth_mechanism =
      section.count("OAuthMechanism") > 0 ? section.at("OAuthMechanism") : "";
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
  std::string language_dialect =
      section.count("LanguageDialect") > 0 ? section.at("LanguageDialect") : "";
  std::string large_dataset_name = section.count("LargeResultsDatasetId") > 0
                                       ? section.at("LargeResultsDatasetId")
                                       : "";
  std::string encryption_key =
      section.count("EncryptionKey") > 0 ? section.at("EncryptionKey") : "";
  std::string rows_per_block = section.count("RowsFetchedPerBlock") > 0
                                   ? section.at("RowsFetchedPerBlock")
                                   : "";
  std::string default_string_length =
      section.count("DefaultStringColumnLength") > 0
          ? section.at("DefaultStringColumnLength")
          : "";
  std::string temp_expiration =
      section.count("LargeResultsTempTableExpirationTime") > 0
          ? section.at("LargeResultsTempTableExpirationTime")
          : "";
  std::string session_location =
      section.count("SessionLocation") > 0 ? section.at("SessionLocation") : "";
  std::string additional_projects = section.count("AdditionalProjects") > 0
                                        ? section.at("AdditionalProjects")
                                        : "";
  std::string query_properties =
      section.count("QueryProperties") > 0 ? section.at("QueryProperties") : "";
  std::string activation_threshold =
      section.count(" HTAPI_ActivationThreshold") > 0
          ? section.at(" HTAPI_ActivationThreshold")
          : "";
  DriverForm form;
  AdvanceOptions advance_form;
  auto createSectionFromForm = [&]() -> Section {
    return {{"Email", email},
            {"KeyFilePath", key_file_path},
            {"OAuthMechanism", o_auth_mechanism},
            {"Catalog", catalog},
            {"Dataset", dataset_name},
            {"EncryptData", encrypt_data},
            {"TrustedCerts", trusted_certs},
            {"Min_TLS", min_tls_version},
            {"Description", description},
            {"LargeResultsDatasetId", large_dataset_name},
            {"EncryptionKey", encryption_key},
            {"RowsFetchedPerBlock", rows_per_block},
            {"DefaultStringColumnLength", default_string_length},
            {"LargeResultsTempTableExpirationTime", temp_expiration},
            {"SessionLocation", session_location},
            {"AdditionalProjects", additional_projects},
            {"QueryProperties", query_properties},
            {"HTAPI_ActivationThreshold", activation_threshold}};
  };

  auto showFormAndReturnValues = [&]() -> std::string {
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
    o_auth_mechanism = form.GetOAuthMechanism();
    catalog = form.GetCatalogName();
    dataset_name = form.GetDatasetName();
    encrypt_data = form.GetEncryptData();
    trusted_certs = form.GetTrustedCerts();
    min_tls_version = form.GetMinTls();
    description = form.GetDescription();

    large_dataset_name = advance_form.GetDatasetName();
    encryption_key = advance_form.GetEncryptionKey();
    rows_per_block = advance_form.GetRowsPerBlock();
    default_string_length = advance_form.GetDefaultStringLength();
    temp_expiration = advance_form.GetTempTableExpiration();
    session_location = advance_form.GetSessionLocation();
    additional_projects = advance_form.GetAdditionalProjects();
    query_properties = advance_form.GetQueryProperties();
    activation_threshold = advance_form.GetActivationThreshold();

    return dsn_name;
  };

  switch (f_request) {
    case ODBC_ADD_DSN: {
      if (hwnd_parent == NULL) {
        Section section = createSectionFromForm();
        AddDSNToRegistry(dsn_value, lpsz_driver, section);
        return true;
      }

      dsn_name = showFormAndReturnValues();
      Section section = createSectionFromForm();
      AddDSNToRegistry(dsn_name, lpsz_driver, section);
      return TRUE;
    }

    case ODBC_CONFIG_DSN: {
      if (hwnd_parent == NULL) {
        Section section2 = createSectionFromForm();
        EditDSNInRegistry(dsn_value, section2);
        return true;
      }

      std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_value;
      auto res = GetSectionWin(registry_key);
      auto section = res.GetValue();
      (*section)["DSN"] = dsn_value;

      form.SetValues(*section);
      advance_form.SetValues(*section);
      dsn_name = showFormAndReturnValues();

      Section section2 = createSectionFromForm();
      EditDSNInRegistry(dsn_value, section2);
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
