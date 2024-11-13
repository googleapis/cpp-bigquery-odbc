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
#include <shlobj.h>

namespace google::cloud::odbc_bq_driver_internal {

char const DriverForm::CLASS_NAME[] = "DriverFormClass";
char const LogTraceDialog::CLASS_NAME[] = "LoggingTraceClass";
std::string DriverForm::dsn_name_;
std::string DriverForm::email_;
std::string DriverForm::key_file_path_;
std::string DriverForm::o_auth_mechanism_;
std::string DriverForm::catalog_;
std::string DriverForm::dataset_;
std::string LogTraceDialog::log_level_;
std::string LogTraceDialog::log_file_path_;

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
DriverForm::DriverForm(HWND parent_hwnd)
    : m_hwnd(NULL), m_parent_hwnd(parent_hwnd) {}

DriverForm::~DriverForm() {
  if (m_hwnd) {
    DestroyWindow(m_hwnd);
  }
}

LogTraceDialog::LogTraceDialog() : parent_hwnd(NULL) {}
LogTraceDialog::~LogTraceDialog() {
  if (parent_hwnd) {
    DestroyWindow(parent_hwnd);
  }
  UnregisterClass(CLASS_NAME, GetModuleHandle(NULL));
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

void OpenFolderDialog(HWND hwnd, HWND hEdit,
                      char const* MockFolderPath = nullptr) {
  if (MockFolderPath) {
    // Directly set the test folder path to the edit control if provided
    SetWindowText(hEdit, MockFolderPath);
    return;
  }

  BROWSEINFO bi = {};
  bi.hwndOwner = hwnd;
  bi.lpszTitle = "Select a Folder";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  // Display the folder selection dialog
  LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
  if (pidl != NULL) {
    // Get the folder path from the item ID list
    char folderPath[MAX_PATH];
    if (SHGetPathFromIDList(pidl, folderPath)) {
      // Set the selected folder path to the edit control
      SetWindowText(hEdit, folderPath);
    }
    // Free the item ID list allocated by SHBrowseForFolder
    CoTaskMemFree(pidl);
  }
}

void DriverForm::SetValues(Section const& attributesMap) {
  dsn_name_ = attributesMap.count("DSN") > 0 ? attributesMap.at("DSN") : "";
  email_ = attributesMap.count("Email") > 0 ? attributesMap.at("Email") : "";
  o_auth_mechanism_ = attributesMap.count("OAuthMechanism") > 0
                          ? attributesMap.at("OAuthMechanism")
                          : "";
  key_file_path_ = attributesMap.count("KeyFilePath") > 0
                       ? attributesMap.at("KeyFilePath")
                       : "";
  catalog_ =
      attributesMap.count("Catalog") > 0 ? attributesMap.at("Catalog") : "";
  dataset_ =
      attributesMap.count("Dataset") > 0 ? attributesMap.at("Dataset") : "";
}

void LogTraceDialog::SetValues(Section const& attributesMap) {
  log_level_ =
      attributesMap.count("LogLevel") > 0 ? attributesMap.at("LogLevel") : "";
  log_file_path_ =
      attributesMap.count("LogPath") > 0 ? attributesMap.at("LogPath") : "";
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

void LogTraceDialog::InitControls() {
  HWND hLogLevelHead =
      CreateLabel(parent_hwnd, "Log Level:", 20, 50, 80, 20, 0);
  HWND hLogLevelBox =
      CreateComboBox(parent_hwnd, 120, 50, 250, 100, kIdclogTraceBox);

  HWND hLogFileAdd = CreateLabel(parent_hwnd, "Log Path:", 20, 80, 80, 20, 0);
  HWND hLogFileEdit =
      CreateEditBox(parent_hwnd, 120, 80, 250, 20, kIdcLogPathEdit);
  CreateButton(parent_hwnd, "Browse", 220, 120, 100, 20, kIdcLogBrowseBtn);

  HWND hLogBtnOk =
      CreateButton(parent_hwnd, "Ok", 120, 180, 80, 30, kIdcLogBtnOk);

  HWND hLogBtnCancel =
      CreateButton(parent_hwnd, "Cancel", 200, 180, 80, 30, kIdcLogBtnCancel);
  // Populate dropdowns
  SendMessage(hLogLevelBox, CB_ADDSTRING, 0, (LPARAM) "LOG_OFF");
  SendMessage(hLogLevelBox, CB_ADDSTRING, 0, (LPARAM) "LOG_TRACE");
  SendMessage(hLogLevelBox, CB_SETCURSEL, 0, 0);

  SetWindowText(hLogLevelBox, log_level_.c_str());
  SetWindowText(hLogFileEdit, log_file_path_.c_str());
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
  HWND hComboBox = CreateComboBox(m_hwnd, 140, 120, 150, 100, kIdcComboBox);

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

  HWND hLoggingButton =
      CreateButton(m_hwnd, "Logging Options", 20, 380, 120, 30, kIdcLoggingBtn);

  HWND hwndOkButton =
      CreateButton(m_hwnd, "Ok", 220, 400, 80, 30, kIdcButtonOk);
  HWND hwndCancelButton =
      CreateButton(m_hwnd, "Cancel", 320, 400, 80, 30, kIdcButtonCancel);

  // Populate dropdowns
  SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM) "For Current User");
  SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM) "For All Users");
  SendMessage(hComboBox, CB_SETCURSEL, 0, 0);

  SendMessage(hCatalogBox, CB_ADDSTRING, 0, (LPARAM) "Project 1");
  SendMessage(hCatalogBox, CB_ADDSTRING, 0, (LPARAM) "Project 2");
  SendMessage(hCatalogBox, CB_SETCURSEL, 0, 0);

  SendMessage(hDatasetBox, CB_ADDSTRING, 0, (LPARAM) "Dataset 1");
  SendMessage(hDatasetBox, CB_ADDSTRING, 0, (LPARAM) "Dataset 2");
  SendMessage(hDatasetBox, CB_SETCURSEL, 0, 0);

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
  SetControlFont(hLoggingButton, hFont);

  SetWindowText(hEmailEdit, email_.c_str());
  SetWindowText(hKeyFileEdit, key_file_path_.c_str());
  SetWindowText(hCatalogBox, catalog_.c_str());
  SetWindowText(hDatasetBox, dataset_.c_str());
  SetWindowText(hComboBox, o_auth_mechanism_.c_str());
}

void LogTraceDialog::Show(HWND hwnd) {
  if (parent_hwnd) {
    ShowWindow(parent_hwnd, SW_SHOW);
    SetForegroundWindow(parent_hwnd);
    return;
  }

  WNDCLASS wcLogging = {};
  wcLogging.lpfnWndProc = LogTraceDialog::LogTraceProc;
  wcLogging.hInstance = GetModuleHandle(NULL);
  wcLogging.lpszClassName = CLASS_NAME;

  RegisterClass(&wcLogging);

  parent_hwnd = CreateWindowEx(
      0, CLASS_NAME, "Logging Options", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
      CW_USEDEFAULT, 450, 300, hwnd, NULL, GetModuleHandle(NULL), this);

  if (parent_hwnd) {
    InitControls();
  }
  ShowWindow(parent_hwnd, SW_SHOW);
  UpdateWindow(parent_hwnd);
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

bool IsValidEmail(std::string const& email) {
  std::regex const pattern(R"((\w+)(\.|\-)?(\w*)@(\w+)(\.\w+)+)");
  return std::regex_match(email, pattern);
}

LRESULT CALLBACK LogTraceDialog::LogTraceProc(HWND hwnd, UINT uMsg,
                                              WPARAM wParam, LPARAM lParam) {
  LogTraceDialog* pThis = NULL;
  if (uMsg == WM_NCCREATE) {
    CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
    pThis = (LogTraceDialog*)pCreate->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
  } else {
    pThis = (LogTraceDialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }
  switch (uMsg) {
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case kIdcLogBrowseBtn: {
          HWND hEdit = GetDlgItem(hwnd, kIdcLogPathEdit);
          OpenFolderDialog(hwnd, hEdit);
          break;
        }

        case kIdcLogBtnOk: {
          HWND hLogtrace = GetDlgItem(hwnd, kIdclogTraceBox);
          char LogTraceBuf[256];
          GetWindowText(hLogtrace, LogTraceBuf, sizeof(LogTraceBuf));
          log_level_ = LogTraceBuf;

          HWND hLogFilePath = GetDlgItem(hwnd, kIdcLogPathEdit);
          char LogFilePathBuf[256];
          GetWindowText(hLogFilePath, LogFilePathBuf, sizeof(LogFilePathBuf));
          log_file_path_ = LogFilePathBuf;
          DestroyWindow(hwnd);  // Close the window

          break;
        }
        case kIdcLogBtnCancel:
          DestroyWindow(hwnd);  // Close the window

          break;
      }
      break;

    case WM_CLOSE:
      DestroyWindow(hwnd);  // Close the window

      return 0;

    case WM_DESTROY:
      if (pThis) {
        pThis->parent_hwnd = NULL;  // Set the window handle to NULL
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK DriverForm::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                        LPARAM lParam) {
  DriverForm* pThis =
      reinterpret_cast<DriverForm*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  auto logForm = new LogTraceDialog;

  switch (uMsg) {
    case WM_CREATE:
      // Set the instance pointer in the window's user data
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
      break;

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case kIdcBrowseButton: {
          HWND hEdit = GetDlgItem(hwnd, kIdcKeyfileEdit);
          OpenFileDialog(hwnd, hEdit);
        } break;

        case kIdcLoggingBtn: {
          if (logForm) {
            logForm->Show(hwnd);
            MSG msg = {};
            while (GetMessage(&msg, NULL, 0, 0)) {
              TranslateMessage(&msg);
              DispatchMessage(&msg);
            }
          }
          break;
        }
        case kIdcButtonOk: {
          HWND hDSN = GetDlgItem(hwnd, kIdcDSNEdit);
          char dsnBuffer[256];
          GetWindowText(hDSN, dsnBuffer, sizeof(dsnBuffer));
          dsn_name_ = dsnBuffer;

          HWND hEmail = GetDlgItem(hwnd, kIdcEmailEdit);
          char emailBuffer[256];
          GetWindowText(hEmail, emailBuffer, sizeof(emailBuffer));
          email_ = emailBuffer;
          if (!IsValidEmail(email_) && !email_.empty()) {
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
