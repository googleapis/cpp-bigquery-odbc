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
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::Section;
using google::cloud::odbc_internal::StatusRecordOr;

char const DriverForm::CLASS_NAME[] = "DriverFormClass";
std::string DriverForm::dsn_name_;
std::string DriverForm::email_;
std::string DriverForm::key_file_path_;
std::string DriverForm::o_auth_mechanism_;
std::string DriverForm::catalog_;
std::string DriverForm::dataset_;

SQLRETURN ConnectUsingRegistryDsn(Authentication auth) {
  Oauth oauth;
  oauth.auth_mechanism = auth.auth_mechanism;
  oauth.credentials_file_path = auth.key_file_path;

  StatusRecordOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(oauth);
  if (!response) {
    return SQL_ERROR;
  }
  auto client_ = *response;

  StatusRecordOr<AccessToken> access_token_resp = client_->GetOAuth2Token();
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

bool DriverForm::TestODBCConnection(std::shared_ptr<Section> const& section) {
  if (!section) {
    return false;
  }

  if (section->find("KeyFilePath") == section->end() ||
      (*section)["KeyFilePath"].empty()) {
    return false;
  }
  if (section->find("OAuthMechanism") == section->end() ||
      (*section)["OAuthMechanism"].empty()) {
    return false;
  }

  std::string oauth_mechanism = (*section)["OAuthMechanism"];
  std::string oauth_value;
  if (oauth_mechanism == "Service Authentication") {
    oauth_value = "0";
  } else if (oauth_mechanism == "Application Default Credentials") {
    oauth_value = "4";
  } else {
    return false;
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
  SQLRETURN ret;
  Authentication auth = CreateAuthentication(*section);
  try {
    ret = ConnectUsingRegistryDsn(auth);
  } catch (std::exception const& e) {
    ret = -1;
  }

  bool status = SQL_SUCCEEDED(ret);
  return status;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
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

void OpenFileDialog(HWND hwnd, HWND hEdit, char const* MockFilePath = nullptr) {
  if (MockFilePath) {
    // Directly set the test file path to the edit control if provided
    SetWindowText(hEdit, MockFilePath);
    return;
  }
  OPENFILENAME ofn;
  char szFile[260] = {0};  // Buffer for file path

  // Initialize OPENFILENAME structure
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "JSON Files\0*.JSON\0All Files\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  // Display the Open File dialog
  if (GetOpenFileName(&ofn)) {
    // Set the selected file path to the edit control
    SetWindowText(hEdit, szFile);
  }
}

void DriverForm::SetValues(Section const& attributesMap) {
  dsn_name_ = attributesMap.count("DSN") > 0 ? attributesMap.at("DSN") : "";
  email_ = attributesMap.count("Email") > 0 ? attributesMap.at("Email") : "";
  if (attributesMap.count("OAuthMechanism") > 0) {
    if (attributesMap.at("OAuthMechanism") == "0") {
      o_auth_mechanism_ = "Service Authentication";
    } else if (attributesMap.at("OAuthMechanism") == "3") {
      o_auth_mechanism_ = "Application Default Credentials";
    } else
      o_auth_mechanism_ = "";
  } else
    o_auth_mechanism_ = "";
  key_file_path_ = attributesMap.count("KeyFilePath") > 0
                       ? attributesMap.at("KeyFilePath")
                       : "";
  catalog_ =
      attributesMap.count("Catalog") > 0 ? attributesMap.at("Catalog") : "";
  dataset_ =
      attributesMap.count("Dataset") > 0 ? attributesMap.at("Dataset") : "";
}

HFONT CreateCustomFont(int fontSize) {
  LOGFONT logFont = {};
  HFONT hFont = NULL;
  logFont.lfHeight = -MulDiv(fontSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY),
                             72);        // Negative height for screen fonts
  lstrcpy(logFont.lfFaceName, "Arial");  // Font face name

  hFont = CreateFontIndirect(&logFont);
  return hFont;
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
  HFONT hFont = CreateCustomFont(10);  // Font size 10

  // Create controls
  HWND hDSNnameHeader = CreateLabel(m_hwnd, "DSN Name:", 20, 80, 100, 20, 0);
  HWND hDSNnameEdit = CreateEditBox(m_hwnd, 100, 80, 200, 20, kIdcDSNEdit);
  SetWindowText(hDSNnameEdit, dsn_name_.c_str());
  if (!dsn_name_.empty()) {
    // If there is a value, make the edit box read-only
    HWND hDsnEditBox = GetDlgItem(m_hwnd, kIdcDSNEdit);
    SendMessage(hDsnEditBox, EM_SETREADONLY, TRUE, 0);
  }

  HWND hAuthHead =
      CreateLabel(m_hwnd, "OAuth Mechanism:", 20, 120, 120, 20, kIdcLabel);
  HWND hComboBox = CreateComboBox(m_hwnd, 140, 120, 220, 100, kIdcComboBox);

  HWND hEmailHeader = CreateLabel(m_hwnd, "Email:", 20, 160, 40, 20, 0);
  HWND hEmailEdit = CreateEditBox(m_hwnd, 100, 160, 200, 20, kIdcEmailEdit);

  HWND hPathAdd = CreateLabel(m_hwnd, "Key File Path:", 20, 200, 100, 30, 0);
  HWND hKeyFileEdit = CreateEditBox(m_hwnd, 120, 200, 250, 20, kIdcKeyfileEdit);
  CreateButton(m_hwnd, "Browse", 150, 230, 100, 20, kIdcBrowseButton);

  HWND hCatalogText = CreateLabel(m_hwnd, "Catalog (Project):", 20, 280, 110,
                                  20, kIdcCatalogLabel);
  HWND hCatalogBox = CreateComboBox(m_hwnd, 160, 280, 230, 100, kIdcCatlogBOX);

  HWND hDatasetText =
      CreateLabel(m_hwnd, "Dataset:", 20, 320, 50, 20, kIdcDatasetLabel);
  HWND hDatasetBox = CreateComboBox(m_hwnd, 160, 320, 230, 100, kIdcDatasetBOX);

  HWND hwndTestButton =
      CreateButton(m_hwnd, "Test...", 120, 400, 80, 30, kIdcButtonTest);

  HWND hwndOkButton =
      CreateButton(m_hwnd, "Ok", 220, 400, 80, 30, kIdcButtonOk);
  HWND hwndCancelButton =
      CreateButton(m_hwnd, "Cancel", 320, 400, 80, 30, kIdcButtonCancel);

  // Populate dropdowns
  SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM) "Service Authentication");
  SendMessage(hComboBox, CB_ADDSTRING, 0,
              (LPARAM) "Application Default Credentials");
  SendMessage(hComboBox, CB_SETCURSEL, 0, 0);

  // Apply font to controls
  SetControlFont(hAuthHead, hFont);
  SetControlFont(hComboBox, hFont);
  SetControlFont(hDSNnameHeader, hFont);
  SetControlFont(hDSNnameEdit, hFont);
  SetControlFont(hEmailHeader, hFont);
  SetControlFont(hEmailEdit, hFont);
  SetControlFont(hPathAdd, hFont);
  SetControlFont(hKeyFileEdit, hFont);
  SetControlFont(hCatalogText, hFont);
  SetControlFont(hCatalogBox, hFont);
  SetControlFont(hDatasetText, hFont);
  SetControlFont(hDatasetBox, hFont);

  SetWindowText(hEmailEdit, email_.c_str());
  SetWindowText(hKeyFileEdit, key_file_path_.c_str());
  SetWindowText(hCatalogBox, catalog_.c_str());
  SetWindowText(hDatasetBox, dataset_.c_str());
  SetWindowText(hComboBox, o_auth_mechanism_.c_str());
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
  int windowWidth = 520;
  int windowHeight = 650;

  int screenWidth = GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = GetSystemMetrics(SM_CYSCREEN);

  int xPos = (screenWidth - windowWidth) / 2;
  int yPos = (screenHeight - windowHeight) / 2;

  m_hwnd = CreateWindowEx(
      0, CLASS_NAME, "Google ODBC Driver for Google Bigquery DSN Setup",
      WS_OVERLAPPEDWINDOW, xPos, yPos, windowWidth, windowHeight, NULL, NULL,
      GetModuleHandle(NULL), this);

  if (m_hwnd) {
    CreateWindowEx(0, "STATIC",
                   "DSN configure",  // Header text
                   WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 50, 200,
                   20,  // Position and size
                   m_hwnd, (HMENU)kIdcHeaderLabel, GetModuleHandle(NULL), NULL);
    InitControls();

    // Create and position OK and Cancel buttons at the bottom
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);

    int buttonWidth = 100;
    int buttonHeight = 30;
    int buttonY = rcClient.bottom - buttonHeight -
                  20;        // Position 20 pixels from the bottom
    int buttonSpacing = 20;  // Space between buttons

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
  }
}

bool DriverForm::IsValidEmail(std::string const& email) {
  std::regex const pattern(R"((\w+)(\.|\-)?(\w*)@(\w+)(\.\w+)+)");
  return std::regex_match(email, pattern);
}

LRESULT CALLBACK DriverForm::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                        LPARAM lParam) {
  DriverForm* pThis =
      reinterpret_cast<DriverForm*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (uMsg) {
    case WM_CREATE:
      // Set the instance pointer in the window's user data
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
      break;
    case WM_KEYDOWN: {
      if (wParam == VK_ESCAPE) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        return 0;
      }
      break;
    }
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case kIdcBrowseButton: {
          HWND hEdit = GetDlgItem(hwnd, kIdcKeyfileEdit);
          OpenFileDialog(hwnd, hEdit);
        } break;
        case kIdcButtonOk: {
          HWND hDSN = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsnBuffer[256];
          GetWindowText(hDSN, dsnBuffer, sizeof(dsnBuffer));
          dsn_name_ = dsnBuffer;

          HWND hEmail = GetDlgItem(hwnd, kIdcEmailEdit);
          char emailBuffer[256];
          GetWindowText(hEmail, emailBuffer, sizeof(emailBuffer));
          email_ = emailBuffer;
          if (!pThis->IsValidEmail(email_) && !email_.empty()) {
            MessageBox(hwnd, "Invalid email address!", "Error",
                       MB_OK | MB_ICONERROR);
            email_ = "";
            return 0;
          }

          HWND hKey = GetDlgItem(hwnd, kIdcKeyfileEdit);
          char keyBuffer[256];
          GetWindowText(hKey, keyBuffer, sizeof(keyBuffer));
          key_file_path_ = keyBuffer;

          HWND hComboBox = GetDlgItem(hwnd, kIdcComboBox);
          char authBuffer[256];
          GetWindowText(hComboBox, authBuffer, sizeof(authBuffer));
          o_auth_mechanism_ = authBuffer;

          HWND hCatalogBox = GetDlgItem(hwnd, kIdcCatlogBOX);
          char catalogBuffer[256];
          GetWindowText(hCatalogBox, catalogBuffer, sizeof(catalogBuffer));
          catalog_ = catalogBuffer;

          HWND hDatasetBox = GetDlgItem(hwnd, kIdcDatasetBOX);
          char dataBuffer[256];
          GetWindowText(hDatasetBox, dataBuffer, sizeof(dataBuffer));
          dataset_ = dataBuffer;

          DestroyWindow(hwnd);  // Close the window
          break;
        }
        case kIdcButtonTest: {
          HWND hDSN = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsnBuffer[256];
          GetWindowText(hDSN, dsnBuffer, sizeof(dsnBuffer));

          HWND hKey = GetDlgItem(hwnd, kIdcKeyfileEdit);
          char keyBuffer[256];
          GetWindowText(hKey, keyBuffer, sizeof(keyBuffer));

          HWND hComboBox = GetDlgItem(hwnd, kIdcComboBox);
          char authBuffer[256];
          GetWindowText(hComboBox, authBuffer, sizeof(authBuffer));

          Section attributesMap;
          attributesMap["DSN"] = dsnBuffer;
          attributesMap["Email"] = email_;
          attributesMap["KeyFilePath"] = keyBuffer;
          attributesMap["OAuthMechanism"] = authBuffer;
          attributesMap["Dataset"] = dataset_;

          bool status =
              TestODBCConnection(std::make_shared<Section>(attributesMap));
          if (status == true) {
            std::string messageText =
                "SUCCESS!\n\nSuccessfully connected to data source!\n\n";
            MessageBox(hwnd, messageText.c_str(), "Test Results",
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
      if (pThis) {
        pThis->m_hwnd = NULL;  // Set the window handle to NULL
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

}  // namespace google::cloud::odbc_bq_driver_internal
#endif /* WIN32*/
