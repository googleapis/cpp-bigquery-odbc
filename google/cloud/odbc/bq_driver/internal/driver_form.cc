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

#ifdef _WIN32
#include "google/cloud/odbc/bq_driver/internal/driver_form.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {

char const DriverForm::CLASS_NAME[] = "DriverFormClass";
std::string DriverForm::dsn_name_;
std::string DriverForm::email_;
std::string DriverForm::key_file_path_;
std::string DriverForm::o_auth_mechanism_;
std::string DriverForm::catalog_;
std::string DriverForm::dataset_;

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
  dsn_name_ = attributes_map.count("DSN") > 0 ? attributes_map.at("DSN") : "";
  email_ = attributes_map.count("Email") > 0 ? attributes_map.at("Email") : "";
  o_auth_mechanism_ = attributes_map.count("OAuthMechanism") > 0
                          ? attributes_map.at("OAuthMechanism")
                          : "";
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

  HWND hwnd_ok_button =
      CreateButton(m_hwnd, "Ok", 220, 400, 80, 30, kIdcButtonOk);
  HWND hwnd_cancel_button =
      CreateButton(m_hwnd, "Cancel", 320, 400, 80, 30, kIdcButtonCancel);

  // Populate dropdowns
  SendMessage(h_auth_box, CB_ADDSTRING, 0, (LPARAM) "For Current User");
  SendMessage(h_auth_box, CB_ADDSTRING, 0, (LPARAM) "For All Users");
  SendMessage(h_auth_box, CB_SETCURSEL, 0, 0);

  SendMessage(h_catalog_box, CB_ADDSTRING, 0, (LPARAM) "Project 1");
  SendMessage(h_catalog_box, CB_ADDSTRING, 0, (LPARAM) "Project 2");
  SendMessage(h_catalog_box, CB_SETCURSEL, 0, 0);

  SendMessage(h_dataset_box, CB_ADDSTRING, 0, (LPARAM) "Dataset 1");
  SendMessage(h_dataset_box, CB_ADDSTRING, 0, (LPARAM) "Dataset 2");
  SendMessage(h_dataset_box, CB_SETCURSEL, 0, 0);

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

  m_hwnd = CreateWindowEx(0, CLASS_NAME,
                          "Google ODBC Driver for Google Bigquery DSN Setup",
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          520, 650, NULL, NULL, GetModuleHandle(NULL), this);

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

bool IsValidEmail(std::string const& email) {
  std::regex const pattern(R"((\w+)(\.|\-)?(\w*)@(\w+)(\.\w+)+)");
  return std::regex_match(email, pattern);
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
          if (!IsValidEmail(email_) && !email_.empty()) {
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
#endif  // _WIN32
