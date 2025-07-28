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

#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/internal/getenv.h"
#include <absl/log/internal/globals.h>
#include <sstream>
namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::internal::GetEnv;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

constexpr int kCharBufSize1 = 1024;
constexpr int kCharBufSize2 = 256;
std::string const kLogLevel = "LogLevel";
std::string const kLogPath = "LogPath";

// Initialize the Singleton instance.
std::shared_ptr<TraceOptions> TraceOptions::options_console_ = nullptr;
std::shared_ptr<TraceOptions> TraceOptions::options_file_ = nullptr;
std::mutex TraceOptions::mu_;

#ifdef WIN32
constexpr char kPathSeparator = '\\';
std::string TraceOptions::default_log_dir_ =
    GetEnv("LOCALAPPDATA").value_or("C:\\Users\\Admin");
#else
constexpr char kPathSeparator = '/';
std::string TraceOptions::default_log_dir_ = GetEnv("HOME").value_or("/tmp");
#endif  // WIN32

std::unique_ptr<FileLogSink> file_sink;

FileLogSink::FileLogSink(std::shared_ptr<TraceOptions> opts)
    : opts_(std::move(opts)) {
  current_file_ = GetLogFileWithIndex(opts_->log_path);
}

void FileLogSink::Send(absl::LogEntry const& entry) {
  std::lock_guard<std::mutex> lock(log_mutex_);

  auto message = entry.text_message_with_prefix_and_newline();
  std::size_t new_log_size = message.size() + 1;
  std::uintmax_t max_file_size_bytes = opts_->max_file_size * 1024 * 1024;

  if (!CanWriteToFile(current_file_, new_log_size, max_file_size_bytes)) {
    if (opts_->current_file_index < opts_->max_file_count - 1) {
      ++opts_->current_file_index;
    } else {
      opts_->current_file_index = 0;
    }
    current_file_ = GetLogFileWithIndex(opts_->log_path);
  }
  auto timestamp = entry.timestamp();
  std::string time_str =
      absl::FormatTime("%Y-%m-%d %H:%M:%S", timestamp, absl::LocalTimeZone());

  auto log_message = std::string(entry.text_message());
  std::string log_tag = absl::LogSeverityName(entry.log_severity());

  std::string file_path = std::string(entry.source_filename());
  std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);

  std::ostringstream formatted_log_msg;
  formatted_log_msg << "[" << log_tag << "] "
                    << "[" << time_str << "] "
                    << "[" << filename << ":" << entry.source_line() << "] "
                    << log_message;

  std::ofstream log_stream(current_file_, std::ios::out | std::ios::app);
  if (log_stream.is_open()) {
    log_stream << formatted_log_msg.str() << std::endl;
  }
}

absl::LogSeverity GetAbslSeverity(LogLevel level) {
  switch (level) {
    case LogLevel::kLogInfo:
      return absl::LogSeverity::kInfo;
    case LogLevel::kLogWarning:
      return absl::LogSeverity::kWarning;
    case LogLevel::kLogError:
      return absl::LogSeverity::kError;
    default:
      return static_cast<absl::LogSeverity>(100);  // disables all logging
  }
}

void UpdateTraceOption(std::optional<std::string> log_level,
                       std::optional<std::string> log_path) {
  if (!kTraceOptsFile.Ok()) {
    std::cout << "Tracing is misconfigured: "
              << kTraceOptsFile.GetStatusRecord().message << std::endl;
    return;
  }

  if (!log_level.has_value() && !log_path.has_value()) {
    return;
  }

  auto const& trace_options = kTraceOptsFile.GetValue();
  std::lock_guard<std::mutex> lock(trace_options->m);

  int level = std::strtol(log_level->c_str(), nullptr, 10);
  if (level > 0) {
    trace_options->log_level = level;
    trace_options->logging_enabled = true;
  }

  trace_options->log_path = *log_path;
  bool const initlize = TraceOptions::InitializeLogging(true);
}

std::string GetLogFileWithIndex(std::string const& log_path) {
  std::string base_dir =
      (!log_path.empty()) ? log_path : TraceOptions::default_log_dir_;

  int file_index = 0;
  if (kTraceOptsFile.Ok()) {
    auto const& trace_opts = kTraceOptsFile.GetValue();
    file_index = trace_opts->current_file_index;
  }

  std::ostringstream oss;
  oss << kLogTraceFileName << "_" << file_index << ".log";
  std::string file_name = oss.str();

  std::string separator =
      (!base_dir.empty() && base_dir.back() != kPathSeparator)
          ? std::string(1, kPathSeparator)
          : "";
  return base_dir + separator + file_name;
}

void FileLogSink::InitializeFileLog(
    std::shared_ptr<TraceOptions> const& trace_opts) {
  if (file_sink || !trace_opts) return;

  file_sink = std::make_unique<FileLogSink>(trace_opts);
  absl::log_internal::AddLogSink(file_sink.get());
}

bool CanWriteToFile(std::string const& log_file, std::size_t new_log_size,
                    std::uintmax_t max_file_size_bytes) {
  std::ifstream file(log_file, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return true;
  }
  std::uintmax_t current_file_size = file.tellg();
  return (current_file_size + new_log_size) <= max_file_size_bytes;
}

bool TraceOptions::InitializeLogging(bool override) {
  if (!kTraceOptsFile.Ok()) return false;
  auto const& trace_opts = kTraceOptsFile.GetValue();

  if (trace_opts->logging_enabled && !override) {
    return true;
  }

  if (trace_opts->logging_enabled && override) {
    auto log_severity =
        GetAbslSeverity(static_cast<LogLevel>(trace_opts->log_level));
    absl::SetMinLogLevel(static_cast<absl::LogSeverityAtLeast>(log_severity));
    FileLogSink::InitializeFileLog(trace_opts);
    return true;
  }
  // If logging is disabled, suppress all stderr output
  if (trace_opts->log_level <= 0) {
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);
    return false;
  }

  if (!absl::log_internal::IsInitialized()) {
    absl::InitializeLog();
  }
  auto log_severity =
      GetAbslSeverity(static_cast<LogLevel>(trace_opts->log_level));
  absl::SetMinLogLevel(static_cast<absl::LogSeverityAtLeast>(log_severity));

  FileLogSink::InitializeFileLog(trace_opts);
  trace_opts->logging_enabled = true;

  // Disable Abseil's stderr logging
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);
  return true;
}

StatusRecordOr<std::shared_ptr<TraceOptions>>
TraceOptions::CreateTraceOptionsConsole(bool logging_enabled, int log_level) {
  std::lock_guard<std::mutex> lk(mu_);
  if (options_console_ == nullptr) {
    // Cannot use std::make_shared because constructor is protected.
    options_console_ = std::shared_ptr<TraceOptions>(new TraceOptions());
  }

  options_console_->log_level = log_level;
  options_console_->logging_enabled = logging_enabled;

  return options_console_;
}

StatusRecordOr<std::shared_ptr<TraceOptions>>
TraceOptions::CreateTraceOptionsFile(std::string const& file_path) {
  auto configs = ParseConfig(file_path);
  if (!configs) {
    return configs.GetStatusRecord();
  }
  std::shared_ptr<Sections> sections_ptr = *configs;
  return CreateTraceOptionsFile(sections_ptr);
}

StatusRecordOr<std::shared_ptr<TraceOptions>>
TraceOptions::CreateTraceOptionsFile(
    std::shared_ptr<Sections> const& config_sections) {
  if (!config_sections) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid ODBC Driver Config"};
  }

  Section trace_sections;
  auto const odbc_section = config_sections->find("Driver");
  if (odbc_section != config_sections->end()) {
    trace_sections = odbc_section->second;
  }

  std::string log_path;
  int log_level = 0;
  bool logging_enabled = false;
  for (auto const& s : trace_sections) {
    if (s.first == kLogLevel && !s.second.empty()) {
      log_level = std::strtol(s.second.c_str(), nullptr, 10);
    } else if (s.first == kLogPath) {
      log_path = s.second;
    }
  }

  std::lock_guard<std::mutex> lk(mu_);
  if (options_file_ == nullptr) {
    // Cannot use std::make_shared because constructor is protected.
    options_file_ = std::shared_ptr<TraceOptions>(new TraceOptions());
  }

  options_file_->log_level = log_level;
  options_file_->log_path = log_path;
  return options_file_;
}

StatusRecordOr<std::shared_ptr<TraceOptions>> TraceOptions::GetTraceOption() {
  if (options_file_ != nullptr && !options_file_->log_file.empty()) {
    return options_file_;
  }
  if (options_file_ != nullptr && options_file_->log_file.empty()) {
    return options_console_;
  }
  if (options_console_ != nullptr) {
    return options_console_;
  }
}

int TracePrintInternalStdOut(TraceOptions& opts, std::string const& s) {
  if (!opts.logging_enabled || s.empty()) {
    return -1;
  }
  std::lock_guard<std::mutex> lk(
      opts.m);  // Releases the mutex when going out of scope.
  std::cout << s << std::endl;
  return 0;
}

int TracePrintInternalFile(TraceOptions& opts, std::string const& s) {
  if (!opts.logging_enabled || s.empty()) {
    return -1;
  }
  std::lock_guard<std::mutex> lk(
      opts.m);  // Releases the mutex when going out of scope.
  if (!opts.trace_file.is_open()) {
    return -1;
  }
  opts.trace_file << s << std::endl;

  return 0;
}

std::string TracePrintInternal(TraceOptions& opts, std::string const& s) {
  if (!opts.logging_enabled || s.empty()) {
    return "";
  }

  int ret = 0;
  if (opts.trace_file.is_open()) {
    ret = TracePrintInternalFile(opts, s);
  } else {
    ret = TracePrintInternalStdOut(opts, s);
  }
  if (ret < 0) {
    return "";
  }

  return s;
}

std::string CollectArgs(va_list src_args, int num_args) {
  std::string trace_str;
  va_list dest_args;
  va_copy(dest_args, src_args);
  for (int i = 0; i < num_args; i++) {
    std::string s = va_arg(dest_args, char const*);
    trace_str.append(s);
  }
  va_end(dest_args);  // src_args needs to be ended by the caller.
  return trace_str;
}

std::string CollectAndPrintArgs(std::string const& func_name,
                                TraceOptions& opts, int num_args, ...) {
  std::string trace_str;
  trace_str.append(func_name);

  if (num_args > 0) {
    va_list args_list;
    va_start(args_list, num_args);
    trace_str.append(CollectArgs(args_list, num_args));
    va_end(args_list);

    int ret = TracePrintInternalStdOut(opts, trace_str);
    if (ret < 0) {
      return "";
    }
  }
  return trace_str;
}

std::string CollectAndPrintArgsFile(std::string const& func_name,
                                    TraceOptions& opts, int num_args, ...) {
  std::string trace_str;
  trace_str.append(func_name);

  if (num_args > 0) {
    va_list args_list;
    va_start(args_list, num_args);
    trace_str.append(CollectArgs(args_list, num_args));
    va_end(args_list);

    int ret = TracePrintInternalFile(opts, trace_str);
    if (ret < 0) {
      return "";
    }
  }
  return trace_str;
}

std::string FormatSqlSmallInt(SQLSMALLINT i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hi\n", "SQLSMALLINT", i);
  return buf;
}

std::string FormatSqlUSmallInt(SQLUSMALLINT i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hu\n", "SQLUSMALLINT", i);
  return buf;
}

std::string FormatSqlInteger(SQLINTEGER i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %d\n", "SQLINTEGER", i);
  return buf;
}

std::string FormatSqlUInteger(SQLUINTEGER i) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %i\n", "SQLUINTEGER", i);
  return buf;
}

std::string FormatSqlHandleType(SQLSMALLINT type) {
  char buf[kCharBufSize1];
  switch (type) {
    case SQL_HANDLE_ENV: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_ENV", type);
      break;
    }
    case SQL_HANDLE_DBC: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_DBC", type);
      break;
    }
    case SQL_HANDLE_DESC: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_DESC", type);
      break;
    }
    case SQL_HANDLE_STMT: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "SQL_HANDLE_STMT", type);
      break;
    }
    default: {
      sprintf(buf, "\t\t%-s, handle type=%hi\n", "Unknown Handle Type", type);
    }
  }
  return buf;
}

std::string FormatSqlHandle(SQLHANDLE handle) {
  char buf[kCharBufSize1];
  if (!handle) {
    sprintf(buf, "\t\t%-s, 0x0\n", "SQL_NULL_HANDLE");
  } else {
    sprintf(buf, "\t\t%-s, %p\n", "SQL_HANDLE", handle);
  }
  return buf;
}

std::string FormatSqlPointer(SQLPOINTER p) {
  char buf[kCharBufSize1];
  if (!p) {
    sprintf(buf, "\t\t%-s, 0x0\n", "SQLPOINTER");
  } else {
    sprintf(buf, "\t\t%-s, %p\n", "SQLPOINTER", p);
  }
  return buf;
}

std::string FormatSqlSmallInt(const SQLSMALLINT* p) {
  char buf[kCharBufSize1];
  if (!p)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLSMALLINT");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %hi\n", "SQLSMALLINT", *p);
  return buf;
}

std::string FormatSqlUSmallInt(const SQLUSMALLINT* p) {
  char buf[kCharBufSize1];
  if (!p)
    snprintf(buf, sizeof(buf), "\t\t%-s *,  0x0\n", "SQLUSMALLINT");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %hu\n", "SQLUSMALLINT", *p);
  return buf;
}

std::string FormatSqlInteger(const SQLINTEGER* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLINTEGER");
  else
    sprintf(buf, "\t\t%-s *, %d\n", "SQLINTEGER", *p);
  return buf;
}

std::string FormatSqlUInteger(const SQLUINTEGER* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLUINTEGER");
  else
    sprintf(buf, "\t\t%-s *, %i\n", "SQLUINTEGER", *p);
  return buf;
}

std::string FormatSqlChar(const SQLCHAR* p) {
  char buf[kCharBufSize1];

  if (!p)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLCHAR");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %.15s\n", "SQLCHAR",
             reinterpret_cast<char const*>(p));
  return buf;
}

std::string FormatSqlPointer(const SQLPOINTER* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLPOINTER");
  else
    sprintf(buf, "\t\t%-s *, %p\n", "SQLPOINTER", p);
  return buf;
}

std::string FormatSqlHandle(const SQLHANDLE* p) {
  char buf[kCharBufSize1];
  if (!p)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLHANDLE");
  else
    sprintf(buf, "\t\t%-s *, %p\n", "SQLHANDLE", p);
  return buf;
}

#if (ODBCVER >= 0x0300)
std::string FormatSqlDate(const SQLDATE* d) {
  char buf[kCharBufSize1];
  if (!d)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLDATE");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %s\n", "SQLDATE", d);

  return buf;
}

std::string FormatSqlDecimal(SQLDECIMAL d) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %d\n", "SQLDECIMAL", d);
  return buf;
}

std::string FormatSqlDecimal(const SQLDECIMAL* d) {
  char buf[kCharBufSize1];
  if (!d)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLDECIMAL");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %d\n", "SQLDECIMAL", *d);
  return buf;
}

std::string FormatSqlNumeric(SQLNUMERIC n) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %d\n", "SQLNUMERIC", n);
  return buf;
}

std::string FormatSqlNumeric(const SQLNUMERIC* n) {
  char buf[kCharBufSize1];
  if (!n)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLNUMERIC");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %d\n", "SQLNUMERIC", *n);
  return buf;
}

std::string FormatSqlDouble(SQLDOUBLE d) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %.4lf\n", "SQLDOUBLE", d);
  return buf;
}

std::string FormatSqlDouble(const SQLDOUBLE* d) {
  char buf[kCharBufSize1];
  if (!d)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLDOUBLE");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %.4lf\n", "SQLDOUBLE", *d);
  return buf;
}

std::string FormatSqlFloat(SQLFLOAT f) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %.4lf\n", "SQLFLOAT", f);
  return buf;
}

std::string FormatSqlFloat(const SQLFLOAT* f) {
  char buf[kCharBufSize1];
  if (!f)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLFLOAT");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %.4lf\n", "SQLFLOAT", *f);
  return buf;
}

std::string FormatSqlReal(SQLREAL r) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %.2f\n", "SQLREAL", r);
  return buf;
}

std::string FormatSqlReal(const SQLREAL* r) {
  char buf[kCharBufSize1];
  if (!r)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLREAL");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %.2f\n", "SQLREAL", *r);
  return buf;
}

std::string FormatSqlTime(const SQLTIME* t) {
  char buf[kCharBufSize1];
  if (!t)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLTIME");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %s\n", "SQLTIME", t);

  return buf;
}

std::string FormatSqlTimestamp(const SQLTIMESTAMP* tp) {
  char buf[kCharBufSize1];
  if (!tp)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLTIMESTAMP");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %s\n", "SQLTIMESTAMP", tp);

  return buf;
}

std::string FormatSqlVarchar(const SQLVARCHAR* s) {
  char buf[kCharBufSize1];
  if (!s)
    snprintf(buf, sizeof(buf), "\t\t%-s *, 0x0\n", "SQLVARCHAR");
  else
    snprintf(buf, sizeof(buf), "\t\t%-s *, %s\n", "SQLVARCHAR", s);

  return buf;
}
#endif /* ODBCVER >= 0x0300 */

std::string FormatSqlLen(SQLLEN l) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %ld\n", "SQLLEN", l);
  return buf;
}

std::string FormatSqlULen(SQLULEN l) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %lu\n", "SQLULEN", l);
  return buf;
}

std::string FormatSqlSetPosiRow(SQLSETPOSIROW rp) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hu\n", "SQLSETPOSIROW", rp);
  return buf;
}

std::string FormatSqlReturnCode(RETCODE ret) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hi\n", "RETCODE", ret);
  return buf;
}

std::string FormatSqlReturn(SQLRETURN ret) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, %hi\n", "SQLRETURN", ret);
  return buf;
}

std::string FormatSqlLen(const SQLLEN* l) {
  char buf[kCharBufSize1];
  if (!l)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLLEN");
  else
    sprintf(buf, "\t\t%-s *, %ld\n", "SQLLEN", *l);
  return buf;
}

std::string FormatSqlULen(const SQLULEN* l) {
  char buf[kCharBufSize1];
  if (!l)
    sprintf(buf, "\t\t%-s *, 0x0\n", "SQLULEN");
  else
    sprintf(buf, "\t\t%-s *, %lu\n", "SQLULEN", *l);
  return buf;
}

std::string FormatString(std::string const& str) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s\n", str.c_str());
  return buf;
}

std::string FormatCharString(char const* str) {
  char buf[kCharBufSize1];
  if (!str)
    sprintf(buf, "\t\t 0x0 null string\n");
  else
    sprintf(buf, "\t\t%s\n", str);
  return buf;
}

std::string FormatCharArray(char const str[]) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s\n", str);
  return buf;
}

std::string FormatChar(char c) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%c\n", c);
  return buf;
}

std::string FormatCharU(unsigned char c) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%c\n", c);
  return buf;
}

std::string FormatInt(int d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%d\n", d);
  return buf;
}

std::string FormatIntU(unsigned int d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%i\n", d);
  return buf;
}

std::string FormatLong(std::int64_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%ld\n", d);
  return buf;
}

std::string FormatLongU(std::uint64_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%li\n", d);
  return buf;
}

std::string FormatShort(std::int16_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%hi\n", d);
  return buf;
}

std::string FormatShortU(std::uint16_t d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%hu\n", d);
  return buf;
}

std::string FormatDouble(double d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%.4lf\n", d);
  return buf;
}

std::string FormatFloat(float d) {
  char buf[kCharBufSize2];
  sprintf(buf, "\t\t%.2f\n", d);
  return buf;
}

std::string FormatPointer(void* p) {
  char buf[kCharBufSize2];
  if (!p)
    sprintf(buf, "\t\t0x0 null pointer\n");
  else
    sprintf(buf, "\t\t%p\n", p);
  return buf;
}

std::string FormatBool(bool b) {
  char buf[kCharBufSize2];
  if (b)
    sprintf(buf, "\t\t%s\n", "TRUE");
  else
    sprintf(buf, "\t\t%s\n", "FALSE");
  return buf;
}

char const* ToCStr(std::string const& str) { return str.c_str(); }

std::string ExitInternal(std::string const& func_name, SQLRETURN ret_code,
                         TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.trace_file.is_open()) {
      auto res = CollectAndPrintArgsFile(func_name, opts, 1,
                                         ToCStr(FormatSqlReturn(ret_code)));
      opts.trace_file.close();
      opts.is_file_closed = true;
      return res;
    }
    return CollectAndPrintArgs(func_name, opts, 1,
                               ToCStr(FormatSqlReturn(ret_code)));
  }
  return "";
}

#if (ODBCVER >= 0x0300)
std::string FormatNumericStruct(SQL_NUMERIC_STRUCT n) {
  char buf[kCharBufSize1];
  if (!n.sign)
    sprintf(buf, "\t\t%-s, precision=%d, scale=%d, val=(-)%s \n",
            "SQL_NUMERIC_STRUCT", n.precision, n.scale, n.val);
  else
    sprintf(buf, "\t\t%-s, precision=%d, scale=%d, val=%s \n",
            "SQL_NUMERIC_STRUCT", n.precision, n.scale, n.val);
  return buf;
}

std::string FormatDateStruct(SQL_DATE_STRUCT d) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s, date(YYYY/MM/DD)=%hi/%hu/%hu\n", "SQL_DATE_STRUCT",
          d.year, d.month, d.day);
  return buf;
}

std::string FormatTimeStruct(SQL_TIME_STRUCT t) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%s, time(hh:mm:ss)=%hu:%hu:%hu\n", "SQL_TIME_STRUCT",
          t.hour, t.minute, t.second);
  return buf;
}

std::string FormatTimestampStruct(SQL_TIMESTAMP_STRUCT ts) {
  char buf[kCharBufSize1];
  sprintf(
      buf,
      "\t\t%s, datetime(YYYY/MM/DD hh:mm:ss.sss)=%hu/%hu/%hu %hu:%hu:%hu.%u\n",
      "SQL_TIMESTAMP_STRUCT", ts.year, ts.month, ts.day, ts.hour, ts.minute,
      ts.second, ts.fraction);
  return buf;
}

std::string GetIntervalType(SQLINTERVAL type) {
  switch (type) {
    case SQL_IS_YEAR:
      return "SQL_IS_YEAR";
    case SQL_IS_MONTH:
      return "SQL_IS_MONTH";
    case SQL_IS_DAY:
      return "SQL_IS_DAY";
    case SQL_IS_HOUR:
      return "SQL_IS_HOUR";
    case SQL_IS_MINUTE:
      return "SQL_IS_MINUTE";
    case SQL_IS_SECOND:
      return "SQL_IS_SECOND";
    case SQL_IS_YEAR_TO_MONTH:
      return "SQL_IS_YEAR_TO_MONTH";
    case SQL_IS_DAY_TO_HOUR:
      return "SQL_IS_DAY_TO_HOUR";
    case SQL_IS_DAY_TO_MINUTE:
      return "SQL_IS_DAY_TO_MINUTE";
    case SQL_IS_DAY_TO_SECOND:
      return "SQL_IS_DAY_TO_SECOND";
    case SQL_IS_HOUR_TO_MINUTE:
      return "SQL_IS_HOUR_TO_MINUTE";
    case SQL_IS_HOUR_TO_SECOND:
      return "SQL_IS_HOUR_TO_SECOND";
    case SQL_IS_MINUTE_TO_SECOND:
      return "SQL_IS_MINUTE_TO_SECOND";
  }

  return "";
}

std::string FormatIntervalYearMonthStruct(SQL_YEAR_MONTH_STRUCT ym) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, year_month(YYYY/MM)=%i/%i\n", "SQL_YEAR_MONTH_STRUCT",
          ym.year, ym.month);
  return buf;
}

std::string FormatIntervalDaySecondStruct(SQL_DAY_SECOND_STRUCT ds) {
  char buf[kCharBufSize1];
  sprintf(buf, "\t\t%-s, day_second(DD hh:mm:ss.ssss)=%i %i:%i:%i.%i\n",
          "SQL_DAY_SECOND_STRUCT", ds.day, ds.hour, ds.minute, ds.second,
          ds.fraction);
  return buf;
}

std::string FormatIntervalStruct(SQL_INTERVAL_STRUCT i) {
  char buf[kCharBufSize1];
  if (i.interval_sign) {
    sprintf(buf, "\t\t%s, interval_type=%s, interval_sign=(+), %s, %s\n",
            "SQL_INTERVAL_STRUCT", ToCStr(GetIntervalType(i.interval_type)),
            ToCStr(FormatIntervalYearMonthStruct(i.intval.year_month)),
            ToCStr(FormatIntervalDaySecondStruct(i.intval.day_second)));
  } else {
    sprintf(buf, "\t\t%s, interval_type=%s, interval_sign=(-), %s, %s\n",
            "SQL_INTERVAL_STRUCT", ToCStr(GetIntervalType(i.interval_type)),
            ToCStr(FormatIntervalYearMonthStruct(i.intval.year_month)),
            ToCStr(FormatIntervalDaySecondStruct(i.intval.day_second)));
  }
  return buf;
}
#endif /* ODBCVER >= 0x0300 */

#ifdef _WIN32
std::string FormatHWND(HWND handle) {
  char buf[kCharBufSize1];
  if (!handle)
    sprintf(buf, "\t\t%-s, 0x0\n", "HWND");
  else
    sprintf(buf, "\t\t%-s, %p\n", "HWND", handle);
  return buf;
}

std::string FormatSqlHWND(SQLHWND handle) {
  char buf[kCharBufSize1];
  if (!handle)
    sprintf(buf, "\t\t%-s, 0x0\n", "SQLHWND");
  else
    sprintf(buf, "\t\t%-s, %p\n", "SQLHWND", handle);
  return buf;
}

std::string FormatRequest(WORD f_request) {
  switch (f_request) {
    case ODBC_ADD_DSN:
      return "ODBC_ADD_DSN";
    case ODBC_CONFIG_DSN:
      return "ODBC_CONFIG_DSN";
    case ODBC_REMOVE_DSN:
      return "ODBC_REMOVE_DSN";
    case ODBC_ADD_SYS_DSN:
      return "ODBC_ADD_SYS_DSN";
    case ODBC_CONFIG_SYS_DSN:
      return "ODBC_CONFIG_SYS_DSN";
    case ODBC_REMOVE_SYS_DSN:
      return "ODBC_REMOVE_SYS_DSN";
    default:
      return "Unknown Request";
  }
}
#endif  // _WIN32

}  // namespace google::cloud::odbc_bq_driver_internal
