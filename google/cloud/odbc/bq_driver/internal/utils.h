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
extern HINSTANCE g_hDllInstance;
#endif  //_WIN32

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/status_or.h"
#include <algorithm>
#include <chrono>
#include <codecvt>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {
extern bool g_suppress_dropdown;

using Section = std::map<std::string, std::string>;
using Sections = std::map<std::string, Section>;
using google::cloud::bigquery_v2_minimal_internal::ConnectionProperty;

std::string const kDefaultDestDatasetId = "_bqodbc_temp_tables";

#ifdef _WIN64
// 64-bit
inline std::string k_trace_reg_path =
    R"(SOFTWARE\\Google\\ODBC Driver for BigQuery)";
#else
// 32-bit
inline std::string k_trace_reg_path =
    R"(SOFTWARE\\WOW6432Node\\Google\\ODBC Driver for BigQuery)";
#endif  // _WIN64

static std::string const kBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

/**
 * @brief Generates a cryptographically-seeded, unique ID string.
 *
 * @param length The length of the resulting string
 * @return A random string.
 */
std::string GenerateRandomId(int length = 16);

/**
 * @brief Generates a ID by prepending the current epoch time
 * to a random string, separated by an underscore.
 *
 * @return A unique ID string.
 */
std::string GenerateTableId();

// Converts a stringified double value into an integral string.
odbc_internal::StatusRecord DoubleStrToInt(std::string& double_str);

// -----------------------------------------------------------------------------
// Generic Parallel Execution Utility
// -----------------------------------------------------------------------------

// Executes a function in parallel for a list of inputs, limiting the number of
// concurrent threads to max_threads.
//
// TaskInput: The type of a single item in the input vector.
// TaskResult: The type of data returned by the function on success.
//
// Returns: A vector containing the results of all successful tasks, or the
// StatusRecord of the first error encountered.
template <typename TaskInput, typename TaskResult>
odbc_internal::StatusRecordOr<std::vector<TaskResult>> ExecuteParallelTasks(
    int max_threads, std::vector<TaskInput> const& inputs,
    std::function<odbc_internal::StatusRecordOr<TaskResult>(TaskInput const&)>
        task_func) {
  using FutureType = std::future<odbc_internal::StatusRecordOr<TaskResult>>;

  std::vector<FutureType> active_futures;
  std::vector<TaskResult> aggregated_results;
  odbc_internal::StatusRecord error_status = odbc_internal::StatusRecord::Ok();
  bool error_occurred = false;

  // Helper to collect result from a finished future
  auto process_future = [&](FutureType& f) {
    auto result = f.get();
    if (!result) {
      // Record the first error that occurs, but keep draining threads
      if (!error_occurred) {
        error_status = result.GetStatusRecord();
        error_occurred = true;
      }
    } else if (!error_occurred) {
      // Only store results if we are still in a success state
      aggregated_results.push_back(std::move(*result));
    }
  };

  for (auto const& input : inputs) {
    // If we have hit the thread limit, wait for at least one thread to finish
    while (active_futures.size() >= max_threads) {
      bool slot_freed = false;
      for (auto it = active_futures.begin(); it != active_futures.end();) {
        // Check if ready without blocking
        if (it->wait_for(std::chrono::milliseconds(1)) ==
            std::future_status::ready) {
          process_future(*it);
          it = active_futures.erase(it);
          slot_freed = true;
          break;  // We freed a slot, proceed to launch next task
        }
        ++it;
      }

      // If no threads finished yet, block on the oldest one to prevent spinning
      if (!slot_freed && !active_futures.empty()) {
        process_future(active_futures.front());
        active_futures.erase(active_futures.begin());
      }
    }

    // If an error occurred previously, we stop spawning new tasks,
    // but the loop continues to ensure we drain existing futures safely.
    if (!error_occurred) {
      active_futures.push_back(
          std::async(std::launch::async, task_func, input));
    }
  }

  // Wait for and collect all remaining threads
  for (auto& f : active_futures) {
    process_future(f);
  }

  if (error_occurred) {
    return error_status;
  }

  return aggregated_results;
}

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

inline void GetUpperStr(std::string& s) {
  std::transform(s.begin(), s.end(), s.begin(), ::toupper);
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

bool IsDiagIdentifierString(SQLSMALLINT DiagIdentifier);

bool IsFieldIdentifierString(SQLSMALLINT FieldIdentifier);

bool IsInfoTypeString(SQLUSMALLINT InfoType);

// To validate target c type supported in SQLGetData
bool CheckTargetType(int c_type);

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

HWND CreateLabel(HWND parent, char const* text, int x, int y, int width,
                 int height, int id);

HWND CreateEditBox(HWND parent, int x, int y, int width, int height, int id);

HWND CreateComboBox(HWND parent, int x, int y, int width, int height, int id);

HWND CreateButton(HWND parent, char const* text, int x, int y, int width,
                  int height, int id);

HWND CreateCheckBox(HWND parent, char const* text, int x, int y, int width,
                    int height, int id);

HWND CreateScrollableEditBox(HWND parent, int x, int y, int width, int height,
                             int id);
HWND CreateGroupBox(HWND parent, char const* text, int x, int y, int width,
                    int height, int id);
HWND CreateNumericEditBox(HWND parent, char const* text, int x, int y,
                          int width, int height, int id);

HWND CreateHyperlinkLabel(HWND parent, char const* text, int x, int y,
                          int width, int height, int id);
void setWindowIcon(HWND hwnd);
std::string GetRootsPemPath();

LRESULT CALLBACK InputSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                   LPARAM l_param, UINT_PTR sub_id,
                                   DWORD_PTR ref_data);

LRESULT CALLBACK EditBlockSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                       LPARAM l_param, UINT_PTR sub_id,
                                       DWORD_PTR ref_data);

LRESULT CALLBACK ComboBoxSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                      LPARAM l_param, UINT_PTR sub_id,
                                      DWORD_PTR ref_data);

LRESULT CALLBACK CheckboxSubclassProc(HWND hwnd, UINT msg, WPARAM w_param,
                                      LPARAM l_param, UINT_PTR sub_id,
                                      DWORD_PTR ref_data);

inline constexpr char kBigQueryDocsURL[] =
    "https://cloud.google.com/bigquery/docs/reference/odbc-jdbc-drivers?hl=en";

inline std::string GetValueOrDefault(Section const& attribute_map,
                                     std::string const& key) {
  auto it = std::find_if(
      attribute_map.begin(), attribute_map.end(), [&](auto const& pair) {
        return std::equal(
            pair.first.begin(), pair.first.end(), key.begin(), key.end(),
            [](char a, char b) { return std::tolower(a) == std::tolower(b); });
      });

  return (it != attribute_map.end()) ? it->second : "";
}

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

std::string GetOdbcTraceConfigPath();

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
#ifdef _WIN32
std::string ConvertLPCSTRToString(LPCSTR lpsz_attributes);

odbc_internal::StatusRecord AddLogTraceToRegistry(Section const& section);

#endif  // _WIN32

inline int GetWholeDigitCount(std::string& src_str) {
  int digit_count = 0;
  for (char ch : src_str) {
    if (std::isdigit(ch)) {
      ++digit_count;
    }
  }
  return digit_count;
}

odbc_internal::StatusRecord PopulateOutputConnectionString(
    SQLCHAR* out_conn_str, SQLSMALLINT out_conn_str_buflen,
    SQLSMALLINT* out_conn_str_len, std::string& conn_string,
    bool is_conn_str_empty = true);

std::string Base64Encode(uint8_t const* data, int length);

odbc_internal::StatusRecordOr<std::vector<ConnectionProperty>>
ParseQueryProperties(std::string const& input);

odbc_internal::StatusRecordOr<SQLUINTEGER> ParseStringToInteger(
    std::string const& input);

std::string GetLocationfromPSC(std::string const& psc);
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_UTILS_H
