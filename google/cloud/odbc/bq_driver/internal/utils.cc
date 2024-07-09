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

#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/internal/getenv.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

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
  LONG status = RegOpenKeyEx(HKEY_CURRENT_USER, LPCSTR(registry_key.c_str()), 0,
                             KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    std::string msg = "Can't open registry key with path: ";
    msg.append(registry_key);
    return StatusRecord{SQLStates::k_HY000(), msg};
  }

  DWORD num_values;
  DWORD longest_data_len;
  status = RegQueryInfoKey(key_handle, NULL, NULL, NULL, NULL, NULL, NULL,
                           &num_values, NULL, &longest_data_len, NULL, NULL);

  BYTE buffer[1024];
  TCHAR property_name[kMaxValueNameLen];
  DWORD buffer_len = kMaxValueNameLen;

  for (int i = 0, status = ERROR_SUCCESS; i < num_values; i++) {
    buffer_len = kMaxValueNameLen;
    property_name[0] = '\0';
    status = RegEnumValue(key_handle, i, property_name, &buffer_len, NULL, NULL,
                          NULL, NULL);
    if (status == ERROR_SUCCESS) {
      buffer_len = longest_data_len;
      buffer[0] = '\0';
      LONG query_status = RegQueryValueEx(key_handle, property_name, 0, NULL,
                                          buffer, &buffer_len);
      std::string value((char*)buffer);
      section[property_name] = value;
    }
  }
  RegCloseKey(key_handle);
  return std::make_shared<Section>(section);
}

StatusRecordOr<std::shared_ptr<Sections>> ParseConfig(
    std::string const& registry_key) {
  HKEY key_handle;
  LONG status = RegOpenKeyEx(HKEY_CURRENT_USER, LPCSTR(registry_key.c_str()), 0,
                             KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    return StatusRecord{SQLStates::k_HY000(),
                        "Can't open registry key with path: " + registry_key};
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
        return get_sections_response_status.GetStatusRecord();
      }
      auto get_sections_response = *get_sections_response_status;
      sections[subkey_name] = *get_sections_response;
    }
  }
  RegCloseKey(key_handle);
  return std::make_shared<Sections>(sections);
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
  } else {
    std::string msg = "Can't open file with path: ";
    msg.append(file_path);
    return StatusRecord{SQLStates::k_HY000(), msg};
  }
  return std::make_shared<Sections>(sections);
}

#endif  //_WIN32

StatusRecordOr<Section> ParseConnectionString(std::string& str) {
  Section section;
  std::vector<std::string> splits = Split(str, ";");
  for (std::string& property : splits) {
    Trim(property);
    if (property.empty()) {
      continue;
    }
    std::vector<std::string> property_splits = Split(property, "=", 2);
    if (property_splits.size() < 2) {
      return StatusRecord{SQLStates::k_HY000(), "Invalid Connection String"};
    }
    std::string field = property_splits[0];
    std::string value = Join(property_splits, "", 1);
    Trim(field);
    Trim(value);
    if (field.empty() || value.empty()) {
      continue;
    }
    section[field] = value;
  }
  return section;
}

std::string GetPathToOdbcIni() {
  absl::optional<std::string> path = google::cloud::internal::GetEnv("ODBCINI");
  if (path) {
    return *path;
  }
  absl::optional<std::string> home = google::cloud::internal::GetEnv("HOME");
  if (home) {
    return *home + "/.odbc.ini";
  }
  return "";
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

std::string Utf16ToUtf8(std::wstring const& utf16Str) {
#ifdef _WIN32
  int utf8Length = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), -1, NULL,
                                       0, NULL, NULL);
  if (utf8Length == 0) {
    throw std::runtime_error("Error determining buffer size");
  }
  std::string utf8Str(utf8Length, 0);
  int result = WideCharToMultiByte(CP_UTF8, 0, utf16Str.c_str(), -1,
                                   &utf8Str[0], utf8Length, NULL, NULL);
  if (result == 0) {
    throw std::runtime_error("Error converting string");
  }
  return utf8Str;
#else
    std::wcout<<"utf16Str :"<<utf16Str.c_str()<<std::endl;
    iconv_t cd = iconv_open("UTF-8", "WCHAR_T");
    if (cd == (iconv_t)-1) {
        throw std::runtime_error("iconv_open failed: " + std::string(strerror(errno)));
    }

    std::vector<char> inbuf(reinterpret_cast<const char*>(utf16Str.data()),
                            reinterpret_cast<const char*>(utf16Str.data() + utf16Str.length()));
    size_t inbytesleft = inbuf.size();
    size_t outbytesleft = inbytesleft * 3; // Allocate more space for output

    std::string utf8str(outbytesleft, '\0');
    char* inptr = inbuf.data();
    char* outptr = &utf8str[0];

    size_t res = iconv(cd, &inptr, &inbytesleft, &outptr, &outbytesleft);
    if (res == static_cast<size_t>(-1)) {
        std::cerr << "conversion for utf16 failed: " << strerror(errno)
                  << " (errno: " << errno << ")" << std::endl;
        iconv_close(cd);
        throw std::runtime_error("iconv16 failed");
    }

    iconv_close(cd);
    std::cout<<"utf16Str converted :"<<utf8str<<std::endl;
    utf8str.resize(outptr - &utf8str[0]);
    return utf8str;
#endif
}

std::wstring Utf8ToUtf16(std::string const& utf8Str) {
#ifdef _WIN32
  int utf16Length =
      MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
  if (utf16Length == 0) {
    throw std::runtime_error("Error determining buffer size");
  }
  std::wstring utf16Str(utf16Length, 0);
  int result = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1,
                                   &utf16Str[0], utf16Length);
  if (result == 0) {
    throw std::runtime_error("Error converting string");
  }
  return utf16Str;
#else

  iconv_t cd = iconv_open("UTF-16LE", "UTF-8");
  int errorno = -1;
  int* errorptr = &errorno;
  if (cd == reinterpret_cast<iconv_t>(errorptr)) {
    std::cerr << "iconv_open failed: " << strerror(errno) << std::endl;
    throw std::runtime_error("iconv_open failed");
  }

  // Use string length for input byte count
  size_t inbytesleft = utf8Str.length();
  // Allocate more space for the output buffer
  size_t outbytesleft = inbytesleft * sizeof(wchar_t);
  std::wstring utf16str(outbytesleft + sizeof(wchar_t), L'\0');

  char* inbuf = const_cast<char*>(utf8Str.data());
  char* outbuf = reinterpret_cast<char*>(utf16str.data());

  size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  if (res == static_cast<size_t>(-1)) {
    std::cerr << "conversion from UTF-8 to UTF-16 failed: " << strerror(errno)
              << std::endl;
    iconv_close(cd);
    throw std::runtime_error("iconv failed");
  }

  iconv_close(cd);

  // Resize the output string to the actual converted size
  utf16str.resize((outbuf - reinterpret_cast<char*>(utf16str.data())) /
                  sizeof(wchar_t));

  return utf16str;
#endif
}
}  // namespace google::cloud::odbc_bq_driver_internal
