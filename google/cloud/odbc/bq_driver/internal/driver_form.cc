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
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_tables.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::GetResultSetForDatasets;
using google::cloud::odbc_bq_driver_internal::GetResultSetForProjects;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

char const DriverForm::CLASS_NAME[] = "DriverFormClass";
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
std::string const kDsnName = "DSN";
std::string const kEmail = "Email";
std::string const kOAuthMechanism = "OAuthMechanism";
std::string const kKeyFilePath = "KeyFilePath";
std::string const kCatalog = "Catalog";
std::string const kDataset = "Dataset";
std::string const kEncryptData = "EncryptData";
std::string const kDescription = "Description";
std::string const kMinTlsVersion = "Min_TLS";
std::string const kTrustedCerts = "TrustedCerts";
std::string const kRefreshToken = "RefreshToken";

SQLRETURN ConnectUsingRegistryDsn(Authentication auth) {
  StatusRecordOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(auth.oauth);
  if (!response) {
    return SQL_ERROR;
  }
  auto client = *response;

  StatusRecordOr<AccessToken> access_token_resp = client->GetOAuth2Token();
  if (!access_token_resp) {
    return SQL_ERROR;
  }
  return SQL_SUCCESS;
}
Authentication CreateAuthentication(Section& dsn_section) {
  Authentication auth;
  int auth_int;
  try {
    auth_int = stoi(dsn_section[kOAuthMechanism]);
  } catch (std::exception const& ex) {
    auth_int = 0;
  }
  auth.oauth.auth_mechanism = static_cast<OauthMechanism>(auth_int);
  // TODO(shivamd-gpartner): DSN section entries should be capitalized
  // to be consistent with ConnectionHandle::SetUp function.
  auth.email = dsn_section[kEmail];
  auth.oauth.credentials_file_path = dsn_section[kKeyFilePath];
  // TODO(shivamd-gpartner): DSN section entries should be capitalized to be
  // consistent with ConnectionHandle::SetUp function.
  auth.refresh_token = dsn_section[kRefreshToken];
  return auth;
}

StatusRecord DriverForm::TestODBCConnection(
    std::shared_ptr<Section> const& section) {
  if (!section) {
    return StatusRecord{SQLStates::k_HY000(), "The provided section is null."};
  }

  if (section->find(kKeyFilePath) == section->end() ||
      (*section)[kKeyFilePath].empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "KeyFilePath is missing or empty."};
  }

  if (section->find(kOAuthMechanism) == section->end() ||
      (*section)[kOAuthMechanism].empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "OAuthMechanism is missing or empty."};
  }

  std::string oauth_mechanism = (*section)[kOAuthMechanism];
  std::string oauth_value;
  if (oauth_mechanism == "Service Authentication") {
    oauth_value = std::to_string(
        static_cast<int>(OauthMechanism::kServiceAndUserAccount));
  } else if (oauth_mechanism == "Application Default Credentials") {
    oauth_value =
        std::to_string(static_cast<int>(OauthMechanism::kApplicationDefault));
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "OAuthMechanism must be 'Service Authentication' or "
                        "'Application Default Credentials'."};
  }

  (*section)[kOAuthMechanism] = oauth_value;

  std::string key_file_path = (*section)[kKeyFilePath];
  std::string key_file_path_up;
  for (char ch : key_file_path) {
    if (ch == '\\') {
      key_file_path_up += "\\\\";
    } else {
      key_file_path_up += ch;
    }
  }

  Authentication auth = CreateAuthentication(*section);

  SQLRETURN ret = ConnectUsingRegistryDsn(auth);

  if (!SQL_SUCCEEDED(ret)) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to establish ODBC connection."};
  }

  return StatusRecord::Ok();
}

bool containsAlphanumeric(std::string const& str) {
  return std::any_of(str.begin(), str.end(),
                     [](unsigned char c) { return std::isalnum(c); });
}

StatusRecordOr<std::string> DriverForm::GetCatalogAndDataset(
    std::string const& action, std::string const& key_file_path,
    std::string const& oauth_token, std::string const& catalog_name) {
  google::cloud::odbc_bigquery_client_interface::OauthMechanism oauth_value;

  // TODO(b/383592420): Add call to user auth once its tested
  if (oauth_token == "Service Authentication") {
    oauth_value = google::cloud::odbc_bigquery_client_interface::
        OauthMechanism::kServiceAndUserAccount;
  } else if (oauth_token == "Application Default Credentials") {
    oauth_value = google::cloud::odbc_bigquery_client_interface::
        OauthMechanism::kApplicationDefault;
  } else {
    oauth_value = google::cloud::odbc_bigquery_client_interface::
        OauthMechanism::kExternalUser;
  }

  SQLULEN metadata_id = 0;
  auto bq_client_ptr =
      ODBCBQClient::CreateBQClient({oauth_value, key_file_path});
  if (!bq_client_ptr) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to create BigQuery client."};
  }

  ODBCBQClient& bq_client = **bq_client_ptr;

  StatusRecordOr<ResultSet> result_set_status;
  if (action == "Catalog") {
    result_set_status = GetResultSetForProjects(bq_client, metadata_id);
  } else if (action == "Dataset") {
    result_set_status =
        GetResultSetForDatasets(bq_client, metadata_id, catalog_name);
  }

  if (!result_set_status.Ok()) {
    return StatusRecord{SQLStates::k_HY000(), "Failed to fetch result set."};
  }

  ResultSet const& result_set = *result_set_status;
  std::string row_string;
  for (auto const& row : result_set.rows) {
    for (auto const& value : row) {
      std::string value_string(value.begin(), value.end());
      value_string.erase(remove(value_string.begin(), value_string.end(), ' '),
                         value_string.end());
      if (!value_string.empty() && containsAlphanumeric(value_string)) {
        row_string += value_string + ";";
      }
    }
  }

  return row_string;
}

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
DriverForm::DriverForm(HWND parent_hwnd)
    : m_hwnd(NULL), m_parent_hwnd(parent_hwnd) {}

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
  dsn_name_ =
      attributes_map.count(kDsnName) > 0 ? attributes_map.at(kDsnName) : "";
  email_ = attributes_map.count(kEmail) > 0 ? attributes_map.at(kEmail) : "";

  if (attributes_map.count(kOAuthMechanism) > 0) {
    std::string const& oauth_value = attributes_map.at(kOAuthMechanism);
    if (oauth_value == std::to_string(static_cast<int>(
                           OauthMechanism::kServiceAndUserAccount))) {
      o_auth_mechanism_ = "Service Authentication";
    } else if (oauth_value == std::to_string(static_cast<int>(
                                  OauthMechanism::kApplicationDefault))) {
      o_auth_mechanism_ = "Application Default Credentials";
    } else {
      o_auth_mechanism_ = "";
    }
  } else {
    o_auth_mechanism_ = "";
  }

  key_file_path_ = attributes_map.count(kKeyFilePath) > 0
                       ? attributes_map.at(kKeyFilePath)
                       : "";
  catalog_ =
      attributes_map.count(kCatalog) > 0 ? attributes_map.at(kCatalog) : "";
  dataset_ =
      attributes_map.count(kDataset) > 0 ? attributes_map.at(kDataset) : "";
  encrypt_data_ = attributes_map.count(kEncryptData) > 0
                      ? attributes_map.at(kEncryptData)
                      : "";
  description_ = attributes_map.count(kDescription) > 0
                     ? attributes_map.at(kDescription)
                     : "";
  min_tls_version_ = attributes_map.count(kMinTlsVersion) > 0
                         ? attributes_map.at(kMinTlsVersion)
                         : "";
  trusted_cert_ = attributes_map.count(kTrustedCerts) > 0
                      ? attributes_map.at(kTrustedCerts)
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
      CreateComboBox(m_hwnd, 180, 100, 220, 100, kIdcEncryptDataComboBox);

  HWND h_auth_header = CreateLabel(m_hwnd, "OAuth Mechanism:", 20, 140, 180, 20,
                                   WS_VISIBLE | SS_LEFT);
  HWND h_auth_combo_box =
      CreateComboBox(m_hwnd, 180, 140, 220, 100, kIdcAuthBox);

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
      CreateComboBox(m_hwnd, 180, 300, 220, 100, kIdcMinTLSComboBox);

  HWND h_trusted_cert_header = CreateLabel(m_hwnd, "Trusted Certificate:", 20,
                                           340, 150, 20, WS_VISIBLE | SS_LEFT);
  HWND h_trusted_cert_edit =
      CreateEditBox(m_hwnd, 180, 340, 220, 20, kIdcTrustedCertEdit);

  HWND h_trusted_cert_browse_button = CreateButton(
      m_hwnd, "Browse", 420, 340, 80, 20, kIdcTrustedCertBrowseButton);

  HWND h_catalog_header =
      CreateLabel(m_hwnd, "Catalog:", 20, 380, 150, 20, WS_VISIBLE | SS_LEFT);
  HWND h_catalog_box =
      CreateComboBox(m_hwnd, 180, 380, 220, 100, kIdcCatlogBOX);

  HWND h_dataset_header =
      CreateLabel(m_hwnd, "Dataset:", 20, 420, 150, 20, WS_VISIBLE | SS_LEFT);
  HWND h_dataset_box =
      CreateComboBox(m_hwnd, 180, 420, 220, 100, kIdcDatasetBOX);

  HWND h_gcp_parent_folder_header = CreateLabel(
      m_hwnd, "GCP Parent Folder:", 20, 460, 180, 20, WS_VISIBLE | SS_LEFT);
  HWND h_gcp_parent_folder_text =
      CreateEditBox(m_hwnd, 180, 460, 220, 20, kIdcGcpFolder);

  HWND h_proxy_options_button = CreateButton(
      m_hwnd, "Proxy Options...", 20, 500, 150, 30, kIdcProxyOptionsButton);
  HWND h_login_button = CreateButton(m_hwnd, "Logging Options...", 190, 500,
                                     150, 30, kIdcLoggingBtn);
  HWND h_advance_opt_button = CreateButton(m_hwnd, "Advance Options...", 360,
                                           500, 130, 30, kIdcAdvanceOptBtn);
  HWND h_test_button =
      CreateButton(m_hwnd, "Test...", 190, 560, 80, 30, kIdcButtonTest);
  EnableWindow(h_test_button, FALSE);
  HWND h_ok_button = CreateButton(m_hwnd, "OK", 280, 560, 80, 30, kIdcButtonOk);
  EnableWindow(h_ok_button, FALSE);
  HWND h_cancel_button =
      CreateButton(m_hwnd, "Cancel", 370, 560, 80, 30, kIdcButtonCancel);

  SendMessage(h_encrypt_data_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "For Current User Only");
  SendMessage(h_encrypt_data_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "For All Users");
  SendMessage(h_encrypt_data_combo_box, CB_SETCURSEL, 0, 0);

  SendMessage(h_auth_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Service Authentication");
  SendMessage(h_auth_combo_box, CB_ADDSTRING, 0,
              (LPARAM) "Application Default Credentials");
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

  int xPos = (screen_width - window_width) / 2;
  int yPos = (screen_height - window_height) / 2;

  m_hwnd = CreateWindowEx(
      0, CLASS_NAME, "Google ODBC Driver for Google Bigquery DSN Setup",
      WS_OVERLAPPEDWINDOW, xPos, yPos, window_width, window_height, NULL, NULL,
      GetModuleHandle(NULL), this);

  if (m_hwnd) {
    InitControls();

    // Create and position OK and Cancel buttons at the bottom
    RECT rc_client;
    GetClientRect(m_hwnd, &rc_client);

    int button_width = 100;
    int button_height = 30;
    int button_y = rc_client.bottom - button_height -
                   20;        // Position 20 pixels from the bottom
    int button_spacing = 20;  // Space between buttons

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
  }
}

StatusRecord DriverForm::IsValidEmail(std::string const& email) {
  std::regex const pattern(R"((\w+)(\.|\-)?(\w*)@(\w+)(\.\w+)+)");
  if (std::regex_match(email, pattern)) {
    return StatusRecord::Ok();
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "Email does not match the required pattern."};
  }
}

void EvaluateFields(HWND hwnd) {
  char dsn_buffer[256] = {0};
  char key_buffer[256] = {0};
  char auth_buffer[256] = {0};
  char catalog_buffer[256] = {0};

  GetWindowText(GetDlgItem(hwnd, kIdcDSNEdit), dsn_buffer, sizeof(dsn_buffer));
  GetWindowText(GetDlgItem(hwnd, kIdcKeyfileEdit), key_buffer,
                sizeof(key_buffer));
  GetWindowText(GetDlgItem(hwnd, kIdcAuthBox), auth_buffer,
                sizeof(auth_buffer));
  GetWindowText(GetDlgItem(hwnd, kIdcCatlogBOX), catalog_buffer,
                sizeof(catalog_buffer));
  BOOL enable = dsn_buffer[0] != '\0' && key_buffer[0] != '\0' &&
                auth_buffer[0] != '\0' && catalog_buffer[0] != '\0';

  EnableWindow(GetDlgItem(hwnd, kIdcButtonOk), enable);
  EnableWindow(GetDlgItem(hwnd, kIdcButtonTest), enable);
}

void PopulateDropdown(HWND h_dataset_box, std::string text,
                      std::string key_file, std::string oauth,
                      std::string catalog) {
  SendMessage(h_dataset_box, CB_RESETCONTENT, 0, 0);

  StatusRecordOr<std::string> status_record =
      DriverForm::GetCatalogAndDataset(text, key_file, oauth, catalog);

  if (!status_record.Ok()) {
    MessageBox(h_dataset_box, status_record.GetStatusRecord().message.c_str(),
               "Error", MB_OK | MB_ICONERROR);
    return;
  }

  std::string row_string = status_record.GetValue();

  std::vector<std::string> values = Split(row_string, ";");

  for (auto const& value : values) {
    if (!value.empty()) {
      SendMessage(h_dataset_box, CB_ADDSTRING, 0,
                  reinterpret_cast<LPARAM>(value.c_str()));
    }
  }
}

void RetrieveFieldText(HWND hwnd, int control_id, char* buffer,
                       size_t buffer_size) {
  HWND h_control = GetDlgItem(hwnd, control_id);
  GetWindowText(h_control, buffer, buffer_size);
}

void HandleDropdown(HWND hwnd, int control_id, char const* field_type,
                    char const* key_buffer, char const* auth_buffer,
                    char const* catalog_buffer = "") {
  HWND h_control = GetDlgItem(hwnd, control_id);
  if (key_buffer[0] && auth_buffer[0] &&
      (strcmp(field_type, "Catalog") == 0 || catalog_buffer[0])) {
    PopulateDropdown(h_control, field_type, key_buffer, auth_buffer,
                     catalog_buffer);
  }
}

void HandleSelectionChange(HWND hwnd, int control_id) {
  HWND h_control = GetDlgItem(hwnd, control_id);
  int selected_index = SendMessage(h_control, CB_GETCURSEL, 0, 0);
  if (selected_index != CB_ERR) {
    char selected_value[256];
    SendMessage(h_control, CB_GETLBTEXT, selected_index,
                reinterpret_cast<LPARAM>(selected_value));
    SetWindowText(h_control, selected_value);
  }
}
LRESULT CALLBACK DriverForm::WindowProc(HWND hwnd, UINT u_msg, WPARAM w_param,
                                        LPARAM l_param) {
  DriverForm* p_this =
      reinterpret_cast<DriverForm*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (u_msg) {
    case WM_CREATE:
      // Set the instance pointer in the window's user data
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p_this));
      break;

    case WM_COMMAND:
      if (HIWORD(w_param) == EN_UPDATE || HIWORD(w_param) == EN_CHANGE) {
        EvaluateFields(hwnd);
      }
      switch (LOWORD(w_param)) {
        case kIdcAuthBox:
        case kIdcDSNEdit:
        case kIdcKeyfileEdit: {
          EvaluateFields(hwnd);
        } break;
        case kIdcBrowseButton: {
          HWND h_edit = GetDlgItem(hwnd, kIdcKeyfileEdit);
          OpenFileDialog(hwnd, h_edit);
        } break;
        case kIdcTrustedCertBrowseButton: {
          HWND h_edit = GetDlgItem(hwnd, kIdcTrustedCertEdit);
          OpenFileDialog(hwnd, h_edit);
        } break;
        case kIdcLoggingBtn: {
          LogTraceDialog log_form;
          if (IsWindowVisible(log_form.GetHwnd())) {
            SetForegroundWindow(log_form.GetHwnd());
            EnableWindow(hwnd, FALSE);
            break;
          }
          if (!log_form.GetHwnd()) {
            log_form = LogTraceDialog();
            EnableWindow(hwnd, FALSE);
          }
          log_form.Show();
          MSG msg = {};
          while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
          EnableWindow(hwnd, TRUE);
          SetForegroundWindow(hwnd);
          break;
        }
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
          if (min_tls_version_ != "1.2") {
            MessageBox(hwnd, "Invalid MIN TLS Version!", "Error",
                       MB_OK | MB_ICONERROR);
            min_tls_version_ = "";
            return 0;
          }

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
        case kIdcButtonTest: {
          HWND h_dsn = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsn_buffer[256];
          GetWindowText(h_dsn, dsn_buffer, sizeof(dsn_buffer));

          HWND h_key = GetDlgItem(hwnd, kIdcKeyfileEdit);
          char key_buffer[256];
          GetWindowText(h_key, key_buffer, sizeof(key_buffer));

          HWND h_auth_box = GetDlgItem(hwnd, kIdcAuthBox);
          char auth_buffer[256];
          GetWindowText(h_auth_box, auth_buffer, sizeof(auth_buffer));

          Section attributes_map;
          attributes_map[kDsnName] = dsn_buffer;
          attributes_map[kEmail] = email_;
          attributes_map[kKeyFilePath] = key_buffer;
          attributes_map[kOAuthMechanism] = auth_buffer;
          attributes_map[kDataset] = dataset_;

          auto status =
              TestODBCConnection(std::make_shared<Section>(attributes_map));
          if (status.ok()) {
            std::string message_text =
                "SUCCESS!\n\nSuccessfully connected to data source!\n\n";
            MessageBox(hwnd, message_text.c_str(), "Test Results",
                       MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            return 0;
          } else {
            MessageBox(hwnd, "Connection Failed!", "Error",
                       MB_OK | MB_ICONERROR);
            return 0;
          }
        }
        case kIdcProxyOptionsButton: {
          ProxyOptions proxy_form;
          if (IsWindowVisible(proxy_form.GetHwnd())) {
            SetForegroundWindow(proxy_form.GetHwnd());
            EnableWindow(hwnd, FALSE);
            break;
          }
          if (!proxy_form.GetHwnd()) {
            proxy_form = ProxyOptions();
            EnableWindow(hwnd, FALSE);
          }
          proxy_form.Show(hwnd);
          MSG msg = {};
          while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
          EnableWindow(hwnd, TRUE);
          SetForegroundWindow(hwnd);
          break;
        }
        case kIdcAdvanceOptBtn: {
          AdvanceOptions adv_form;
          if (IsWindowVisible(adv_form.GetHwnd())) {
            SetForegroundWindow(adv_form.GetHwnd());
            EnableWindow(hwnd, FALSE);
            break;
          }
          if (!adv_form.GetHwnd()) {
            adv_form = AdvanceOptions();
            EnableWindow(hwnd, FALSE);
          }
          adv_form.Show(hwnd);
          MSG msg = {};
          while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
          }
          EnableWindow(hwnd, TRUE);
          SetForegroundWindow(hwnd);
          break;
        }
        case kIdcCatlogBOX:
        case kIdcDatasetBOX: {
          EvaluateFields(hwnd);
          char catalog_buffer[256] = {};
          char dsn_buffer[256] = {};
          char key_buffer[256] = {};
          char auth_buffer[256] = {};
          char data_buffer[256] = {};  // Only used for kIdcDatasetBOX

          RetrieveFieldText(hwnd, kIdcCatlogBOX, catalog_buffer,
                            sizeof(catalog_buffer));
          RetrieveFieldText(hwnd, kIdcDSNEdit, dsn_buffer, sizeof(dsn_buffer));
          RetrieveFieldText(hwnd, kIdcKeyfileEdit, key_buffer,
                            sizeof(key_buffer));
          RetrieveFieldText(hwnd, kIdcAuthBox, auth_buffer,
                            sizeof(auth_buffer));

          if (LOWORD(w_param) == kIdcDatasetBOX) {
            RetrieveFieldText(hwnd, kIdcDatasetBOX, data_buffer,
                              sizeof(data_buffer));
          }

          switch (HIWORD(w_param)) {
            case CBN_DROPDOWN:
              if (LOWORD(w_param) == kIdcCatlogBOX) {
                HandleDropdown(hwnd, kIdcCatlogBOX, "Catalog", key_buffer,
                               auth_buffer);
              } else if (LOWORD(w_param) == kIdcDatasetBOX) {
                HandleDropdown(hwnd, kIdcDatasetBOX, "Dataset", key_buffer,
                               auth_buffer, catalog_buffer);
              }
              break;

            case CBN_SELCHANGE:
              if (LOWORD(w_param) == kIdcCatlogBOX) {
                HandleSelectionChange(hwnd, kIdcCatlogBOX);
              } else if (LOWORD(w_param) == kIdcDatasetBOX) {
                HandleSelectionChange(hwnd, kIdcDatasetBOX);
              }
              break;
          }
          break;
        }
        case kIdcButtonCancel:
          DestroyWindow(hwnd);  // Close the window
          break;
      }
      break;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;

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
