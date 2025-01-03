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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_H

#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {
// NEXTID:125
static int const kIdcAuthBox = 102;
static int const kIdcButtonOk = 103;
static int const kIdcHeaderLabel = 104;
static int const kIdcLabel = 105;
static int const kIdcButtonCancel = 106;
static int const kIdcEmailEdit = 107;
static int const kIdcKeyfileEdit = 108;
static int const kIdcBrowseButton = 109;
static int const kIdcCatalogLabel = 110;
static int const kIdcDatasetLabel = 111;
static int const kIdcCatlogBOX = 112;
static int const kIdcDatasetBOX = 113;
static int const kIdcDSNEdit = 114;
static int const kIdcButtonTest = 115;
static int const kIdcMinTLSComboBox = 116;
static int const kIdcTrustedCertEdit = 117;
static int const kIdcTrustedCertBrowseButton = 118;
static int const kIdcDescriptionEdit = 119;
static int const kIdcEncryptDataComboBox = 120;
static int const kIdcLoggingBtn = 121;
static int const kIdcProxyOptionsButton = 122;
static int const kIdcAdvanceOptBtn = 123;
static int const kIdcGcpFolder = 124;

class DriverForm {
 public:
  DriverForm(HWND parent_hwnd = NULL);
  ~DriverForm();
  void Show();
  HWND GetHwnd() const;
  void InitControls();
  void SetValues(Section const& attributes_map);
  odbc_internal::StatusRecord IsValidEmail(std::string const& email);
  inline std::string const& GetDSN() const { return dsn_name_; }
  inline std::string const& GetEmail() const { return email_; }

  inline std::string const& GetKeyFilePath() const { return key_file_path_; }

  inline std::string const& GetOAuthMechanism() const {
    return o_auth_mechanism_;
  }

  inline std::string const& GetDatasetName() const { return dataset_; }

  inline std::string const& GetCatalogName() const { return catalog_; }
  inline std::string const& GetEncryptData() const { return encrypt_data_; }
  inline std::string const& GetTrustedCerts() const { return trusted_cert_; }
  inline std::string const& GetMinTls() const { return min_tls_version_; }
  inline std::string const& GetDescription() const { return description_; }
  static odbc_internal::StatusRecord TestODBCConnection(
      std::shared_ptr<odbc_bq_driver_internal::Section> const& section);

  static std::string GetCatalogAndDataset(std::string const& action,
                                          std::string const& key_file_path,
                                          std::string const& oauth_token,
                                          std::string const& catalog_name = "");

 private:
  static std::string dsn_name_;
  static std::string email_;
  static std::string key_file_path_;
  static std::string o_auth_mechanism_;
  static std::string dataset_;
  static std::string catalog_;
  static std::string encrypt_data_;
  static std::string min_tls_version_;
  static std::string trusted_cert_;
  static std::string description_;
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT u_msg, WPARAM w_param,
                                     LPARAM l_param);
  HWND m_hwnd;
  HWND m_parent_hwnd;
  static char const CLASS_NAME[];
};

void OpenFileDialog(HWND hwnd, HWND h_edit, char const* mock_file_path);

}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_H
