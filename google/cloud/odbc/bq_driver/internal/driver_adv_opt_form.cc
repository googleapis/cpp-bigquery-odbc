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

#include "google/cloud/odbc/bq_driver/internal/driver_adv_opt_form.h"
#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include <set>
#include <shellapi.h>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bq_driver_internal::kDefaultMaxThreads;
using google::cloud::odbc_bq_driver_internal::LanguageDialect;
using google::cloud::odbc_internal::StatusRecord;

char const AdvanceOptions::CLASS_NAME[] = "AdvanceOptClass";

std::string const kDefaultLanguageDialect = "GoogleSQL";
std::string const kDefaultRowsPerBlock = "100000";
std::string const kDefaultStringLength = "16384";
std::string const kDefaultEncryptionType = "Google-managed encryption key";
std::string const kDefaultLargeResultsDatasetId = "_odbc_temp_tables";

// Specifies the default SQL dialect used for queries.
// The default is "Standard SQL", but it may be overridden by the user.
std::string AdvanceOptions::language_dialect_ = kDefaultLanguageDialect;
std::string AdvanceOptions::adv_dataset_name_;

// Default expiration time for temporary objects in milliseconds.
// This value (3600000 ms) corresponds to 1 hour.
// It is used as a default configuration for temporary resource cleanup
// in the existing driver, ensuring unused resources do not persist
// indefinitely.
std::string AdvanceOptions::temp_expiration_ =
    kDefaultLargeResultsTableExpiration;
std::string AdvanceOptions::encryption_key_;

// Defines the number of rows per data block when fetching results.
// Default is 100,000 rows per block as per existing driver.
std::string AdvanceOptions::rows_per_block_ = kDefaultRowsPerBlock;

// Defines the default length of string columns in characters.
// The default is 16,384 characters as per existing driver.
std::string AdvanceOptions::default_string_length_ = kDefaultStringLength;
std::string AdvanceOptions::session_location_;
std::string AdvanceOptions::additional_projects_;
std::string AdvanceOptions::allowed_projects_;
std::string AdvanceOptions::query_properties_;
std::string AdvanceOptions::use_wchar_;
std::string AdvanceOptions::enable_session_;
std::string AdvanceOptions::allow_htapi_for_large_results_checkbox_;
std::string AdvanceOptions::allow_large_results_;
std::string AdvanceOptions::use_default_large_results_;
std::string AdvanceOptions::encryption_type_ = kDefaultEncryptionType;
std::string AdvanceOptions::max_threads_ = std::to_string(kDefaultMaxThreads);
std::string AdvanceOptions::max_retries_ = std::to_string(kDefaultMaxRetries);
std::string AdvanceOptions::private_service_connect_uris_;
std::string AdvanceOptions::enable_gcd_;
std::string AdvanceOptions::universe_domain_;
int AdvanceOptions::scroll_pos_ = 0;
int AdvanceOptions::wheel_remainder_ = 0;

std::string const kLanguageDialect = "SQLDialect";
std::string const kLargeResultsDatasetId = "LargeResultsDatasetId";
std::string const kEncryptionKey = "KMSKeyName";
std::string const kRowsFetchedPerBlock = "RowsFetchedPerBlock";
std::string const kDefaultStringColumnLength = "DefaultStringColumnLength";
std::string const kLargeResultsTempTableExpirationTime =
    "LargeResultsTempTableExpirationTime";
std::string const kSessionLocation = "SessionLocation";
std::string const kAdditionalProjects = "AdditionalProjects";
std::string const kAllowedProjects = "AllowedProjects";
std::string const kQueryProperties = "QueryProperties";
std::string const kUseWChar = "UseWVarChar";
std::string const kEnableSession = "EnableSession";
std::string const kAllowHtapiForLargeResults = "AllowHtapiForLargeResults";
std::string const kAllowLargeResults = "AllowLargeResults";
std::string const kUseDefaultLargeResultsDataset =
    "UseDefaultLargeResultsDataset";
std::string const kEncryptionType = "EncryptionType";
std::string const kMaxThreads = "MaxThreads";
std::string const kMaxRetries = "MaxRetries";
std::string const kPrivateServiceConnectUris = "PrivateServiceConnectUris";
std::string const kEnableGcd = "EnableGCD";
std::string const kUniverseDomain = "UniverseDomain";

// Control dimensions and positions
int const kHeight = 20;
int const kWidth = 50;
int const kButtonHeight = 17;
int const kButtonWidth = 68;
int const kXAxis = 10;
int const kOkButtonX = 330;
int const kCancelButtonX = 410;
// The allowed-projects pick list and its "Load Projects" button occupy the
// space below the query-properties box, so the OK/Cancel row sits lower than
// the other control offsets in this file would suggest.
int const kButtonY = 753;
int const kAllowedProjectsListHeight = 100;
int const kYAxis = 20;
int const kEditBoxWidth = 260;
int const kEditBoxHeight = 17;
int const kinputComboBoxXAxis = 237;
int const KComboBoxHeight = 100;

// Full height of the laid-out controls. The window is clamped to the desktop
// work area, so on a short or DPI-scaled display this is larger than the client
// area and the difference is what scrolls.
int const kContentHeight = kButtonY + 44 + kButtonHeight + 10;

// Pixels scrolled per scrollbar arrow click.
int const kScrollLine = 20;

HWND AdvanceOptions::GetHwnd() const { return adv_hwnd; }
AdvanceOptions::AdvanceOptions() : adv_hwnd(NULL) {}
AdvanceOptions::~AdvanceOptions() {
  if (adv_hwnd) {
    DestroyWindow(adv_hwnd);
  }
  UnregisterClass(CLASS_NAME, g_hDllInstance);
}

void SetPscGcdEnabled(HWND hwnd, bool enabled) {
  EnableWindow(GetDlgItem(hwnd, kIdcPrivateServiceNameEdit),
               enabled ? TRUE : FALSE);
  EnableWindow(GetDlgItem(hwnd, kIdcUniverseDomainEdit),
               enabled ? TRUE : FALSE);
  if (!enabled) {
    SetWindowText(GetDlgItem(hwnd, kIdcPrivateServiceNameEdit), TEXT(""));
    SetWindowText(GetDlgItem(hwnd, kIdcUniverseDomainEdit), TEXT(""));
  }
}
void AdvanceOptions::CreateLanguageControls(HFONT h_font) {
  HWND h_language_header =
      CreateLabel(adv_hwnd, "Language dialect", kXAxis, kYAxis, kWidth * 3,
                  kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_language_header, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_language_combo_box =
      CreateComboBox(adv_hwnd, kinputComboBoxXAxis, kYAxis, kEditBoxWidth,
                     KComboBoxHeight, kIdcLanguageDialectComboBox);
  SendMessage(h_language_combo_box, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcLanguageDialectComboBox),
                    ComboBoxSubclassProc, 0, 0);
  SendMessage(h_language_combo_box, CB_ADDSTRING, 0, (LPARAM) "GoogleSQL");
  SendMessage(h_language_combo_box, CB_ADDSTRING, 0, (LPARAM) "LegacySQL");
  SendMessage(h_language_combo_box, CB_SETCURSEL, 0, 0);
  SetWindowText(h_language_combo_box, language_dialect_.c_str());

  HWND h_edit = GetWindow(h_language_combo_box, GW_CHILD);
  SetWindowSubclass(h_edit, EditBlockSubclassProc, 1, 0);
}

void AdvanceOptions::CreateLargeResultsControls(HFONT h_font) {
  HWND h_large_results_header =
      CreateGroupBox(adv_hwnd, "Large results options", kXAxis, kYAxis + 25,
                     kWidth + 445, kHeight + 128, KIdcLargeResultHeader);
  SendMessage(h_large_results_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_allow_large_results_checkbox = CreateCheckBox(
      adv_hwnd, "Allow large result sets", kXAxis + 5, kYAxis + 50, kWidth * 6,
      kHeight, kIdcAllowLargeResultsCheckbox);
  SendMessage(h_allow_large_results_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  CheckDlgButton(adv_hwnd, kIdcAllowLargeResultsCheckbox,
                 (allow_large_results_ == "1") ? BST_CHECKED : BST_UNCHECKED);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcAllowLargeResultsCheckbox),
                    CheckboxSubclassProc, 0, 0);
  HWND h_language_box = GetDlgItem(adv_hwnd, kIdcLanguageDialectComboBox);
  char language_buffer[256] = {0};
  GetWindowText(h_language_box, language_buffer, sizeof(language_buffer));
  HWND h_use_default_checkbox = CreateCheckBox(
      adv_hwnd, "Use default \"_bqodbc_temp_tables\" large results dataset",
      kXAxis + 5, kYAxis + 75, kWidth * 6 + 15, kHeight,
      kIdcUseDefaultCheckbox);
  SendMessage(h_use_default_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcUseDefaultCheckbox),
                    CheckboxSubclassProc, 0, 0);

  HWND h_dataset_name_label =
      CreateLabel(adv_hwnd, "Dataset name for large result sets:", kXAxis + 5,
                  kYAxis + 100, kWidth * 4, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_dataset_name_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_dataset_name_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 100, kEditBoxWidth,
                    kEditBoxHeight, kIdcDatasetNameEdit);
  SendMessage(h_dataset_name_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_dataset_name_edit, adv_dataset_name_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcDatasetNameEdit),
                    InputSubclassProc, 0, 0);
  if (use_default_large_results_ == "1") {
    CheckDlgButton(adv_hwnd, kIdcUseDefaultCheckbox, BST_CHECKED);
    EnableWindow(h_dataset_name_edit, FALSE);
  }
  HWND h_temp_expiration_label = CreateLabel(
      adv_hwnd, "Default temp table expiration time (ms):", kXAxis + 5,
      kYAxis + 125, kWidth * 4.3, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_temp_expiration_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_temp_expiration_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 125, kEditBoxWidth,
                    kEditBoxHeight, kIdcTempExpirationEdit);
  SendMessage(h_temp_expiration_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_temp_expiration_edit, temp_expiration_.c_str());
  SetWindowLong(h_temp_expiration_edit, GWL_STYLE,
                GetWindowLong(h_temp_expiration_edit, GWL_STYLE) | ES_NUMBER);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcTempExpirationEdit),
                    InputSubclassProc, 0, 0);
}

void AdvanceOptions::CreateHighThroughputControls(HFONT h_font) {
  HWND h_allow_high_throughput_checkbox = CreateCheckBox(
      adv_hwnd, "Allow BigQuery Storage API for large results queries",
      kXAxis + 5, kYAxis + 150, kWidth * 7 + 20, kHeight,
      kIdcAllowHighThroughputCheckbox);
  SendMessage(h_allow_high_throughput_checkbox, WM_SETFONT, (WPARAM)h_font,
              TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcAllowHighThroughputCheckbox),
                    CheckboxSubclassProc, 0, 0);
  CheckDlgButton(adv_hwnd, kIdcAllowHighThroughputCheckbox,
                 (allow_htapi_for_large_results_checkbox_ == "1")
                     ? BST_CHECKED
                     : BST_UNCHECKED);
}

void AdvanceOptions::CreatePscGcdControls(HFONT h_font) {
  HWND h_psc_header = CreateGroupBox(
      adv_hwnd, "Private Service Connect and Google Cloud Dedicated Options",
      kXAxis, kYAxis + 186, kWidth + 445, kHeight + 72, 0);
  SendMessage(h_psc_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_enable_psc_gcd_checkbox = CreateCheckBox(
      adv_hwnd, "Enable PSC and GCD Configuration", kXAxis + 5, kYAxis + 201,
      kWidth * 7, kHeight, kIdcEnablePscGcdCheckbox);
  SendMessage(h_enable_psc_gcd_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  CheckDlgButton(adv_hwnd, kIdcEnablePscGcdCheckbox,
                 (enable_gcd_ == "1" || enable_gcd_ == "true") ? BST_CHECKED
                                                               : BST_UNCHECKED);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcEnablePscGcdCheckbox),
                    CheckboxSubclassProc, 0, 0);

  HWND h_private_service_name_label =
      CreateLabel(adv_hwnd, "Private Service name:", kXAxis + 5, kYAxis + 229,
                  kWidth * 4, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_private_service_name_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_private_service_name_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 225, kEditBoxWidth,
                    kEditBoxHeight, kIdcPrivateServiceNameEdit);
  SendMessage(h_private_service_name_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_private_service_name_edit,
                private_service_connect_uris_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcPrivateServiceNameEdit),
                    InputSubclassProc, 0, 0);

  HWND h_universe_domain_label =
      CreateLabel(adv_hwnd, "Universe Domain:", kXAxis + 5, kYAxis + 257,
                  kWidth * 4, kHeight - 5, WS_VISIBLE | SS_LEFT);
  SendMessage(h_universe_domain_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_universe_domain_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 253, kEditBoxWidth,
                    kEditBoxHeight, kIdcUniverseDomainEdit);
  SendMessage(h_universe_domain_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_universe_domain_edit, universe_domain_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcUniverseDomainEdit),
                    InputSubclassProc, 0, 0);

  SetPscGcdEnabled(
      adv_hwnd,
      IsDlgButtonChecked(adv_hwnd, kIdcEnablePscGcdCheckbox) == BST_CHECKED);
}

void AdvanceOptions::CreateEncryptionControls(HFONT h_font) {
  HWND h_high_encryption_header =
      CreateLabel(adv_hwnd, "Encryption", kXAxis, kYAxis + 281, kWidth * 5,
                  kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_high_encryption_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_encryption_combo_box =
      CreateComboBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 281, kEditBoxWidth,
                     KComboBoxHeight, kIdcEncryptionKeyComboBox);
  SendMessage(h_encryption_combo_box, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcEncryptionKeyComboBox),
                    ComboBoxSubclassProc, 0, 0);
  SendMessage(h_encryption_combo_box, CB_ADDSTRING, 0, (LPARAM) "(Empty)");
  SendMessage(h_encryption_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Google-managed encryption key");
  SendMessage(h_encryption_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Customer-managed encryption key");
  SendMessage(h_encryption_combo_box, CB_SETCURSEL, 0, 0);
  SetWindowText(h_encryption_combo_box, encryption_type_.c_str());

  HWND h_edit = GetWindow(h_encryption_combo_box, GW_CHILD);
  SetWindowSubclass(h_edit, EditBlockSubclassProc, 1, 0);

  HWND h_encryption_key_header =
      CreateLabel(adv_hwnd, "Customer-managed encryption key:", kXAxis,
                  kYAxis + 310, kWidth * 4.3, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_encryption_key_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_encryption_key_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 310, kEditBoxWidth,
                    kEditBoxHeight, kIdcEncryptionKeyEdit);
  SendMessage(h_encryption_key_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcEncryptionKeyEdit),
                    InputSubclassProc, 0, 0);
  if (encryption_type_ == "Customer-managed encryption key") {
    SetWindowText(h_encryption_key_edit, encryption_key_.c_str());
    EnableWindow(h_encryption_key_edit, TRUE);
  } else {
    SetWindowText(h_encryption_key_edit, "");
    EnableWindow(h_encryption_key_edit, FALSE);
  }
}

void AdvanceOptions::CreateSessionControls(HFONT h_font) {
  HWND h_rows_per_block_label =
      CreateLabel(adv_hwnd, "Rows per block:", kXAxis, kYAxis + 335, kWidth * 3,
                  kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_rows_per_block_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_rows_per_block_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 335, kEditBoxWidth,
                    kEditBoxHeight, kIdcRowsPerBlockEdit);
  SendMessage(h_rows_per_block_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcRowsPerBlockEdit),
                    InputSubclassProc, 0, 0);
  SetWindowText(h_rows_per_block_edit, rows_per_block_.c_str());
  SetWindowLongPtr(h_rows_per_block_edit, GWL_STYLE,
                   GetWindowLongPtr(h_rows_per_block_edit, GWL_STYLE) |
                       ES_RIGHT | ES_NUMBER);
  HWND h_default_string_label =
      CreateLabel(adv_hwnd, "Default string column length:", kXAxis,
                  kYAxis + 360, kWidth * 4.3, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_default_string_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_default_string_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 360, kEditBoxWidth,
                    kEditBoxHeight, kIdcDefaultStringEdit);
  SendMessage(h_default_string_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcDefaultStringEdit),
                    InputSubclassProc, 0, 0);
  SetWindowText(h_default_string_edit, default_string_length_.c_str());
  SetWindowLongPtr(h_default_string_edit, GWL_STYLE,
                   GetWindowLongPtr(h_default_string_edit, GWL_STYLE) |
                       ES_RIGHT | ES_NUMBER);

  HWND h_enable_session_checkbox =
      CreateCheckBox(adv_hwnd, "Enable session", kXAxis, kYAxis + 385,
                     kWidth * 2 + 30, kHeight, kIdcEnableSessionCheckbox);
  SendMessage(h_enable_session_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcEnableSessionCheckbox),
                    CheckboxSubclassProc, 0, 0);
  CheckDlgButton(adv_hwnd, kIdcEnableSessionCheckbox,
                 (enable_session_ == "1") ? BST_CHECKED : BST_UNCHECKED);

  HWND h_session_location_label =
      CreateLabel(adv_hwnd, "Session location:", kXAxis, kYAxis + 415,
                  kWidth * 2 + 30, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_session_location_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_session_location_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 410, kEditBoxWidth,
                    kEditBoxHeight, kIdcSessionLocationEdit);
  SendMessage(h_session_location_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcSessionLocationEdit),
                    InputSubclassProc, 0, 0);
  EnableWindow(h_session_location_edit, FALSE);
  SetWindowText(h_session_location_edit, session_location_.c_str());
  h_session_location_edit = GetDlgItem(adv_hwnd, kIdcSessionLocationEdit);
  EnableWindow(h_session_location_edit,
               TRUE);  // Enable the session location input box
}

void AdvanceOptions::CreateAdditionalControls(HFONT h_font) {
  // max threads
  HWND h_max_threads_label =
      CreateLabel(adv_hwnd, "Default number of Threads:", kXAxis, kYAxis + 440,
                  kWidth * 7, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_max_threads_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_max_threads_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 435, kEditBoxWidth,
                    kEditBoxHeight, kIdcMaxThreadsEdit);
  SendMessage(h_max_threads_edit, WM_SETFONT, (WPARAM)h_font, TRUE);

  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcMaxThreadsEdit), InputSubclassProc,
                    0, 0);
  SetWindowText(h_max_threads_edit, max_threads_.c_str());
  SetWindowLongPtr(
      h_max_threads_edit, GWL_STYLE,
      GetWindowLongPtr(h_max_threads_edit, GWL_STYLE) | ES_RIGHT | ES_NUMBER);

  // max retries
  HWND h_max_retries_label =
      CreateLabel(adv_hwnd, "Max Retries:", kXAxis, kYAxis + 465, kWidth * 7,
                  kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_max_retries_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_max_retries_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 465, kEditBoxWidth,
                    kEditBoxHeight, kIdcMaxRetriesEdit);
  SendMessage(h_max_retries_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcMaxRetriesEdit), InputSubclassProc,
                    0, 0);
  SetWindowText(h_max_retries_edit, max_retries_.c_str());
  SetWindowLongPtr(
      h_max_retries_edit, GWL_STYLE,
      GetWindowLongPtr(h_max_retries_edit, GWL_STYLE) | ES_RIGHT | ES_NUMBER);
  // TODO(b/497725655): Enable UI feature after public release
  // HWND h_variables_checkbox = CreateCheckBox(
  //     adv_hwnd, "Use SQL_WVARCHAR instead of SQL_VARCHAR", kXAxis, kYAxis +
  //     390, kWidth * 7, kHeight, kIdcVariableCheckbox);
  // CheckDlgButton(adv_hwnd, kIdcVariableCheckbox,
  //                (use_wchar_ == "1") ? BST_CHECKED : BST_UNCHECKED);
  // SendMessage(h_variables_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  // SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcVariableCheckbox),
  //                   CheckboxSubclassProc, 0, 0);
  HWND h_additional_projects_label =
      CreateLabel(adv_hwnd, "Additional projects:", kXAxis, kYAxis + 495,
                  kWidth * 5, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_additional_projects_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_additional_projects_edit =
      CreateScrollableEditBox(adv_hwnd, kXAxis, kYAxis + 515, kWidth + 445,
                              kHeight + 32, kIdcAdditionalProjectsEdit);
  SendMessage(h_additional_projects_edit, WM_SETFONT, (WPARAM)h_font, TRUE);

  SetWindowText(h_additional_projects_edit, additional_projects_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcAdditionalProjectsEdit),
                    InputSubclassProc, 0, 0);

  HWND h_query_properties_label =
      CreateLabel(adv_hwnd, "Query properties:", kXAxis, kYAxis + 575,
                  kWidth * 5, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_query_properties_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_query_properties_edit =
      CreateScrollableEditBox(adv_hwnd, kXAxis, kYAxis + 595, kWidth + 445,
                              kHeight + 13, kIdcQueryPropertiesEdit);
  SendMessage(h_query_properties_edit, WM_SETFONT, (WPARAM)h_font, TRUE);

  SetWindowText(h_query_properties_edit, query_properties_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcQueryPropertiesEdit),
                    InputSubclassProc, 0, 0);

  HWND h_allowed_projects_label = CreateLabel(
      adv_hwnd, "Allowed projects (all accessible if none checked):", kXAxis,
      kYAxis + 635, kWidth * 6, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_allowed_projects_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_load_projects_button =
      CreateButton(adv_hwnd, "Load Projects", kXAxis + 400, kYAxis + 633,
                   kButtonWidth + 22, kButtonHeight, kIdcLoadProjectsButton);
  SendMessage(h_load_projects_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_allowed_projects_list_view =
      CreateListView(adv_hwnd, kXAxis, kYAxis + 655, kWidth + 445,
                     kAllowedProjectsListHeight, kIdcAllowedProjectsListView);
  SendMessage(h_allowed_projects_list_view, WM_SETFONT, (WPARAM)h_font, TRUE);
  // Without contacting the account, the ids saved in the DSN are all we know.
  // Show them ticked so OK round-trips the value even if the user never
  // presses "Load Projects".
  PopulateAllowedProjectsListView(h_allowed_projects_list_view, {});

  // This feature is turned off for the private release. It will be restored for
  // the public release with an accompanying documentation link.
  // TODO(b/461668255):Restore BigQuery documentation URL

  // HWND h_doc_text = CreateLabel(adv_hwnd, "Not sure what to enter? See",
  // kXAxis,
  //                               kButtonY + 10, kWidth + 110, kHeight, 0);
  // SendMessage(h_doc_text, WM_SETFONT, (WPARAM)h_font, TRUE);
  // HWND h_hyperlink =
  //     CreateHyperlinkLabel(adv_hwnd, "BigQuery documentation", 144,
  //                          kButtonY + 10, kWidth + 90, kHeight,
  //                          kIdcHyperlink2);
  // SendMessage(h_hyperlink, WM_SETFONT, (WPARAM)h_font, TRUE);
}

void AdvanceOptions::UpdateScrollInfo(HWND hwnd) {
  RECT client = {};
  GetClientRect(hwnd, &client);
  int const page = client.bottom - client.top;

  SCROLLINFO si = {};
  si.cbSize = sizeof(si);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = kContentHeight - 1;
  si.nPage = page;
  si.nPos = scroll_pos_;
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

  // Growing the window can leave us scrolled past the end; pull the content
  // back so there is never blank space below the last control.
  int const max_pos = (kContentHeight > page) ? kContentHeight - page : 0;
  if (scroll_pos_ > max_pos) {
    ScrollWindow(hwnd, 0, scroll_pos_ - max_pos, NULL, NULL);
    scroll_pos_ = max_pos;
    si.fMask = SIF_POS;
    si.nPos = scroll_pos_;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
  }
}

void AdvanceOptions::ScrollTo(HWND hwnd, int new_pos) {
  RECT client = {};
  GetClientRect(hwnd, &client);
  int const page = client.bottom - client.top;
  int const max_pos = (kContentHeight > page) ? kContentHeight - page : 0;
  new_pos = std::max(0, std::min(new_pos, max_pos));
  if (new_pos == scroll_pos_) {
    return;
  }

  int const delta = scroll_pos_ - new_pos;
  scroll_pos_ = new_pos;
  // ScrollWindow shifts the child controls along with the client area, which is
  // what makes absolutely-positioned controls scroll without repositioning each
  // one by hand.
  ScrollWindow(hwnd, 0, delta, NULL, NULL);

  SCROLLINFO si = {};
  si.cbSize = sizeof(si);
  si.fMask = SIF_POS;
  si.nPos = scroll_pos_;
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
  UpdateWindow(hwnd);
}

void AdvanceOptions::PopulateAllowedProjectsListView(
    HWND h_list_view, std::vector<std::string> const& project_ids) {
  if (!h_list_view) {
    return;
  }

  // Ticked ids must survive a reload, whether they came from the saved DSN or
  // from ticks the user made before pressing "Load Projects".
  std::set<std::string> checked;
  for (auto& project_id : Split(allowed_projects_, ",")) {
    Trim(project_id);
    if (!project_id.empty()) {
      checked.insert(project_id);
    }
  }
  int const existing_count = ListView_GetItemCount(h_list_view);
  for (int i = 0; i < existing_count; ++i) {
    char buffer[256] = {0};
    ListView_GetItemText(h_list_view, i, 0, buffer,
                         static_cast<int>(sizeof(buffer)));
    if (buffer[0] != '\0' && ListView_GetCheckState(h_list_view, i)) {
      checked.insert(buffer);
    }
  }

  // Keep ticked ids the account no longer reports, so reloading the list never
  // silently drops a value the user had already saved.
  std::vector<std::string> rows = project_ids;
  std::set<std::string> const listed(project_ids.begin(), project_ids.end());
  for (auto const& project_id : checked) {
    if (listed.find(project_id) == listed.end()) {
      rows.push_back(project_id);
    }
  }

  ListView_DeleteAllItems(h_list_view);
  for (size_t i = 0; i < rows.size(); ++i) {
    LVITEM item = {};
    item.mask = LVIF_TEXT;
    item.iItem = static_cast<int>(i);
    item.iSubItem = 0;
    item.pszText = const_cast<char*>(rows[i].c_str());
    int const index = ListView_InsertItem(h_list_view, &item);
    if (index >= 0 && checked.find(rows[i]) != checked.end()) {
      ListView_SetCheckState(h_list_view, index, TRUE);
    }
  }
}

std::string AdvanceOptions::CollectCheckedProjects(HWND h_list_view) {
  // Without the control there is nothing to read; keep the value already held
  // rather than clearing it.
  if (!h_list_view) {
    return allowed_projects_;
  }
  std::vector<std::string> checked;
  int const count = ListView_GetItemCount(h_list_view);
  for (int i = 0; i < count; ++i) {
    if (!ListView_GetCheckState(h_list_view, i)) {
      continue;
    }
    char buffer[256] = {0};
    ListView_GetItemText(h_list_view, i, 0, buffer,
                         static_cast<int>(sizeof(buffer)));
    if (buffer[0] != '\0') {
      checked.push_back(buffer);
    }
  }
  return Join(checked, ",");
}

void AdvanceOptions::CreateButtons(HFONT h_font) {
  HWND h_ok_button =
      CreateButton(adv_hwnd, "OK", kOkButtonX + 12, kButtonY + 44, kButtonWidth,
                   kButtonHeight, kIdcOKButton);
  SendMessage(h_ok_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_cancel_button =
      CreateButton(adv_hwnd, "Cancel", kCancelButtonX + 10, kButtonY + 44,
                   kButtonWidth, kButtonHeight, kIdcCancelButton);
  SendMessage(h_cancel_button, WM_SETFONT, (WPARAM)h_font, TRUE);
}

LRESULT CALLBACK AdvanceOptions::AdvanceOptProc(HWND hwnd, UINT u_msg,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  AdvanceOptions* p_current_window = NULL;
  if (u_msg == WM_NCCREATE) {
    CREATESTRUCT* pCreate = (CREATESTRUCT*)l_param;
    p_current_window = (AdvanceOptions*)pCreate->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)p_current_window);
  } else {
    p_current_window = (AdvanceOptions*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }
  switch (u_msg) {
    case WM_CREATE:
      setWindowIcon(hwnd);
      break;
    case WM_ERASEBKGND: {
      HDC hdc = (HDC)w_param;
      RECT rc;
      GetClientRect(hwnd, &rc);
      HBRUSH h_brush = CreateSolidBrush(RGB(240, 240, 240));
      FillRect(hdc, &rc, h_brush);
      DeleteObject(h_brush);
      return 1;  // Indicate we handled the background redraw
    }
    case WM_LBUTTONDOWN: {
      // Same guard as in driver_form.cc: the documentation hyperlink is not
      // created at present, so without checking for a NULL control the
      // uninitialised rect can swallow clicks anywhere on the dialog.
      HWND h_hyperlink = GetDlgItem(hwnd, kIdcHyperlink2);
      if (h_hyperlink == NULL) {
        break;
      }
      RECT rect = {};
      if (!GetClientRect(h_hyperlink, &rect)) {
        break;
      }
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
      MapWindowPoints(h_hyperlink, hwnd, (LPPOINT)&rect, 2);
      if (PtInRect(&rect, pt)) {
        ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL, SW_SHOWNORMAL);
      }
      break;
    }
    case WM_CTLCOLORSTATIC: {
      HDC hdc_static = (HDC)w_param;
      HWND hwnd_static = (HWND)l_param;
      if (GetDlgCtrlID(hwnd_static) == kIdcHyperlink2) {
        SetTextColor(hdc_static, RGB(0, 0, 255));  // Set text color to blue
        SetBkMode(hdc_static, TRANSPARENT);        // Transparent background
        static HFONT h_font_underline = NULL;
        if (!h_font_underline) {
          LOGFONT lf;
          HFONT h_font = (HFONT)SendMessage(hwnd_static, WM_GETFONT, 0, 0);
          if (h_font && GetObject(h_font, sizeof(LOGFONT), &lf)) {
            lf.lfUnderline = TRUE;  // Enable underline
            h_font_underline = CreateFontIndirect(&lf);
          }
        }
        if (h_font_underline) {
          SelectObject(hdc_static, h_font_underline);
        }
        return (LRESULT)GetSysColorBrush(COLOR_3DFACE);
      }
      break;
    }
    case WM_COMMAND: {
      int wm_id = LOWORD(w_param);
      switch (wm_id) {
        case kIdcHyperlink2:
          if (HIWORD(w_param) == STN_CLICKED) {
            ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL,
                         SW_SHOWNORMAL);
          }
          break;
        case kIdcOKButton: {
          HWND h_language_box = GetDlgItem(hwnd, kIdcLanguageDialectComboBox);
          char language_buffer[256] = {0};
          GetWindowText(h_language_box, language_buffer,
                        sizeof(language_buffer));
          language_dialect_ = language_buffer;

          HWND h_dataset_name_edit = GetDlgItem(hwnd, kIdcDatasetNameEdit);
          char dataset_name_buffer[256] = {0};
          GetWindowText(h_dataset_name_edit, dataset_name_buffer,
                        sizeof(dataset_name_buffer));
          adv_dataset_name_ = dataset_name_buffer;

          HWND h_temp_expiration_edit =
              GetDlgItem(hwnd, kIdcTempExpirationEdit);
          char temp_expiration_buffer[256] = {0};
          GetWindowText(h_temp_expiration_edit, temp_expiration_buffer,
                        sizeof(temp_expiration_buffer));
          if (isValidUint32(temp_expiration_buffer)) {
            temp_expiration_ = temp_expiration_buffer;
          } else {
            auto err_msg =
                "Invalid temporary table expiry: Valid values are in range "
                "[0," +
                std::to_string(UINT32_MAX) + "]";
            ShowErrorWindow(hwnd, err_msg);
            return true;
          }
          HWND h_encryption_key_edit = GetDlgItem(hwnd, kIdcEncryptionKeyEdit);
          char encryption_key_buffer[256] = {0};
          GetWindowText(h_encryption_key_edit, encryption_key_buffer,
                        sizeof(encryption_key_buffer));
          encryption_key_ = encryption_key_buffer;

          HWND h_rows_per_block_edit = GetDlgItem(hwnd, kIdcRowsPerBlockEdit);
          char rows_per_block_buffer[256] = {0};
          GetWindowText(h_rows_per_block_edit, rows_per_block_buffer,
                        sizeof(rows_per_block_buffer));
          if (isValidUint32(rows_per_block_buffer)) {
            rows_per_block_ = rows_per_block_buffer;
          } else {
            auto err_msg =
                "Invalid rows per block: Valid values are in range [0," +
                std::to_string(UINT32_MAX) + "]";
            ShowErrorWindow(hwnd, err_msg);
            return true;
          }
          HWND h_default_string_edit = GetDlgItem(hwnd, kIdcDefaultStringEdit);
          char default_string_buffer[256] = {0};
          GetWindowText(h_default_string_edit, default_string_buffer,
                        sizeof(default_string_buffer));
          if (isValidUint32(default_string_buffer)) {
            default_string_length_ = default_string_buffer;
          } else {
            auto err_msg =
                "Invalid default string length: Valid values are in range [0," +
                std::to_string(UINT32_MAX) + "]";
            ShowErrorWindow(hwnd, err_msg);
            return true;
          }
          HWND h_encryption_combo_box =
              GetDlgItem(hwnd, kIdcEncryptionKeyComboBox);
          char encryption_type_buffer[256] = {0};
          GetWindowText(h_encryption_combo_box, encryption_type_buffer,
                        sizeof(encryption_type_buffer));
          if (strcmp(encryption_type_buffer, "(Empty)") == 0) {
            encryption_type_.clear();
          } else {
            encryption_type_ = encryption_type_buffer;
          }

          HWND h_session_location_edit =
              GetDlgItem(hwnd, kIdcSessionLocationEdit);
          char session_location_buffer[256] = {0};
          GetWindowText(h_session_location_edit, session_location_buffer,
                        sizeof(session_location_buffer));
          if (IsDlgButtonChecked(hwnd, kIdcEnableSessionCheckbox)) {
            session_location_ = session_location_buffer;
          } else {
            session_location_ = "";
          }

          HWND h_max_threads_edit = GetDlgItem(hwnd, kIdcMaxThreadsEdit);
          char max_threads_buff[256] = {0};
          GetWindowText(h_max_threads_edit, max_threads_buff,
                        sizeof(max_threads_buff));
          if (isValidUint32(max_threads_buff)) {
            max_threads_ = max_threads_buff;
          } else {
            std::string err_msg =
                "Invalid number of max threads: Valid values are in range [0," +
                std::to_string(UINT32_MAX) + "]";
            ShowErrorWindow(hwnd, err_msg);
            return true;
          }
          HWND h_max_retries_edit = GetDlgItem(hwnd, kIdcMaxRetriesEdit);
          char max_retries_buff[256] = {0};
          GetWindowText(h_max_retries_edit, max_retries_buff,
                        sizeof(max_retries_buff));
          if (isValidUint32(max_retries_buff)) {
            max_retries_ = max_retries_buff;
          } else {
            std::string err_msg =
                "Invalid number of max retries: Valid values are in range [0," +
                std::to_string(UINT32_MAX) + "]";
            ShowErrorWindow(hwnd, err_msg);
            return true;
          }

          HWND h_additional_projects_edit =
              GetDlgItem(hwnd, kIdcAdditionalProjectsEdit);
          char additional_projects_buffer[1024] = {0};
          GetWindowText(h_additional_projects_edit, additional_projects_buffer,
                        sizeof(additional_projects_buffer));
          additional_projects_ = additional_projects_buffer;

          allowed_projects_ = CollectCheckedProjects(
              GetDlgItem(hwnd, kIdcAllowedProjectsListView));

          HWND h_query_properties_edit =
              GetDlgItem(hwnd, kIdcQueryPropertiesEdit);
          char query_properties_buffer[1024] = {0};
          GetWindowText(h_query_properties_edit, query_properties_buffer,
                        sizeof(query_properties_buffer));
          query_properties_ = query_properties_buffer;
          auto parse_result = ParseQueryProperties(query_properties_);

          if (!parse_result) {
            LOG(ERROR)
                << "AdvanceOptions::AdvanceOptProc::ParseQueryProperties:: "
                << parse_result.GetStatusRecord().message;
            MessageBox(h_query_properties_edit,
                       parse_result.GetStatusRecord().message.c_str(), "Error",
                       MB_OK | MB_ICONERROR);
            return 0;
          }

          HWND h_private_service_name_edit =
              GetDlgItem(hwnd, kIdcPrivateServiceNameEdit);
          char private_service_name_buffer[1024] = {0};
          GetWindowText(h_private_service_name_edit,
                        private_service_name_buffer,
                        sizeof(private_service_name_buffer));
          private_service_connect_uris_ = private_service_name_buffer;

          HWND h_universe_domain_edit =
              GetDlgItem(hwnd, kIdcUniverseDomainEdit);
          char universe_domain_buffer[256] = {0};
          GetWindowText(h_universe_domain_edit, universe_domain_buffer,
                        sizeof(universe_domain_buffer));
          universe_domain_ = universe_domain_buffer;
          // TODO(b/497725655): Enable UI feature after public release
          // use_wchar_ =
          //     (IsDlgButtonChecked(hwnd, kIdcVariableCheckbox) == BST_CHECKED)
          //         ? "1"
          //         : "0";

          enable_session_ =
              (IsDlgButtonChecked(hwnd, kIdcEnableSessionCheckbox) ==
               BST_CHECKED)
                  ? "1"
                  : "0";

          allow_htapi_for_large_results_checkbox_ =
              (IsDlgButtonChecked(hwnd, kIdcAllowHighThroughputCheckbox) ==
               BST_CHECKED)
                  ? "1"
                  : "0";

          enable_gcd_ = (IsDlgButtonChecked(hwnd, kIdcEnablePscGcdCheckbox) ==
                         BST_CHECKED)
                            ? "1"
                            : "0";

          allow_large_results_ =
              (IsDlgButtonChecked(hwnd, kIdcAllowLargeResultsCheckbox) ==
               BST_CHECKED)
                  ? "1"
                  : "0";

          use_default_large_results_ =
              (IsDlgButtonChecked(hwnd, kIdcUseDefaultCheckbox) == BST_CHECKED)
                  ? "1"
                  : "0";
          DestroyWindow(hwnd);
          break;
        }
        case kIdcUseDefaultCheckbox: {
          if (HIWORD(w_param) == BN_CLICKED) {
            BOOL is_checked =
                (IsDlgButtonChecked(hwnd, kIdcUseDefaultCheckbox) ==
                 BST_CHECKED);

            HWND h_dataset_name_edit = GetDlgItem(hwnd, kIdcDatasetNameEdit);
            if (is_checked) {
              EnableWindow(h_dataset_name_edit, FALSE);
            } else {
              EnableWindow(h_dataset_name_edit, TRUE);
            }
          }
          break;
        }
        case kIdcAllowLargeResultsCheckbox: {
          if (HIWORD(w_param) == BN_CLICKED) {
            BOOL is_checked =
                IsDlgButtonChecked(hwnd, kIdcAllowLargeResultsCheckbox);
          }
          break;
        }
        case kIdcEnableSessionCheckbox: {
          if (HIWORD(w_param) == BN_CLICKED) {
            BOOL is_checked =
                (IsDlgButtonChecked(hwnd, kIdcEnableSessionCheckbox) ==
                 BST_CHECKED);
            EnableWindow(GetDlgItem(hwnd, kIdcSessionLocationEdit), is_checked);
          }
          break;
        }
        case kIdcEnablePscGcdCheckbox: {
          if (HIWORD(w_param) == BN_CLICKED) {
            BOOL is_checked =
                (IsDlgButtonChecked(hwnd, kIdcEnablePscGcdCheckbox) ==
                 BST_CHECKED);
            SetPscGcdEnabled(hwnd, is_checked);
          }
          break;
        }
        case kIdcEncryptionKeyComboBox: {
          if (HIWORD(w_param) == CBN_SELCHANGE) {
            HWND h_combo = GetDlgItem(hwnd, kIdcEncryptionKeyComboBox);
            HWND h_key_edit = GetDlgItem(hwnd, kIdcEncryptionKeyEdit);
            char buffer[256] = {0};
            int index = SendMessage(h_combo, CB_GETCURSEL, 0, 0);
            if (index != CB_ERR) {
              SendMessage(h_combo, CB_GETLBTEXT, index, (LPARAM)buffer);
              if (strcmp(buffer, "Customer-managed encryption key") == 0) {
                EnableWindow(h_key_edit, TRUE);
              } else {
                if (strcmp(buffer, "(Empty)") == 0) {
                  // Clear the selection so combo shows no text
                  SendMessage(h_combo, CB_SETCURSEL, (WPARAM)-1, 0);
                }
                SetWindowText(h_key_edit, "");
                EnableWindow(h_key_edit, FALSE);
              }
            }
          }
          break;
        }

        case kIdcLoadProjectsButton: {
          if (HIWORD(w_param) != BN_CLICKED) {
            break;
          }
          // The credentials live on the main DSN dialog; this dialog holds no
          // copy of them. This is a top-level owned window rather than a child,
          // and GetParent only reports the owner for WS_POPUP windows, so ask
          // for the owner explicitly.
          HWND h_parent = GetWindow(hwnd, GW_OWNER);
          if (h_parent == NULL) {
            h_parent = GetParent(hwnd);
          }
          if (h_parent == NULL) {
            ShowErrorWindow(hwnd,
                            "Internal error: cannot locate the main dialog to "
                            "read the connection settings from.");
            break;
          }
          char key_file_buffer[1024] = {0};
          char auth_buffer[256] = {0};
          GetWindowText(GetDlgItem(h_parent, kIdcKeyfileEdit), key_file_buffer,
                        sizeof(key_file_buffer));
          GetWindowText(GetDlgItem(h_parent, kIdcAuthBox), auth_buffer,
                        sizeof(auth_buffer));
          if (auth_buffer[0] == '\0') {
            ShowErrorWindow(hwnd,
                            "Select an OAuth mechanism on the main dialog "
                            "before loading projects.");
            break;
          }
          bool const is_adc =
              (strcmp(auth_buffer, "Application Default Credentials") == 0);
          if (!is_adc && key_file_buffer[0] == '\0') {
            ShowErrorWindow(hwnd,
                            "Enter a key file path on the main dialog before "
                            "loading projects.");
            break;
          }

          auto projects_or = DriverForm::GetCatalogAndDataset(
              "Catalog", is_adc ? "" : key_file_buffer, auth_buffer, "");
          if (!projects_or.Ok()) {
            LOG(ERROR) << "AdvanceOptions::AdvanceOptProc::GetCatalogAndDataset"
                          ":: "
                       << projects_or.GetStatusRecord().message;
            MessageBox(hwnd, projects_or.GetStatusRecord().message.c_str(),
                       "Error", MB_OK | MB_ICONERROR);
            break;
          }

          std::vector<std::string> project_ids;
          for (auto& project_id : Split(projects_or.GetValue(), ";")) {
            Trim(project_id);
            if (!project_id.empty()) {
              project_ids.push_back(std::move(project_id));
            }
          }
          PopulateAllowedProjectsListView(
              GetDlgItem(hwnd, kIdcAllowedProjectsListView), project_ids);
          break;
        }

        case kIdcCancelButton:
          DestroyWindow(hwnd);  // Close the window
          break;
      }
      break;
    }
    case WM_VSCROLL: {
      RECT client = {};
      GetClientRect(hwnd, &client);
      int const page = client.bottom - client.top;
      int pos = scroll_pos_;
      switch (LOWORD(w_param)) {
        case SB_TOP:
          pos = 0;
          break;
        case SB_BOTTOM:
          pos = kContentHeight;
          break;
        case SB_LINEUP:
          pos -= kScrollLine;
          break;
        case SB_LINEDOWN:
          pos += kScrollLine;
          break;
        case SB_PAGEUP:
          pos -= page;
          break;
        case SB_PAGEDOWN:
          pos += page;
          break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
          SCROLLINFO si = {};
          si.cbSize = sizeof(si);
          si.fMask = SIF_TRACKPOS;
          if (GetScrollInfo(hwnd, SB_VERT, &si)) {
            pos = si.nTrackPos;
          }
          break;
        }
        default:
          break;
      }
      ScrollTo(hwnd, pos);
      return 0;
    }
    case WM_MOUSEWHEEL: {
      // Accumulate: precision trackpads and high-resolution wheels send deltas
      // smaller than WHEEL_DELTA, which would round to zero on their own.
      wheel_remainder_ += GET_WHEEL_DELTA_WPARAM(w_param);
      int const notches = wheel_remainder_ / WHEEL_DELTA;
      if (notches == 0) {
        return 0;
      }
      wheel_remainder_ -= notches * WHEEL_DELTA;

      // Honour the system "roll the mouse wheel to scroll" setting.
      UINT lines_per_notch = 3;
      if (!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines_per_notch,
                                0)) {
        lines_per_notch = 3;
      }
      RECT client = {};
      GetClientRect(hwnd, &client);
      int step = 0;
      if (lines_per_notch == WHEEL_PAGESCROLL) {
        step = client.bottom - client.top;
      } else {
        step = static_cast<int>(lines_per_notch) * kScrollLine;
      }
      ScrollTo(hwnd, scroll_pos_ - notches * step);
      return 0;
    }
    case WM_SIZE:
      UpdateScrollInfo(hwnd);
      break;
    case WM_KEYDOWN:  // Capture global key presses
      if (w_param == VK_ESCAPE) {
        if (p_current_window) {
          p_current_window->adv_hwnd = NULL;
        }
        DestroyWindow(hwnd);  // Close dialog when ESC is pressed
        return 0;
      } else if (w_param == VK_RETURN) {
        // Simulate a button click on the OK button when Enter is pressed
        SendMessage(GetDlgItem(hwnd, kIdcOKButton), BM_CLICK, 0, 0);
        return 0;
      }
      break;
    case WM_CLOSE:
      DestroyWindow(hwnd);  // Close the window
      return 0;
    case WM_DESTROY:
      if (p_current_window) {
        p_current_window->adv_hwnd = NULL;
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, u_msg, w_param, l_param);
}

void AdvanceOptions::SetValues(Section const& attribute_map) {
  language_dialect_ = GetValueOrDefault(attribute_map, kLanguageDialect);
  std::string lang_dialect_value =
      GetValueOrDefault(attribute_map, kLanguageDialect);
  if (lang_dialect_value ==
      std::to_string(static_cast<int>(LanguageDialect::kStandardSQL))) {
    language_dialect_ = "GoogleSQL";
  } else if (lang_dialect_value ==
             std::to_string(static_cast<int>(LanguageDialect::kLegacySQL))) {
    language_dialect_ = "LegacySQL";
  } else {
    language_dialect_ = "";
  }
  adv_dataset_name_ = GetValueOrDefault(attribute_map, kLargeResultsDatasetId,
                                        kDefaultLargeResultsDatasetId);
  encryption_key_ = GetValueOrDefault(attribute_map, kEncryptionKey);
  rows_per_block_ = GetValueOrDefault(attribute_map, kRowsFetchedPerBlock,
                                      kDefaultRowsPerBlock);
  default_string_length_ = GetValueOrDefault(
      attribute_map, kDefaultStringColumnLength, kDefaultStringLength);
  temp_expiration_ =
      GetValueOrDefault(attribute_map, kLargeResultsTempTableExpirationTime,
                        kDefaultLargeResultsTableExpiration);
  max_threads_ = GetValueOrDefault(attribute_map, kMaxThreads,
                                   std::to_string(kDefaultMaxThreads));
  max_retries_ = GetValueOrDefault(attribute_map, kMaxRetries,
                                   std::to_string(kDefaultMaxRetries));
  session_location_ = GetValueOrDefault(attribute_map, kSessionLocation);
  additional_projects_ = GetValueOrDefault(attribute_map, kAdditionalProjects);
  allowed_projects_ = GetValueOrDefault(attribute_map, kAllowedProjects);
  query_properties_ = GetValueOrDefault(attribute_map, kQueryProperties);
  // TODO(b/497725655): Enable UI feature after public release
  // use_wchar_ = GetValueOrDefault(attribute_map, kUseWChar);
  enable_session_ = GetValueOrDefault(attribute_map, kEnableSession);
  allow_htapi_for_large_results_checkbox_ =
      GetValueOrDefault(attribute_map, kAllowHtapiForLargeResults);
  allow_large_results_ = GetValueOrDefault(attribute_map, kAllowLargeResults);
  use_default_large_results_ =
      GetValueOrDefault(attribute_map, kUseDefaultLargeResultsDataset);
  encryption_type_ = GetValueOrDefault(attribute_map, kEncryptionType);
  private_service_connect_uris_ =
      GetValueOrDefault(attribute_map, kPrivateServiceConnectUris);
  enable_gcd_ = GetValueOrDefault(attribute_map, kEnableGcd);
  universe_domain_ = GetValueOrDefault(attribute_map, kUniverseDomain);
}

void AdvanceOptions::ResetToDefaults() {
  language_dialect_ = kDefaultLanguageDialect;
  adv_dataset_name_ = kDefaultLargeResultsDatasetId;
  temp_expiration_ = kDefaultLargeResultsTableExpiration;
  encryption_key_.clear();
  rows_per_block_ = kDefaultRowsPerBlock;
  default_string_length_ = kDefaultStringLength;
  max_threads_ = std::to_string(kDefaultMaxThreads);
  max_retries_ = std::to_string(kDefaultMaxRetries);
  session_location_.clear();
  additional_projects_.clear();
  allowed_projects_.clear();
  query_properties_.clear();
  // use_wchar_.clear();
  enable_session_.clear();
  allow_htapi_for_large_results_checkbox_.clear();
  allow_large_results_.clear();
  use_default_large_results_.clear();
  encryption_type_ = kDefaultEncryptionType;
  private_service_connect_uris_.clear();
  enable_gcd_.clear();
  universe_domain_.clear();
}

void AdvanceOptions::Show(HWND hwnd) {
  if (adv_hwnd) {
    ShowWindow(adv_hwnd, SW_SHOW);
    SetForegroundWindow(adv_hwnd);
    return;
  }
  WNDCLASS wc_adv = {};
  wc_adv.lpfnWndProc = AdvanceOptions::AdvanceOptProc;
  wc_adv.hInstance = g_hDllInstance;
  wc_adv.lpszClassName = CLASS_NAME;
  wc_adv.hbrBackground =
      (HBRUSH)(COLOR_WINDOW + 1);  // Sets background to white
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  // ICC_LISTVIEW_CLASSES registers WC_LISTVIEW, which the allowed-projects pick
  // list needs; the other controls on this dialog are standard classes.
  icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
  InitCommonControlsEx(&icc);

  RegisterClass(&wc_adv);

  scroll_pos_ = 0;
  wheel_remainder_ = 0;

  // The vertical scrollbar is carved out of the client area, so widen the frame
  // by its width; otherwise every right-aligned control loses that many pixels
  // and the edit boxes are clipped.
  int window_width = 525 + GetSystemMetrics(SM_CXVSCROLL);
  int window_height = 860;

  // Never open taller than the desktop work area: on a short or DPI-scaled
  // display the full control layout does not fit, and a window whose OK button
  // sits below the screen edge cannot be dismissed. Whatever does not fit is
  // reachable through the vertical scrollbar instead.
  RECT work_area = {};
  int work_left = 0;
  int work_top = 0;
  int work_width = GetSystemMetrics(SM_CXSCREEN);
  int work_height = GetSystemMetrics(SM_CYSCREEN);
  if (SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0)) {
    work_left = work_area.left;
    work_top = work_area.top;
    work_width = work_area.right - work_area.left;
    work_height = work_area.bottom - work_area.top;
  }
  if (window_height > work_height) {
    window_height = work_height;
  }
  int x_pos = work_left + (work_width - window_width) / 2;
  int y_pos = work_top + (work_height - window_height) / 2;

  adv_hwnd = CreateWindowEx(
      WS_EX_TOPMOST, CLASS_NAME, "Advanced Options",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_DLGFRAME | WS_VSCROLL, x_pos,
      y_pos, window_width, window_height, hwnd, NULL, g_hDllInstance, this);
  if (adv_hwnd) {
    HFONT h_font =
        CreateFont(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                   DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Inter");

    CreateLanguageControls(h_font);
    CreateLargeResultsControls(h_font);
    CreateHighThroughputControls(h_font);
    CreatePscGcdControls(h_font);
    CreateEncryptionControls(h_font);
    CreateSessionControls(h_font);
    CreateAdditionalControls(h_font);
    CreateButtons(h_font);

    UpdateScrollInfo(adv_hwnd);

    ShowWindow(adv_hwnd, SW_SHOW);
    UpdateWindow(adv_hwnd);

    // Message loop for tab and esc handling
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
      if (msg.message == WM_KEYDOWN) {
        if (SendMessage(adv_hwnd, msg.message, msg.wParam, msg.lParam) != 0) {
          continue;
        }
      }

      if (!IsDialogMessage(adv_hwnd, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      if (!IsWindow(adv_hwnd)) {
        break;
      }
    }
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
