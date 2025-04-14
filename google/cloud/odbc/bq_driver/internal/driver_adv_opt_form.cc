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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include <shellapi.h>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bq_driver_internal::LanguageDialect;

char const AdvanceOptions::CLASS_NAME[] = "AdvanceOptClass";

std::string AdvanceOptions::activation_threshold_;

// Specifies the default SQL dialect used for queries.
// The default is "Standard SQL", but it may be overridden by the user.
std::string AdvanceOptions::language_dialect_ = "GoogleSQL";
std::string AdvanceOptions::adv_dataset_name_;

// Default expiration time for temporary objects in milliseconds.
// This value (3600000 ms) corresponds to 1 hour.
// It is used as a default configuration for temporary resource cleanup
// in the existing driver, ensuring unused resources do not persist
// indefinitely.
std::string AdvanceOptions::temp_expiration_ = "3600000";
std::string AdvanceOptions::encryption_key_;

// Defines the number of rows per data block when fetching results.
// Default is 100,000 rows per block as per existing driver.
std::string AdvanceOptions::rows_per_block_ = "100000";

// Defines the default length of string columns in characters.
// The default is 16,384 characters as per existing driver.
std::string AdvanceOptions::default_string_length_ = "16384";
std::string AdvanceOptions::session_location_;
std::string AdvanceOptions::additional_projects_;
std::string AdvanceOptions::query_properties_;
std::string AdvanceOptions::use_wchar_;
std::string AdvanceOptions::enable_session_;
std::string AdvanceOptions::activation_threshold_checkbox_;
std::string AdvanceOptions::allow_large_results_;
std::string AdvanceOptions::use_default_large_results_;
std::string AdvanceOptions::encryption_type_ = "Google-managed encryption key";

std::string const kLanguageDialect = "SQLDialect";
std::string const kLargeResultsDatasetId = "LargeResultsDatasetId";
std::string const kEncryptionKey = "KMSKeyName";
std::string const kRowsFetchedPerBlock = "RowsFetchedPerBlock";
std::string const kDefaultStringColumnLength = "DefaultStringColumnLength";
std::string const kLargeResultsTempTableExpirationTime =
    "LargeResultsTempTableExpirationTime";
std::string const kSessionLocation = "SessionLocation";
std::string const kAdditionalProjects = "AdditionalProjects";
std::string const kQueryProperties = "QueryProperties";
std::string const kActivationThreshold = "HTAPI_ActivationThreshold";
std::string const kUseWChar = "UseWVarChar";
std::string const kEnableSession = "EnableSession";
std::string const kHTAPIActivationThresholdCheck = "AllowHtapiForLargeResults";
std::string const kAllowLargeResults = "AllowLargeResults";
std::string const kUseDefaultLargeResultsDataset =
    "UseDefaultLargeResultsDataset";
std::string const kEncryptionType = "EncryptionType";

// Control dimensions and positions
int const kHeight = 20;
int const kWidth = 50;
int const kButtonHeight = 17;
int const kButtonWidth = 68;
int const kXAxis = 10;
int const kOkButtonX = 290;
int const kCancelButtonX = 370;
int const kButtonY = 543;
int const kYAxis = 20;
int const kEditBoxWidth = 203;
int const kEditBoxHeight = 17;
int const kinputComboBoxXAxis = 237;
int const KComboBoxHeight = 100;

HWND AdvanceOptions::GetHwnd() const { return adv_hwnd; }
AdvanceOptions::AdvanceOptions() : adv_hwnd(NULL) {}
AdvanceOptions::~AdvanceOptions() {
  if (adv_hwnd) {
    DestroyWindow(adv_hwnd);
  }
  UnregisterClass(CLASS_NAME, GetModuleHandle(NULL));
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
}

void AdvanceOptions::CreateLargeResultsControls(HFONT h_font) {
  HWND h_large_results_header =
      CreateGroupBox(adv_hwnd, "Large results options", kXAxis, kYAxis + 25,
                     kWidth + 385, kHeight + 153, KIdcLargeResultHeader);
  SendMessage(h_large_results_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_allow_large_results_checkbox = CreateCheckBox(
      adv_hwnd, "Allow large result sets", kXAxis + 5, kYAxis + 50, kWidth * 6,
      kHeight, kIdcAllowLargeResultsCheckbox);
  SendMessage(h_allow_large_results_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  EnableWindow(h_allow_large_results_checkbox, FALSE);
  CheckDlgButton(adv_hwnd, kIdcAllowLargeResultsCheckbox,
                 (allow_large_results_ == "1") ? BST_CHECKED : BST_UNCHECKED);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcAllowLargeResultsCheckbox),
                    CheckboxSubclassProc, 0, 0);
  HWND h_language_box = GetDlgItem(adv_hwnd, kIdcLanguageDialectComboBox);
  char language_buffer[256] = {0};
  GetWindowText(h_language_box, language_buffer, sizeof(language_buffer));
  if (strcmp(language_buffer, "LegacySQL") == 0) {
    EnableWindow(GetDlgItem(adv_hwnd, kIdcAllowLargeResultsCheckbox), TRUE);
  } else {
    EnableWindow(GetDlgItem(adv_hwnd, kIdcAllowLargeResultsCheckbox), FALSE);
  }
  if (allow_large_results_ == "1") {
    CheckDlgButton(adv_hwnd, kIdcAllowLargeResultsCheckbox, BST_CHECKED);
  } else {
    CheckDlgButton(adv_hwnd, kIdcAllowLargeResultsCheckbox, BST_UNCHECKED);
  }
  HWND h_use_default_checkbox = CreateCheckBox(
      adv_hwnd, "Use default \"_bqodbc_temp_tables\" large results dataset",
      kXAxis + 5, kYAxis + 75, kWidth * 6 + 15, kHeight,
      kIdcUseDefaultCheckbox);
  SendMessage(h_use_default_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcUseDefaultCheckbox),
                    CheckboxSubclassProc, 0, 0);
  CheckDlgButton(
      adv_hwnd, kIdcUseDefaultCheckbox,
      (use_default_large_results_ == "1") ? BST_CHECKED : BST_UNCHECKED);

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
    EnableWindow(GetDlgItem(adv_hwnd, kIdcDatasetNameEdit), FALSE);
  } else {
    EnableWindow(GetDlgItem(adv_hwnd, kIdcDatasetNameEdit), TRUE);
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
      adv_hwnd,
      "Allow BigQuery Storage API for large results queries:", kXAxis + 5,
      kYAxis + 150, kWidth * 7 + 20, kHeight, kIdcAllowHighThroughputCheckbox);
  SendMessage(h_allow_high_throughput_checkbox, WM_SETFONT, (WPARAM)h_font,
              TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcAllowHighThroughputCheckbox),
                    CheckboxSubclassProc, 0, 0);
  CheckDlgButton(
      adv_hwnd, kIdcAllowHighThroughputCheckbox,
      (activation_threshold_checkbox_ == "1") ? BST_CHECKED : BST_UNCHECKED);

  HWND h_high_encryption_header =
      CreateLabel(adv_hwnd, "Encryption", kXAxis, kYAxis + 206, kWidth * 5,
                  kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_high_encryption_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_encryption_combo_box =
      CreateComboBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 206, kEditBoxWidth,
                     KComboBoxHeight, kIdcEncryptionKeyComboBox);
  SendMessage(h_encryption_combo_box, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcEncryptionKeyComboBox),
                    ComboBoxSubclassProc, 0, 0);
  SendMessage(h_encryption_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Google-managed encryption key");
  SendMessage(h_encryption_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Customer-managed encryption key");
  SendMessage(h_encryption_combo_box, CB_SETCURSEL, 0, 0);
  SetWindowText(h_encryption_combo_box, encryption_type_.c_str());

  HWND h_activation_threshold_label = CreateLabel(
      adv_hwnd, "Activation threshold for BigQuery Storage API:", kXAxis + 5,
      kYAxis + 175, kWidth * 4.5, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_activation_threshold_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_activation_threshold_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 175, kEditBoxWidth,
                    kEditBoxHeight, kIdcActivationThresholdEdit);
  SendMessage(h_activation_threshold_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowLong(
      h_activation_threshold_edit, GWL_STYLE,
      GetWindowLong(h_activation_threshold_edit, GWL_STYLE) | ES_NUMBER);
  SetWindowText(h_activation_threshold_edit, activation_threshold_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcActivationThresholdEdit),
                    InputSubclassProc, 0, 0);
  // Disable and clear if 'Allow large result sets' is unchecked
 if (allow_large_results_ != "1") {
  HWND h_use_default_checkbox = GetDlgItem(adv_hwnd, kIdcUseDefaultCheckbox);
  HWND h_high_throughput_checkbox =
      GetDlgItem(adv_hwnd, kIdcAllowHighThroughputCheckbox);
  HWND h_dataset_name_edit = GetDlgItem(adv_hwnd, kIdcDatasetNameEdit);
  HWND h_temp_expiration_edit = GetDlgItem(adv_hwnd, kIdcTempExpirationEdit);
  HWND h_activation_threshold_edit =
      GetDlgItem(adv_hwnd, kIdcActivationThresholdEdit);

  EnableWindow(h_use_default_checkbox, FALSE);
  EnableWindow(h_high_throughput_checkbox, FALSE);
  EnableWindow(h_dataset_name_edit, FALSE);
  EnableWindow(h_temp_expiration_edit, FALSE);
  EnableWindow(h_activation_threshold_edit, FALSE);

  CheckDlgButton(adv_hwnd, kIdcUseDefaultCheckbox, BST_UNCHECKED);
  CheckDlgButton(adv_hwnd, kIdcAllowHighThroughputCheckbox, BST_UNCHECKED);

  SetWindowText(h_dataset_name_edit, TEXT(""));
  SetWindowText(h_temp_expiration_edit, TEXT(""));  
  SetWindowText(h_activation_threshold_edit, TEXT(""));
}
}

void AdvanceOptions::CreateEncryptionControls(HFONT h_font) {
  HWND h_encryption_key_header =
      CreateLabel(adv_hwnd, "Customer-managed encryption key:", kXAxis,
                  kYAxis + 235, kWidth * 4.3, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_encryption_key_header, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_encryption_key_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 235, kEditBoxWidth,
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
      CreateLabel(adv_hwnd, "Rows per block:", kXAxis, kYAxis + 260, kWidth * 3,
                  kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_rows_per_block_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_rows_per_block_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 260, kEditBoxWidth,
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
                  kYAxis + 285, kWidth * 4.3, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_default_string_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_default_string_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 285, kEditBoxWidth,
                    kEditBoxHeight, kIdcDefaultStringEdit);
  SendMessage(h_default_string_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcDefaultStringEdit),
                    InputSubclassProc, 0, 0);
  SetWindowText(h_default_string_edit, default_string_length_.c_str());
  SetWindowLongPtr(h_default_string_edit, GWL_STYLE,
                   GetWindowLongPtr(h_default_string_edit, GWL_STYLE) |
                       ES_RIGHT | ES_NUMBER);

  HWND h_enable_session_checkbox =
      CreateCheckBox(adv_hwnd, "Enable session", kXAxis, kYAxis + 310,
                     kWidth * 2 + 30, kHeight, kIdcEnableSessionCheckbox);
  SendMessage(h_enable_session_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcEnableSessionCheckbox),
                    CheckboxSubclassProc, 0, 0);
  CheckDlgButton(adv_hwnd, kIdcEnableSessionCheckbox,
                 (enable_session_ == "1") ? BST_CHECKED : BST_UNCHECKED);

  HWND h_session_location_label =
      CreateLabel(adv_hwnd, "Session location:", kXAxis, kYAxis + 335,
                  kWidth * 2 + 30, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_session_location_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_session_location_edit =
      CreateEditBox(adv_hwnd, kinputComboBoxXAxis, kYAxis + 335, kEditBoxWidth,
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
  HWND h_variables_checkbox = CreateCheckBox(
      adv_hwnd, "Use SQL_WVARCHAR instead of SQL_VARCHAR", kXAxis, kYAxis + 360,
      kWidth * 7, kHeight, kIdcVariableCheckbox);
  CheckDlgButton(adv_hwnd, kIdcVariableCheckbox,
                 (use_wchar_ == "1") ? BST_CHECKED : BST_UNCHECKED);
  SendMessage(h_variables_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcVariableCheckbox),
                    CheckboxSubclassProc, 0, 0);
  HWND h_additional_projects_label =
      CreateLabel(adv_hwnd, "Additional projects:", kXAxis, kYAxis + 385,
                  kWidth * 5, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_additional_projects_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_additional_projects_edit =
      CreateScrollableEditBox(adv_hwnd, kXAxis, kYAxis + 405, kWidth + 380,
                              kHeight + 32, kIdcAdditionalProjectsEdit);
  SendMessage(h_additional_projects_edit, WM_SETFONT, (WPARAM)h_font, TRUE);

  SetWindowText(h_additional_projects_edit, additional_projects_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcAdditionalProjectsEdit),
                    InputSubclassProc, 0, 0);

  HWND h_query_properties_label =
      CreateLabel(adv_hwnd, "Query properties:", kXAxis, kYAxis + 465,
                  kWidth * 5, kHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_query_properties_label, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_query_properties_edit =
      CreateScrollableEditBox(adv_hwnd, kXAxis, kYAxis + 485, kWidth + 380,
                              kHeight + 13, kIdcQueryPropertiesEdit);
  SendMessage(h_query_properties_edit, WM_SETFONT, (WPARAM)h_font, TRUE);

  SetWindowText(h_query_properties_edit, query_properties_.c_str());
  SetWindowSubclass(GetDlgItem(adv_hwnd, kIdcQueryPropertiesEdit),
                    InputSubclassProc, 0, 0);
  HWND h_doc_text = CreateLabel(adv_hwnd, "Not sure what to enter? See", kXAxis,
                                kButtonY + 10, kWidth + 110, kHeight, 0);
  SendMessage(h_doc_text, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_hyperlink = CreateHyperlinkLabel(adv_hwnd, "BigQuery documentation",
                                          144, kButtonY + 10, kWidth + 100,
                                          kHeight, kIdcHyperlink2);
  SendMessage(h_hyperlink, WM_SETFONT, (WPARAM)h_font, TRUE);
}

void AdvanceOptions::CreateButtons(HFONT h_font) {
  HWND h_ok_button = CreateButton(adv_hwnd, "OK", kOkButtonX + 2, kButtonY + 10,
                                  kButtonWidth, kButtonHeight, kIdcOKButton);
  SendMessage(h_ok_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_cancel_button =
      CreateButton(adv_hwnd, "Cancel", kCancelButtonX, kButtonY + 10,
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
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
      HWND h_hyperlink = GetDlgItem(hwnd, kIdcHyperlink2);
      RECT rect;
      GetClientRect(h_hyperlink, &rect);
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
          if (!IsDlgButtonChecked(hwnd, kIdcUseDefaultCheckbox)) {
            adv_dataset_name_ = dataset_name_buffer;
          } else {
            adv_dataset_name_ = "";
          }

          HWND h_temp_expiration_edit =
              GetDlgItem(hwnd, kIdcTempExpirationEdit);
          char temp_expiration_buffer[256] = {0};
          GetWindowText(h_temp_expiration_edit, temp_expiration_buffer,
                        sizeof(temp_expiration_buffer));
          temp_expiration_ = temp_expiration_buffer;

          HWND h_encryption_key_edit = GetDlgItem(hwnd, kIdcEncryptionKeyEdit);
          char encryption_key_buffer[256] = {0};
          GetWindowText(h_encryption_key_edit, encryption_key_buffer,
                        sizeof(encryption_key_buffer));
          encryption_key_ = encryption_key_buffer;

          HWND h_rows_per_block_edit = GetDlgItem(hwnd, kIdcRowsPerBlockEdit);
          char rows_per_block_buffer[256] = {0};
          GetWindowText(h_rows_per_block_edit, rows_per_block_buffer,
                        sizeof(rows_per_block_buffer));
          rows_per_block_ = rows_per_block_buffer;

          HWND h_default_string_edit = GetDlgItem(hwnd, kIdcDefaultStringEdit);
          char default_string_buffer[256] = {0};
          GetWindowText(h_default_string_edit, default_string_buffer,
                        sizeof(default_string_buffer));
          default_string_length_ = default_string_buffer;

          HWND h_encryption_combo_box =
              GetDlgItem(hwnd, kIdcEncryptionKeyComboBox);
          char encryption_type_buffer[256] = {0};
          GetWindowText(h_encryption_combo_box, encryption_type_buffer,
                        sizeof(encryption_type_buffer));
          encryption_type_ = encryption_type_buffer;

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

          HWND h_additional_projects_edit =
              GetDlgItem(hwnd, kIdcAdditionalProjectsEdit);
          char additional_projects_buffer[1024] = {0};
          GetWindowText(h_additional_projects_edit, additional_projects_buffer,
                        sizeof(additional_projects_buffer));
          additional_projects_ = additional_projects_buffer;

          HWND h_query_properties_edit =
              GetDlgItem(hwnd, kIdcQueryPropertiesEdit);
          char query_properties_buffer[1024] = {0};
          GetWindowText(h_query_properties_edit, query_properties_buffer,
                        sizeof(query_properties_buffer));
          query_properties_ = query_properties_buffer;

          HWND h_activation_threshold =
              GetDlgItem(hwnd, kIdcActivationThresholdEdit);
          char activation_threshold_buffer[1024] = {0};
          GetWindowText(h_activation_threshold, activation_threshold_buffer,
                        sizeof(activation_threshold_buffer));
          activation_threshold_ = activation_threshold_buffer;

          use_wchar_ =
              (IsDlgButtonChecked(hwnd, kIdcVariableCheckbox) == BST_CHECKED)
                  ? "1"
                  : "0";

          enable_session_ =
              (IsDlgButtonChecked(hwnd, kIdcEnableSessionCheckbox) ==
               BST_CHECKED)
                  ? "1"
                  : "0";

          activation_threshold_checkbox_ =
              (IsDlgButtonChecked(hwnd, kIdcAllowHighThroughputCheckbox) ==
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
            EnableWindow(GetDlgItem(hwnd, kIdcDatasetNameEdit), !is_checked);
          }
          break;
        }
        case kIdcAllowLargeResultsCheckbox: {
          if (HIWORD(w_param) == BN_CLICKED) {
            BOOL is_checked =
                IsDlgButtonChecked(hwnd, kIdcAllowLargeResultsCheckbox);
        
            HWND h_use_default_checkbox =
                GetDlgItem(hwnd, kIdcUseDefaultCheckbox);
            HWND h_high_throughput_checkbox =
                GetDlgItem(hwnd, kIdcAllowHighThroughputCheckbox);
            HWND h_dataset_name_edit = GetDlgItem(hwnd, kIdcDatasetNameEdit);
            HWND h_temp_expiration_edit =
                GetDlgItem(hwnd, kIdcTempExpirationEdit);
            HWND h_activation_threshold_edit =
                GetDlgItem(hwnd, kIdcActivationThresholdEdit);
        
            // Enable or disable controls
            EnableWindow(h_use_default_checkbox, is_checked);
            EnableWindow(h_high_throughput_checkbox, is_checked);
            EnableWindow(h_dataset_name_edit,
                         is_checked &&
                         !IsDlgButtonChecked(hwnd, kIdcUseDefaultCheckbox));
            EnableWindow(h_temp_expiration_edit, is_checked);
            EnableWindow(h_activation_threshold_edit, is_checked);
        
            if (is_checked) {
              // Set default value when checkbox is checked
              SetWindowText(h_temp_expiration_edit, TEXT("3600000"));
            } else {
              // Uncheck dependent checkboxes
              CheckDlgButton(hwnd, kIdcUseDefaultCheckbox, BST_UNCHECKED);
              CheckDlgButton(hwnd, kIdcAllowHighThroughputCheckbox,
                             BST_UNCHECKED);
        
              // Clear dependent textboxes
              SetWindowText(h_dataset_name_edit, TEXT(""));
              SetWindowText(h_temp_expiration_edit, TEXT(""));
              SetWindowText(h_activation_threshold_edit, TEXT(""));
            }
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
        case kIdcLanguageDialectComboBox: {
          if (HIWORD(w_param) == CBN_SELCHANGE) {
            HWND h_language_box = GetDlgItem(hwnd, kIdcLanguageDialectComboBox);
            char language_buffer[256] = {0};
            int index = SendMessage(h_language_box, CB_GETCURSEL, 0, 0);

            if (index != CB_ERR) {
              SendMessage(h_language_box, CB_GETLBTEXT, index,
                          (LPARAM)language_buffer);
              HWND h_checkbox = GetDlgItem(hwnd, kIdcAllowLargeResultsCheckbox);
              if (strcmp(language_buffer, "LegacySQL") == 0) {
                EnableWindow(h_checkbox, TRUE);
              } else {
                EnableWindow(h_checkbox, FALSE);
                CheckDlgButton(hwnd, kIdcAllowLargeResultsCheckbox,
                               BST_UNCHECKED);
              }
            }
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
                SetWindowText(h_key_edit, "");
                EnableWindow(h_key_edit, FALSE);
              }
            }
          }
          break;
        }

        case kIdcCancelButton:
          DestroyWindow(hwnd);  // Close the window
          break;
      }
      break;
    }
    case WM_KEYDOWN:  // Capture global key presses
      if (w_param == VK_ESCAPE) {
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
  adv_dataset_name_ = GetValueOrDefault(attribute_map, kLargeResultsDatasetId);
  encryption_key_ = GetValueOrDefault(attribute_map, kEncryptionKey);
  rows_per_block_ = GetValueOrDefault(attribute_map, kRowsFetchedPerBlock);
  default_string_length_ =
      GetValueOrDefault(attribute_map, kDefaultStringColumnLength);
  temp_expiration_ =
      GetValueOrDefault(attribute_map, kLargeResultsTempTableExpirationTime);
  session_location_ = GetValueOrDefault(attribute_map, kSessionLocation);
  additional_projects_ = GetValueOrDefault(attribute_map, kAdditionalProjects);
  query_properties_ = GetValueOrDefault(attribute_map, kQueryProperties);
  activation_threshold_ =
      GetValueOrDefault(attribute_map, kActivationThreshold);
  use_wchar_ = GetValueOrDefault(attribute_map, kUseWChar);
  enable_session_ = GetValueOrDefault(attribute_map, kSessionLocation);
  activation_threshold_checkbox_ =
      GetValueOrDefault(attribute_map, kHTAPIActivationThresholdCheck);
  allow_large_results_ = GetValueOrDefault(attribute_map, kAllowLargeResults);
  use_default_large_results_ =
      GetValueOrDefault(attribute_map, kUseDefaultLargeResultsDataset);
  encryption_type_ = GetValueOrDefault(attribute_map, kEncryptionType);
}

void AdvanceOptions::Show(HWND hwnd) {
  if (adv_hwnd) {
    ShowWindow(adv_hwnd, SW_SHOW);
    SetForegroundWindow(adv_hwnd);
    return;
  }
  WNDCLASS wc_adv = {};
  wc_adv.lpfnWndProc = AdvanceOptions::AdvanceOptProc;
  wc_adv.hInstance = GetModuleHandle(NULL);
  wc_adv.lpszClassName = CLASS_NAME;
  wc_adv.hbrBackground =
      (HBRUSH)(COLOR_WINDOW + 1);  // Sets background to white
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&icc);

  RegisterClass(&wc_adv);

  int window_width = 462;
  int window_height = 618;
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  adv_hwnd = CreateWindowEx(
      0, CLASS_NAME, "Advanced Options",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_DLGFRAME, x_pos, y_pos,
      window_width, window_height, hwnd, NULL, GetModuleHandle(NULL), this);
  if (adv_hwnd) {
    HFONT h_font =
        CreateFont(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                   DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Inter");

    CreateLanguageControls(h_font);
    CreateLargeResultsControls(h_font);
    CreateHighThroughputControls(h_font);
    CreateEncryptionControls(h_font);
    CreateSessionControls(h_font);
    CreateAdditionalControls(h_font);
    CreateButtons(h_font);
  }
  ShowWindow(adv_hwnd, SW_SHOW);
  UpdateWindow(adv_hwnd);
}
}  // namespace google::cloud::odbc_bq_driver_internal
