// Copyright 2023 Google LLC
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

#ifndef _WIN32
#include <iconv.h>
#endif  // LINUX

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/internal/getenv.h"
#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#ifdef _WIN32
#include <uxtheme.h>                 // Required for SetWindowTheme
#pragma comment(lib, "UxTheme.lib")  // Link UxTheme.lib
HINSTANCE g_hDllInstance = NULL;
#endif
#include <filesystem>
namespace fs = std::filesystem;

namespace google::cloud::odbc_bq_driver_internal {
bool g_suppress_dropdown = false;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
#ifdef _WIN32
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
static std::string const kOAuthMechanism = "OAuthMechanism";
static std::string const kKeyFilePath = "KeyFilePath";
#endif

#ifdef __APPLE__
std::string const kFromCode = "UTF-32LE";
#else
std::string const kFromCode = "WCHAR_T";
#endif

constexpr char kRandomIdChars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789-_";

std::string GenerateRandomId(int length) {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<std::size_t> distrib(
      0, sizeof(kRandomIdChars) - 2);  // -2 because of null terminator

  std::string id(length, ' ');
  for (int i = 0; i < length; ++i) {
    id[i] = kRandomIdChars[distrib(gen)];
  }
  return id;
}

std::string GetDefaultPemFile() {
  fs::path base;
#ifdef WIN32
  HMODULE hm = nullptr;

  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(&GetDefaultPemFile), &hm)) {
    return {};
  }

  char path[MAX_PATH];
  if (GetModuleFileNameA(hm, path, MAX_PATH) == 0) {
    return {};
  }

  base = fs::path(path).parent_path();

  return (base / "assets" / "roots.pem").string();
#else
  Dl_info info;
  if (dladdr(reinterpret_cast<void*>(&GetDefaultPemFile), &info) == 0) {
    return {};
  }

  base = fs::path(info.dli_fname).parent_path();
  return (base / "roots.pem").string();
#endif /* WIN32 */
}

std::string GenerateTableId() {
  auto now = std::chrono::system_clock::now();
  auto epoch_time =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
          .count();
  std::string time_str = std::to_string(epoch_time);
  std::string random_id = GenerateRandomId(6);
  std::string table_id = time_str + "_" + random_id;
  return table_id;
}

std::wstring SQLWcharToWstring(const SQLWCHAR* in_str) {
  if (!in_str) return {};
#ifdef _WIN32
  return std::wstring(reinterpret_cast<wchar_t const*>(in_str));
#else
  std::wstring result;
  while (*in_str) {
    result.push_back(static_cast<wchar_t>(*in_str));
    ++in_str;
  }
  return result;
#endif /* _WIN32 */
}

StatusRecord DoubleStrToInt(std::string& double_str) {
  std::istringstream iss(double_str);
  int64_t int_value;
  iss >> int_value;
  if (iss.fail()) {
    LOG(ERROR) << "DoubleStrToInt:: Not a valid floating point value: "
               << double_str;
    return StatusRecord{SQLStates::k_HY000(),
                        "Internal error: Not a valid floating point value"};
  }
  double_str = std::to_string(int_value);
  return StatusRecord::Ok();
}

std::vector<std::string> Split(std::string const& s,
                               std::string const& delimiter, int limit) {
  int start_ind = 0;
  int end_ind;
  int len_del = delimiter.length();
  std::string split;
  std::vector<std::string> splits;

  while ((end_ind = s.find(delimiter, start_ind)) != std::string::npos &&
         --limit) {
    split = s.substr(start_ind, end_ind - start_ind);
    start_ind = end_ind + len_del;
    splits.push_back(split);
  }

  splits.push_back(s.substr(start_ind));
  return splits;
}

std::string Join(std::vector<std::string> v, std::string const& separator,
                 int start_ind) {
  if (v.empty() || start_ind >= v.size()) {
    return "";
  }
  if (start_ind < 0) {
    start_ind = 0;
  }
  std::string joined;
  for (; start_ind < v.size() - 1; start_ind++) {
    joined.append(v[start_ind]);
    joined.append(separator);
  }
  joined.append(v[v.size() - 1]);
  return joined;
}

#ifdef _WIN32

StatusRecordOr<std::shared_ptr<Section>> GetSectionWin(
    std::string const& registry_key) {
  Section section;
  HKEY key_handle;
  LONG status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, LPCSTR(registry_key.c_str()),
                             0, KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    std::string msg = "Can't open registry key with path: ";
    msg.append(registry_key);
    LOG(ERROR) << "GetSectionWin::RegOpenKeyEx:: " << msg;
    return StatusRecord{SQLStates::k_HY000(), msg};
  }

  DWORD num_values;
  DWORD longest_data_len;
  status = RegQueryInfoKey(key_handle, NULL, NULL, NULL, NULL, NULL, NULL,
                           &num_values, NULL, &longest_data_len, NULL, NULL);

  BYTE buffer[kMaxValueNameLen];
  TCHAR property_name[kMaxValueNameLen];
  DWORD buffer_len = kMaxValueNameLen;

  for (int i = 0, status = ERROR_SUCCESS; i < num_values; i++) {
    buffer_len = kMaxValueNameLen;
    DWORD data_len = sizeof(buffer);
    property_name[0] = '\0';
    status = RegEnumValue(key_handle, i, property_name, &buffer_len, NULL, NULL,
                          NULL, NULL);
    if (status == ERROR_SUCCESS) {
      buffer_len = longest_data_len;
      buffer[0] = '\0';
      LONG query_status = RegQueryValueEx(key_handle, property_name, 0, NULL,
                                          buffer, &data_len);
      if (query_status == ERROR_SUCCESS) {
        std::string value(reinterpret_cast<char*>(buffer), data_len);
        value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
        std::string property(property_name);
        section[property] = value;
      }
    }
  }
  RegCloseKey(key_handle);
  return std::make_shared<Section>(section);
}

StatusRecordOr<std::shared_ptr<Sections>> ParseConfig(
    std::string const& registry_key) {
  HKEY key_handle;
  LONG status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, LPCSTR(registry_key.c_str()),
                             0, KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    return std::make_shared<Sections>();
  }

  TCHAR subkey_name[kMaxKeyLength];
  DWORD name_len;
  DWORD num_sub_keys = 0;

  status = RegQueryInfoKey(key_handle, NULL, NULL, NULL, &num_sub_keys, NULL,
                           NULL, NULL, NULL, NULL, NULL, NULL);

  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    std::string msg = "RegQueryInfoKey failed with error code: ";
    msg.append(registry_key);
    LOG(ERROR) << "ParseConfig::RegQueryInfoKey:: " << msg;
    return StatusRecord{SQLStates::k_HY000(), msg};
  }

  Sections sections;
  // List all the sections
  for (int i = 0; i < num_sub_keys; i++) {
    name_len = kMaxKeyLength;
    status = RegEnumKeyEx(key_handle, i, subkey_name, &name_len, NULL, NULL,
                          NULL, NULL);
    if (status == ERROR_SUCCESS) {
      auto get_sections_response_status =
          GetSectionWin(registry_key + "\\" + std::string(subkey_name));
      if (!get_sections_response_status) {
        LOG(ERROR) << "ParseConfig::GetSectionWin:: "
                   << get_sections_response_status.GetStatusRecord().message;
        return get_sections_response_status.GetStatusRecord();
      }
      auto get_sections_response = *get_sections_response_status;
      sections[subkey_name] = *get_sections_response;
    }
  }
  RegCloseKey(key_handle);
  return std::make_shared<Sections>(sections);
}

// Helper function to create a static label
HWND CreateLabel(HWND parent, char const* text, int x, int y, int width,
                 int height, int id) {
  return CreateWindowEx(0, "STATIC", text,
                        WS_VISIBLE | WS_CHILD | SS_LEFT | SS_NOTIFY, x, y,
                        width, height, parent, (HMENU)id, g_hDllInstance, NULL);
}

// Helper function to create an edit box
HWND CreateEditBox(HWND parent, int x, int y, int width, int height, int id) {
  return CreateWindowEx(
      0, "EDIT", "",
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
      x, y, width, height, parent, (HMENU)id, g_hDllInstance, NULL);
}

HWND CreateScrollableEditBox(HWND parent, int x, int y, int width, int height,
                             int id) {
  HWND hwndEdit = CreateWindowEx(
      0, "EDIT", "",
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_MULTILINE |
          ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
      x, y, width, height, parent, (HMENU)id, g_hDllInstance, NULL);

  // Attach the input subclass to handle VK_TAB and VK_ESCAPE
  if (hwndEdit) {
    SetWindowSubclass(hwndEdit, InputSubclassProc, 1, 0);
  }

  return hwndEdit;
}

// Helper function to create a combo box (dropdown)
HWND CreateComboBox(HWND parent, int x, int y, int width, int height, int id) {
  HWND hwndCombo = CreateWindowEx(
      0, "COMBOBOX", "",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | ES_LEFT |
          ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | CBS_DROPDOWN |
          CBS_HASSTRINGS,
      x, y, width, height, parent, (HMENU)id, g_hDllInstance, NULL);

  return hwndCombo;
}

HWND CreateButton(HWND parent, char const* text, int x, int y, int width,
                  int height, int id) {
  HWND hButton = CreateWindowEx(
      0, "BUTTON", text, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_FLAT, x, y,
      width, height, parent, (HMENU)id, g_hDllInstance, NULL);

  // Disable Windows theme to remove any rounding
  if (hButton) {
    SetWindowTheme(hButton, L"", L"");
  }

  return hButton;
}

// Helper function to create a checkbox
HWND CreateCheckBox(HWND parent, char const* text, int x, int y, int width,
                    int height, int id) {
  return CreateWindowEx(
      0, "BUTTON", text, WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
      x, y, width, height, parent, (HMENU)id, g_hDllInstance, NULL);
}
// Helper function to create a group box
HWND CreateGroupBox(HWND parent, char const* text, int x, int y, int width,
                    int height, int id) {
  return CreateWindowEx(0, "BUTTON", text, WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                        x, y, width, height, parent, (HMENU)id, g_hDllInstance,
                        NULL);
}
HWND CreateNumericEditBox(HWND parent, char const* text, int x, int y,
                          int width, int height, int id) {
  HWND hEditBox = CreateWindowEx(
      WS_EX_CLIENTEDGE, "EDIT", text,
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER |
          ES_RIGHT,  // ES_NUMBER restricts input to numbers
      x, y, width, height, parent, (HMENU)id, g_hDllInstance, NULL);

  if (hEditBox) {
    SendMessage(hEditBox, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
                TRUE);
  }

  return hEditBox;
}
HWND CreateHyperlinkLabel(HWND parent, char const* text, int x, int y,
                          int width, int height, int id) {
  HWND h_hyperlink =
      CreateWindowEx(0, "STATIC", text, WS_CHILD | WS_VISIBLE | SS_NOTIFY, x, y,
                     width, height, parent, (HMENU)id, g_hDllInstance, NULL);

  return h_hyperlink;
}
void ShowErrorWindow(HWND hwnd, std::string const message) {
  MessageBoxA(hwnd, message.c_str(), "DSN Configuration Error",
              MB_OK | MB_ICONWARNING);
}

extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason,
                                 LPVOID lpReserved) {
  switch (ul_reason) {
    case DLL_PROCESS_ATTACH:
      g_hDllInstance = hModule;
      break;
  }
  return TRUE;
}

std::wstring GetModuleDirectory() {
  HMODULE hModule = NULL;

  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(&GetModuleDirectory),
                         &hModule)) {
    return L"";
  }

  wchar_t dll_path[MAX_PATH];
  if (GetModuleFileNameW(hModule, dll_path, MAX_PATH) == 0) {
    return L"";
  }

  std::wstring path(dll_path);
  size_t pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return L"";

  return path.substr(0, pos);  // directory only
}

void setWindowIcon(HWND hwnd) {
  std::wstring dir = GetModuleDirectory();
  if (dir.empty()) return;

  // Compose icon path (e.g., DLL directory + "\\assets\\bq.ico")
  std::wstring iconPath = dir + L"\\assets\\bq.ico";

  HICON hIcon = (HICON)LoadImageW(NULL, iconPath.c_str(), IMAGE_ICON, 32, 32,
                                  LR_LOADFROMFILE);

  if (!hIcon) {
    OutputDebugStringW(
        (L"Failed to load icon at: " + iconPath + L"\n").c_str());
    return;
  }
  SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
  SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
}
std::string GetRootsPemPath() {
  std::wstring dir = GetModuleDirectory();
  if (dir.empty()) return "";

  std::wstring full_wpath = dir + L"\\assets\\roots.pem";

  int size_needed = WideCharToMultiByte(CP_UTF8, 0, full_wpath.c_str(), -1,
                                        nullptr, 0, nullptr, nullptr);

  std::string utf8_path(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, full_wpath.c_str(), -1, &utf8_path[0],
                      size_needed, nullptr, nullptr);

  if (!utf8_path.empty() && utf8_path.back() == '\0') {
    utf8_path.pop_back();
  }

  return utf8_path;
}

LRESULT CALLBACK InputSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                   LPARAM l_param, UINT_PTR sub_id,
                                   DWORD_PTR ref_data) {
  if (msg == WM_KEYDOWN) {
    if (w_param == VK_ESCAPE) {
      SendMessage(GetParent(hwnd), WM_CLOSE, 0, 0);  // Close the parent dialog
      return 0;                                      // Mark message as handled
    } else if (w_param == VK_TAB) {
      // Move focus to next or previous control
      BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000);
      HWND next = GetNextDlgTabItem(GetParent(hwnd), hwnd, shiftPressed);
      if (next) SetFocus(next);
      return 0;  // Mark as handled to prevent tab character insertion
    }
  }
  return DefSubclassProc(hwnd, msg, w_param, l_param);
}

LRESULT CALLBACK EditBlockSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                       LPARAM l_param, UINT_PTR sub_id,
                                       DWORD_PTR ref_data) {
  switch (msg) {
    case WM_CHAR:  // block character input
    case WM_PASTE:
    case WM_CUT:
      return 0;  // block typing and clipboard actions
  }
  return DefSubclassProc(hwnd, msg, w_param, l_param);
}

LRESULT CALLBACK ComboBoxSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                      LPARAM l_param, UINT_PTR sub_id,
                                      DWORD_PTR ref_data) {
  if (msg == WM_CTLCOLORLISTBOX) {
    if (g_suppress_dropdown) {
      SendMessage(hwnd, CB_SHOWDROPDOWN, FALSE, 0);
      return (LRESULT)GetStockObject(WHITE_BRUSH);
    }
  }
  if (msg == WM_KEYDOWN) {
    if (w_param == VK_ESCAPE) {
      SendMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
      return 0;
    } else if (w_param == VK_RETURN) {
      HWND h_ok = GetDlgItem(GetParent(hwnd), IDOK);
      if (h_ok) SendMessage(GetParent(hwnd), WM_COMMAND, IDOK, (LPARAM)h_ok);
      return 0;
    }
  }
  return DefSubclassProc(hwnd, msg, w_param, l_param);
}

LRESULT CALLBACK CheckboxSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                      LPARAM l_param, UINT_PTR sub_id,
                                      DWORD_PTR ref_data) {
  if (msg == WM_KEYDOWN && w_param == VK_ESCAPE) {
    SendMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
    return 0;
  }
  return DefSubclassProc(hwnd, msg, w_param, l_param);
}

#else

StatusRecordOr<std::shared_ptr<Sections>> ParseConfig(
    std::string const& file_path) {
  std::ifstream is(file_path);
  is.exceptions(std::ios::badbit);  // Minimal error handling
  Sections sections;
  if (is.is_open()) {
    std::string line;
    std::string current_section_name;
    while (getline(is, line)) {
      Trim(line);
      if (line.empty() || line.at(0) == ';' || line.at(0) == '#') {
        // Blank lines and comment lines are ignored.
      } else if (line.at(0) == '[' && line.back() == ']') {
        // Section line.
        line.erase(0, 1);
        line.pop_back();
        Trim(line);
        current_section_name = line;
      } else {
        // Property line.
        size_t pos = line.find_first_of('=');
        std::string property = line.substr(0, pos);
        Trim(property);
        std::string value;
        if (pos != std::string::npos) {
          value = line.substr(pos + 1);
          Trim(value);
        }
        if (!current_section_name.empty()) {
          sections[current_section_name][property] = value;
        }
      }
    }
    return std::make_shared<Sections>(sections);
  }
  return std::make_shared<Sections>();
}

#endif  //_WIN32

StatusRecordOr<Section> ParseConnectionString(std::string& str) {
  LOG(INFO) << "ParseConnectionString:: Received connection string: " << str
            << std::endl;
  Section section;
  std::vector<std::string> splits = Split(str, ";");
  for (std::string& property : splits) {
    Trim(property);
    if (property.empty()) {
      continue;
    }
    std::vector<std::string> property_splits = Split(property, "=", 2);
    if (property_splits.size() < 2) {
      LOG(ERROR) << "ParseConnectionString:: Invalid Connection String part: "
                 << property;
      return StatusRecord{SQLStates::k_HY000(), "Invalid Connection String"};
    }
    std::string field = property_splits[0];
    std::string value = Join(property_splits, "", 1);
    Trim(field);
    Trim(value);

    // Remove enclosing curly braces if they exist
    if (!value.empty() && value.front() == '{' && value.back() == '}') {
      value = value.substr(1, value.size() - 2);
    }
    if (field.empty() || value.empty()) {
      continue;
    }
    if (!section.count(field)) {
      section[field] = value;
    }
  }
  return section;
}

std::string GetPathToOdbcIni() {
#ifdef _WIN32
  // 64-bit
  absl::optional<std::string> path = "SOFTWARE\\ODBC\\ODBC.INI";
#ifndef _WIN64
  // 32-bit
  path = "SOFTWARE\\WOW6432Node\\ODBC\\ODBC.INI";
#endif  // _WIN64
  if (path) {
    return *path;
  }
#else
  absl::optional<std::string> path = google::cloud::internal::GetEnv("ODBCINI");
  if (path) {
    return *path;
  }
  absl::optional<std::string> home = google::cloud::internal::GetEnv("HOME");
  if (home) {
    return *home + "/.odbc.ini";
  }
#endif  // _WIN32
  return "";
}

std::string GetOdbcTraceConfigPath() {
#ifndef _WIN32
  absl::optional<std::string> path =
      google::cloud::internal::GetEnv("GOOGLEBIGQUERYODBCINI");
  if (path) {
    return *path;
  }
  // Default to using ~ path directly
  return "/etc/google.googlebigqueryodbc.ini";
#else
  return k_trace_reg_path;
#endif  // _WIN32
}

std::vector<std::string> SplitTableTypes(std::string const& table_types) {
  std::vector<std::string> types = Split(table_types, ",");
  for (auto& type : types) {
    Trim(type);
    if (type[0] == '\'' && type[type.length() - 1] == '\'') {
      type = type.substr(1, type.length() - 2);
      Trim(type);
    }
  }
  return types;
}

odbc_internal::StatusRecordOr<std::string> Utf16ToUtf8(
    std::wstring const& utf_16_str) {
  if (utf_16_str.empty()) {
    return std::string();
  }
#ifdef _WIN32
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int utf8Length = WideCharToMultiByte(CP_UTF8, 0, utf_16_str.c_str(), -1, NULL,
                                       0, NULL, NULL);
  if (utf8Length == 0) {
    LOG(ERROR) << "Utf16ToUtf8:: Error determining buffer size while "
                  "converting wstring to string";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Error determining buffer size while converting wstring to string"};
  }
  if (sizeof(SQLWCHAR) == 2) {
    utf8Length = utf8Length * sizeof(SQLWCHAR);
  }
  std::string utf8Str(utf8Length, 0);
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int result = WideCharToMultiByte(CP_UTF8, 0, utf_16_str.c_str(), -1,
                                   &utf8Str[0], utf8Length, NULL, NULL);
  if (result == 0) {
    LOG(ERROR) << "Utf16ToUtf8:: Error while converting wstring to string";
    return StatusRecord{SQLStates::k_HY000(),
                        "Error while converting wstring to string"};
  }
  return utf8Str;
#else
  iconv_t cd = iconv_open("UTF-8", kFromCode.c_str());
  int errorno = -1;
  int* errorptr = &errorno;
  if (cd == reinterpret_cast<iconv_t>(errorptr)) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "iconv_open failed while converting wstring to string: " +
            std::string(strerror(errno))};
  }

  std::vector<char> inbuf(
      reinterpret_cast<char const*>(utf_16_str.data()),
      reinterpret_cast<char const*>(utf_16_str.data() + utf_16_str.length()));
  size_t inbytesleft = inbuf.size();
  size_t outbytesleft = inbytesleft * 4;  // Allocate more space for utf8 output

  std::string utf8str(outbytesleft, '\0');
  char* inptr = inbuf.data();
  char* outptr = utf8str.data();

  size_t res = iconv(cd, &inptr, &inbytesleft, &outptr, &outbytesleft);
  if (res == static_cast<size_t>(-1)) {
    iconv_close(cd);
    return StatusRecord{SQLStates::k_HY000(),
                        "iconv16 failed while converting wstring to string " +
                            std::string(strerror(errno))};
  }

  iconv_close(cd);
  utf8str.resize(outptr - utf8str.data());
  return utf8str;
#endif
}

odbc_internal::StatusRecordOr<std::wstring> Utf8ToUtf16(
    std::string const& utf_8_str) {
  if (utf_8_str.empty()) {
    return std::wstring();
  }
#ifdef _WIN32
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
  int utf16Length =
      MultiByteToWideChar(CP_UTF8, 0, utf_8_str.c_str(), -1, NULL, 0);
  if (utf16Length == 0) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Error determining buffer size while converting string to wstring"};
  }
  std::wstring utf16Str(utf16Length, 0);
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
  int result = MultiByteToWideChar(CP_UTF8, 0, utf_8_str.c_str(), -1,
                                   &utf16Str[0], utf16Length);
  if (result == 0) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Error while converting string to wstring"};
  }
  return utf16Str;
#else
  iconv_t cd = iconv_open(kFromCode.c_str(), "UTF-8");
  int errorno = -1;
  int* errorptr = &errorno;
  if (cd == reinterpret_cast<iconv_t>(errorptr)) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "iconv_open failed while converting string to wstring " +
            std::string(strerror(errno))};
  }

  // Use string length for input byte count
  size_t inbytesleft = utf_8_str.length();
  // Allocate more space for the output buffer
  size_t outbytesleft = inbytesleft * sizeof(wchar_t);
  std::wstring utf16str(outbytesleft + sizeof(wchar_t), L'\0');

  char* inbuf = const_cast<char*>(utf_8_str.data());
  char* outbuf = reinterpret_cast<char*>(utf16str.data());

  size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  if (res == static_cast<size_t>(-1)) {
    iconv_close(cd);
    return StatusRecord{SQLStates::k_HY000(),
                        "iconv8 failed while converting string to wstring " +
                            std::string(strerror(errno))};
  }

  iconv_close(cd);

  // Resize the output string to the actual converted size
  utf16str.resize((outbuf - reinterpret_cast<char*>(utf16str.data())) /
                  sizeof(wchar_t));

  return utf16str;
#endif
}

odbc_internal::StatusRecordOr<std::string> BqConvertSQLWCHARToString(
    SQLWCHAR* in_str, SQLINTEGER in_str_len) {
  if (in_str == nullptr) {
    return StatusRecord{SQLStates::k_HY000(), "in_str string is empty/Null"};
  }
  if (((in_str != nullptr) && (in_str[0] == '\0'))) {
    return std::string();
  }
  if (in_str_len == SQL_NTS || in_str_len == NULL) {
    in_str_len =
        static_cast<SQLINTEGER>(std::char_traits<SQLWCHAR>::length(in_str));
  }

  // Directly create a wide string
  std::wstring wstr(in_str, in_str + in_str_len);

  return Utf16ToUtf8(wstr);
}

bool IsDiagIdentifierString(SQLSMALLINT DiagIdentifier) {
  switch (DiagIdentifier) {
    case SQL_DIAG_DYNAMIC_FUNCTION:
    case SQL_DIAG_CLASS_ORIGIN:
    case SQL_DIAG_CONNECTION_NAME:
    case SQL_DIAG_MESSAGE_TEXT:
    case SQL_DIAG_SERVER_NAME:
    case SQL_DIAG_SQLSTATE:
    case SQL_DIAG_SUBCLASS_ORIGIN:
      return true;
      break;

    default:
      return false;
      break;
  }
}

StatusRecordOr<SQLUINTEGER> ParseStringToInteger(std::string const& input) {
  SQLUINTEGER value = 0;
  for (char c : input) {
    if (!std::isdigit(c)) {
      return StatusRecord{SQLStates::k_HY000(),
                          "Input value must be an integer"};
    }
    int digit = c - '0';
    if (value > (std::numeric_limits<SQLUINTEGER>::max() - digit) / 10) {
      return StatusRecord{SQLStates::k_HY000(),
                          "Input value value is too large"};
    }
    value = value * 10 + digit;
  }
  return value;  // success
}

bool IsFieldIdentifierString(SQLSMALLINT FieldIdentifier) {
  switch (FieldIdentifier) {
    case SQL_DESC_BASE_COLUMN_NAME:
    case SQL_DESC_BASE_TABLE_NAME:
    case SQL_DESC_CATALOG_NAME:
    case SQL_DESC_LABEL:
    case SQL_DESC_LITERAL_PREFIX:
    case SQL_DESC_LITERAL_SUFFIX:
    case SQL_DESC_LOCAL_TYPE_NAME:
    case SQL_DESC_NAME:
    case SQL_DESC_SCHEMA_NAME:
    case SQL_DESC_TABLE_NAME:
    case SQL_DESC_TYPE_NAME:
      return true;
      break;

    default:
      return false;
      break;
  }
}

bool IsInfoTypeString(SQLUSMALLINT InfoType) {
  switch (InfoType) {
      /* info_type that returns a string */
    case SQL_ACCESSIBLE_PROCEDURES:
    case SQL_ACCESSIBLE_TABLES:
    case SQL_CATALOG_NAME:
    case SQL_CATALOG_NAME_SEPARATOR:
    case SQL_CATALOG_TERM:
    case SQL_COLLATION_SEQ:
    case SQL_COLUMN_ALIAS:
    case SQL_DATA_SOURCE_NAME:
    case SQL_DATA_SOURCE_READ_ONLY:
    case SQL_DATABASE_NAME:
    case SQL_DBMS_NAME:
    case SQL_DBMS_VER:
    case SQL_DESCRIBE_PARAMETER:
    case SQL_DRIVER_NAME:
    case SQL_DRIVER_ODBC_VER:
    case SQL_DRIVER_VER:
    case SQL_EXPRESSIONS_IN_ORDERBY:
    case SQL_IDENTIFIER_QUOTE_CHAR:
    case SQL_INTEGRITY:
    case SQL_KEYWORDS:
    case SQL_LIKE_ESCAPE_CLAUSE:
    case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
    case SQL_MULT_RESULT_SETS:
    case SQL_MULTIPLE_ACTIVE_TXN:
    case SQL_NEED_LONG_DATA_LEN:
    case SQL_ORDER_BY_COLUMNS_IN_SELECT:
    case SQL_PROCEDURE_TERM:
    case SQL_PROCEDURES:
    case SQL_ROW_UPDATES:
    case SQL_SCHEMA_TERM:
    case SQL_SEARCH_PATTERN_ESCAPE:
    case SQL_SERVER_NAME:
    case SQL_SPECIAL_CHARACTERS:
    case SQL_TABLE_TERM:
    case SQL_USER_NAME:
    case SQL_XOPEN_CLI_YEAR:
      return true;
      break;

    default:
      return false;
      break;
  }
}

std::regex BuildRegex(std::string filter_pattern, SQLULEN metadata_id) {
  if (metadata_id == SQL_TRUE) {
    RTrim(filter_pattern);
    return std::regex(filter_pattern, std::regex_constants::icase);
  }
  return std::regex(CastOdbcRegexToCppRegex(filter_pattern));
}

StatusRecord ValidateTableParameters(const SQLCHAR* catalog_name,
                                     SQLSMALLINT catalog_name_len,
                                     const SQLCHAR* schema_name,
                                     SQLSMALLINT schema_name_len,
                                     const SQLCHAR* table_name,
                                     SQLSMALLINT table_name_len,
                                     SQLULEN metadata_id) {
  if (catalog_name_len < 0 && catalog_name_len != SQL_NTS) {
    LOG(ERROR) << "ValidateTableParameters:: Invalid catalog name length: "
               << catalog_name_len;
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - catalog length is invalid"};
  }
  if (schema_name_len < 0 && schema_name_len != SQL_NTS) {
    LOG(ERROR) << "ValidateTableParameters:: Invalid catalog name length: "
               << catalog_name_len;
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - schema length is invalid"};
  }
  if (table_name_len < 0 && table_name_len != SQL_NTS) {
    LOG(ERROR) << "ValidateTableParameters:: Invalid table name length: "
               << table_name_len;
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - table name length is invalid"};
  }
  if (metadata_id == SQL_TRUE) {
    if (!catalog_name) {
      LOG(ERROR) << "ValidateTableParameters:: Invalid catalog name: NULL";
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for catalog name"};
    }
    if (!schema_name) {
      LOG(ERROR) << "ValidateTableParameters:: Invalid schema name: NULL";
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for schema name"};
    }
    if (!table_name) {
      LOG(ERROR) << "ValidateTableParameters:: Invalid table name: NULL";
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for table name"};
    }
  }
  return StatusRecord::Ok();
}

StatusRecord PopulateOutputConnectionString(SQLCHAR* out_conn_str,
                                            SQLSMALLINT out_conn_str_buflen,
                                            SQLSMALLINT* out_conn_str_len,
                                            std::string& conn_string,
                                            bool is_conn_str_empty) {
  if (is_conn_str_empty) {
    if (conn_string.empty()) {
      LOG(ERROR)
          << "PopulateOutputConnectionString:: Invalid Connection String";
      return StatusRecord{SQLStates::k_HY000(), "Invalid Connection String"};
    }
  }

  std::string out_tmp_str = conn_string;
  if (!out_tmp_str.empty() && out_tmp_str.back() != ';') {
    out_tmp_str.append(";");
  }

  auto out_str_len = out_tmp_str.length();

  if (out_str_len >= out_conn_str_buflen) {
    strncpy(reinterpret_cast<char*>(out_conn_str), out_tmp_str.c_str(),
            out_conn_str_buflen - 1);
    out_conn_str[out_conn_str_buflen - 1] = '\0';
    if (out_conn_str_len) {
      *out_conn_str_len = out_str_len;
    }
    LOG(ERROR)
        << "PopulateOutputConnectionString:: String data, right truncated";
    return StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
  }
  strncpy(reinterpret_cast<char*>(out_conn_str), out_tmp_str.c_str(),
          out_tmp_str.length());
  out_conn_str[out_tmp_str.length()] = '\0';
  if (out_conn_str_len) {
    *out_conn_str_len = out_tmp_str.length();
  }
  return StatusRecord::Ok();
}

std::string Base64Encode(uint8_t const* data, int length) {
  std::string encoded_str;
  uint32_t val = 0;
  int val_b = -6;
  for (size_t i = 0; i < length; i++) {
    val = (val << 8) + data[i];
    val_b += 8;
    while (val_b >= 0) {
      encoded_str.push_back(kBase64Chars[(val >> val_b) & 0x3F]);
      val_b -= 6;
    }
  }
  if (val_b > -6)
    encoded_str.push_back(kBase64Chars[((val << 8) >> (val_b + 8)) & 0x3F]);
  while (encoded_str.size() % 4) encoded_str.push_back('=');
  return encoded_str;
}

#ifdef _WIN32
std::string ConvertLPCSTRToString(LPCSTR lpszAttributes) {
  if (lpszAttributes == nullptr) return "";

  std::string result;
  while (true) {
    while (*lpszAttributes != '\0') {
      result += *lpszAttributes;
      lpszAttributes++;
    }
    result += ';';
    lpszAttributes++;
    if (*lpszAttributes == '\0') {
      result += ';';
      break;
    }
  }

  return result;
}

// TODO(b/385158889): Support other log levels for tracing
StatusRecord SetRegValues(HKEY registry_root, std::string const& registry_path,
                          Section const& section) {
  HKEY h_key = nullptr;
  if (RegCreateKeyExA(registry_root, registry_path.c_str(), 0, NULL, 0,
                      KEY_WRITE, NULL, &h_key, NULL) != ERROR_SUCCESS) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to create or open registry key for DSN"};
  }
  for (auto const& kv : section) {
    if (RegSetValueExA(h_key, kv.first.c_str(), 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(kv.second.c_str()),
                       static_cast<DWORD>(kv.second.size() + 1)) !=
        ERROR_SUCCESS) {
      RegCloseKey(h_key);
      return StatusRecord{SQLStates::k_HY000(),
                          "Failed to set " + kv.first + " value"};
    }
  }
  RegCloseKey(h_key);
  return StatusRecord::Ok();
}

StatusRecord AddLogTraceToRegistry(Section const& section) {
  std::string const registry_path = GetOdbcTraceConfigPath() + "\\Driver";
  StatusRecord status =
      SetRegValues(HKEY_LOCAL_MACHINE, registry_path, section);
  if (!status.ok()) {
    return status;
  }
  return StatusRecord::Ok();
}

#endif  // _WIN32

bool IsLengthSensitiveType(SQLSMALLINT c_type) {
  switch (c_type) {
    case SQL_C_CHAR:
    case SQL_C_WCHAR:
    case SQL_C_BINARY:
      return true;
    default:
      return false;
  }
}

bool CheckTargetType(int c_type) {
  switch (c_type) {
    case SQL_C_CHAR:
    case SQL_C_LONG:
    case SQL_C_SHORT:
    case SQL_C_FLOAT:
    case SQL_C_NUMERIC:
    case SQL_C_DEFAULT:
    case SQL_C_TYPE_DATE:
    case SQL_C_TYPE_TIME:
    case SQL_C_TYPE_TIMESTAMP:
    case SQL_C_INTERVAL_YEAR:
    case SQL_C_INTERVAL_MONTH:
    case SQL_C_INTERVAL_DAY:
    case SQL_C_INTERVAL_HOUR:
    case SQL_C_INTERVAL_MINUTE:
    case SQL_C_INTERVAL_SECOND:
    case SQL_C_INTERVAL_YEAR_TO_MONTH:
    case SQL_C_INTERVAL_DAY_TO_HOUR:
    case SQL_C_INTERVAL_DAY_TO_MINUTE:
    case SQL_C_INTERVAL_DAY_TO_SECOND:
    case SQL_C_INTERVAL_HOUR_TO_MINUTE:
    case SQL_C_INTERVAL_HOUR_TO_SECOND:
    case SQL_C_INTERVAL_MINUTE_TO_SECOND:
    case SQL_C_BINARY:
    case SQL_C_BIT:
    case SQL_C_SBIGINT:
    case SQL_C_UBIGINT:
    case SQL_C_TINYINT:
    case SQL_C_SLONG:
    case SQL_C_SSHORT:
    case SQL_C_STINYINT:
    case SQL_C_ULONG:
    case SQL_C_USHORT:
    case SQL_C_UTINYINT:
    case SQL_C_GUID:
    case SQL_C_WCHAR:
    case SQL_C_DOUBLE:
    case SQL_ARD_TYPE:
      return true;

    default:
      return false;
  }
}

StatusRecordOr<std::vector<ConnectionProperty>> ParseQueryProperties(
    std::string const& input) {
  std::string temp_input = input;
  Trim(temp_input);

  if (temp_input.empty()) {
    return std::vector<ConnectionProperty>{};
  }
  std::vector<ConnectionProperty> properties;
  std::vector<std::string> splits = Split(input, ",");

  for (std::string& property_str : splits) {
    Trim(property_str);

    if (property_str.empty()) {
      LOG(ERROR) << "ParseQueryProperties:: Empty property string";
      return StatusRecord{SQLStates::k_HY000(),
                          "Malformed list of key-value pairs. Property not "
                          "separated by an equals sign (=)."};
    }
    if (absl::StrContains(property_str, ';')) {
      LOG(ERROR) << "ParseQueryProperties:: Malformed property string: "
                 << property_str;
      return StatusRecord{
          SQLStates::k_HY000(),
          "Malformed list of key-value pairs. Multiple properties not "
          "separated by a comma (,)."};
    }

    std::vector<std::string> property_splits = Split(property_str, "=", 2);
    if (property_splits.size() != 2) {
      LOG(ERROR) << "ParseQueryProperties:: Invalid Query Property Format: "
                 << property_str;
      return StatusRecord{
          SQLStates::k_HY000(),
          "Invalid Query Property Format: Missing '=' or value"};
    }

    std::string key = property_splits[0];
    std::string value = property_splits[1];
    Trim(key);
    Trim(value);

    if (key.empty()) {
      LOG(ERROR) << "ParseQueryProperties:: Invalid Query Property Format: "
                    "Empty key name";
      return StatusRecord{SQLStates::k_HY000(),
                          "Invalid Query Property Format: Empty key name"};
    }
    if (value.empty()) {
      LOG(ERROR) << "ParseQueryProperties:: Invalid Query Property Format: "
                    "Empty value for key '"
                 << key << "'";
      return StatusRecord{
          SQLStates::k_HY000(),
          "Invalid Query Property Format: Empty value for key '" + key + "'"};
    }
    if (absl::StrContains(value, '=')) {
      LOG(ERROR) << "ParseQueryProperties:: Invalid Query Property Format: "
                    "Value for key '"
                 << key << "' contains an unexpected '='";
      return StatusRecord{
          SQLStates::k_HY000(),
          "Invalid Query Property Format: Value for key '" + key +
              "' contains an unexpected '='. Values cannot contain '='."};
    }

    properties.emplace_back(ConnectionProperty{key, value});
  }

  return properties;
}

std::string GetLocationfromPSC(std::string const& psc) {
  std::string location;
  std::string key = "BIGQUERY=https://";
  auto start_pos = psc.find(key);
  if (start_pos != std::string::npos) {
    start_pos += key.length();
    std::string suffix = "-bigquery.googleapis.com";
    auto end_pos = psc.find(suffix, start_pos);

    if (end_pos != std::string::npos) {
      location = psc.substr(start_pos, end_pos - start_pos);
    }
  }
  return location;
}
#ifdef _WIN32
std::string BuildConnectionString(Section const& section) {
  std::ostringstream ss;
  for (auto const& [k, v] : section) {
    if (!v.empty()) {
      ss << k << "=" << v << ";";
    }
  }
  return ss.str();
}

StatusRecord AllocateEnvAndDbc(SQLHENV& env, SQLHDBC& dbc) {
  SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
  if (!SQL_SUCCEEDED(rc)) {
    return {SQLStates::k_HY000(),
            "Failed to allocate ODBC environment handle."};
  }

  rc = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
  if (!SQL_SUCCEEDED(rc)) {
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    env = nullptr;
    return {SQLStates::k_HY000(), "Failed to set ODBC environment attributes."};
  }

  rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
  if (!SQL_SUCCEEDED(rc)) {
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    env = nullptr;
    return {SQLStates::k_HY000(), "Failed to allocate ODBC connection handle."};
  }
  return StatusRecord::Ok();
}

StatusRecord ExtractOdbcError(SQLHANDLE handle, SQLSMALLINT handle_type) {
  SQLCHAR sqlstate[6] = {};
  SQLCHAR msg[1024] = {};
  SQLINTEGER native = 0;
  SQLSMALLINT len = 0;

  SQLGetDiagRec(handle_type, handle, 1, sqlstate, &native, msg, sizeof(msg),
                &len);

  return {std::string(reinterpret_cast<char*>(sqlstate), 5),
          std::string(reinterpret_cast<char*>(msg))};
}

StatusRecord CheckSqlInfo(SQLHDBC dbc, SQLUSMALLINT info_type,
                          char const* name) {
  SQLCHAR buf[256] = {};
  SQLSMALLINT len = 0;

  SQLRETURN rc = SQLGetInfo(dbc, info_type, buf, sizeof(buf), &len);
  if (!SQL_SUCCEEDED(rc) || len == 0) {
    return {SQLStates::k_HY000(), std::string("Failed to retrieve ") + name};
  }
  return StatusRecord::Ok();
}

StatusRecord NormalizeOAuthMechanism(Section& section) {
  auto it = section.find(kOAuthMechanism);
  if (it == section.end() || it->second.empty()) {
    return {SQLStates::k_HY000(), "OAuthMechanism is missing or empty."};
  }

  std::string oauth_value;
  if (it->second == "Service Authentication") {
    if (section[kKeyFilePath].empty()) {
      return {SQLStates::k_HY000(), "KeyFilePath is missing or empty."};
    }
    oauth_value = std::to_string(
        static_cast<int>(OauthMechanism::kServiceAndUserAccount));
  } else if (it->second == "Application Default Credentials") {
    oauth_value =
        std::to_string(static_cast<int>(OauthMechanism::kApplicationDefault));
    section[kKeyFilePath].clear();
  } else if (it->second == "External Account Authentication") {
    if (section[kKeyFilePath].empty()) {
      return {SQLStates::k_HY000(), "Config File Path is missing or empty."};
    }
    oauth_value =
        std::to_string(static_cast<int>(OauthMechanism::kExternalUser));
  } else {
    return {SQLStates::k_HY000(),
            "OAuthMechanism must be 'Service Authentication', "
            "'Application Default Credentials', or "
            "'External Account Authentication'."};
  }

  section[kOAuthMechanism] = oauth_value;
  return StatusRecord::Ok();
}
#endif  // _WIN32
}  // namespace google::cloud::odbc_bq_driver_internal
