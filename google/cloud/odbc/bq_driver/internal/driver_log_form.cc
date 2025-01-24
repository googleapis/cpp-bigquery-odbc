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

#include "google/cloud/odbc/bq_driver/internal/driver_log_form.h"
#include <shlobj.h>

namespace google::cloud::odbc_bq_driver_internal {
char const LogTraceDialog::CLASS_NAME[] = "LoggingTraceClass";

std::string LogTraceDialog::log_level_;
std::string LogTraceDialog::log_file_path_;
std::string const kLogLevel = "LogLevel";
std::string const kLogFile = "LogFile";
std::string const kLogOff = "LOG_OFF";
std::string const kLogTrace = "LOG_TRACE";
int const kBtnWidth = 80;
int const kBtnHeight = 30;
int const kComboBoxWidth = 220;
int const KComboBoxHeight = 100;
int const kLabelHeight = 20;
int const kEditBoxWidth = 220;
int const kEditBoxHeight = 20;

HWND LogTraceDialog::GetHwnd() const { return parent_hwnd; }
LogTraceDialog::LogTraceDialog() : parent_hwnd(NULL) {}
LogTraceDialog::~LogTraceDialog() {
  if (parent_hwnd) {
    DestroyWindow(parent_hwnd);
  }
  UnregisterClass(CLASS_NAME, GetModuleHandle(NULL));
}

void OpenFolderDialog(HWND hwnd, HWND h_edit,
                      char const* mock_folder_path = nullptr) {
  if (mock_folder_path) {
    SetWindowText(h_edit, mock_folder_path);
    return;
  }
  BROWSEINFO bi = {};
  bi.hwndOwner = hwnd;
  bi.lpszTitle = "Select a Folder";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
  if (pidl != NULL) {
    char folder_path[MAX_PATH];
    if (SHGetPathFromIDList(pidl, folder_path)) {
      SetWindowText(h_edit, folder_path);
    }
    CoTaskMemFree(pidl);
  }
}

void LogTraceDialog::SetValues(Section const& attributes_map) {
  if (attributes_map.count(kLogLevel) > 0) {
    if (attributes_map.at(kLogLevel) == "0") {
      log_level_ = kLogOff.c_str();
    } else if (attributes_map.at(kLogLevel) == "6") {
      log_level_ = kLogTrace.c_str();
    }
  } else {
    log_level_ = "";
  }
  log_file_path_ =
      attributes_map.count(kLogFile) > 0 ? attributes_map.at(kLogFile) : "";
}

void LogTraceDialog::InitControls() {
  HWND h_log_level_head =
      CreateLabel(parent_hwnd, "Log Level:", 20, 50, 80, 20, 0);
  HWND h_log_level_box = CreateComboBox(parent_hwnd, 120, 50, kComboBoxWidth,
                                        KComboBoxHeight, kIdclogTraceBox);

  HWND h_log_file_add =
      CreateLabel(parent_hwnd, "Log File:", 20, 80, 80, kLabelHeight, 0);
  HWND h_log_file_edit = CreateEditBox(parent_hwnd, 120, 80, kEditBoxWidth,
                                       kEditBoxHeight, kIdcLogFileEdit);

  HWND h_log_browse_btn = CreateButton(parent_hwnd, "Browse", 220, 120,
                                       kBtnWidth, kBtnHeight, kIdcLogBrowseBtn);

  HWND h_log_btn_ok = CreateButton(parent_hwnd, "Ok", 120, 180, kBtnWidth,
                                   kBtnHeight, kIdcLogBtnOk);

  HWND h_log_btn_cancel = CreateButton(parent_hwnd, "Cancel", 200, 180,
                                       kBtnWidth, kBtnHeight, kIdcLogBtnCancel);
  // Populate dropdowns
  SendMessage(h_log_level_box, CB_ADDSTRING, 0, (LPARAM)kLogOff.c_str());
  SendMessage(h_log_level_box, CB_ADDSTRING, 0, (LPARAM)kLogTrace.c_str());
  SendMessage(h_log_level_box, CB_SETCURSEL, 0, 0);

  // Set initial selection based on stored log_level_
  int initial_index = (log_level_ == kLogTrace.c_str()) ? 1 : 0;
  SendMessage(h_log_level_box, CB_SETCURSEL, initial_index, 0);
  SetWindowText(h_log_file_edit, log_file_path_.c_str());

  BOOL enable_controls = (log_level_ == kLogTrace.c_str());
  EnableWindow(h_log_file_edit, enable_controls);
  EnableWindow(h_log_browse_btn, enable_controls);
}

void LogTraceDialog::Show() {
  if (parent_hwnd) {
    ShowWindow(parent_hwnd, SW_SHOW);
    SetForegroundWindow(parent_hwnd);
    return;
  }

  WNDCLASS wc_logging = {};
  wc_logging.lpfnWndProc = LogTraceDialog::LogTraceProc;
  wc_logging.hInstance = GetModuleHandle(NULL);
  wc_logging.lpszClassName = CLASS_NAME;

  RegisterClass(&wc_logging);

  int window_width = 520;
  int window_height = 650;
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  parent_hwnd = CreateWindowEx(0, CLASS_NAME, "Logging Options",
                               WS_OVERLAPPEDWINDOW, x_pos, y_pos, 450, 300,
                               parent_hwnd, NULL, GetModuleHandle(NULL), this);

  if (parent_hwnd) {
    InitControls();
  }
  ShowWindow(parent_hwnd, SW_SHOW);
  UpdateWindow(parent_hwnd);
}

LRESULT CALLBACK LogTraceDialog::LogTraceProc(HWND hwnd, UINT u_msg,
                                              WPARAM w_param, LPARAM l_param) {
  LogTraceDialog* p_this = NULL;
  if (u_msg == WM_NCCREATE) {
    CREATESTRUCT* p_create = (CREATESTRUCT*)l_param;
    p_this = (LogTraceDialog*)p_create->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)p_this);
  } else {
    p_this = (LogTraceDialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }
  switch (u_msg) {
    case WM_COMMAND:
      switch (LOWORD(w_param)) {
        case kIdclogTraceBox: {
          if (HIWORD(w_param) == CBN_SELCHANGE) {
            HWND h_log_trace = GetDlgItem(hwnd, kIdclogTraceBox);
            int selected_index =
                (int)SendMessage(h_log_trace, CB_GETCURSEL, 0, 0);

            if (selected_index != CB_ERR) {
              char selected_value[256];
              SendMessage(h_log_trace, CB_GETLBTEXT, selected_index,
                          (LPARAM)selected_value);

              HWND h_log_file_edit = GetDlgItem(hwnd, kIdcLogFileEdit);
              HWND h_log_browse_btn = GetDlgItem(hwnd, kIdcLogBrowseBtn);

              BOOL enable_controls =
                  (strcmp(selected_value, kLogTrace.c_str()) == 0);
              EnableWindow(h_log_file_edit, enable_controls);
              EnableWindow(h_log_browse_btn, enable_controls);

              log_level_ = selected_value;
            }
          }
          break;
        }
        case kIdcLogBrowseBtn: {
          HWND h_edit = GetDlgItem(hwnd, kIdcLogFileEdit);
          OpenFolderDialog(hwnd, h_edit);
          break;
        }
        case kIdcLogBtnOk: {
          HWND h_log_trace = GetDlgItem(hwnd, kIdclogTraceBox);
          int selected_index =
              (int)SendMessage(h_log_trace, CB_GETCURSEL, 0, 0);

          if (selected_index != CB_ERR) {
            char log_trace_buf[256];
            SendMessage(h_log_trace, CB_GETLBTEXT, selected_index,
                        (LPARAM)log_trace_buf);
            log_level_ = log_trace_buf;
          }

          HWND h_log_file_path = GetDlgItem(hwnd, kIdcLogFileEdit);
          char log_file_buf[256];
          GetWindowText(h_log_file_path, log_file_buf, sizeof(log_file_buf));
          log_file_path_ = log_file_buf;

          DestroyWindow(hwnd);
          break;
        }
        case kIdcLogBtnCancel:
          DestroyWindow(hwnd);
          break;
      }
      break;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      if (p_this) {
        p_this->parent_hwnd = NULL;
      }
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hwnd, u_msg, w_param, l_param);
}
}  // namespace google::cloud::odbc_bq_driver_internal
