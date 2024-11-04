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
#include <string>

namespace google::cloud::odbc_bq_driver_internal {

#ifdef _WIN32
using google::cloud::odbc_bq_driver_internal::Section;
static int const kIdcComboBox = 102;
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
static int const kIdcButtonTest= 115;

class DriverForm {
 public:
  DriverForm();
  ~DriverForm();
  void Show();
  HWND GetHwnd() const;
  void InitControls();
  std::string SetValues(Section const& attributesMap);

  inline std::string const& GetDSN() const { return kDsnName; }
  inline std::string const& GetEmail() const { return kEmail; }

  inline std::string const& GetKeyFilePath() const { return kKeyFilePath; }

  inline std::string const& GetOAuthMechanism() const {
    return kOAuthMechanism;
  }

  inline std::string const& GetDatasetName() const { return kDataset; }

  inline std::string const& GetCatalogName() const { return kCatalog; }

 static bool returnStatus(bool status){
     kConnectionStatus =status;
    return kConnectionStatus;
  }
  static void CaptureValues(HWND hwnd);
 private:
  static bool kConnectionStatus;
  static std::string kDsnName;
  static std::string kEmail;
  static std::string kKeyFilePath;
  static std::string kOAuthMechanism;
  static std::string kDataset;
  static std::string kCatalog;
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);
  HWND m_hwnd;
  static char const CLASS_NAME[];
};

void OpenFileDialog(HWND hwnd, HWND hEdit, char const* MockFilePath);

#endif /* WIN32 */

}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_H
