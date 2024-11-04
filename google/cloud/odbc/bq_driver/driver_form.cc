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
#include "google/cloud/odbc/bq_driver/driver_form.h"
#include "google/cloud/odbc/bq_driver/odbc_connection.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::odbc_bq_driver::TestODBCConnectionAd;
char const DriverForm::CLASS_NAME[] = "DriverFormClass";
std::string DriverForm::kDsnName;
std::string DriverForm::kEmail;
std::string DriverForm::kKeyFilePath;
std::string DriverForm::kOAuthMechanism;
std::string DriverForm::kCatalog;
std::string DriverForm::kDataset;
bool DriverForm::kConnectionStatus;

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

std::string DriverForm::SetValues(Section const& attributesMap) {
  kDsnName = attributesMap.count("DSN") > 0 ? attributesMap.at("DSN") : "";
  kEmail = attributesMap.count("Email") > 0 ? attributesMap.at("Email") : "";
  kOAuthMechanism = attributesMap.count("OAuthMechanism") > 0
                        ? attributesMap.at("OAuthMechanism")
                        : "";
  kKeyFilePath = attributesMap.count("KeyFilePath") > 0
                     ? attributesMap.at("KeyFilePath")
                     : "";
  kCatalog =
      attributesMap.count("Catalog") > 0 ? attributesMap.at("Catalog") : "";
  kDataset =
      attributesMap.count("Dataset") > 0 ? attributesMap.at("Dataset") : "";

  return "Success";
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
  SetWindowText(hDSNnameEdit, kDsnName.c_str());
  if (!kDsnName.empty()) {
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

   HWND hwndTestButton =
      CreateButton(m_hwnd, "Test...", 120, 400, 80, 30, kIdcButtonTest);

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

  SetWindowText(hEmailEdit, kEmail.c_str());
  SetWindowText(hKeyFileEdit, kKeyFilePath.c_str());
  SetWindowText(hCatalogBox, kCatalog.c_str());
  SetWindowText(hDatasetBox, kDataset.c_str());
  SetWindowText(hComboBox, kOAuthMechanism.c_str());
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
      0, CLASS_NAME,
      "Google ODBC Driver for Google Bigquery DSN Setup",
      WS_OVERLAPPEDWINDOW, xPos, yPos, windowWidth, windowHeight,
      NULL, NULL, GetModuleHandle(NULL), this);

  if (m_hwnd) {
    CreateWindowEx(0, "STATIC",
                   "DSN configure",  // Header text
                   WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 50, 200,
                   20,  // Position and size
                   m_hwnd, (HMENU)kIdcHeaderLabel, GetModuleHandle(NULL), NULL);
    InitControls();

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

void DriverForm::CaptureValues(HWND hwnd) {
    // Retrieve DSN
    HWND hDSN = GetDlgItem(hwnd, kIdcDSNEdit);
    char dsnBuffer[256];
    GetWindowText(hDSN, dsnBuffer, sizeof(dsnBuffer));
    kDsnName = dsnBuffer;

    // Retrieve Email
    HWND hEmail = GetDlgItem(hwnd, kIdcEmailEdit);
    char emailBuffer[256];
    GetWindowText(hEmail, emailBuffer, sizeof(emailBuffer));
    kEmail = emailBuffer;

    //Validate Email
    if (!IsValidEmail(kEmail) && !kEmail.empty()) {
        MessageBox(hwnd, "Invalid email address!", "Error", MB_OK | MB_ICONERROR);
        return ; // Exit if email is invalid
    }

    // Retrieve Key File Path
    HWND hKey = GetDlgItem(hwnd, kIdcKeyfileEdit);
    char keyBuffer[256];
    GetWindowText(hKey, keyBuffer, sizeof(keyBuffer));
    kKeyFilePath = keyBuffer;

    // Retrieve OAuth Mechanism
    HWND hComboBox = GetDlgItem(hwnd, kIdcComboBox);
    char authBuffer[256];
    GetWindowText(hComboBox, authBuffer, sizeof(authBuffer));
    kOAuthMechanism = authBuffer;

    // Retrieve Catalog Name
    HWND hCatalogBox = GetDlgItem(hwnd, kIdcCatlogBOX);
    char catalogBuffer[256];
    GetWindowText(hCatalogBox, catalogBuffer, sizeof(catalogBuffer));
    kCatalog = catalogBuffer;

    // Retrieve Dataset Name
    HWND hDatasetBox = GetDlgItem(hwnd, kIdcDatasetBOX);
    char dataBuffer[256];
    GetWindowText(hDatasetBox, dataBuffer, sizeof(dataBuffer));
    kDataset = dataBuffer;
    
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

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case kIdcBrowseButton: {
          HWND hEdit = GetDlgItem(hwnd, kIdcKeyfileEdit);
          OpenFileDialog(hwnd, hEdit);
        } break;
        case kIdcButtonOk: {

          CaptureValues(hwnd);
          DestroyWindow(hwnd);  // Close the window
          break;
        }
        case kIdcButtonTest:{
          CaptureValues(hwnd);
          Section attributesMap;
          attributesMap["DSN"] = kDsnName;
          attributesMap["Email"] = kEmail;
          attributesMap["KeyFilePath"] = kKeyFilePath;
          attributesMap["OAuthMechanism"] = kOAuthMechanism;
          attributesMap["Catalog"] = kCatalog;
          attributesMap["Dataset"] = kDataset;

          bool status =TestODBCConnectionAd(std::make_shared<Section>(attributesMap));
          if(status==true){
            std::string messageText = 
        "SUCCESS!\n\nSuccessfully connected to data source!\n\n"
        "ODBC Version: 03.80\n"
        "Driver Version: 3.0.5.1011\n"
        "Bitness: 64-bit\n"
        "Locale: en_IN\n\n";
         MessageBox(hwnd,messageText.c_str(), "Test Results", MB_OK | MB_ICONINFORMATION| MB_TOPMOST );
         return 0;
            } else {
                MessageBox(hwnd, "Connection Failed!", "Error", MB_OK | MB_ICONERROR);
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
