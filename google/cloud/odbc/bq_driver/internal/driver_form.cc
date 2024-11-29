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

#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
char const DriverForm::CLASS_NAME[] = "DriverFormClass";
char const AdvanceOptions::CLASS_NAME[] = "AdvanceOptClass";
char const ProxyOptions::CLASS_NAME[] = "ProxyOptClass";
std::string DriverForm::dsn_name_;
std::string DriverForm::email_;
std::string DriverForm::key_file_path_;
std::string DriverForm::o_auth_mechanism_;
std::string DriverForm::catalog_;
std::string DriverForm::dataset_;
std::string DriverForm::encrypt_data_;
std::string DriverForm::min_tls_version_;
std::string DriverForm::trusted_cert_;
std::string DriverForm::description_;
std::string AdvanceOptions::activation_threshold_;
std::string AdvanceOptions::language_dialect_;
std::string AdvanceOptions::adv_dataset_name_;
std::string AdvanceOptions::temp_expiration_;
std::string AdvanceOptions::encryption_key_;
std::string AdvanceOptions::rows_per_block_;
std::string AdvanceOptions::default_string_length_;
std::string AdvanceOptions::session_location_;
std::string AdvanceOptions::additional_projects_;
std::string AdvanceOptions::query_properties_;

int WINAPI wWinMain(HINSTANCE h_instance, HINSTANCE h_prev_instance,
                    PWSTR p_cmd_line, int n_cmd_show) {
  DriverForm DriverForm;
  DriverForm.Show();

  // Run the message loop
  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}

HWND DriverForm::GetHwnd() const { return m_hwnd; }
DriverForm::DriverForm() : m_hwnd(NULL) {}

DriverForm::~DriverForm() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
  }
}

void OpenFileDialog(HWND hwnd, HWND h_edit,
                    char const* mock_file_path = nullptr) {
  if (mock_file_path) {
    // Directly set the test file path to the edit control if provided
    SetWindowText(h_edit, mock_file_path);
    return;
  }
  OPENFILENAME ofn;
  char sz_file[260] = {0};  // Buffer for file path

  // Initialize OPENFILENAME structure
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFile = sz_file;
  ofn.nMaxFile = sizeof(sz_file);
  ofn.lpstrFilter = "JSON Files\0*.JSON\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  // Display the Open File dialog
  if (GetOpenFileName(&ofn)) {
    // Set the selected file path to the edit control
    SetWindowText(h_edit, sz_file);
  }
}

void DriverForm::SetValues(Section const& attributes_map) {
  dsn_name_ = attributes_map.count("DSN") > 0 ? attributes_map.at("DSN") : "";
  email_ = attributes_map.count("Email") > 0 ? attributes_map.at("Email") : "";
  if (attributes_map.count("OAuthMechanism") > 0) {
    if (attributes_map.at("OAuthMechanism") == "0") {
      o_auth_mechanism_ = "Service Authentication";
    } else if (attributes_map.at("OAuthMechanism") == "3") {
      o_auth_mechanism_ = "Application Default Credentials";
    } else
      o_auth_mechanism_ = "";
  } else
    o_auth_mechanism_ = "";
  key_file_path_ = attributes_map.count("KeyFilePath") > 0
                       ? attributes_map.at("KeyFilePath")
                       : "";
  catalog_ =
      attributes_map.count("Catalog") > 0 ? attributes_map.at("Catalog") : "";
  dataset_ =
      attributes_map.count("Dataset") > 0 ? attributes_map.at("Dataset") : "";
  encrypt_data_ = attributes_map.count("EncryptData") > 0
                      ? attributes_map.at("EncryptData")
                      : "";
  description_ = attributes_map.count("Description") > 0
                     ? attributes_map.at("Description")
                     : "";
  min_tls_version_ =
      attributes_map.count("Min_TLS") > 0 ? attributes_map.at("Min_TLS") : "";
  trusted_cert_ = attributes_map.count("TrustedCerts") > 0
                      ? attributes_map.at("TrustedCerts")
                      : "";
}

HFONT CreateCustomFont(int font_size) {
  LOGFONT log_font = {};
  HFONT h_font = NULL;
  log_font.lfHeight = -MulDiv(font_size, GetDeviceCaps(GetDC(NULL), LOGPIXELSY),
                              72);        // Negative height for screen fonts
  lstrcpy(log_font.lfFaceName, "Arial");  // Font face name

  h_font = CreateFontIndirect(&log_font);
  return h_font;
}
// Helper function to create a static label
HWND CreateLabel(HWND parent, char const* text, int x, int y, int width,
                 int height, int id) {
  return CreateWindowEx(0, "STATIC", text, WS_VISIBLE | WS_CHILD | SS_LEFT, x,
                        y, width, height, parent, (HMENU)id,
                        GetModuleHandle(NULL), NULL);
}

// Helper function to create an edit box
HWND CreateEditBox(HWND parent, int x, int y, int width, int height, int id) {
  return CreateWindowEx(
      0, "EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, x, y, width,
      height, parent, (HMENU)id, GetModuleHandle(NULL), NULL);
}

// Helper function to create a combo box (dropdown)
HWND CreateComboBox(HWND parent, int x, int y, int width, int height, int id) {
  return CreateWindowEx(
      0, "COMBOBOX", NULL, WS_TABSTOP | WS_VISIBLE | WS_CHILD | CBS_DROPDOWN, x,
      y, width, height, parent, (HMENU)id, GetModuleHandle(NULL), NULL);
}

// Helper function to create a button
HWND CreateButton(HWND parent, char const* text, int x, int y, int width,
                  int height, int id) {
  return CreateWindowEx(
      0, "BUTTON", text, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, x,
      y, width, height, parent, (HMENU)id, GetModuleHandle(NULL), NULL);
}
HWND CreateCheckBox(HWND parent, char const* text, int x, int y, int width,
                    int height, int id) {
  return CreateWindowEx(0, "BUTTON", text,
                        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, x, y, width,
                        height, parent, (HMENU)id, GetModuleHandle(NULL), NULL);
}
// Helper function to set font for controls
void SetControlFont(HWND hwnd, HFONT font) {
  SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

void DriverForm::InitControls() {
  HFONT h_font =
      CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                 DEFAULT_PITCH | FF_SWISS, "Segoe UI");

  HWND h_dsn_name_header = CreateLabel(m_hwnd, "Data Source Name:", 20, 20, 150,
                                       20, WS_VISIBLE | SS_LEFT);
  HWND h_dsn_name_edit = CreateEditBox(m_hwnd, 180, 20, 220, 20, kIdcDSNEdit);

  SetWindowText(h_dsn_name_edit, dsn_name_.c_str());
  if (!dsn_name_.empty()) {
    SendMessage(h_dsn_name_edit, EM_SETREADONLY, TRUE, 0);
  }

  HWND h_description_header = CreateLabel(m_hwnd, "Description:", 20, 60, 100,
                                          20, WS_VISIBLE | SS_LEFT);
  HWND h_description_edit =
      CreateEditBox(m_hwnd, 180, 60, 220, 20, kIdcDescriptionEdit);

  HWND h_encrypt_data_header =
      CreateLabel(m_hwnd, "Encrypt Sensitive Data:", 20, 100, 180, 20,
                  WS_VISIBLE | SS_LEFT);
  HWND h_encrypt_data_combo_box =
      CreateComboBox(m_hwnd, 180, 100, 220, 20, kIdcEncryptDataComboBox);

  HWND h_auth_header = CreateLabel(m_hwnd, "Authentication", 20, 140, 120, 20,
                                   WS_VISIBLE | SS_LEFT);
  HWND h_auth_combo_box =
      CreateComboBox(m_hwnd, 180, 140, 220, 20, kIdcAuthBox);

  HWND h_email_header =
      CreateLabel(m_hwnd, "Email:", 20, 180, 100, 20, WS_VISIBLE | SS_LEFT);
  HWND h_email_edit = CreateEditBox(m_hwnd, 180, 180, 220, 20, kIdcEmailEdit);

  HWND h_key_file_path_header = CreateLabel(m_hwnd, "Key File Path:", 20, 220,
                                            120, 20, WS_VISIBLE | SS_LEFT);
  HWND h_key_file_edit =
      CreateEditBox(m_hwnd, 180, 220, 220, 20, kIdcKeyfileEdit);

  HWND h_browse_button =
      CreateButton(m_hwnd, "Browse", 420, 220, 80, 20, kIdcBrowseButton);

  HWND h_ssl_header = CreateLabel(m_hwnd, "SSL Options:", 20, 260, 120, 20,
                                  WS_VISIBLE | SS_LEFT);

  HWND h_min_tls_header = CreateLabel(m_hwnd, "Minimum TLS Version:", 20, 300,
                                      180, 20, WS_VISIBLE | SS_LEFT);
  HWND h_min_tls_combo_box =
      CreateComboBox(m_hwnd, 180, 300, 220, 20, kIdcMinTLSComboBox);

  HWND h_trusted_cert_header = CreateLabel(m_hwnd, "Trusted Certificate:", 20,
                                           340, 150, 20, WS_VISIBLE | SS_LEFT);
  HWND h_trusted_cert_edit =
      CreateEditBox(m_hwnd, 180, 340, 220, 20, kIdcTrustedCertEdit);

  HWND h_trusted_cert_browse_button = CreateButton(
      m_hwnd, "Browse", 420, 340, 80, 20, kIdcTrustedCertBrowseButton);

  HWND h_catalog_header = CreateLabel(m_hwnd, "Catalog (Project):", 20, 380,
                                      150, 20, WS_VISIBLE | SS_LEFT);
  HWND h_catalog_box = CreateComboBox(m_hwnd, 180, 380, 220, 20, kIdcCatlogBOX);

  HWND h_dataset_header =
      CreateLabel(m_hwnd, "Dataset:", 20, 420, 150, 20, WS_VISIBLE | SS_LEFT);
  HWND h_dataset_box =
      CreateComboBox(m_hwnd, 180, 420, 220, 20, kIdcDatasetBOX);

  HWND h_proxy_options_button = CreateButton(
      m_hwnd, "Proxy Options...", 20, 460, 150, 30, kIdcProxyOptionsButton);
  HWND h_login_button = CreateButton(m_hwnd, "Logging Options...", 190, 460,
                                     150, 30, kIdcLoggingBtn);
  HWND h_advance_opt_button = CreateButton(m_hwnd, "Advance Options...", 360,
                                           460, 130, 30, kIdcAdvanceOptBtn);
  HWND h_test_button =
      CreateButton(m_hwnd, "Test...", 190, 520, 80, 30, kIdcButtonTest);
  HWND h_ok_button = CreateButton(m_hwnd, "OK", 280, 520, 80, 30, kIdcButtonOk);
  HWND h_cancel_button =
      CreateButton(m_hwnd, "Cancel", 370, 520, 80, 30, kIdcButtonCancel);

  SendMessage(h_encrypt_data_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "For Current User Only");
  SendMessage(h_encrypt_data_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "For All Users");
  SendMessage(h_encrypt_data_combo_box, CB_SETCURSEL, 0, 0);

  SendMessage(h_auth_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Service Authentication");
  SendMessage(h_auth_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Application Credentials");
  SendMessage(h_auth_combo_box, CB_SETCURSEL, 0, 0);

  SendMessage(h_min_tls_combo_box, CB_ADDSTRING, 0, (LPARAM) "1.0");
  SendMessage(h_min_tls_combo_box, CB_ADDSTRING, 0, (LPARAM) "1.1");
  SendMessage(h_min_tls_combo_box, CB_ADDSTRING, 0, (LPARAM) "1.2");
  SendMessage(h_min_tls_combo_box, CB_SETCURSEL, 2, 0);

  SetWindowText(h_email_edit, email_.c_str());
  SetWindowText(h_key_file_edit, key_file_path_.c_str());
  SetWindowText(h_catalog_box, catalog_.c_str());
  SetWindowText(h_dataset_box, dataset_.c_str());
  SetWindowText(h_auth_combo_box, o_auth_mechanism_.c_str());
  SetWindowText(h_description_edit, description_.c_str());
  SetWindowText(h_trusted_cert_edit, trusted_cert_.c_str());
  SetWindowText(h_min_tls_combo_box, min_tls_version_.c_str());
  SetWindowText(h_encrypt_data_combo_box, encrypt_data_.c_str());
}

// Function to initialize and display the form
void DriverForm::Show() {
  if (m_hwnd) {
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    return;
  }

  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = CLASS_NAME;

  RegisterClass(&wc);
  int window_width = 520;
  int window_height = 650;

  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);

  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  m_hwnd = CreateWindowEx(
      0, CLASS_NAME, "Google ODBC Driver for Google Bigquery DSN Setup",
      WS_OVERLAPPEDWINDOW, x_pos, y_pos, window_width, window_height, NULL,
      NULL, GetModuleHandle(NULL), this);

  if (m_hwnd) {
    InitControls();
    RECT rc_client;
    GetClientRect(m_hwnd, &rc_client);

    int button_width = 100;
    int button_height = 30;
    int button_y = rc_client.bottom - button_height - 20;
    int button_spacing = 20;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
  }
}

StatusRecord DriverForm::IsValidEmail(std::string const& email) {
  std::regex const pattern(R"((\w+)(\.|\-)?(\w*)@(\w+)(\.\w+)+)");
  if (std::regex_match(email, pattern)) {
    return StatusRecord();
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "Email does not match the required pattern."};
  }
}
HWND AdvanceOptions::GetHwnd() const { return parent_hwnd; }
AdvanceOptions::AdvanceOptions() : parent_hwnd(NULL) {}
AdvanceOptions::~AdvanceOptions() {
  if (parent_hwnd) {
    DestroyWindow(parent_hwnd);
  }
  UnregisterClass(CLASS_NAME, GetModuleHandle(NULL));
}
void AdvanceOptions::SetValues(Section const& attribute_map) {
  language_dialect_ = attribute_map.count("LanguageDialect") > 0
                          ? attribute_map.at("LanguageDialect")
                          : "";
  adv_dataset_name_ = attribute_map.count("LargeResultsDatasetId") > 0
                          ? attribute_map.at("LargeResultsDatasetId")
                          : "";
  encryption_key_ = attribute_map.count("EncryptionKey") > 0
                        ? attribute_map.at("EncryptionKey")
                        : "";
  rows_per_block_ = attribute_map.count("RowsFetchedPerBlock") > 0
                        ? attribute_map.at("RowsFetchedPerBlock")
                        : "";
  default_string_length_ = attribute_map.count("DefaultStringColumnLength") > 0
                               ? attribute_map.at("DefaultStringColumnLength")
                               : "";
  temp_expiration_ =
      attribute_map.count("LargeResultsTempTableExpirationTime") > 0
          ? attribute_map.at("LargeResultsTempTableExpirationTime")
          : "";
  session_location_ = attribute_map.count("SessionLocation") > 0
                          ? attribute_map.at("SessionLocation")
                          : "";
  additional_projects_ = attribute_map.count("AdditionalProjects") > 0
                             ? attribute_map.at("AdditionalProjects")
                             : "";
  query_properties_ = attribute_map.count("QueryProperties") > 0
                          ? attribute_map.at("QueryProperties")
                          : "";
  activation_threshold_ = attribute_map.count("HTAPI_ActivationThreshold") > 0
                              ? attribute_map.at("HTAPI_ActivationThreshold")
                              : "";
}

void AdvanceOptions::InitControls() {
  HFONT h_font =
      CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                 DEFAULT_PITCH | FF_SWISS, "Segoe UI");

  HWND h_language_header = CreateLabel(parent_hwnd, "Language Dialect", 20, 10,
                                       150, 20, WS_VISIBLE | SS_LEFT);

  HWND h_language_combo_box = CreateComboBox(parent_hwnd, 200, 10, 150, 120,
                                             kIdcLanguageDialectComboBox);
  SendMessage(h_language_combo_box, WM_SETFONT, (WPARAM)h_font, TRUE);
  SendMessage(h_language_combo_box, CB_ADDSTRING, 0, (LPARAM) "Standard SQL");
  SendMessage(h_language_combo_box, CB_ADDSTRING, 0, (LPARAM) "Legacy SQL");
  SendMessage(h_language_combo_box, CB_SETCURSEL, 0, 0);

  HWND h_large_results_header =
      CreateLabel(parent_hwnd, "Large Results Options", 20, 50, 250, 20,
                  WS_VISIBLE | SS_LEFT | SS_NOPREFIX);

  HWND h_allow_large_results_checkbox =
      CreateCheckBox(parent_hwnd, "Allow Large Result Sets", 20, 80, 300, 20,
                     kIdcAllowLargeResultsCheckbox);

  HWND h_use_default_checkbox =
      CreateCheckBox(parent_hwnd, "Use Default \"_bqodbc_temp_tables\" Dataset",
                     40, 110, 320, 20, kIdcUseDefaultCheckbox);

  HWND h_dataset_name_label =
      CreateLabel(parent_hwnd, "Dataset Name for Large Result Sets:", 40, 140,
                  250, 20, WS_VISIBLE | SS_LEFT);

  HWND h_dataset_name_edit =
      CreateEditBox(parent_hwnd, 300, 140, 100, 20, kIdcDatasetNameEdit);

  HWND h_temp_expiration_label =
      CreateLabel(parent_hwnd, "Default temp table expiration time(ms):", 40,
                  170, 270, 20, WS_VISIBLE | SS_LEFT);

  HWND h_temp_expiration_edit =
      CreateEditBox(parent_hwnd, 300, 170, 100, 20, kIdcTempExpirationEdit);
  SetWindowText(h_temp_expiration_edit, "3600000");

  HWND h_allow_high_throughput_checkbox = CreateCheckBox(
      parent_hwnd, "Allow High-Throughput API for Large Results queries:", 20,
      200, 370, 20, kIdcAllowHighThroughputCheckbox);

  HWND h_high_throughput_header =
      CreateLabel(parent_hwnd, "High-Throughput API Options", 20, 230, 250, 20,
                  WS_VISIBLE | SS_LEFT);

  HWND h_activation_threshold_label =
      CreateLabel(parent_hwnd, "Activation Threshold for High-Throughput:", 20,
                  260, 300, 20, WS_VISIBLE | SS_LEFT);

  HWND h_activation_threshold_edit = CreateEditBox(
      parent_hwnd, 300, 260, 100, 20, kIdcActivationThresholdEdit);

  HWND h_encryption_key_header =
      CreateLabel(parent_hwnd, "Customer-Managed Encryption Key:", 20, 290, 300,
                  20, WS_VISIBLE | SS_LEFT);

  HWND h_encryption_key_edit =
      CreateEditBox(parent_hwnd, 20, 320, 380, 20, kIdcEncryptionKeyEdit);

  HWND h_rows_per_block_label = CreateLabel(parent_hwnd, "Rows Per Block:", 20,
                                            350, 150, 20, WS_VISIBLE | SS_LEFT);

  HWND h_rows_per_block_edit =
      CreateEditBox(parent_hwnd, 300, 350, 100, 20, kIdcRowsPerBlockEdit);
  SetWindowText(h_rows_per_block_edit, "100000");

  HWND h_default_string_label =
      CreateLabel(parent_hwnd, "Default String Column Length:", 20, 380, 250,
                  20, WS_VISIBLE | SS_LEFT);

  HWND h_default_string_edit =
      CreateEditBox(parent_hwnd, 300, 380, 100, 20, kIdcDefaultStringEdit);
  SetWindowText(h_default_string_edit, "16384");

  HWND h_enable_session_checkbox =
      CreateCheckBox(parent_hwnd, "Enable Session", 20, 410, 130, 20,
                     kIdcEnableSessionCheckbox);

  HWND h_session_location_label =
      CreateLabel(parent_hwnd, "Session Location:", 180, 410, 130, 20,
                  WS_VISIBLE | SS_LEFT);

  HWND h_session_location_edit =
      CreateEditBox(parent_hwnd, 300, 410, 100, 20, kIdcSessionLocationEdit);

  HWND h_additional_projects_label =
      CreateLabel(parent_hwnd, "Additional Projects:", 20, 440, 250, 20,
                  WS_VISIBLE | SS_LEFT);

  HWND h_additional_projects_edit =
      CreateEditBox(parent_hwnd, 20, 470, 380, 40, kIdcAdditionalProjectsEdit);

  HWND h_query_properties_label = CreateLabel(
      parent_hwnd, "Query Properties:", 20, 520, 250, 20, WS_VISIBLE | SS_LEFT);

  HWND h_query_properties_edit =
      CreateEditBox(parent_hwnd, 20, 550, 380, 20, kIdcQueryPropertiesEdit);

  HWND h_ok_button =
      CreateButton(parent_hwnd, "OK", 220, 580, 80, 30, kIdcOKButton);
  SendMessage(h_ok_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_cancel_button =
      CreateButton(parent_hwnd, "Cancel", 310, 580, 80, 30, kIdcCancelButton);
  SendMessage(h_cancel_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  SetWindowText(h_language_combo_box, language_dialect_.c_str());
  SetWindowText(h_dataset_name_edit, adv_dataset_name_.c_str());
  SetWindowText(h_temp_expiration_edit, temp_expiration_.c_str());
  SetWindowText(h_encryption_key_edit, encryption_key_.c_str());
  SetWindowText(h_rows_per_block_edit, rows_per_block_.c_str());
  SetWindowText(h_default_string_edit, default_string_length_.c_str());
  SetWindowText(h_session_location_edit, session_location_.c_str());
  SetWindowText(h_additional_projects_edit, additional_projects_.c_str());
  SetWindowText(h_query_properties_edit, query_properties_.c_str());
}

LRESULT CALLBACK AdvanceOptions::AdvanceOptProc(HWND hwnd, UINT u_msg,
                                                WPARAM w_param,
                                                LPARAM l_param) {
  AdvanceOptions* p_this = NULL;
  if (u_msg == WM_NCCREATE) {
    CREATESTRUCT* pCreate = (CREATESTRUCT*)l_param;
    p_this = (AdvanceOptions*)pCreate->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)p_this);
  } else {
    p_this = (AdvanceOptions*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }
  switch (u_msg) {
    case WM_COMMAND:
      switch (LOWORD(w_param)) {
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

          HWND h_session_location_edit =
              GetDlgItem(hwnd, kIdcSessionLocationEdit);
          char session_location_buffer[256] = {0};
          GetWindowText(h_session_location_edit, session_location_buffer,
                        sizeof(session_location_buffer));
          session_location_ = session_location_buffer;

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

          DestroyWindow(hwnd);
          break;
        }
        case kIdcCancelButton:
          DestroyWindow(hwnd);  // Close the window
          break;
      }
      break;

    case WM_CLOSE:
      DestroyWindow(hwnd);  // Close the window
      return 0;

    case WM_DESTROY:
      if (p_this) {
        p_this->parent_hwnd = NULL;
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, u_msg, w_param, l_param);
}
void AdvanceOptions::Show(HWND hwnd) {
  if (parent_hwnd) {
    ShowWindow(parent_hwnd, SW_SHOW);
    SetForegroundWindow(parent_hwnd);
    return;
  }

  WNDCLASS wc_logging = {};
  wc_logging.lpfnWndProc = AdvanceOptions::AdvanceOptProc;
  wc_logging.hInstance = GetModuleHandle(NULL);
  wc_logging.lpszClassName = CLASS_NAME;

  RegisterClass(&wc_logging);

  int window_width = 420;
  int window_height = 650;
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  parent_hwnd = CreateWindowEx(
      0, CLASS_NAME, "Advanced Options", WS_OVERLAPPEDWINDOW, x_pos, y_pos,
      window_width, window_height, hwnd, NULL, GetModuleHandle(NULL), this);

  if (parent_hwnd) {
    InitControls();
  }
  ShowWindow(parent_hwnd, SW_SHOW);
  UpdateWindow(parent_hwnd);
}
HWND ProxyOptions::GetHwnd() const { return proxy_hwnd; }
ProxyOptions::ProxyOptions() : proxy_hwnd(NULL) {}
ProxyOptions::~ProxyOptions() {
  if (proxy_hwnd) {
    DestroyWindow(proxy_hwnd);
  }
  UnregisterClass(CLASS_NAME, GetModuleHandle(NULL));
}

void ProxyOptions::InitControls() {
  HFONT h_font =
      CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                 DEFAULT_PITCH | FF_SWISS, "Segoe UI");

  HWND h_proxy_checkbox = CreateCheckBox(proxy_hwnd, "Use Proxy Server", 20, 20,
                                         150, 20, kIdcProxyCheckbox);

  HWND h_proxy_host_label = CreateLabel(proxy_hwnd, "Proxy Host:", 20, 60, 150,
                                        20, kIdcProxyHostLabel);
  HWND h_proxy_host_edit =
      CreateEditBox(proxy_hwnd, 150, 60, 240, 20,
                    kIdcProxyHostName | WS_BORDER | WS_EX_CLIENTEDGE);

  HWND h_proxy_port_label = CreateLabel(proxy_hwnd, "Proxy Port:", 20, 100, 150,
                                        20, kIdcProxyPortLabel);
  HWND h_proxy_port_edit =
      CreateEditBox(proxy_hwnd, 150, 100, 240, 20, kIdcProxyPortEdit);

  HWND h_proxy_username_label = CreateLabel(
      proxy_hwnd, "Proxy Username:", 20, 140, 180, 20, kIdcProxyUsernameLabel);
  HWND h_proxy_username_edit =
      CreateEditBox(proxy_hwnd, 150, 140, 240, 20, kIdcProxyUsernameEdit);

  HWND h_proxy_password_label = CreateLabel(
      proxy_hwnd, "Proxy Password:", 20, 180, 180, 20, kIdcProxyPasswordLabel);
  HWND h_proxy_password_edit = CreateEditBox(
      proxy_hwnd, 150, 180, 240, 20, kIdcProxyPasswordEdit | ES_PASSWORD);

  HWND h_ok_button =
      CreateButton(proxy_hwnd, "OK", 180, 220, 80, 30, kIdcProxyOKButton);

  HWND h_cancel_button = CreateButton(proxy_hwnd, "Cancel", 280, 220, 80, 30,
                                      kIdcProxyCancelButton);
}

void ProxyOptions::Show(HWND hwnd) {
  if (proxy_hwnd) {
    ShowWindow(proxy_hwnd, SW_SHOW);
    SetForegroundWindow(proxy_hwnd);
    return;
  }

  WNDCLASS wc_logging = {};
  wc_logging.lpfnWndProc = ProxyOptions::ProxyOptProc;
  wc_logging.hInstance = GetModuleHandle(NULL);
  wc_logging.lpszClassName = CLASS_NAME;

  RegisterClass(&wc_logging);

  int window_width = 470;
  int window_height = 400;
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  proxy_hwnd =
      CreateWindowEx(0, CLASS_NAME, "Proxy Options", WS_OVERLAPPEDWINDOW,
                     CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height,
                     NULL, NULL, GetModuleHandle(NULL), this);

  if (proxy_hwnd) {
    InitControls();

    ShowWindow(proxy_hwnd, SW_SHOW);
    UpdateWindow(proxy_hwnd);
  }
}

LRESULT CALLBACK ProxyOptions::ProxyOptProc(HWND hwnd, UINT msg, WPARAM w_param,
                                            LPARAM l_param) {
  switch (msg) {
    case WM_COMMAND: {
      int wm_id = LOWORD(w_param);

      if (wm_id == kIdcProxyOKButton) {
        DestroyWindow(hwnd);
      } else if (wm_id == kIdcProxyCancelButton) {
        DestroyWindow(hwnd);
      }
      return 0;
    }

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, msg, w_param, l_param);
}

LRESULT CALLBACK DriverForm::WindowProc(HWND hwnd, UINT u_msg, WPARAM w_param,
                                        LPARAM l_param) {
  DriverForm* p_this =
      reinterpret_cast<DriverForm*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  auto adv_form = new AdvanceOptions;
  auto proxy_form = new ProxyOptions;
  switch (u_msg) {
    case WM_CREATE:
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p_this));
      break;
    case WM_COMMAND:
      switch (LOWORD(w_param)) {
        case kIdcBrowseButton: {
          HWND h_edit = GetDlgItem(hwnd, kIdcKeyfileEdit);
          OpenFileDialog(hwnd, h_edit);
        } break;
        case kIdcTrustedCertBrowseButton: {
          HWND h_edit = GetDlgItem(hwnd, kIdcTrustedCertEdit);
          OpenFileDialog(hwnd, h_edit);
        } break;
        case kIdcButtonOk: {
          HWND h_dsn = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsn_buffer[256];
          GetWindowText(h_dsn, dsn_buffer, sizeof(dsn_buffer));
          dsn_name_ = dsn_buffer;

          HWND h_email = GetDlgItem(hwnd, kIdcEmailEdit);
          char email_buffer[256];
          GetWindowText(h_email, email_buffer, sizeof(email_buffer));
          email_ = email_buffer;
          StatusRecord status = p_this->IsValidEmail(email_);

          if (!status.ok() && !email_.empty()) {
            MessageBox(hwnd, "Invalid email address!", "Error",
                       MB_OK | MB_ICONERROR);
            email_ = "";
            return 0;
          }

          HWND h_key = GetDlgItem(hwnd, kIdcKeyfileEdit);
          char key_buffer[256];
          GetWindowText(h_key, key_buffer, sizeof(key_buffer));
          key_file_path_ = key_buffer;

          HWND h_auth_box = GetDlgItem(hwnd, kIdcAuthBox);
          char auth_buffer[256];
          GetWindowText(h_auth_box, auth_buffer, sizeof(auth_buffer));
          o_auth_mechanism_ = auth_buffer;

          HWND h_catalog_box = GetDlgItem(hwnd, kIdcCatlogBOX);
          char catalog_buffer[256];
          GetWindowText(h_catalog_box, catalog_buffer, sizeof(catalog_buffer));
          catalog_ = catalog_buffer;

          HWND h_dataset_box = GetDlgItem(hwnd, kIdcDatasetBOX);
          char data_buffer[256];
          GetWindowText(h_dataset_box, data_buffer, sizeof(data_buffer));
          dataset_ = data_buffer;

          HWND h_encrypt_combo_box = GetDlgItem(hwnd, kIdcEncryptDataComboBox);
          char encrypt_buffer[256];
          GetWindowText(h_encrypt_combo_box, encrypt_buffer,
                        sizeof(encrypt_buffer));
          encrypt_data_ = encrypt_buffer;

          HWND h_min_tls_box = GetDlgItem(hwnd, kIdcMinTLSComboBox);
          char min_tls_buffer[256];
          GetWindowText(h_min_tls_box, min_tls_buffer, sizeof(min_tls_buffer));
          min_tls_version_ = min_tls_buffer;

          HWND h_trusted_cert_box = GetDlgItem(hwnd, kIdcTrustedCertEdit);
          char trusted_cert_buffer[256];
          GetWindowText(h_trusted_cert_box, trusted_cert_buffer,
                        sizeof(trusted_cert_buffer));
          trusted_cert_ = trusted_cert_buffer;

          HWND h_description_box = GetDlgItem(hwnd, kIdcDescriptionEdit);
          char description_buffer[256];
          GetWindowText(h_description_box, description_buffer,
                        sizeof(description_buffer));
          description_ = description_buffer;

          DestroyWindow(hwnd);  // Close the window
          break;
        }
        case kIdcButtonCancel: {
          HWND h_dsn = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsn_buffer[256];
          GetWindowText(h_dsn, dsn_buffer, sizeof(dsn_buffer));

          std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_buffer;
          auto res = GetSectionWin(registry_key);
          Section prev_section = *res.GetValue();
          adv_form->SetValues(prev_section);
          DestroyWindow(hwnd);
        } break;
        case kIdcAdvanceOptBtn: {
          if (adv_form) {
            adv_form->Show(hwnd);
            MSG msg = {};
            while (GetMessage(&msg, NULL, 0, 0)) {
              TranslateMessage(&msg);
              DispatchMessage(&msg);
            }
          }
          break;
        }
        case kIdcProxyOptionsButton: {
          if (proxy_form) {
            proxy_form->Show(hwnd);
            MSG msg = {};
            while (GetMessage(&msg, NULL, 0, 0)) {
              TranslateMessage(&msg);
              DispatchMessage(&msg);
            }
          }
          break;
        }
      }
      break;

    case WM_CLOSE: {
      HWND h_dsn = GetDlgItem(hwnd, kIdcDSNEdit);
      char dsn_buffer[256];
      GetWindowText(h_dsn, dsn_buffer, sizeof(dsn_buffer));

      std::string registry_key = GetPathToOdbcIni() + "\\" + dsn_buffer;
      auto res = GetSectionWin(registry_key);
      Section prev_section = *res.GetValue();
      adv_form->SetValues(prev_section);
      DestroyWindow(hwnd);
      return 0;
    }

    case WM_DESTROY:
      if (p_this) {
        p_this->m_hwnd = NULL;  // Set the window handle to NULL
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, u_msg, w_param, l_param);
}

}  // namespace google::cloud::odbc_bq_driver_internal
