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
static int const kIdcButtonTest=115;
static int const kIdcProxyOKButton=116;
static int const kIdcProxyCancelButton=117;
static int const kIdcProxyHostLabel=118;
static int const kIdcMinTLSComboBox=119;
static int const kIdcTrustStoreCheckbox=120;
static int const kIdcTrustedCertEdit=121;
static int const kIdcTrustedCertBrowseButton=122;
static int const kIdcDescriptionEdit=123;
static int const kIdcEncryptDataComboBox=124;
static int const kIdcLoggingBtn = 125;
static int const kIdcProxyOptionsButton=126;
static int const kIdcAdvanceOptBtn=127;
static int const kIdcUseDefaultCheckbox = 128;
static int const kIdcDatasetNameEdit = 129;
static int const kIdcTempExpirationEdit = 130;
static int const kIdcAllowHighThroughputCheckbox = 131;
static int const kIdcActivationThresholdEdit = 132;
static int const kIdcEncryptionKeyEdit = 133;
static int const kIdcRowsPerBlockEdit = 134;
static int const kIdcDefaultStringEdit = 135;
static int const kIdcEnableSessionCheckbox = 136;
static int const kIdcSessionLocationEdit = 137;
static int const kIdcProxyHostName=138;
static int const kIdcAdditionalProjectsEdit = 139;
static int const kIdcQueryPropertiesEdit = 140;
static int const kIdcOKButton = 141;
static int const kIdcCancelButton = 142;
static int const kIdcLanguageDialectComboBox = 143;
static int const kIdcAllowLargeResultsCheckbox = 144;
static int const kIdcProxyCheckbox=145;
static int const kIdcProxyPortLabel=146;
static int const kIdcProxyPortEdit=147;
static int const kIdcProxyUsernameLabel=148;
static int const kIdcProxyUsernameEdit=149;
static int const kIdcProxyPasswordLabel=150;
static int const kIdcProxyPasswordEdit=151;


class DriverForm {
 public:
  DriverForm();
  ~DriverForm();
  void Show();
  HWND GetHwnd() const;
  void InitControls();
  void SetValues(Section const& attribute_map);
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
  inline std::string const& GetDescription() const{ return description_;}
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
  static char const CLASS_NAME[];
};
class AdvanceOptions {
 public:
  AdvanceOptions();
  ~AdvanceOptions();
  void InitControls();
  void Show(HWND parent);
  HWND GetHwnd() const;
  void SetValues(Section const& attributes_map);
  inline std::string const& GetLanguageDialect() const { return language_dialect_; }
  inline std::string const& GetDatasetName() const { return adv_dataset_name_; }
  inline std::string const& GetEncryptionKey() const { return encryption_key_; }
  inline std::string const& GetSessionLocation() const { return session_location_; }
  inline std::string const& GetAdditionalProjects() const { return additional_projects_; }
  inline std::string const& GetQueryProperties() const {return query_properties_; }
  inline std::string const& GetRowsPerBlock() const {return rows_per_block_; }
  inline std::string const& GetDefaultStringLength() const { return default_string_length_; }
  inline std::string const& GetTempTableExpiration() const { return temp_expiration_;}
  inline std::string const& GetActivationThreshold() const { return activation_threshold_;}
  private:
  HWND parent_hwnd;
  static std::string language_dialect_;
  static std::string adv_dataset_name_;
  static std::string temp_expiration_;
  static std::string encryption_key_;
  static std::string rows_per_block_;
  static std::string default_string_length_;
  static std::string session_location_;
  static std::string additional_projects_;
  static std::string query_properties_;
  static std::string activation_threshold_;
  static LRESULT CALLBACK AdvanceOptProc(HWND hwnd, UINT uMsg, WPARAM w_param,
                                       LPARAM l_param);
  static char const CLASS_NAME[];
};

class ProxyOptions {
 public:
  ProxyOptions();
  ~ProxyOptions();
  void InitControls();
  void Show(HWND parent);
  HWND GetHwnd() const;
  private:
  HWND proxy_hwnd;
  static LRESULT CALLBACK ProxyOptProc(HWND hwnd, UINT u_msg, WPARAM w_param,
                                       LPARAM l_param);
  static char const CLASS_NAME[];
};

void OpenFileDialog(HWND hwnd, HWND h_edit, char const* mock_file_path);

}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_H
