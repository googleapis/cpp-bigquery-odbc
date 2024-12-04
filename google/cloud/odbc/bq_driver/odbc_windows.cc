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
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"

namespace google::cloud::odbc_bq_driver {
using google::cloud::odbc_bq_driver_internal::AddDSNToRegistry;
using google::cloud::odbc_bq_driver_internal::ConvertLPCSTRToString;
using google::cloud::odbc_bq_driver_internal::DriverForm;
using google::cloud::odbc_bq_driver_internal::EditDSNInRegistry;
using google::cloud::odbc_bq_driver_internal::GetPathToOdbcIni;
using google::cloud::odbc_bq_driver_internal::GetSectionWin;
using google::cloud::odbc_bq_driver_internal::ParseConnectionString;
using google::cloud::odbc_bq_driver_internal::RemoveDSNFromRegistry;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::odbc_bigquery_client_interface::OauthMechanism;

std::string ConvertOAuthMechanism(std::string o_auth_mechanism){
      std::string o_auth_value;
      if (o_auth_mechanism == "Service Authentication") {
        o_auth_value =
            std::to_string(static_cast<int>(OauthMechanism::kServiceAccount));
      } else if (o_auth_mechanism == "Application Default Credentials") {
        o_auth_value = std::to_string(
            static_cast<int>(OauthMechanism::kApplicationDefault));
      } else
        o_auth_value = "";
    return o_auth_value;
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
  std::string o_auth_mechanism =ConvertOAuthMechanism(
      section.count("OAuthMechanism") > 0 ? section.at("OAuthMechanism") : "");
  std::string catalog =
      section.count("Catalog") > 0 ? section.at("Catalog") : "";
  std::string dataset_name =
      section.count("Dataset") > 0 ? section.at("Dataset") : "";

  DriverForm form;
  switch (f_request) {
    case ODBC_ADD_DSN: {
      if (hwnd_parent == NULL) {
        Section section = {{"Email", email},
                           {"KeyFilePath", key_file_path},
                           {"OAuthMechanism", o_auth_mechanism},
                           {"Catalog", catalog},
                           {"Dataset", dataset_name}};
        AddDSNToRegistry(dsn_value, lpsz_driver, section);
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
      o_auth_mechanism = ConvertOAuthMechanism(form.GetOAuthMechanism());
      catalog = form.GetCatalogName();
      dataset_name = form.GetDatasetName();
      Section section = {{"Email", email},
                         {"KeyFilePath", key_file_path},
                         {"OAuthMechanism", o_auth_mechanism},
                         {"Catalog", catalog},
                         {"Dataset", dataset_name}};
      AddDSNToRegistry(dsn_name, lpsz_driver, section);
      return TRUE;
    }
    case ODBC_CONFIG_DSN: {
      if (hwnd_parent == NULL) {
        Section section2 = {{"Email", email},
                            {"KeyFilePath", key_file_path},
                            {"OAuthMechanism", o_auth_mechanism},
                            {"Catalog", catalog},
                            {"Dataset", dataset_name}};
        EditDSNInRegistry(dsn_value, section2);
        return true;
      }
      std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_value;
      auto res = GetSectionWin(registry_key);
      auto section = res.GetValue();
      (*section)["DSN"] = dsn_value;
      form.SetValues(*section);
      form.Show();
      form.GetHwnd();
      MSG msg = {};
      while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      email = form.GetEmail();
      key_file_path = form.GetKeyFilePath();
      o_auth_mechanism = ConvertOAuthMechanism(form.GetOAuthMechanism());
      catalog = form.GetCatalogName();
      dataset_name = form.GetDatasetName();
      Section section2 = {{"Email", email},
                          {"KeyFilePath", key_file_path},
                          {"OAuthMechanism", o_auth_mechanism},
                          {"Catalog", catalog},
                          {"Dataset", dataset_name}};
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
