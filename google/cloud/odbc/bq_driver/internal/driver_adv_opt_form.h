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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_ADV_OPT_FORM_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_ADV_OPT_FORM_H
// TODO(@khushikathuria008): Adding testcases for this file in follow up PR.
#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver_internal {
// NEXTID:144
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
static int const kIdcAllowLargeResultsCheckbox = 138;
static int const kIdcAdditionalProjectsEdit = 139;
static int const kIdcQueryPropertiesEdit = 140;
static int const kIdcOKButton = 141;
static int const kIdcCancelButton = 142;
static int const kIdcLanguageDialectComboBox = 143;

class AdvanceOptions {
 public:
  AdvanceOptions();
  ~AdvanceOptions();
  void CreateLanguageControls(HFONT h_font);
  void CreateLargeResultsControls(HFONT h_font);
  void CreateHighThroughputControls(HFONT h_font);
  void CreateEncryptionControls(HFONT h_font);
  void CreateSessionControls(HFONT h_font);
  void CreateAdditionalControls(HFONT h_font);
  void CreateButtons(HFONT h_font);
  void Show(HWND parent);
  HWND GetHwnd() const;

 private:
  HWND adv_hwnd;

  // The advanced options form is a child form, and only a single instance of it
  // should exist at any time.
  // Since UI interactions are generally not multithreaded and are handled
  // manually, using a static instance is safe.
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

}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DRIVER_ADV_OPT_FORM_H
