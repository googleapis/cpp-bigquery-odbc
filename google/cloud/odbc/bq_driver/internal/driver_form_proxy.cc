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

#include "google/cloud/odbc/bq_driver/internal/driver_form_proxy.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include <commctrl.h>  // Required for SetWindowSubclass
#include <shellapi.h>
#pragma comment(lib, "Comctl32.lib")  // Link with Comctl32.lib

namespace google::cloud::odbc_bq_driver_internal {

char const ProxyOptions::CLASS_NAME[] = "ProxyOptClass";
constexpr char kBigQueryDocsURL[] =
    "https://cloud.google.com/bigquery/docs/reference/odbc-jdbc-drivers?hl=en";

// Default proxy checkbox value
std::string ProxyOptions::proxy_check_ = "0";
std::string ProxyOptions::proxy_host_;
std::string ProxyOptions::proxy_username_;
std::string ProxyOptions::proxy_port_;
std::string ProxyOptions::proxy_pwd_enc_;

std::string const kProxyCheck = "ProxyEnable";
std::string const kProxyPort = "ProxyPort";
std::string const kProxyHost = "ProxyHost";
std::string const kProxyUsername = "ProxyUid";
std::string const kProxyPassEncryption = "ProxyPwd_Enc";

// Control dimensions and positions
int const kBtnWidth = 68;
int const kBtnHeight = 17;
int const kEditBoxWidth = 203;
int const kEditBoxHeight = 17;
int const kLabelWidth = 180;
int const kLabelHeight = 20;
int const kCheckboxWidth = 150;
int const kCheckboxHeight = 20;
int const kControlSpacing = 40;
int const kControlStartX = 15;
int const kEditBoxStartX = 230;
int const kOkButtonX = 285;
int const kCancelButtonX = 365;
int const kButtonY = 148;
// max port number
int const kMaxPortNumber = 65536;

HWND ProxyOptions::GetHwnd() const { return proxy_hwnd; }
ProxyOptions::ProxyOptions() : proxy_hwnd(NULL) {}
ProxyOptions::~ProxyOptions() {
  if (proxy_hwnd) {
    DestroyWindow(proxy_hwnd);
  }
  UnregisterClass(CLASS_NAME, GetModuleHandle(NULL));
}
void SetWindowIcon(HWND proxy_hwnd) {
  // Get the environment variable
  char repo_path[MAX_PATH];
  DWORD result = GetEnvironmentVariableA("CPP_BIGQUERY_ODBC_REPO_PATH",
                                         repo_path, MAX_PATH);
std::cout << "CPP_BIGQUERY_ODBC_REPO_PATH: " << repo_path << std::endl;                                   

  // Construct the full icon path
  std::string icon_path =
      std::string(repo_path) + "\\ci\\installer\\InstallerProj\\Assets\\bq.ico";
  // Load the icon
  HICON h_icon = (HICON)LoadImageA(NULL, icon_path.c_str(), IMAGE_ICON, 32, 32,
                                   LR_LOADFROMFILE);
  if (!h_icon) {
    MessageBoxA(NULL, "Failed to load icon", "Error", MB_OK | MB_ICONERROR);
    return;
  }
  // Set the icon for the window
  SendMessage(proxy_hwnd, WM_SETICON, ICON_BIG, (LPARAM)h_icon);
  SendMessage(proxy_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)h_icon);
}
void ProxyOptions::InitControls() {
  HFONT h_font =
      CreateFont(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                 DEFAULT_PITCH | FF_SWISS, "Inter");

  HWND h_proxy_checkbox = CreateCheckBox(
      proxy_hwnd, "Use proxy server", kControlStartX, kLabelHeight - 10,
      kCheckboxWidth, kCheckboxHeight, kIdcProxyCheckbox);
  SendMessage(h_proxy_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  CheckDlgButton(proxy_hwnd, kIdcProxyCheckbox,
                 (proxy_check_ == "1") ? BST_CHECKED : BST_UNCHECKED);

  HWND h_proxy_host_label = CreateLabel(
      proxy_hwnd, "Proxy host:", kControlStartX, kControlSpacing + 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_proxy_host_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_proxy_host_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing + 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyHostName);
  SendMessage(h_proxy_host_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_proxy_host_edit, proxy_host_.c_str());

  HWND h_proxy_port_label = CreateLabel(
      proxy_hwnd, "Proxy port:", kControlStartX, kControlSpacing * 1.6 + 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_proxy_port_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_proxy_port_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 1.6 + 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyPortEdit);
  SetWindowLong(h_proxy_port_edit, GWL_STYLE,
                GetWindowLong(h_proxy_port_edit, GWL_STYLE) | ES_NUMBER);
  SendMessage(h_proxy_port_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_proxy_port_edit, proxy_port_.c_str());

  HWND h_proxy_username_label = CreateLabel(
      proxy_hwnd, "Proxy username:", kControlStartX, kControlSpacing * 2.2 + 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_proxy_username_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_proxy_username_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 2.2 + 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyUsernameEdit);
  SendMessage(h_proxy_username_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_proxy_username_edit, proxy_username_.c_str());

  HWND h_proxy_password_label = CreateLabel(
      proxy_hwnd, "Proxy password:", kControlStartX, kControlSpacing * 2.8 + 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_proxy_password_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_proxy_password_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 2.8 + 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyPasswordEdit);
  SetWindowLong(h_proxy_password_edit, GWL_STYLE,
                GetWindowLong(h_proxy_password_edit, GWL_STYLE) | WS_TABSTOP);
  SendMessage(h_proxy_password_edit, EM_SETPASSWORDCHAR, (WPARAM)'*', 0);
  SendMessage(h_proxy_password_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_proxy_password_edit, proxy_pwd_enc_.c_str());

  // Documentation Hyperlink
  HWND h_doc_text =
      CreateLabel(proxy_hwnd, "Not sure what to enter? See", kLabelHeight - 8,
                  kButtonY, kLabelWidth - 20, kLabelHeight, 0);
  SendMessage(h_doc_text, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_hyperlink = CreateHyperlinkLabel(
      proxy_hwnd, "BigQuery documentation", kLabelWidth - 36, kButtonY,
      kLabelWidth - 30, kLabelHeight, kIdcHyperlink1);
  SendMessage(h_hyperlink, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_ok_button = CreateButton(proxy_hwnd, "OK", kOkButtonX, kButtonY,
                                  kBtnWidth, kBtnHeight, kIdcProxyOKButton);
  LONG style = GetWindowLong(h_ok_button, GWL_STYLE);
  SendMessage(h_ok_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_cancel_button =
      CreateButton(proxy_hwnd, "Cancel", kCancelButtonX, kButtonY, kBtnWidth,
                   kBtnHeight, kIdcProxyCancelButton);
  SendMessage(h_cancel_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  SetWindowSubclass(GetDlgItem(proxy_hwnd, kIdcProxyHostName),
                    InputSubclassProc, 0, 0);
  SetWindowSubclass(GetDlgItem(proxy_hwnd, kIdcProxyPortEdit),
                    InputSubclassProc, 0, 0);
  SetWindowSubclass(GetDlgItem(proxy_hwnd, kIdcProxyUsernameEdit),
                    InputSubclassProc, 0, 0);
  SetWindowSubclass(GetDlgItem(proxy_hwnd, kIdcProxyPasswordEdit),
                    InputSubclassProc, 0, 0);
  SetWindowSubclass(GetDlgItem(proxy_hwnd, kIdcProxyCheckbox),
                    CheckboxSubclassProc, 0, 0);
  if (proxy_check_ == "0") {
    EnableWindow(h_proxy_host_edit, FALSE);
    EnableWindow(h_proxy_port_edit, FALSE);
    EnableWindow(h_proxy_username_edit, FALSE);
    EnableWindow(h_proxy_password_edit, FALSE);
  }
}

void ProxyOptions::Show(HWND hwnd) {
  if (proxy_hwnd) {
    ShowWindow(proxy_hwnd, SW_SHOW);
    SetForegroundWindow(proxy_hwnd);
    return;
  }

  WNDCLASS wc_proxy = {};
  wc_proxy.lpfnWndProc = ProxyOptions::ProxyOptProc;
  wc_proxy.hInstance = GetModuleHandle(NULL);
  wc_proxy.lpszClassName = CLASS_NAME;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_STANDARD_CLASSES;  // Load standard control classes
  InitCommonControlsEx(&icc);

  RegisterClass(&wc_proxy);

  int window_width = 462;
  int window_height = 207;
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  proxy_hwnd = CreateWindowEx(WS_EX_CONTROLPARENT, CLASS_NAME, "Proxy options",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x_pos,
                              y_pos, window_width, window_height + 6, NULL,
                              NULL, GetModuleHandle(NULL), this);

  if (proxy_hwnd) {
    InitControls();
  }
  ShowWindow(proxy_hwnd, SW_SHOW);
  UpdateWindow(proxy_hwnd);
}

void GetControlText(HWND hwnd, int control_id, std::string& out_value) {
  char buffer[256] = {0};
  HWND h_control = GetDlgItem(hwnd, control_id);
  if (h_control) {
    GetWindowText(h_control, buffer, sizeof(buffer));
    out_value = buffer;
  }
}
void ProxyOptions::SetValues(Section const& attribute_map) {
  proxy_check_ = GetValueOrDefault(attribute_map, kProxyCheck);
  proxy_host_ = GetValueOrDefault(attribute_map, kProxyHost);
  proxy_port_ = GetValueOrDefault(attribute_map, kProxyPort);
  proxy_username_ = GetValueOrDefault(attribute_map, kProxyUsername);
  proxy_pwd_enc_ =
      DecryptPassword(GetValueOrDefault(attribute_map, kProxyPassEncryption));
}

LRESULT CALLBACK ProxyOptions::ProxyOptProc(HWND hwnd, UINT msg, WPARAM w_param,
                                            LPARAM l_param) {
  switch (msg) {
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

      HWND h_hyperlink = GetDlgItem(hwnd, kIdcHyperlink1);
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

      if (GetDlgCtrlID(hwnd_static) == kIdcHyperlink1) {
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
      int wm_event = HIWORD(w_param);

      if (wm_id == kIdcProxyCheckbox && wm_event == BN_CLICKED) {
        BOOL is_checked =
            (IsDlgButtonChecked(hwnd, kIdcProxyCheckbox) == BST_CHECKED);

        EnableWindow(GetDlgItem(hwnd, kIdcProxyHostName), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyPasswordEdit), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyPortEdit), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyUsernameEdit), is_checked);

        if (is_checked) {
          // OK button remains disabled initially when checkbox is checked
          EnableWindow(GetDlgItem(hwnd, kIdcProxyOKButton), FALSE);
        } else {
          // Enable OK button when checkbox is unchecked
          EnableWindow(GetDlgItem(hwnd, kIdcProxyOKButton), TRUE);

          // Reset fields when checkbox is unchecked
          SetWindowText(GetDlgItem(hwnd, kIdcProxyHostName), "");
          SetWindowText(GetDlgItem(hwnd, kIdcProxyPasswordEdit), "");
          SetWindowText(GetDlgItem(hwnd, kIdcProxyPortEdit), "0");
          SetWindowText(GetDlgItem(hwnd, kIdcProxyUsernameEdit), "");

          proxy_host_.clear();
          proxy_port_.clear();
          proxy_username_.clear();
          proxy_pwd_enc_.clear();
        }

      } else if (wm_id == kIdcProxyOKButton) {
        // Save values and close window
        proxy_check_ =
            (IsDlgButtonChecked(hwnd, kIdcProxyCheckbox) == BST_CHECKED) ? "1"
                                                                         : "0";
        std::string temp_port;
        GetControlText(hwnd, kIdcProxyPortEdit, temp_port);
        bool is_valid_port = false;
        if (!temp_port.empty()) {
          int port = atoi(temp_port.c_str());
          is_valid_port = (port >= 0 && port < kMaxPortNumber);
        }
        if (!is_valid_port) {
          std::string error_msg =
              "[Google][BigQuery] (1060) Invalid port: '" + temp_port +
              "'.\nValid values are in the range [0, 65535].";
          MessageBoxA(hwnd, error_msg.c_str(), "DSN Configuration Error",
                      MB_ICONWARNING | MB_OK);
          return 0;
        }
        proxy_port_ = temp_port;
        GetControlText(hwnd, kIdcProxyHostName, proxy_host_);
        GetControlText(hwnd, kIdcProxyUsernameEdit, proxy_username_);
        GetControlText(hwnd, kIdcProxyPasswordEdit, proxy_pwd_enc_);

        DestroyWindow(hwnd);

      } else if (wm_id == kIdcProxyCancelButton) {
        DestroyWindow(hwnd);

      } else if ((wm_id == kIdcProxyHostName || wm_id == kIdcProxyPortEdit) &&
                 HIWORD(w_param) == EN_CHANGE) {
        // Ensure the OK button is enabled when the checkbox is checked
        BOOL is_checked =
            (IsDlgButtonChecked(hwnd, kIdcProxyCheckbox) == BST_CHECKED);
        HWND h_ok_button = GetDlgItem(hwnd, kIdcProxyOKButton);

        if (is_checked) {
          // Only check the fields if the proxy checkbox is checked
          char host_text[256], port_text[10];
          GetWindowText(GetDlgItem(hwnd, kIdcProxyHostName), host_text,
                        sizeof(host_text));
          GetWindowText(GetDlgItem(hwnd, kIdcProxyPortEdit), port_text,
                        sizeof(port_text));
          // Enable OK button if both host and port are provided
          if (strlen(host_text) > 0 && strlen(port_text) > 0) {
            EnableWindow(h_ok_button, TRUE);
          } else {
            EnableWindow(h_ok_button, FALSE);
          }
        } else {
          // If the checkbox is unchecked, always enable OK button
          EnableWindow(h_ok_button, TRUE);
        }
      }
      // Insert hyperlink click handler
      else if (wm_id == kIdcHyperlink1 && wm_event == STN_CLICKED) {
        ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL, SW_SHOWNORMAL);
      }
      return 0;
    }
    case WM_KEYDOWN: {
      if (w_param == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 0;
      } else if (w_param == VK_RETURN) {
        // Simulate a button click on the OK button when Enter is pressed
        SendMessage(GetDlgItem(hwnd, kIdcProxyOKButton), BM_CLICK, 0, 0);
        return 0;
      } else if (w_param == VK_TAB) {
        BOOL shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        HWND next_control = GetNextDlgTabItem(hwnd, GetFocus(), shift_pressed);
        if (next_control) {
          SetFocus(next_control);
        }
        return 0;
      }
      break;
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

}  // namespace google::cloud::odbc_bq_driver_internal
