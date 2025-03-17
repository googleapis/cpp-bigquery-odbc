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
#include "google/cloud/odbc/bq_driver/internal/driver_adv_opt_form.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#ifdef _WIN32
#include <shellapi.h>
#endif

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
      CreateFont(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                 DEFAULT_PITCH | FF_SWISS, "Inter");

  HWND h_proxy_checkbox = CreateCheckBox(
      proxy_hwnd, "Use proxy server", kControlStartX, kLabelHeight - 10,
      kCheckboxWidth, kCheckboxHeight, kIdcProxyCheckbox);
  SendMessage(h_proxy_checkbox, WM_SETFONT, (WPARAM)h_font, TRUE);
  HWND h_proxy_host_label = CreateLabel(
      proxy_hwnd, "Proxy host:", kControlStartX, kControlSpacing + 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_proxy_host_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_proxy_host_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing + 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyHostName);
  SendMessage(h_proxy_host_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  EnableWindow(h_proxy_host_edit, FALSE);

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
  EnableWindow(h_proxy_port_edit, FALSE);

  HWND h_proxy_username_label = CreateLabel(
      proxy_hwnd, "Proxy username:", kControlStartX, kControlSpacing * 2.2 + 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);
  SendMessage(h_proxy_username_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_proxy_username_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 2.2 + 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyUsernameEdit);
  SendMessage(h_proxy_username_edit, WM_SETFONT, (WPARAM)h_font, TRUE);

  EnableWindow(h_proxy_username_edit, FALSE);

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
  EnableWindow(h_proxy_password_edit, FALSE);

  // Documentation Hyperlink
  HWND h_doc_text =
      CreateLabel(proxy_hwnd, "Not sure what to enter? See", kLabelHeight - 8,
                  kButtonY, kLabelWidth - 20, kLabelHeight, 0);
  SendMessage(h_doc_text, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_hyperlink = CreateWindowEx(
      0, "STATIC", "BigQuery documentation", WS_CHILD | WS_VISIBLE | SS_NOTIFY,
      kLabelWidth - 36, kButtonY, kLabelWidth - 30, kLabelHeight, proxy_hwnd,
      (HMENU)kIdcHyperlink, GetModuleHandle(NULL), NULL);
  SendMessage(h_hyperlink, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_ok_button = CreateButton(proxy_hwnd, "OK", kOkButtonX, kButtonY,
                                  kBtnWidth, kBtnHeight, kIdcProxyOKButton);
  LONG style = GetWindowLong(h_ok_button, GWL_STYLE);
  // SetWindowLong(h_ok_button, GWL_STYLE, style | BS_DEFPUSHBUTTON);
  SendMessage(h_ok_button, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_cancel_button =
      CreateButton(proxy_hwnd, "Cancel", kCancelButtonX, kButtonY, kBtnWidth,
                   kBtnHeight, kIdcProxyCancelButton);
  SendMessage(h_cancel_button, WM_SETFONT, (WPARAM)h_font, TRUE);
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
    case WM_ERASEBKGND: {
      HDC hdc = (HDC)w_param;
      RECT rc;
      GetClientRect(hwnd, &rc);

      // Define a custom background color (#F0F0F0)
      HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240));

      FillRect(hdc, &rc, hBrush);
      DeleteObject(hBrush);
      return 1;  // Indicate we handled the background redraw
    }

    case WM_LBUTTONDOWN: {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);

      HWND h_hyperlink = GetDlgItem(hwnd, kIdcHyperlink);
      RECT rect;
      GetClientRect(h_hyperlink, &rect);
      MapWindowPoints(h_hyperlink, hwnd, (LPPOINT)&rect, 2);

      if (PtInRect(&rect, pt)) {
        ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL, SW_SHOWNORMAL);
      }
      break;
    }
    case WM_CTLCOLORSTATIC: {
      HDC hdcStatic = (HDC)w_param;
      HWND hwndStatic = (HWND)l_param;

      if (GetDlgCtrlID(hwndStatic) == kIdcHyperlink) {
        SetTextColor(hdcStatic, RGB(0, 0, 255));  // Set text color to blue
        SetBkMode(hdcStatic, TRANSPARENT);        // Transparent background

        static HFONT hFontUnderline = NULL;
        if (!hFontUnderline) {
          LOGFONT lf;
          HFONT hFont = (HFONT)SendMessage(hwndStatic, WM_GETFONT, 0, 0);
          if (hFont && GetObject(hFont, sizeof(LOGFONT), &lf)) {
            lf.lfUnderline = TRUE;  // Enable underline
            hFontUnderline = CreateFontIndirect(&lf);
          }
        }

        if (hFontUnderline) {
          SelectObject(hdcStatic, hFontUnderline);
        }

        return (LRESULT)GetSysColorBrush(COLOR_3DFACE);
      }
      break;
    }

    case WM_COMMAND: {
      int wm_id = LOWORD(w_param);

      if (wm_id == kIdcHyperlink && HIWORD(w_param) == STN_CLICKED) {
        ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL, SW_SHOWNORMAL);
        break;
      }

      if (wm_id == kIdcProxyCheckbox && HIWORD(w_param) == BN_CLICKED) {
        BOOL is_checked =
            IsDlgButtonChecked(hwnd, kIdcProxyCheckbox) == BST_CHECKED;

        HWND h_ok_button = GetDlgItem(hwnd, kIdcProxyOKButton);
        HWND h_host_edit = GetDlgItem(hwnd, kIdcProxyHostName);
        HWND h_port_edit = GetDlgItem(hwnd, kIdcProxyPortEdit);
        HWND h_username_edit = GetDlgItem(hwnd, kIdcProxyUsernameEdit);
        HWND h_password_edit = GetDlgItem(hwnd, kIdcProxyPasswordEdit);

        // Enable/disable input fields
        EnableWindow(h_host_edit, is_checked);
        EnableWindow(h_port_edit, is_checked);
        EnableWindow(h_username_edit, is_checked);
        EnableWindow(h_password_edit, is_checked);

        if (is_checked) {
          // Disable OK button and remove blue border
          EnableWindow(h_ok_button, FALSE);
          LONG style = GetWindowLong(h_ok_button, GWL_STYLE);
          SetWindowLong(h_ok_button, GWL_STYLE, style & ~BS_DEFPUSHBUTTON);
        } else {
          // Enable OK button with default push button style
          EnableWindow(h_ok_button, TRUE);
          LONG style = GetWindowLong(h_ok_button, GWL_STYLE);
          SetWindowLong(h_ok_button, GWL_STYLE, style | BS_DEFPUSHBUTTON);
        }
      } else if ((wm_id == kIdcProxyHostName || wm_id == kIdcProxyPortEdit) &&
                 HIWORD(w_param) == EN_CHANGE) {
        // Check if both fields are filled
        char host_text[256], port_text[10];
        GetWindowText(GetDlgItem(hwnd, kIdcProxyHostName), host_text,
                      sizeof(host_text));
        GetWindowText(GetDlgItem(hwnd, kIdcProxyPortEdit), port_text,
                      sizeof(port_text));

        HWND h_ok_button = GetDlgItem(hwnd, kIdcProxyOKButton);

        if (strlen(host_text) > 0 && strlen(port_text) > 0) {
          // Enable OK button and add blue border
          EnableWindow(h_ok_button, TRUE);
          LONG style = GetWindowLong(h_ok_button, GWL_STYLE);
          SetWindowLong(h_ok_button, GWL_STYLE, style | BS_DEFPUSHBUTTON);
        } else {
          // Disable OK button and remove blue border
          EnableWindow(h_ok_button, FALSE);
          LONG style = GetWindowLong(h_ok_button, GWL_STYLE);
          SetWindowLong(h_ok_button, GWL_STYLE, style & ~BS_DEFPUSHBUTTON);
        }
      } else if (wm_id == kIdcProxyOKButton) {
        DestroyWindow(hwnd);
      } else if (wm_id == kIdcProxyCancelButton) {
        DestroyWindow(hwnd);
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
