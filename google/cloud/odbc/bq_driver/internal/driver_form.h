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

#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver_internal {

#ifdef _WIN32
static int const IDC_COMBOBOX = 102;
static int const IDC_BUTTON_OK = 103;
static int const IDC_HEADER_LABEL = 104;
static int const IDC_LABEL = 105;
static int const IDC_BUTTON_CANCEL = 106;
static int const IDC_EMAIL_EDIT = 107;
static int const IDC_KEYFILE_EDIT = 108;
static int const IDC_BROWSE_BUTTON = 109;
static int const IDC_Catalog_LABEL = 110;
static int const IDC_Dataset_LABEL = 111;
static int const IDC_Catlog_BOX = 112;
static int const IDC_Dataset_BOX = 113;

class DriverForm {
 public:
  DriverForm();
  ~DriverForm();
  void Show();
  HWND GetHwnd() const;

 private:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);
  HWND m_hwnd;
  static char const CLASS_NAME[];
};

void OpenFileDialog(HWND hwnd, HWND hEdit, char const* MockFilePath);

#endif /* WIN32 */

}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_FORM_H
