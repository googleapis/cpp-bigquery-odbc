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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_UTILS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_UTILS_H

#ifdef _WIN32

#define _WINSOCKAPI_
#include <windows.h>
#include <winreg.h>

#endif //_WIN32

#include <algorithm>
#include <fstream>
#include <memory>
#include <map>
#include <vector>
#include <string>

#include "google/cloud/status_or.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bq_driver {

using Section = std::map<std::string, std::string>;
using Sections = std::map<std::string, Section>;

inline void LTrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(),
    [](char ch) { return (std::isspace(ch) == 0); }));
}

inline void RTrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](char ch) { return (std::isspace(ch) == 0); }).base(), s.end());
}

inline void Trim(std::string& s) {
  LTrim(s);
  RTrim(s);
}

/**
 * @param s The string to be split
 *
 * @param delimiter The substring which creates the splits. This will not be included
 *  in the output
 *
 * @param limit The maximum size of the output list. Splitting stops when the size reaches this
 *  0/undefined imples it will find all possible splits
 *
 * @return Vector containing the substrings
 *
 * @example Split("SOFTWARE\\ODBC\\ODBC.INI", "\\", 2) will return ["SOFTWARE", "ODBC"]
*/
std::vector<std::string> Split(std::string s, std::string delimiter, int limit = 0);

std::string Join(std::vector<std::string> v, std::string separator = "", int start_ind = 0);

#ifdef _WIN32

constexpr int kMaxKeyLength = 4096;

constexpr int kMaxValueNameLen = 4096;

/**
 * @param registry_key Registry key path assuming it has a flat hierarchy. Keys which have sub_keys
 *  are ignored.
 *
 * @return Map of property->value(string-string)
 *
 * @example GetSectionWin("SOFTWARE\\ODBC\\ODBC.INI\\ODBCTestsDSN")
*/
StatusOr<std::shared_ptr<Section>> GetSectionWin(std::string const& registry_key);


/**
 * @param registry_key Registry key path assuming it keys which have sub-keys.
 *  When it looks for keys in the registry key path, it will ignore keys which
 *  do not have sub-keys
 *
 * @return Map of depth 2
 *
 * @example ParseConfig("SOFTWARE\\ODBC\\ODBC.INI")
*/
StatusOr<std::shared_ptr<Sections>> ParseConfig(std::string const& registry_key);

#else

StatusOr<std::shared_ptr<Sections>> ParseConfig(std::string const& file_path);

#endif //_WIN32

Section ParseConnectionString(std::string& str);

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_UTILS_H
