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

namespace google::cloud::odbc_bq_driver_internal {

char const DriverForm::CLASS_NAME[] = "DriverFormClass";

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

// Function to create a font
HFONT CreateCustomFont(int fontSize) {
  LOGFONT logFont = {};
  HFONT hFont = NULL;
  logFont.lfHeight = -MulDiv(fontSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY),
                             72);        // Negative height for screen fonts
  lstrcpy(logFont.lfFaceName, "Arial");  // Font face name

  hFont = CreateFontIndirect(&logFont);
  return hFont;
}

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

  m_hwnd = CreateWindowEx(0, CLASS_NAME, "Driver Form", WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, 900, 800, NULL, NULL,
                          GetModuleHandle(NULL), this);

  if (m_hwnd) {
    // Set custom font for the controls
    HFONT hFont = CreateCustomFont(10);  // Font size 10

    // Create (Authentication header) before the dropdown
    CreateWindowEx(0, "STATIC",
                   "Authentication",  // Header text
                   WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 90, 120,
                   20,  // Position and size
                   m_hwnd, (HMENU)kIdcHeaderLabel, GetModuleHandle(NULL), NULL);

    // Create (header) and dropdown on the same line
    HWND hAuthHead = CreateWindowEx(
        0, "STATIC", "OAuth Mechanism:", WS_VISIBLE | WS_CHILD | SS_LEFT,
        // labelX, 90, labelWidth, 20,  // Position and size of the text
        20, 120, 120, 20, m_hwnd, (HMENU)kIdcLabel, GetModuleHandle(NULL),
        NULL);

    // create dropdown
    HWND hComboBox =
        CreateWindowEx(0, "COMBOBOX", NULL,
                       WS_TABSTOP | WS_VISIBLE | WS_CHILD |
                           CBS_DROPDOWN,  // Styles for a dropdown
                       // comboX, 90, comboWidth, 100,
                       140, 120, 150, 100, m_hwnd, (HMENU)kIdcComboBox,
                       GetModuleHandle(NULL), NULL);

    // Create Email Label
    HWND hEmailHeader = CreateWindowEx(
        0, "STATIC", "Email:", WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 160, 40, 20,
        m_hwnd, NULL, GetModuleHandle(NULL), NULL);

    // Create Email Edit Control
    HWND hEmailEdit = CreateWindowEx(
        0, "EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 100, 160,
        200, 20, m_hwnd, (HMENU)kIdcEmailEdit, GetModuleHandle(NULL), NULL);

    // Create Key File Path Label
    HWND hPathAdd = CreateWindowEx(
        0, "STATIC", "Key File Path:", WS_VISIBLE | WS_CHILD | SS_LEFT, 20, 200,
        100, 30, m_hwnd, NULL, GetModuleHandle(NULL), NULL);

    // Create Key File Path Edit Control
    HWND hKeyFileEdit = CreateWindowEx(
        0, "EDIT", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT, 120, 200,
        250, 20, m_hwnd, (HMENU)kIdcKeyfileEdit, GetModuleHandle(NULL), NULL);

    // Create Browse Button
    CreateWindowEx(0, "BUTTON", "Browse",
                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 150, 230,
                   100, 20, m_hwnd, (HMENU)kIdcBrowseButton,
                   GetModuleHandle(NULL), NULL);

    HWND hCatalogText = CreateWindowEx(
        0, "STATIC", "Catalog (Project):", WS_VISIBLE | WS_CHILD | SS_LEFT,
        // labelX, 90, labelWidth, 20,  // Position and size of the text
        20, 280, 110, 20, m_hwnd, (HMENU)kIdcCatalogLabel,
        GetModuleHandle(NULL), NULL);
    // create dropdown
    HWND hCatlogBox =
        CreateWindowEx(0, "COMBOBOX", NULL,
                       WS_TABSTOP | WS_VISIBLE | WS_CHILD |
                           CBS_DROPDOWN,  // Styles for a dropdown
                       // comboX, 90, comboWidth, 100,
                       160, 280, 230, 100, m_hwnd, (HMENU)kIdcCatlogBOX,
                       GetModuleHandle(NULL), NULL);

    HWND hDatasetText = CreateWindowEx(
        0, "STATIC", "Dataset:", WS_VISIBLE | WS_CHILD | SS_LEFT,
        // labelX, 90, labelWidth, 20,  // Position and size of the text
        20, 320, 50, 20, m_hwnd, (HMENU)kIdcDatasetLabel, GetModuleHandle(NULL),
        NULL);
    // create dropdown
    HWND hDatasetBox =
        CreateWindowEx(0, "COMBOBOX", NULL,
                       WS_TABSTOP | WS_VISIBLE | WS_CHILD |
                           CBS_DROPDOWN,  // Styles for a dropdown
                       // comboX, 90, comboWidth, 100,
                       160, 320, 230, 100, m_hwnd, (HMENU)kIdcDatasetBOX,
                       GetModuleHandle(NULL), NULL);

    // For OAuthMechanism dropdown values
    SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM) "For Current User");
    SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM) "For All Users");
    SendMessage(hComboBox, CB_SETCURSEL, 0, 0);

    // For Catalog dropdown values
    SendMessage(hCatlogBox, CB_ADDSTRING, 0, (LPARAM) "Project 1");
    SendMessage(hCatlogBox, CB_ADDSTRING, 0, (LPARAM) "Project 2");
    SendMessage(hCatlogBox, CB_SETCURSEL, 0, 0);

    // For Dataset dropdown values
    SendMessage(hDatasetBox, CB_ADDSTRING, 0, (LPARAM) "Dataset 1");
    SendMessage(hDatasetBox, CB_ADDSTRING, 0, (LPARAM) "Dataset 2");
    SendMessage(hDatasetBox, CB_SETCURSEL, 0, 0);

    // Apply font to controls
    // SendMessage(hAuthHead, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hAuthHead, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hEmailEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hEmailHeader, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hPathAdd, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hKeyFileEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hCatalogText, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hDatasetText, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(GetDlgItem(m_hwnd, kIdcBrowseButton), WM_SETFONT, (WPARAM)hFont,
                TRUE);

    // Create and position OK and Cancel buttons at the bottom
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);

    int buttonWidth = 100;
    int buttonHeight = 30;
    int buttonY = rcClient.bottom - buttonHeight -
                  20;        // Position 20 pixels from the bottom
    int buttonSpacing = 20;  // Space between buttons

    // Create an OK button
    CreateWindowEx(0, "BUTTON", "OK",
                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 20,
                   buttonY, buttonWidth, buttonHeight, m_hwnd,
                   (HMENU)kIdcButtonOk, GetModuleHandle(NULL), NULL);

    // Create a Cancel button
    CreateWindowEx(0, "BUTTON", "Cancel",
                   WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                   20 + buttonWidth + buttonSpacing, buttonY, buttonWidth,
                   buttonHeight, m_hwnd, (HMENU)kIdcButtonCancel,
                   GetModuleHandle(NULL), NULL);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
  }
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
        case kIdcButtonOk:

          DestroyWindow(hwnd);  // Close the window
          break;
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
