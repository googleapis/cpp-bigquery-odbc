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

namespace google::cloud::odbc_bq_driver_internal {

char const ProxyOptions::CLASS_NAME[] = "ProxyOptClass";
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
int const kBtnWidth = 80;
int const kBtnHeight = 30;
int const kEditBoxWidth = 240;
int const kEditBoxHeight = 20;
int const kLabelWidth = 180;
int const kLabelHeight = 20;
int const kCheckboxWidth = 150;
int const kCheckboxHeight = 20;
int const kControlSpacing = 40;
int const kControlStartX = 20;
int const kEditBoxStartX = 150;
int const kOkButtonX = 180;
int const kCancelButtonX = 280;
int const kButtonY = 220;

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

  HWND h_proxy_checkbox = CreateCheckBox(
      proxy_hwnd, "Use Proxy Server", kControlStartX, kControlStartX,
      kCheckboxWidth, kCheckboxHeight, kIdcProxyCheckbox);
  CheckDlgButton(proxy_hwnd, kIdcProxyCheckbox,
                 (proxy_check_ == "1") ? BST_CHECKED : BST_UNCHECKED);

  HWND h_proxy_host_label =
      CreateLabel(proxy_hwnd, "Proxy Host:", kControlStartX, kControlSpacing,
                  kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_host_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing, kEditBoxWidth,
                    kEditBoxHeight, kIdcProxyHostName);
  SetWindowText(h_proxy_host_edit, proxy_host_.c_str());

  HWND h_proxy_port_label = CreateLabel(
      proxy_hwnd, "Proxy Port:", kControlStartX, kControlSpacing * 2,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_port_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 2,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyPortEdit);
  SetWindowLong(h_proxy_port_edit, GWL_STYLE,
                GetWindowLong(h_proxy_port_edit, GWL_STYLE) | ES_NUMBER);
  SetWindowText(h_proxy_port_edit, proxy_port_.c_str());

  HWND h_proxy_username_label = CreateLabel(
      proxy_hwnd, "Proxy Username:", kControlStartX, kControlSpacing * 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_username_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyUsernameEdit);
  SetWindowText(h_proxy_username_edit, proxy_username_.c_str());

  HWND h_proxy_password_label = CreateLabel(
      proxy_hwnd, "Proxy Password:", kControlStartX, kControlSpacing * 4,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_password_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 4,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyPasswordEdit);
  SetWindowLong(h_proxy_password_edit, GWL_STYLE,
                GetWindowLong(h_proxy_password_edit, GWL_STYLE) | WS_TABSTOP);
  SendMessage(h_proxy_password_edit, EM_SETPASSWORDCHAR, (WPARAM)'*', 0);
  SetWindowText(h_proxy_password_edit, proxy_pwd_enc_.c_str());

  HWND h_ok_button = CreateButton(proxy_hwnd, "OK", kOkButtonX, kButtonY,
                                  kBtnWidth, kBtnHeight, kIdcProxyOKButton);

  HWND h_cancel_button =
      CreateButton(proxy_hwnd, "Cancel", kCancelButtonX, kButtonY, kBtnWidth,
                   kBtnHeight, kIdcProxyCancelButton);

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
  wc_proxy.hbrBackground =
      (HBRUSH)(COLOR_WINDOW + 1);  // Sets background to white

  RegisterClass(&wc_proxy);

  int window_width = 520;
  int window_height = 650;
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  proxy_hwnd =
      CreateWindowEx(0, CLASS_NAME, "Proxy Options", WS_OVERLAPPEDWINDOW, x_pos,
                     y_pos, 470, 400, NULL, NULL, GetModuleHandle(NULL), this);

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
    case WM_COMMAND: {
      int wm_id = LOWORD(w_param);
      int wm_event = HIWORD(w_param);

      if (wm_id == kIdcProxyCheckbox && wm_event == BN_CLICKED) {
        BOOL is_checked =
            IsDlgButtonChecked(hwnd, kIdcProxyCheckbox) == BST_CHECKED;

        EnableWindow(GetDlgItem(hwnd, kIdcProxyHostName), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyPasswordEdit), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyPortEdit), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyUsernameEdit), is_checked);
        if (is_checked) {
          EnableWindow(GetDlgItem(hwnd, kIdcProxyOKButton), FALSE);
        }
        if (!is_checked) {
          // Reset fields when checkbox is unchecked
          EnableWindow(GetDlgItem(hwnd, kIdcProxyOKButton), TRUE);
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

        GetControlText(hwnd, kIdcProxyPortEdit, proxy_port_);
        GetControlText(hwnd, kIdcProxyHostName, proxy_host_);
        GetControlText(hwnd, kIdcProxyUsernameEdit, proxy_username_);
        GetControlText(hwnd, kIdcProxyPasswordEdit, proxy_pwd_enc_);

        DestroyWindow(hwnd);

      } else if (wm_id == kIdcProxyCancelButton) {
        DestroyWindow(hwnd);

      } else if ((wm_id == kIdcProxyHostName || wm_id == kIdcProxyPortEdit) &&
                 HIWORD(w_param) == EN_CHANGE) {
        // Check if both fields are filled then only enable OK button
        char host_text[256], port_text[10];
        GetWindowText(GetDlgItem(hwnd, kIdcProxyHostName), host_text,
                      sizeof(host_text));
        GetWindowText(GetDlgItem(hwnd, kIdcProxyPortEdit), port_text,
                      sizeof(port_text));

        HWND h_ok_button = GetDlgItem(hwnd, kIdcProxyOKButton);

        if (strlen(host_text) > 0 && strlen(port_text) > 0) {
          EnableWindow(h_ok_button, TRUE);
        } else {
          EnableWindow(h_ok_button, FALSE);
        }
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

}  // namespace google::cloud::odbc_bq_driver_internal
