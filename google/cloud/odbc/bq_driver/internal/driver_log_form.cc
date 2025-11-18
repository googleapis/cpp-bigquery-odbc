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
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#pragma comment(lib, "Comctl32.lib")  // Link with Comctl32.lib

namespace google::cloud::odbc_bq_driver_internal {
char const LogTraceDialog::CLASS_NAME[] = "LoggingTraceClass";

std::string const kLogLevel = "LogLevel";
std::string const kLogPath = "LogPath";
std::string const kLogOff = "LOG_OFF";
std::string const kLogError = "LOG_ERROR";
std::string const kLogInfo = "LOG_INFO";
std::string const kLogWarning = "LOG_WARNING";
std::string const kLogMaxFiles = "LogFileCount";
std::string const kLogMaxSize = "LogFileSize";
std::string LogTraceDialog::log_level_ = kLogOff;
std::string LogTraceDialog::log_file_path_;
std::string LogTraceDialog::original_log_level;
std::string LogTraceDialog::original_log_file_path;
std::string LogTraceDialog::max_files_ = "50";
std::string LogTraceDialog::max_size_ = "20";
int const kBtnWidth = 66;
int const kBtnHeight = 16;
int const kComboBoxWidth = 202;
int const KComboBoxHeight = 100;
int const kLabelHeight = 16;
int const kEditBoxWidth = 203;
int const kEditBoxHeight = 17;
int const kOkCancelHeight = 17;
int const KAxisX = 20;
int const KAxisY = 10;
int const KLabelWidth = 80;

HWND LogTraceDialog::GetHwnd() const { return parent_hwnd; }
LogTraceDialog::LogTraceDialog() : parent_hwnd(NULL) {}
LogTraceDialog::~LogTraceDialog() {
  if (parent_hwnd) {
    DestroyWindow(parent_hwnd);
  }
  UnregisterClass(CLASS_NAME, g_hDllInstance);
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

int GetLogLevelIndex(std::string& log_level) {
  if (log_level == kLogOff) return 0;
  if (log_level == kLogError) return 1;
  if (log_level == kLogWarning) return 2;
  if (log_level == kLogInfo) return 3;
  return 0;  // Default to kLogOff if unknown
}

void LogTraceDialog::SetValues(Section const& attributes_map) {
  if (attributes_map.count(kLogLevel) > 0) {
    if (attributes_map.at(kLogLevel) == "0") {
      log_level_ = kLogOff.c_str();
    } else if (attributes_map.at(kLogLevel) == "1") {
      log_level_ = kLogError.c_str();
    } else if (attributes_map.at(kLogLevel) == "2") {
      log_level_ = kLogWarning.c_str();
    } else if (attributes_map.at(kLogLevel) == "3") {
      log_level_ = kLogInfo.c_str();
    }
  } else {
    log_level_ = kLogOff.c_str();
  }
  log_file_path_ =
      attributes_map.count(kLogPath) > 0 ? attributes_map.at(kLogPath) : "";
  max_files_ = GetValueOrDefault(attributes_map, kLogMaxFiles);
  max_size_ = GetValueOrDefault(attributes_map, kLogMaxSize);
}
void LogTraceDialog::InitControls() {
  HFONT h_font =
      CreateFont(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                 DEFAULT_PITCH | FF_SWISS, "Inter");

  HWND h_log_level_head = CreateLabel(parent_hwnd, "Log level:", KAxisX, KAxisY,
                                      KLabelWidth, kLabelHeight, 0);
  SendMessage(h_log_level_head, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_log_level_box =
      CreateComboBox(parent_hwnd, KAxisX + 205, KAxisY, kComboBoxWidth,
                     KComboBoxHeight, kIdclogTraceBox);
  SendMessage(h_log_level_box, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowSubclass(GetDlgItem(parent_hwnd, kIdclogTraceBox),
                    ComboBoxSubclassProc, 0, 0);
  HWND h_edit = GetWindow(h_log_level_box, GW_CHILD);
  SetWindowSubclass(h_edit, EditBlockSubclassProc, 1, 0);

  HWND h_log_file_add = CreateLabel(parent_hwnd, "Log path:", KAxisX,
                                    KAxisY + 30, KLabelWidth, kLabelHeight, 0);
  SendMessage(h_log_file_add, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_log_file_edit =
      CreateEditBox(parent_hwnd, KAxisX + 205, KAxisY + 30, kEditBoxWidth,
                    kEditBoxHeight, kIdcLogFileEdit);
  SendMessage(h_log_file_edit, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_log_browse_btn =
      CreateButton(parent_hwnd, "Browse...", KAxisX + 205, KAxisY + 55,
                   kBtnWidth, kBtnHeight, kIdcLogBrowseBtn);
  SendMessage(h_log_browse_btn, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_group_box = CreateGroupBox(parent_hwnd, "Log rotation", KAxisX - 10,
                                    KLabelWidth + 5, 427, 75, kIdcGroupBox);
  SendMessage(h_group_box, WM_SETFONT, (WPARAM)h_font, TRUE);

  // Max Number of Files Label and Edit Box
  HWND h_max_files_label =
      CreateLabel(parent_hwnd, "Max number of files:", KAxisX, KAxisY + 95,
                  KLabelWidth + 60, kLabelHeight, 0);
  SendMessage(h_max_files_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_max_files_edit =
      CreateEditBox(parent_hwnd, KAxisX + 205, KAxisY + 95, kEditBoxWidth,
                    kEditBoxHeight, kIdcMaxFilesEdit);
  SendMessage(h_max_files_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_max_files_edit, max_files_.c_str());

  // Max File Size (MB) Label and Edit Box
  HWND h_max_size_label =
      CreateLabel(parent_hwnd, "Max file size (MB):", KAxisX, KAxisY + 125,
                  KLabelWidth + 60, kLabelHeight, 0);
  SendMessage(h_max_size_label, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_max_size_edit =
      CreateEditBox(parent_hwnd, KAxisX + 205, KAxisY + 125, kEditBoxWidth,
                    kEditBoxHeight, kIdcMaxSizeEdit);

  SendMessage(h_max_size_edit, WM_SETFONT, (WPARAM)h_font, TRUE);
  SetWindowText(h_max_size_edit, max_size_.c_str());

  // Documentation Hyperlink
  HWND h_doc_text =
      CreateLabel(parent_hwnd, "Not sure what to select? See", KAxisX - 10,
                  KAxisY + 160, KLabelWidth + 80, kLabelHeight, 0);
  SendMessage(h_doc_text, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_hyperlink = CreateHyperlinkLabel(
      parent_hwnd, "BigQuery documentation", KAxisX + 125, KAxisY + 160,
      KLabelWidth + 40, kLabelHeight, kIdcHyperlink);
  SendMessage(h_hyperlink, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_log_btn_ok =
      CreateButton(parent_hwnd, "OK", KAxisX + 262, KAxisY + 160, kBtnWidth,
                   kOkCancelHeight, kIdcLogBtnOk);
  SendMessage(h_log_btn_ok, WM_SETFONT, (WPARAM)h_font, TRUE);

  HWND h_log_btn_cancel =
      CreateButton(parent_hwnd, "Cancel", KAxisX + 341, KAxisY + 160, kBtnWidth,
                   kOkCancelHeight, kIdcLogBtnCancel);
  SendMessage(h_log_btn_cancel, WM_SETFONT, (WPARAM)h_font, TRUE);

  SendMessage(h_log_level_box, CB_ADDSTRING, 0, (LPARAM)kLogOff.c_str());
  SendMessage(h_log_level_box, CB_ADDSTRING, 0, (LPARAM)kLogError.c_str());
  SendMessage(h_log_level_box, CB_ADDSTRING, 0, (LPARAM)kLogWarning.c_str());
  SendMessage(h_log_level_box, CB_ADDSTRING, 0, (LPARAM)kLogInfo.c_str());
  SendMessage(h_log_level_box, CB_SETCURSEL, 0, 0);

  // Set initial selection based on stored log_level_
  int initial_index = GetLogLevelIndex(log_level_);
  SendMessage(h_log_level_box, CB_SETCURSEL, initial_index, 0);
  SetWindowText(h_log_file_edit, log_file_path_.c_str());

  BOOL enable_controls = (log_level_ != kLogOff.c_str());
  EnableWindow(h_log_file_edit, enable_controls);
  EnableWindow(h_log_browse_btn, enable_controls);
  EnableWindow(h_max_files_edit, enable_controls);  // Disable max files edit
  EnableWindow(h_max_size_edit, enable_controls);   // Disable max size edit

  SetWindowSubclass(GetDlgItem(parent_hwnd, kIdcLogFileEdit), InputSubclassProc,
                    0, 0);
  SetWindowSubclass(GetDlgItem(parent_hwnd, kIdcMaxFilesEdit),
                    InputSubclassProc, 0, 0);
  SetWindowSubclass(GetDlgItem(parent_hwnd, kIdcMaxSizeEdit), InputSubclassProc,
                    0, 0);
}

void LogTraceDialog::Show() {
  if (parent_hwnd) {
    ShowWindow(parent_hwnd, SW_SHOW);
    SetForegroundWindow(parent_hwnd);
    return;
  }

  WNDCLASS wc_logging = {};
  wc_logging.lpfnWndProc = LogTraceDialog::LogTraceProc;
  wc_logging.hInstance = g_hDllInstance;
  wc_logging.lpszClassName = CLASS_NAME;
  INITCOMMONCONTROLSEX icc;
  icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icc.dwICC = ICC_STANDARD_CLASSES;  // Load standard control classes
  InitCommonControlsEx(&icc);

  RegisterClass(&wc_logging);

  int window_width = 462;
  int window_height = 232;
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int x_pos = (screen_width - window_width) / 2;
  int y_pos = (screen_height - window_height) / 2;

  parent_hwnd = CreateWindowEx(WS_EX_TOPMOST, CLASS_NAME, "Logging options",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, x_pos,
                               y_pos, window_width, window_height, parent_hwnd,
                               NULL, g_hDllInstance, this);

  if (parent_hwnd) {
    InitControls();

    ShowWindow(parent_hwnd, SW_SHOW);
    SetForegroundWindow(parent_hwnd);
    UpdateWindow(parent_hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
      if (msg.message == WM_KEYDOWN) {
        if (SendMessage(parent_hwnd, msg.message, msg.wParam, msg.lParam) !=
            0) {
          continue;
        }
      }

      // Let IsDialogMessage handle Tab/Enter navigation
      if (!IsDialogMessage(parent_hwnd, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      // Exit loop if window was destroyed
      if (!IsWindow(parent_hwnd)) break;
    }
  }
}

LRESULT CALLBACK LogTraceDialog::LogTraceProc(HWND hwnd, UINT u_msg,
                                              WPARAM w_param, LPARAM l_param) {
  static HFONT h_font_hyperlink = NULL;
  LogTraceDialog* p_this = NULL;
  if (u_msg == WM_NCCREATE) {
    CREATESTRUCT* p_create = (CREATESTRUCT*)l_param;
    p_this = (LogTraceDialog*)p_create->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)p_this);
  } else {
    p_this = (LogTraceDialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }
  switch (u_msg) {
    case WM_CREATE:
      setWindowIcon(hwnd);
      break;
    case WM_INITDIALOG: {
      p_this->parent_hwnd = hwnd;  // Store parent window handle

      // Store the original values from saved settings
      original_log_level = log_level_;
      original_log_file_path = log_file_path_;

      // Initialize UI controls
      p_this->InitControls();

      // Ensure the hyperlink control has SS_NOTIFY style
      HWND h_hyperlink = GetDlgItem(hwnd, kIdcHyperlink);
      LONG_PTR hyperlink_style = GetWindowLongPtr(h_hyperlink, GWL_STYLE);
      SetWindowLongPtr(h_hyperlink, GWL_STYLE, hyperlink_style | SS_NOTIFY);

      return TRUE;
    }
    case WM_ERASEBKGND: {
      HDC hdc = (HDC)w_param;
      RECT rc;
      GetClientRect(hwnd, &rc);

      // Define a custom background color (#F0F0F0)
      HBRUSH h_brush = CreateSolidBrush(RGB(240, 240, 240));

      FillRect(hdc, &rc, h_brush);
      DeleteObject(h_brush);
      return 1;  // Indicate we handled the background redraw
    }
    case WM_CTLCOLORSTATIC: {
      HDC hdc_static = (HDC)w_param;
      HWND h_control = (HWND)l_param;

      if (GetDlgCtrlID(h_control) == kIdcHyperlink) {
        SetTextColor(hdc_static, RGB(0, 0, 255));
        SetBkMode(hdc_static, TRANSPARENT);

        if (!h_font_hyperlink) {
          h_font_hyperlink = CreateFont(
              -10, 0, 0, 0, FW_NORMAL, FALSE, TRUE, FALSE, DEFAULT_CHARSET,
              OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
              VARIABLE_PITCH, "Inter");
        }
        SelectObject(hdc_static, h_font_hyperlink);
        return (LRESULT)GetStockObject(NULL_BRUSH);
      }
      break;
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
    // Ensure Esc works globally even when inside input/combo box
    case WM_GETDLGCODE:
      if (w_param == VK_ESCAPE || w_param == VK_RETURN) {
        return DLGC_WANTALLKEYS;  // Ensure we receive Esc/Enter key
      }
      break;

    case WM_KEYDOWN:  // Capture global key presses
      if (w_param == VK_ESCAPE) {
        DestroyWindow(hwnd);  // Close dialog when ESC is pressed
        return 0;
      } else if (w_param == VK_RETURN) {
        // Simulate a button click on the OK button when Enter is pressed
        SendMessage(GetDlgItem(hwnd, kIdcLogBtnOk), BM_CLICK, 0, 0);
        return 0;
      }
      break;
    case WM_COMMAND:
      switch (LOWORD(w_param)) {
        case kIdcHyperlink:  // Handle hyperlink click
          if (HIWORD(w_param) == STN_CLICKED) {
            ShellExecute(NULL, "open", kBigQueryDocsURL, NULL, NULL,
                         SW_SHOWNORMAL);
          }
          break;
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
              HWND h_max_files_edit =
                  GetDlgItem(hwnd, kIdcMaxFilesEdit);  // Get max files edit
              HWND h_max_size_edit =
                  GetDlgItem(hwnd, kIdcMaxSizeEdit);  // Get max size edit

              // Enable Browse button for all log levels except LOG_OFF
              BOOL enable_controls =
                  (strcmp(selected_value, kLogOff.c_str()) != 0);
              EnableWindow(h_log_file_edit, enable_controls);
              EnableWindow(h_log_browse_btn, enable_controls);
              EnableWindow(h_max_files_edit,
                           enable_controls);  // Disable max files if LOG_OFF
              EnableWindow(h_max_size_edit,
                           enable_controls);  // Disable max size if LOG_OFF
              log_level_ = selected_value;
              HWND h_path_edit = GetDlgItem(hwnd, kIdcLogFileEdit);
              char path_buffer[256] = {0};
              GetWindowTextA(h_path_edit, path_buffer, sizeof(path_buffer));

              HWND h_ok_button = GetDlgItem(hwnd, kIdcLogBtnOk);
              if (path_buffer[0] == '\0') {
                EnableWindow(h_ok_button, FALSE);
              } else {
                EnableWindow(h_ok_button, TRUE);
              }
            }
          }
          break;
        }
        case kIdcLogFileEdit: {
          HWND h_log_trace = GetDlgItem(hwnd, kIdclogTraceBox);
          char log_buffer[256] = {0};
          GetWindowTextA(h_log_trace, log_buffer, sizeof(log_buffer));

          HWND h_path_edit = GetDlgItem(hwnd, kIdcLogFileEdit);
          char path_buffer[256] = {0};
          GetWindowTextA(h_path_edit, path_buffer, sizeof(path_buffer));

          HWND h_ok_button = GetDlgItem(hwnd, kIdcLogBtnOk);
          if (log_buffer != kLogOff) {
            if (path_buffer[0] == '\0') {
              EnableWindow(h_ok_button, FALSE);  // Disable OK
            } else {
              EnableWindow(h_ok_button, TRUE);  // Enable OK
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
          // Save the final selection only when OK is clicked
          original_log_level = log_level_;

          original_log_file_path = log_file_path_;
          HWND h_max_files_edit =
              GetDlgItem(hwnd, kIdcMaxFilesEdit);  // Get max files edit
          char log_max_files_buf[256];
          GetWindowText(h_max_files_edit, log_max_files_buf,
                        sizeof(log_max_files_buf));
          max_files_ = log_max_files_buf;

          HWND h_max_size_edit =
              GetDlgItem(hwnd, kIdcMaxSizeEdit);  // Get max size edit
          char log_max_size_buf[256];
          GetWindowText(h_max_size_edit, log_max_size_buf,
                        sizeof(log_max_size_buf));
          max_size_ = log_max_size_buf;

          DestroyWindow(hwnd);
          break;
        }
        case kIdcLogBtnCancel:
          DestroyWindow(hwnd);
          break;
      }
      break;
    case WM_CLOSE: {
      DestroyWindow(hwnd);
      return 0;
    }

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
