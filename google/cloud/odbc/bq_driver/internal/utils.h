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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_UTILS_H

#ifdef _WIN32

#define _WINSOCKAPI_
// This prevents Windows.h from defining min/max macros
#define NOMINMAX
#include <limits>
#include <windows.h>
#undef max
#undef GetJob
#include <winreg.h>
#endif  //_WIN32

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/status_or.h"
#include <algorithm>
#include <codecvt>
#include <fstream>
#include <locale>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

using Section = std::map<std::string, std::string>;
using Sections = std::map<std::string, Section>;

inline void LTrim(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](char ch) {
            return (std::isspace(ch) == 0);
          }));
}

inline void RTrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](char ch) { return (std::isspace(ch) == 0); })
              .base(),
          s.end());
}

inline void Trim(std::string& s) {
  LTrim(s);
  RTrim(s);
}

/**
 * @param s The string to be split
 *
 * @param delimiter The substring which creates the splits. This will not be
 * included in the output
 *
 * @param limit The maximum size of the output list. Splitting stops when the
 * size reaches this. 0/undefined imples it will find all possible splits
 *
 * @return Vector containing the substrings
 *
 * @example Split("SOFTWARE\\ODBC\\ODBC.INI", "\\", 2) will return ["SOFTWARE",
 * "ODBC"]
 */
std::vector<std::string> Split(std::string const& s,
                               std::string const& delimiter = " ",
                               int limit = 0);

std::string Join(std::vector<std::string> v, std::string const& separator = "",
                 int start_ind = 0);

odbc_internal::StatusRecordOr<std::string> Utf16ToUtf8(
    std::wstring const& utf_16_str);

odbc_internal::StatusRecordOr<std::wstring> Utf8ToUtf16(
    std::string const& utf_8_str);

odbc_internal::StatusRecordOr<std::string> ConvertSQLWCHARToString(
    SQLWCHAR* in_str, SQLINTEGER in_str_len);

#ifdef _WIN32

constexpr int kMaxKeyLength = 4096;

constexpr int kMaxValueNameLen = 4096;

/**
 * @param registry_key Registry key path assuming it has a flat hierarchy. Keys
 * which have sub_keys are ignored.
 *
 * @return Map of property->value(string-string)
 *
 * @example GetSectionWin("SOFTWARE\\ODBC\\ODBC.INI\\ODBCTestsDSN")
 */
odbc_internal::StatusRecordOr<std::shared_ptr<Section>> GetSectionWin(
    std::string const& registry_key);

/**
 * @param registry_key Registry key path assuming it keys which have sub-keys.
 *  When it looks for keys in the registry key path, it will ignore keys which
 *  do not have sub-keys
 *
 * @return Map of depth 2
 *
 * @example ParseConfig("SOFTWARE\\ODBC\\ODBC.INI")
 */
odbc_internal::StatusRecordOr<std::shared_ptr<Sections>> ParseConfig(
    std::string const& registry_key);

#else

odbc_internal::StatusRecordOr<std::shared_ptr<Sections>> ParseConfig(
    std::string const& file_path);

#endif  //_WIN32

odbc_internal::StatusRecordOr<Section> ParseConnectionString(std::string& str);

// Common validation used by both SQLTables and SQLColumns
odbc_internal::StatusRecord ValidateTableParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* table_name, SQLSMALLINT table_name_len, SQLULEN metadata_id);

std::string GetPathToOdbcIni();

inline std::string CastOdbcRegexToCppRegex(std::string const& str) {
  auto percent_filter_out =
      std::regex_replace(str, std::regex("^%|([^\\\\])%"), "$1.*");
  auto underscore_filter_out = std::regex_replace(
      percent_filter_out, std::regex("^_|([^\\\\])_"), "$1.");
  return std::regex_replace(underscore_filter_out, std::regex("\\\\"), "");
}

std::vector<std::string> SplitTableTypes(std::string const& table_types);

std::regex BuildRegex(std::string filter_pattern, SQLULEN metadata_id);

inline bool IsSearchPatternArgument(std::string const& arg) {
  return (absl::StrContains(arg, "_") || absl::StrContains(arg, "%") ||
          absl::StrContains(arg, "\\"));
}

inline bool IsQuotedIDArgument(std::string const& arg) {
  return (absl::StrContains(arg, "'") || absl::StrContains(arg, "\""));
}

inline void RemoveQuotes(std::string& str) {
  str.erase(std::remove(str.begin(), str.end(), '\''), str.end());
  str.erase(std::remove(str.begin(), str.end(), '\"'), str.end());
}

inline void SanitizeIdentifierArgument(std::string& id_arg) {
  if (IsQuotedIDArgument(id_arg)) {
    // For quotes arguments, remove leading and trailing blanks and remove
    // quotes
    Trim(id_arg);
    RemoveQuotes(id_arg);
  } else {
    // For non-quoted args, remove trailing blanks and convert the string to
    // upper case.
    RTrim(id_arg);
    std::transform(id_arg.begin(), id_arg.end(), id_arg.begin(), ::toupper);
  }
}
#ifdef WIN32
odbc_internal::StatusRecord AddDSNToRegistry(std::string const& dsn_ame,
                                             std::string const& driver,
                                             std::string const& description,
                                             std::string const& server_name,
                                             std::string const& database_name,
                                             WORD fRequest);

odbc_internal::StatusRecord EditDSNInRegistry(
    std::string const& dsn_name, std::string const& lpsz_driver,
    std::string const& email, std::string const& server_name,
    std::string const& key_file_path, std::string const& oAuthMechanism,
    std::string const& catalog, std::string const& dataset_name, WORD fRequest);

odbc_internal::StatusRecord RemoveDSNFromRegistry(std::string const& dsn_name,
                                                  WORD fRequest);

std::string GetDSNInfo(std::string const& dsn_name, WORD fRequest);

#endif

inline int GetWholeDigitCount(std::string& src_str) {
  int digit_count = 0;
  for (char ch : src_str) {
    if (std::isdigit(ch)) {
      ++digit_count;
    }
  }
  return digit_count;
}
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_UTILS_H
