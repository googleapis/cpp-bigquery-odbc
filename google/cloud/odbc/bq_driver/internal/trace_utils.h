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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_TRACE_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_TRACE_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/log/internal/log_sink_set.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace google::cloud::odbc_bq_driver_internal {

/////////////////////////////////////////////////////////////////////////////////
// TraceOptions facilitates ODBC tracing.
// Multiple instances of this class is forbidden.
//
// Usage:
//   auto options = CreateTraceOptionsConsole(true, 0);
//   if (!options.ok()) {
//      return options.status();
//   }
//   if (*options.logging_enabled) {
//     ....
//   }
//
//   auto options = CreateTraceOptionsFromODBCConfigs("/tmp/odbc.ini");
//   if (!options.ok()) {
//      return options.status();
//   }
//   if (*options.logging_enabled) {
//     ....
//   }
/////////////////////////////////////////////////////////////////////////////////
struct TraceOptions {
  // Disallow Copy and Assignment.
  TraceOptions(TraceOptions& other) = delete;
  void operator=(TraceOptions const&) = delete;

  //////////////////////////////////////////////////////////
  // Creates TraceOptions for emitting to Stdout.
  // No TraceFile is opened.
  //
  // Returns a singleton object for console tracing.
  //////////////////////////////////////////////////////////
  static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>>
  CreateTraceOptionsConsole(bool logging_enabled, int log_level);

  //////////////////////////////////////////////////////////
  // Creates TraceOptions for emitting to a trace file
  // specified in the ODBC ini Config file.
  //
  // Loads the ini config file, parses it and opens a trace file for logging.
  //
  // Returns a singleton object for file tracing
  //////////////////////////////////////////////////////////
  static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>>
  CreateTraceOptionsFile(std::string const& file_path);

  //////////////////////////////////////////////////////////
  // Creates TraceOptions based on the trace section in the
  // ODBC config file.
  //
  // Similar to the above version in that a trace file is opened for
  // logging but the ODBC config file is loaded and parsed by the caller.
  //
  // Returns a singleton object for file tracing
  //////////////////////////////////////////////////////////
  static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>>
  CreateTraceOptionsFile(std::shared_ptr<Sections> const& config_sections);

  ///////////////////////////////////////////////////////////
  // Get TraceOptions based on the trace section in the
  // ODBC config file.
  //
  // Returns a singleton object for file tracing
  ///////////////////////////////////////////////////////////
  static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>>
  GetTraceOption();

  static bool InitializeLogging(bool override = false);

  // Shared members.
  bool logging_enabled;
  bool is_file_closed;
  int log_level{0};
  int max_file_size{50};   // max number of log files (50).
  int max_file_count{50};  // max file size of a single file(50 MB)
  int current_file_index{0};
  std::string log_path;
  static std::string default_log_dir_;
  std::string log_file;
  std::ofstream trace_file;
  std::mutex
      m;  // Used for guarding any logging operations with file or stdout.
 private:
  TraceOptions() = default;
  static std::shared_ptr<TraceOptions> options_console_;
  static std::shared_ptr<TraceOptions> options_file_;
  static std::mutex
      mu_;  // used for guarding update of internal options members.
};

inline std::string const kLogTraceFileName = "googleodbcdriverforbigquery";

enum class LogLevel {
  kLogOff = 0,
  kLogError = 1,
  kLogWarning = 2,
  kLogInfo = 3,
};

class FileLogSink : public absl::LogSink {
 public:
  explicit FileLogSink(std::shared_ptr<TraceOptions> opts);
  ~FileLogSink();

  void Send(absl::LogEntry const& entry) override;
  [[nodiscard]] int GetLogLevel() const { return opts_->log_level; }

  static void InitializeFileLog(
      std::shared_ptr<TraceOptions> const& trace_opts);

 private:
  static std::unique_ptr<FileLogSink> file_sink; // static now
  std::shared_ptr<TraceOptions> opts_;
  std::string current_file_;
  std::mutex log_mutex_;
};

// Get abseil severity as per internal driver log levels
absl::LogSeverity GetAbslSeverity(LogLevel level);

///////////////////////////////////////////////////////////////
// Emit methods for actually printing the trace
// lines to stdout or a trace file.
///////////////////////////////////////////////////////////////

// Clients of this utility should use the two methods below to emit
// a trace of all parameters to an stdout or a trace file.
std::string CollectAndPrintArgs(std::string const& func_name,
                                TraceOptions& opts, int num_args, ...);
std::string CollectAndPrintArgsFile(std::string const& func_name,
                                    TraceOptions& opts, int num_args, ...);

// Below are Helper methods for the above.

// Prints the trace string to stdout.
int TracePrintInternalStdOut(TraceOptions& opts, std::string const& s);
// Prints the trace string to a trace file.
// It is the responsibility of the caller to open and close the time
int TracePrintInternalFile(TraceOptions& opts, std::string const& s);
// Writes the string to file or stdout based on trace options.
std::string TracePrintInternal(TraceOptions& opts, std::string const& s);
// Collects all the passed in arguments and returns a
// formatted string to be traced for all the args.
std::string CollectArgs(va_list src_args, int num_args);

///////////////////////////////////////////
// Convenience Helper Methods.
////////////////////////////////////////////
char const* ToCStr(std::string const& str);
std::string ExitInternal(std::string const& func_name, SQLRETURN ret_code,
                         TraceOptions& opts);

void UpdateTraceOption(std::optional<std::string> log_level,
                       std::optional<std::string> log_path);

bool CanWriteToFile(std::string const& log_file, std::size_t new_log_size,
                    std::uintmax_t max_file_size_bytes);

std::string GetLogFileWithIndex(std::string const& log_path);
////////////////////////////////////////////////////////////////////
// Additional Helper methods for validating and formatting strings
// based on parameter types.
////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////
// ODBC SQL TYPES
/////////////////////////////////////////////
// Basic types.
std::string FormatSqlSmallInt(SQLSMALLINT i);
std::string FormatSqlUSmallInt(SQLUSMALLINT i);
std::string FormatSqlInteger(SQLINTEGER i);
std::string FormatSqlUInteger(SQLUINTEGER i);
// Handles.
std::string FormatSqlHandleType(SQLSMALLINT type);
std::string FormatSqlHandle(SQLHANDLE handle);
// Pointers.
std::string FormatSqlPointer(SQLPOINTER p);
std::string FormatSqlSmallInt(const SQLSMALLINT* p);
std::string FormatSqlUSmallInt(const SQLUSMALLINT* p);
std::string FormatSqlInteger(const SQLINTEGER* p);
std::string FormatSqlUInteger(const SQLUINTEGER* p);
std::string FormatSqlChar(const SQLCHAR* p);
std::string FormatSqlPointer(const SQLPOINTER* p);
std::string FormatSqlHandle(const SQLHANDLE* p);
// length.
std::string FormatSqlLen(SQLLEN l);
std::string FormatSqlULen(SQLULEN l);
std::string FormatSqlSetPosiRow(SQLSETPOSIROW rp);
std::string FormatSqlLen(const SQLLEN* l);
std::string FormatSqlULen(const SQLULEN* l);
// Return codes.
std::string FormatSqlReturnCode(RETCODE ret);
std::string FormatSqlReturn(SQLRETURN ret);
// Additional types specific to 3.x
#if (ODBCVER >= 0x0300)
std::string FormatSqlDate(const SQLDATE* d);
std::string FormatSqlDecimal(SQLDECIMAL d);
std::string FormatSqlDecimal(const SQLDECIMAL* d);
std::string FormatSqlNumeric(SQLNUMERIC n);
std::string FormatSqlNumeric(const SQLNUMERIC* n);
std::string FormatSqlDouble(SQLDOUBLE d);
std::string FormatSqlDouble(const SQLDOUBLE* d);
std::string FormatSqlFloat(SQLFLOAT f);
std::string FormatSqlFloat(const SQLFLOAT* f);
std::string FormatSqlReal(SQLREAL r);
std::string FormatSqlReal(const SQLREAL* r);
std::string FormatSqlTime(const SQLTIME* t);
std::string FormatSqlTimestamp(const SQLTIMESTAMP* tp);
std::string FormatSqlVarchar(const SQLVARCHAR* s);
#endif /* ODBCVER >= 0x0300 */

/////////////////////////////////////////////
// Basic C Types
/////////////////////////////////////////////
std::string FormatString(std::string const& str);
std::string FormatCharString(char const* str);
std::string FormatCharArray(char const str[]);
std::string FormatChar(char c);
std::string FormatCharU(unsigned char c);
std::string FormatInt(int d);
std::string FormatIntU(unsigned int d);
std::string FormatLong(std::int64_t d);
std::string FormatLongU(std::uint64_t d);
std::string FormatShort(std::int16_t d);
std::string FormatShortU(std::uint16_t d);
std::string FormatDouble(double d);
std::string FormatFloat(float d);
std::string FormatPointer(void* p);
std::string FormatBool(bool b);
// Additional basic types (e.g. array or pointer versions of the above)
// may be added as needed.

///////////////////////////////////////////////
// To Be Implemented:
// 1) Unicode types.
///////////////////////////////////////////////

/////////////////////////////////////////////
// Unicode Types
/////////////////////////////////////////////

/////////////////////////////////////////////
// Window specific types.
/////////////////////////////////////////////
#ifdef _WIN32
std::string FormatWindowHandle(HWND handle);
std::string FormatHWND(HWND handle);
std::string FormatSqlHWND(SQLHWND handle);
std::string FormatWindowHandle(SQLHWND handle);
std::string FormatRequest(WORD f_request);
#endif  // _WIN32
/////////////////////////////////////////////
// Struct types.
/////////////////////////////////////////////
#if (ODBCVER >= 0x0300)
std::string FormatNumericStruct(SQL_NUMERIC_STRUCT n);
std::string FormatDateStruct(SQL_DATE_STRUCT d);
std::string FormatTimeStruct(SQL_TIME_STRUCT t);
std::string FormatTimestampStruct(SQL_TIMESTAMP_STRUCT ts);
// Interval related functions.
std::string GetIntervalType(SQLINTERVAL type);
std::string FormatIntervalYearMonthStruct(SQL_YEAR_MONTH_STRUCT ym);
std::string FormatIntervalDaySecondStruct(SQL_DAY_SECOND_STRUCT ds);
std::string FormatIntervalStruct(SQL_INTERVAL_STRUCT i);
#endif /* ODBCVER >= 0x0300 */

// We want this to be created once on startup and shared by all APIs.
// Replace the console call with the file version, for the final release.
static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>> const
    kTraceOptsConsole =
        TraceOptions::CreateTraceOptionsConsole(/*logging_enabled*/ true,
                                                /*unused log_level*/ 0);

static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>> const
    kTraceOptsFile =
        TraceOptions::CreateTraceOptionsFile(GetOdbcTraceConfigPath());

static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>> const
    kTraceOption = TraceOptions::GetTraceOption();

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_TRACE_UTILS_H
