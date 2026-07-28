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
#include <limits>
#include <windows.h>
#undef max
#undef GetJob
#include <winreg.h>
extern HINSTANCE g_hDllInstance;
#else
#include <dlfcn.h>
#endif  //_WIN32

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include "google/cloud/status_or.h"
#include "re2/re2.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <codecvt>
#include <fstream>
#include <functional>
#include <future>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {
extern bool g_suppress_dropdown;

using Section = std::map<std::string, std::string>;
using Sections = std::map<std::string, Section>;
using google::cloud::bigquery_v2_minimal_internal::ConnectionProperty;

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

size_t NormalizeBufferSize(int size, size_t max_size = 256);

size_t BufferSizeForType(SQLSMALLINT type, size_t requested);

// -----------------------------------------------------------------------------
// Generic Parallel Execution Utility
// -----------------------------------------------------------------------------

// Executes a function in parallel for a list of inputs, limiting the number of
// concurrent threads to max_threads.
//
// TaskInput: The type of a single item in the input vector.
// TaskResult: The type of data returned by the function on success.
//
// Returns: A vector containing the results of all successful tasks in input
// order, or the StatusRecord of the first (in input order) error encountered.
// After a task fails, tasks not yet started are skipped, but tasks already
// in flight run to completion before this function returns.
//
// Implemented as a fixed pool of max_threads workers pulling the next input
// from a shared atomic index. Each worker moves on to the next input the
// moment its current task finishes, so one slow task (a straggler REST call)
// never stalls the dispatch of the remaining work -- a previous
// dispatcher-based implementation blocked on the oldest in-flight task to
// free a slot, which serialized the whole batch behind stragglers and made
// catalog enumeration latency vary heavily from run to run.
template <typename TaskInput, typename TaskResult>
odbc_internal::StatusRecordOr<std::vector<TaskResult>> ExecuteParallelTasks(
    std::uint32_t max_threads, std::vector<TaskInput> const& inputs,
    std::function<odbc_internal::StatusRecordOr<TaskResult>(TaskInput const&)>
        task_func) {
  if (inputs.empty()) {
    return std::vector<TaskResult>{};
  }
  // A misconfigured MaxThreads of 0 previously spun the dispatch loop forever;
  // treat it as serial execution. Never spawn more workers than inputs.
  std::size_t const num_workers = (std::min)(
      static_cast<std::size_t>((std::max)(max_threads, std::uint32_t{1})),
      inputs.size());

  // One pre-sized slot per input: workers write disjoint indices, so no
  // synchronization is needed, and results come back in input order.
  std::vector<std::optional<odbc_internal::StatusRecordOr<TaskResult>>> slots(
      inputs.size());
  std::atomic<std::size_t> next_index{0};
  std::atomic<bool> error_occurred{false};
  std::exception_ptr first_exception;
  std::mutex exception_mutex;

  auto worker = [&] {
    while (!error_occurred.load(std::memory_order_relaxed)) {
      std::size_t const index =
          next_index.fetch_add(1, std::memory_order_relaxed);
      if (index >= inputs.size()) {
        return;
      }
      try {
        auto result = task_func(inputs[index]);
        if (!result) {
          error_occurred.store(true, std::memory_order_relaxed);
        }
        slots[index] = std::move(result);
      } catch (...) {
        // Propagate the exception on the calling thread (std::async did this
        // via future::get); letting it escape a worker would call terminate.
        std::lock_guard<std::mutex> lock(exception_mutex);
        if (!first_exception) {
          first_exception = std::current_exception();
        }
        error_occurred.store(true, std::memory_order_relaxed);
        return;
      }
    }
  };

  std::vector<std::thread> workers;
  workers.reserve(num_workers);
  for (std::size_t i = 0; i < num_workers; ++i) {
    workers.emplace_back(worker);
  }
  for (auto& thread : workers) {
    thread.join();
  }

  if (first_exception) {
    std::rethrow_exception(first_exception);
  }

  std::vector<TaskResult> aggregated_results;
  aggregated_results.reserve(inputs.size());
  for (auto& slot : slots) {
    if (!slot.has_value()) {
      continue;  // Skipped after an error; an errored slot below reports it.
    }
    if (!*slot) {
      return slot->GetStatusRecord();
    }
    aggregated_results.push_back(std::move(**slot));
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
    std::string_view utf_8_str);

odbc_internal::StatusRecordOr<std::string> BqConvertSQLWCHARToString(
    SQLWCHAR* in_str, SQLINTEGER in_str_len);

std::wstring SQLWcharToWstring(const SQLWCHAR* in_str);

bool IsDiagIdentifierString(SQLSMALLINT DiagIdentifier);

bool IsFieldIdentifierString(SQLSMALLINT FieldIdentifier);

bool IsInfoTypeString(SQLUSMALLINT InfoType);

// To validate target c type supported in SQLGetData
bool CheckTargetType(int c_type);

// To validate target c type is length sensitive in SQLBindCol
bool IsLengthSensitiveType(SQLSMALLINT c_type);

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
void ShowErrorWindow(HWND hwnd, std::string const message);
void setWindowIcon(HWND hwnd);
std::string GetRootsPemPath();

std::string BuildConnectionString(Section const& section);

odbc_internal::StatusRecord AllocateEnvAndDbc(SQLHENV& env, SQLHDBC& dbc);

odbc_internal::StatusRecord ExtractOdbcError(SQLHANDLE handle,
                                             SQLSMALLINT handle_type);

odbc_internal::StatusRecord CheckSqlInfo(SQLHDBC dbc, SQLUSMALLINT info_type,
                                         char const* name);

odbc_internal::StatusRecord NormalizeOAuthMechanism(Section& section);

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
                                     std::string const& key,
                                     std::string const& default_value = "") {
  auto it = std::find_if(
      attribute_map.begin(), attribute_map.end(), [&](auto const& pair) {
        return std::equal(
            pair.first.begin(), pair.first.end(), key.begin(), key.end(),
            [](char a, char b) { return std::tolower(a) == std::tolower(b); });
      });

  return (it != attribute_map.end() && !it->second.empty()) ? it->second
                                                            : default_value;
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

std::string GetDefaultPemFile();

// Translates an ODBC LIKE pattern into an RE2-compatible regex pattern.
// Implemented as a manual single-pass loop rather than chained
// std::regex_replace calls to avoid std::regex DFA initialization crashes
// on some hosts (e.g. SAP HANA with libstdc++/libc++).
std::string CastOdbcRegexToCppRegex(std::string const& str);

std::vector<std::string> SplitTableTypes(std::string const& table_types);

std::unique_ptr<re2::RE2> BuildRegex(std::string filter_pattern,
                                     SQLULEN metadata_id);

inline bool IsSearchPatternArgument(std::string const& arg) {
  return (absl::StrContains(arg, "_") || absl::StrContains(arg, "%") ||
          absl::StrContains(arg, "\\"));
}

inline bool IsQuotedIDArgument(std::string const& arg) {
  return (absl::StrContains(arg, "'") || absl::StrContains(arg, "\""));
}

inline bool isValidUint32(char const* str) {
  if (!str || *str == '\0') return false;
  std::string t = str;
  t.erase(0, t.find_first_not_of('0'));

  static std::string const kMaxVal = std::to_string(UINT32_MAX);
  return (t.size() < kMaxVal.size()) ||
         (t.size() == kMaxVal.size() && t <= kMaxVal);
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
