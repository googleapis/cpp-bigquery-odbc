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
  LONG status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, LPCSTR(registry_key.c_str()),
                             0, KEY_READ, &key_handle);
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
        section[std::string(property_name)] = value;
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
#endif /* WIN64 */
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
#endif /* WIN32 */
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

StatusRecordOr<std::string> Utf16ToUtf8(std::wstring const& utf_16_str) {
  if (utf_16_str.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "utf16 string is empty/Null"};
  }
#ifdef _WIN32
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int utf8Length = WideCharToMultiByte(CP_ACP, 0, utf_16_str.c_str(), -1, NULL,
                                       0, NULL, NULL);
  if (utf8Length == 0) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Error determining buffer size while converting wstring to string"};
  }
  std::string utf8Str(utf8Length, 0);
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int result = WideCharToMultiByte(CP_ACP, 0, utf_16_str.c_str(), -1,
                                   &utf8Str[0], utf8Length, NULL, NULL);
  if (result == 0) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Error while converting wstring to string"};
  }
  return utf8Str;
#else
  iconv_t cd = iconv_open("UTF-8", "WCHAR_T");
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

StatusRecordOr<std::wstring> Utf8ToUtf16(std::string const& utf_8_str) {
  if (utf_8_str.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "utf_8_str string isempty/Null"};
  }
#ifdef _WIN32
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
  int utf16Length =
      MultiByteToWideChar(CP_ACP, 0, utf_8_str.c_str(), -1, NULL, 0);
  if (utf16Length == 0) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Error determining buffer size while converting string to wstring"};
  }
  std::wstring utf16Str(utf16Length, 0);
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
  int result = MultiByteToWideChar(CP_ACP, 0, utf_8_str.c_str(), -1,
                                   &utf16Str[0], utf16Length);
  if (result == 0) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Error while converting string to wstring"};
  }
  return utf16Str;
#else

  iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
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

StatusRecordOr<std::string> ConvertSQLWCHARToString(SQLWCHAR* in_str,
                                                    SQLINTEGER in_str_len) {
  if (((in_str != nullptr) && (in_str[0] == '\0'))) {
    return StatusRecord{SQLStates::k_HY000(), "in_str string is empty/Null"};
  }
  std::wstring stmt_txt_wstr;
  std::wstring wstr(reinterpret_cast<wchar_t const*>(in_str));
  if (in_str_len == SQL_NTS || in_str_len == NULL) {
    in_str_len = wstr.size();
  }
  stmt_txt_wstr.reserve(in_str_len);
  for (SQLINTEGER i = 0; i < in_str_len; ++i) {
    stmt_txt_wstr.push_back(static_cast<wchar_t>(in_str[i]));
  }
  return Utf16ToUtf8(stmt_txt_wstr);
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
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - catalog length is invalid"};
  }
  if (schema_name_len < 0 && schema_name_len != SQL_NTS) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - schema length is invalid"};
  }
  if (table_name_len < 0 && table_name_len != SQL_NTS) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - table name length is invalid"};
  }
  if (metadata_id == SQL_TRUE) {
    if (!catalog_name) {
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for catalog name"};
    }
    if (!schema_name) {
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for schema name"};
    }
    if (!table_name) {
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for table name"};
    }
  }
  return StatusRecord::Ok();
}
#ifdef _WIN32
StatusRecord AddDSNToRegistry(const std::string& dsnName, 
                      const std::string& driver, 
                      const std::string& description, 
                      const std::string& serverName, 
                      const std::string& databaseName) 
{
    const std::string registryPath = "SOFTWARE\\ODBC\\ODBC.INI\\" + dsnName;
    const std::string odbcPath = "SOFTWARE\\ODBC\\ODBC.INI\\ODBC Data Sources";

    HKEY hKey = nullptr;

    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, registryPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
       return StatusRecord{SQLStates::k_HY000(),"Failed to create or open registry key for DSN"};
    }

    if (RegSetValueExA(hKey, "Driver", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(driver.c_str()), 
            static_cast<DWORD>(driver.size() + 1)) != ERROR_SUCCESS) 
    {
      RegCloseKey(hKey);
      return StatusRecord{SQLStates::k_HY000(),"Failed to set Driver value for DSN"};   
    }

    if (RegSetValueExA(hKey, "Description", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(description.c_str()), 
            static_cast<DWORD>(description.size() + 1)) != ERROR_SUCCESS) 
    {
       RegCloseKey(hKey);
       return StatusRecord{SQLStates::k_HY000(),"Failed to set Description value for DSN"}; 
    }

    if (RegSetValueExA(hKey, "Server", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(serverName.c_str()), 
            static_cast<DWORD>(serverName.size() + 1)) != ERROR_SUCCESS) 
    {  RegCloseKey(hKey);
       return StatusRecord{SQLStates::k_HY000(),"Failed to set Server value for DSN"}; 
    }

    if (RegSetValueExA(hKey, "Database", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(databaseName.c_str()), 
            static_cast<DWORD>(databaseName.size() + 1)) != ERROR_SUCCESS) 
    {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to set Database value for DSN"}; 
    }

    RegCloseKey(hKey);

    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, odbcPath.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return StatusRecord{SQLStates::k_HY000(),"Failed to open ODBC Data Sources registry key"}; 
    }

    if (RegSetValueExA(hKey, dsnName.c_str(), 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(driver.c_str()), 
            static_cast<DWORD>(driver.size() + 1)) != ERROR_SUCCESS) 
    {   
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to add DSN to ODBC Data Sources"}; 
    }

    RegCloseKey(hKey);
    return StatusRecord::Ok();
}
StatusRecord EditDSNInRegistry(const std::string& dsnName, 
                       const std::string& lpszDriver, 
                       const std::string& email, 
                       const std::string& serverName, 
                       const std::string& keyFilePath, 
                       const std::string& oAuthMechanism, 
                       const std::string& catalog, 
                       const std::string& datasetName) 
{
    const std::string registryPath = "SOFTWARE\\ODBC\\ODBC.INI\\" + dsnName;

    HKEY hKey = nullptr;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, registryPath.c_str(), 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return StatusRecord{SQLStates::k_HY000(),"Failed to open registry key for DSN"};
    }

    if (!lpszDriver.empty() && RegSetValueExA(hKey, "Driver", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(lpszDriver.c_str()), 
            static_cast<DWORD>(lpszDriver.size() + 1)) != ERROR_SUCCESS) 
    { 
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to modify Driver value for DSN"};
    }

    if (!email.empty() && RegSetValueExA(hKey, "Email", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(email.c_str()), 
            static_cast<DWORD>(email.size() + 1)) != ERROR_SUCCESS) 
    {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to modify Email value for DSN"};
    }

    if (!serverName.empty() && RegSetValueExA(hKey, "Server", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(serverName.c_str()), 
            static_cast<DWORD>(serverName.size() + 1)) != ERROR_SUCCESS) 
    {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to modify Server value for DSN"};
    }

    if (!keyFilePath.empty() && RegSetValueExA(hKey, "KeyFilePath", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(keyFilePath.c_str()), 
            static_cast<DWORD>(keyFilePath.size() + 1)) != ERROR_SUCCESS) 
    {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to modify KeyFilePath value for DSN"};
       
    }

    if (!oAuthMechanism.empty() && RegSetValueExA(hKey, "OAuthMechanism", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(oAuthMechanism.c_str()), 
            static_cast<DWORD>(oAuthMechanism.size() + 1)) != ERROR_SUCCESS) 
    {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to modify OAuthMechanism value for DSN"};
    }

    if (!catalog.empty() && RegSetValueExA(hKey, "Catalog", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(catalog.c_str()), 
            static_cast<DWORD>(catalog.size() + 1)) != ERROR_SUCCESS) 
    {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to modify Catalog value for DSN"};
    }

    if (!datasetName.empty() && RegSetValueExA(hKey, "Dataset", 0, REG_SZ, 
            reinterpret_cast<const BYTE*>(datasetName.c_str()), 
            static_cast<DWORD>(datasetName.size() + 1)) != ERROR_SUCCESS) 
    {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to modify Dataset value for DSN"};
    }

    RegCloseKey(hKey);

    return StatusRecord::Ok();
}
StatusRecord RemoveDSNFromRegistry(const std::string& dsnName) {

    const std::string registryPath = "SOFTWARE\\ODBC\\ODBC.INI\\" + dsnName;
    const std::string odbcPath = "SOFTWARE\\ODBC\\ODBC.INI\\ODBC Data Sources";

    HKEY hKey = nullptr;

    if (RegDeleteKeyA(HKEY_LOCAL_MACHINE, registryPath.c_str()) != ERROR_SUCCESS) {
        return StatusRecord{SQLStates::k_HY000(),"Failed to remove registry key for DSN"};
    }

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, odbcPath.c_str(), 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return StatusRecord{SQLStates::k_HY000(),"Failed to open ODBC Data Sources registry key"};
    }

    if (RegDeleteValueA(hKey, dsnName.c_str()) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return StatusRecord{SQLStates::k_HY000(),"Failed to remove DSN from ODBC Data Sources"};
    }

    RegCloseKey(hKey);

    return StatusRecord::Ok();
}
#endif

}  // namespace google::cloud::odbc_bq_driver_internal
