// Copyright 2025 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_LOG_FORM_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_LOG_FORM_H

#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver_internal {
static int const kIdcLogBrowseBtn = 100;
static int const kIdcLogBtnOk = 101;
static int const kIdcLogBtnCancel = 102;
static int const kIdcLogFileEdit = 103;
static int const kIdclogTraceBox = 104;
static int const kIdcMaxFilesEdit = 105;  // Max Number Files Edit Box
static int const kIdcMaxSizeEdit = 106;   // Max File Size (MB) Edit Box
static int const kIdcGroupBox = 107;      // Group Box for Log Rotation
static int const kIdcHyperlink = 108;

class LogTraceDialog {
 public:
  LogTraceDialog();
  ~LogTraceDialog();
  void InitControls();
  HWND GetHwnd() const;
  void Show();
  void SetValues(Section const& attributes_map);
  inline std::string const& GetLogLevel() const { return log_level_; }
  inline std::string const& GetLogFilePath() const { return log_file_path_; }

  friend class DriverForm;

 private:
  HWND parent_hwnd;
  static std::string log_level_;
  static std::string log_file_path_;

  static LRESULT CALLBACK LogTraceProc(HWND hwnd, UINT u_msg, WPARAM w_param,
                                       LPARAM l_param);
  static char const CLASS_NAME[];
};

void OpenFolderDialog(HWND hwnd, HWND h_edit, char const* mock_folder_path);
}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_LOG_FORM_H
