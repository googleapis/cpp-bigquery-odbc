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

namespace google::cloud::odbc_bq_driver_internal {

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

StatusOr<std::shared_ptr<Section>> GetSectionWin(
    std::string const& registry_key) {
  Section section;
  HKEY key_handle;
  LONG status = RegOpenKeyEx(HKEY_CURRENT_USER, LPCSTR(registry_key.c_str()), 0,
                             KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    return Status(StatusCode::kInvalidArgument,
                  "Can't open registry key with path: " + registry_key);
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

StatusOr<std::shared_ptr<Sections>> ParseConfig(
    std::string const& registry_key) {
  HKEY key_handle;
  LONG status = RegOpenKeyEx(HKEY_CURRENT_USER, LPCSTR(registry_key.c_str()), 0,
                             KEY_READ, &key_handle);
  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    return Status(StatusCode::kInvalidArgument,
                  "Can't open registry key with path: " + registry_key);
  }

  TCHAR subkey_name[kMaxKeyLength];
  DWORD name_len;
  DWORD num_sub_keys = 0;

  status = RegQueryInfoKey(key_handle, NULL, NULL, NULL, &num_sub_keys, NULL,
                           NULL, NULL, NULL, NULL, NULL, NULL);

  if (status != ERROR_SUCCESS) {
    RegCloseKey(key_handle);
    return Status(StatusCode::kInternal,
                  "RegQueryInfoKey failed with error code: " + status);
  }

  Sections sections;
  // List all the sections
  for (int i = 0; i < num_sub_keys; i++) {
    name_len = kMaxKeyLength;
    status = RegEnumKeyEx(key_handle, i, subkey_name, &name_len, NULL, NULL,
                          NULL, NULL);
    if (status == ERROR_SUCCESS) {
      auto get_sections_response =
          GetSectionWin(registry_key + "\\" + std::string(subkey_name));
      if (get_sections_response.status().code() == StatusCode::kOk) {
        sections[subkey_name] = *get_sections_response.value();
      }
    }
  }
  RegCloseKey(key_handle);
  return std::make_shared<Sections>(sections);
}

#else

StatusOr<std::shared_ptr<Sections>> ParseConfig(std::string const& file_path) {
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
    return Status(StatusCode::kInvalidArgument,
                  "Can't open file with path: " + file_path);
  }
  return std::make_shared<Sections>(sections);
}

#endif  //_WIN32

Section ParseConnectionString(std::string& str) {
  Section section;
  std::vector<std::string> splits = Split(str, ";");
  for (std::string const& property : splits) {
    std::vector<std::string> property_splits = Split(property, "=", 2);
    if (property_splits.size() < 2) {
      continue;
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

}  // namespace google::cloud::odbc_bq_driver_internal
