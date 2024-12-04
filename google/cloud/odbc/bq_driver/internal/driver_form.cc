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
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bq_driver_internal::Authentication;
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

SQLRETURN ConnectUsingRegistryDsn(Authentication auth) {
  Oauth o_auth;
  o_auth.auth_mechanism = auth.auth_mechanism;
  o_auth.credentials_file_path = auth.key_file_path;

  StatusRecordOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(o_auth);
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
    auth_int = stoi(dsn_section["OAuthMechanism"]);
  } catch (std::exception const& ex) {
    auth_int = 0;
  }
  auth.auth_mechanism = static_cast<OauthMechanism>(auth_int);
  auth.email = dsn_section["Email"];
  auth.key_file_path = dsn_section["KeyFilePath"];
  auth.refresh_token = dsn_section["RefreshToken"];
  return auth;
}

StatusRecord DriverForm::TestODBCConnection(
    std::shared_ptr<Section> const& section) {
  if (!section) {
    return StatusRecord{SQLStates::k_HY000(), "The provided section is null."};
  }

  if (section->find("KeyFilePath") == section->end() ||
      (*section)["KeyFilePath"].empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "KeyFilePath is missing or empty."};
  }

  if (section->find("OAuthMechanism") == section->end() ||
      (*section)["OAuthMechanism"].empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "OAuthMechanism is missing or empty."};
  }

  std::string oauth_mechanism = (*section)["OAuthMechanism"];
  std::string oauth_value;
  if (oauth_mechanism == "Service Authentication") {
    oauth_value =
        std::to_string(static_cast<int>(OauthMechanism::kServiceAccount));
  } else if (oauth_mechanism == "Application Default Credentials") {
    oauth_value =
        std::to_string(static_cast<int>(OauthMechanism::kApplicationDefault));
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "OAuthMechanism must be 'Service Authentication' or "
                        "'Application Default Credentials'."};
  }

  (*section)["OAuthMechanism"] = oauth_value;

  std::string key_file_path = (*section)["KeyFilePath"];
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
    if (attributes_map.at("OAuthMechanism") ==
        std::to_string(static_cast<int>(OauthMechanism::kServiceAccount))) {
      o_auth_mechanism_ = "Service Authentication";
    } else if (attributes_map.at("OAuthMechanism") ==
               std::to_string(
                   static_cast<int>(OauthMechanism::kApplicationDefault))) {
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

// Helper function to set font for controls
void SetControlFont(HWND hwnd, HFONT font) {
  SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
}

// Function to initialize controls
void DriverForm::InitControls() {
  // Set custom font for the controls
  HFONT h_font = CreateCustomFont(10);  // Font size 10

  // Create controls
  HWND h_dsn_name_header = CreateLabel(m_hwnd, "DSN Name:", 20, 80, 100, 20, 0);
  HWND h_dsn_name_edit = CreateEditBox(m_hwnd, 100, 80, 200, 20, kIdcDSNEdit);
  SetWindowText(h_dsn_name_edit, dsn_name_.c_str());
  if (!dsn_name_.empty()) {
    // If there is a value, make the edit box read-only
    HWND h_dsn_edit_box = GetDlgItem(m_hwnd, kIdcDSNEdit);
    SendMessage(h_dsn_edit_box, EM_SETREADONLY, TRUE, 0);
  }

  HWND h_auth_head =
      CreateLabel(m_hwnd, "OAuth Mechanism:", 20, 120, 120, 20, kIdcLabel);
  HWND h_auth_box = CreateComboBox(m_hwnd, 140, 120, 150, 100, kIdcAuthBox);

  HWND h_email_header = CreateLabel(m_hwnd, "Email:", 20, 160, 40, 20, 0);
  HWND h_email_edit = CreateEditBox(m_hwnd, 100, 160, 200, 20, kIdcEmailEdit);

  HWND h_path_add = CreateLabel(m_hwnd, "Key File Path:", 20, 200, 100, 30, 0);
  HWND h_keyfile_edit =
      CreateEditBox(m_hwnd, 120, 200, 250, 20, kIdcKeyfileEdit);
  CreateButton(m_hwnd, "Browse", 150, 230, 100, 20, kIdcBrowseButton);

  HWND h_catalog_text = CreateLabel(m_hwnd, "Catalog (Project):", 20, 280, 110,
                                    20, kIdcCatalogLabel);
  HWND h_catalog_box =
      CreateComboBox(m_hwnd, 160, 280, 230, 100, kIdcCatlogBOX);

  HWND h_dataset_text =
      CreateLabel(m_hwnd, "Dataset:", 20, 320, 50, 20, kIdcDatasetLabel);
  HWND h_dataset_box =
      CreateComboBox(m_hwnd, 160, 320, 230, 100, kIdcDatasetBOX);

  HWND hwnd_test_button =
      CreateButton(m_hwnd, "Test...", 120, 400, 80, 30, kIdcButtonTest);

  HWND hwnd_ok_button =
      CreateButton(m_hwnd, "Ok", 220, 400, 80, 30, kIdcButtonOk);
  HWND hwnd_cancel_button =
      CreateButton(m_hwnd, "Cancel", 320, 400, 80, 30, kIdcButtonCancel);

  // Populate dropdowns
  SendMessage(h_auth_box, CB_ADDSTRING, 0, (LPARAM) "Service Authentication");
  SendMessage(h_auth_box, CB_ADDSTRING, 0,
              (LPARAM) "Application Default Credentials");
  SendMessage(h_auth_box, CB_SETCURSEL, 0, 0);

  // Apply font to controls
  SetControlFont(h_auth_head, h_font);
  SetControlFont(h_auth_box, h_font);
  SetControlFont(h_dsn_name_header, h_font);
  SetControlFont(h_dsn_name_edit, h_font);
  SetControlFont(h_email_header, h_font);
  SetControlFont(h_email_edit, h_font);
  SetControlFont(h_path_add, h_font);
  SetControlFont(h_keyfile_edit, h_font);
  SetControlFont(h_catalog_text, h_font);
  SetControlFont(h_catalog_box, h_font);
  SetControlFont(h_dataset_text, h_font);
  SetControlFont(h_dataset_box, h_font);

  SetWindowText(h_email_edit, email_.c_str());
  SetWindowText(h_keyfile_edit, key_file_path_.c_str());
  SetWindowText(h_catalog_box, catalog_.c_str());
  SetWindowText(h_dataset_box, dataset_.c_str());
  SetWindowText(h_auth_box, o_auth_mechanism_.c_str());
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
    CreateWindowEx(0, "STATIC",
                   "DSN configure",  // Header text
                   WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 50, 200,
                   20,  // Position and size
                   m_hwnd, (HMENU)kIdcHeaderLabel, GetModuleHandle(NULL), NULL);
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
      switch (LOWORD(w_param)) {
        case kIdcBrowseButton: {
          HWND h_edit = GetDlgItem(hwnd, kIdcKeyfileEdit);
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
          attributes_map["DSN"] = dsn_buffer;
          attributes_map["Email"] = email_;
          attributes_map["KeyFilePath"] = key_buffer;
          attributes_map["OAuthMechanism"] = auth_buffer;
          attributes_map["Dataset"] = dataset_;

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
