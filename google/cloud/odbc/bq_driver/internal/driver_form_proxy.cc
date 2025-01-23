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

namespace google::cloud::odbc_bq_driver_internal {

char const ProxyOptions::CLASS_NAME[] = "ProxyOptClass";

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

  HWND h_proxy_host_label =
      CreateLabel(proxy_hwnd, "Proxy Host:", kControlStartX, kControlSpacing,
                  kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_host_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing, kEditBoxWidth,
                    kEditBoxHeight, kIdcProxyHostName);
  EnableWindow(h_proxy_host_edit, FALSE);

  HWND h_proxy_port_label = CreateLabel(
      proxy_hwnd, "Proxy Port:", kControlStartX, kControlSpacing * 2,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_port_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 2,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyPortEdit);
  EnableWindow(h_proxy_port_edit, FALSE);

  HWND h_proxy_username_label = CreateLabel(
      proxy_hwnd, "Proxy Username:", kControlStartX, kControlSpacing * 3,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_username_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 3,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyUsernameEdit);
  EnableWindow(h_proxy_username_edit, FALSE);

  HWND h_proxy_password_label = CreateLabel(
      proxy_hwnd, "Proxy Password:", kControlStartX, kControlSpacing * 4,
      kLabelWidth, kLabelHeight, WS_VISIBLE | SS_LEFT);

  HWND h_proxy_password_edit =
      CreateEditBox(proxy_hwnd, kEditBoxStartX, kControlSpacing * 4,
                    kEditBoxWidth, kEditBoxHeight, kIdcProxyPasswordEdit);
  EnableWindow(h_proxy_password_edit, FALSE);

  HWND h_ok_button = CreateButton(proxy_hwnd, "OK", kOkButtonX, kButtonY,
                                  kBtnWidth, kBtnHeight, kIdcProxyOKButton);

  HWND h_cancel_button =
      CreateButton(proxy_hwnd, "Cancel", kCancelButtonX, kButtonY, kBtnWidth,
                   kBtnHeight, kIdcProxyCancelButton);
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

LRESULT CALLBACK ProxyOptions::ProxyOptProc(HWND hwnd, UINT msg, WPARAM w_param,
                                            LPARAM l_param) {
  switch (msg) {
    case WM_COMMAND: {
      int wm_id = LOWORD(w_param);
      if (wm_id == kIdcProxyCheckbox && HIWORD(w_param) == BN_CLICKED) {
        BOOL is_checked =
            IsDlgButtonChecked(hwnd, kIdcProxyCheckbox) == BST_CHECKED;
        EnableWindow(GetDlgItem(hwnd, kIdcProxyHostName), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyPasswordEdit), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyPortEdit), is_checked);
        EnableWindow(GetDlgItem(hwnd, kIdcProxyUsernameEdit), is_checked);
      } else if (wm_id == kIdcProxyOKButton) {
        DestroyWindow(hwnd);
      } else if (wm_id == kIdcProxyCancelButton) {
        DestroyWindow(hwnd);
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
