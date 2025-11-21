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
#include <absl/log/internal/globals.h>
#include <absl/strings/str_format.h>
#include <sstream>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

constexpr int kCharBufSize1 = 1024;
constexpr int kCharBufSize2 = 256;
std::string const kLogLevel = "LogLevel";
std::string const kLogPath = "LogPath";
std::string const kLogFileCount = "LogFileCount";
std::string const kLogFileSize = "LogFileSize";

static std::once_flag absl_log_init_flag;
// Initialize the Singleton instance.
std::shared_ptr<TraceOptions> TraceOptions::options_console_ = nullptr;
std::shared_ptr<TraceOptions> TraceOptions::options_file_ = nullptr;
std::mutex TraceOptions::mu_;

#ifdef _WIN32
constexpr char kPathSeparator = '\\';
#else
constexpr char kPathSeparator = '/';
#endif  // _WIN32

std::unique_ptr<FileLogSink> FileLogSink::file_sink_ = nullptr;

FileLogSink::FileLogSink(std::shared_ptr<TraceOptions> opts)
    : opts_(std::move(opts)) {
  current_file_ = GetLogFileWithIndex(opts_->log_path);
  // File is created only when both log path and log level are provided
  if (opts_->log_level > 0 && !opts_->log_path.empty()) {
    // If file open fails, driver continues silently.
    fp_ = fopen(current_file_.c_str(), "a");
  }
}

FileLogSink::~FileLogSink() {
  // Close the file pointer if it was opened
  if (fp_ != nullptr) {
    fclose(fp_);
    fp_ = nullptr;
  }
}
// Required for custom log formatting and writing to the driver's default log
// file
void FileLogSink::Send(absl::LogEntry const& entry) {
  std::lock_guard<std::mutex> lock(log_mutex_);
  auto message = entry.text_message_with_prefix_and_newline();
  std::size_t new_log_size = message.size() + 1;
  std::uintmax_t max_file_size_bytes = opts_->max_file_size * 1024 * 1024;

  if (!CanWriteToFile(current_file_, new_log_size, max_file_size_bytes)) {
    if (fp_ != nullptr) {
      fclose(fp_);
      fp_ = nullptr;
    }

    // Remove the oldest log file (if limit reached), then advance to the next
    // index.
    ClearOldLogFiles(opts_->log_path, opts_->current_file_index,
                     opts_->max_file_count);
    ++opts_->current_file_index;

    current_file_ = GetLogFileWithIndex(opts_->log_path);
    fp_ = fopen(current_file_.c_str(), "a");
  }

  if (fp_ == nullptr) {
    fp_ = fopen(current_file_.c_str(), "a");
  }
  std::string time_str = absl::FormatTime(
      "%Y-%m-%d %H:%M:%S", entry.timestamp(), absl::LocalTimeZone());

  auto log_message = std::string(entry.text_message());
  std::string log_tag = absl::LogSeverityName(entry.log_severity());

  absl::string_view full_path = entry.source_filename();
  size_t last_sep = full_path.find_last_of("/\\");
  absl::string_view file_name = (last_sep == absl::string_view::npos)
                                    ? full_path
                                    : full_path.substr(last_sep + 1);

  absl::FPrintF(fp_, "[%s] [%s] [%s:%d] %s\n", log_tag, time_str, file_name,
                entry.source_line(), entry.text_message());
  fflush(fp_);
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

void ClearOldLogFiles(std::string const& base_dir, int next_index,
                      int max_file_count) {
  // No rotation needed if only 1 file allowed
  if (max_file_count <= 1) return;

  // Oldest index that must be deleted
  int oldest_index = next_index - (max_file_count - 1);
  if (oldest_index < 0) return;  // Not enough history yet

  std::string separator =
      (!base_dir.empty() && base_dir.back() != kPathSeparator)
          ? std::string(1, kPathSeparator)
          : "";

  std::string old_log_file = absl::StrFormat(
      "%s%s%s_%d.log", base_dir, separator, kLogTraceFileName, oldest_index);

  if (std::filesystem::exists(old_log_file)) {
    std::filesystem::remove(old_log_file);
  }
}

void UpdateTraceOption(std::optional<std::string> log_level,
                       std::optional<std::string> log_path,
                       std::optional<int> log_file_size,
                       std::optional<int> log_file_count) {
  if (!kTraceOptsFile.Ok() ||
      !(log_level || log_path || log_file_size || log_file_count))
    return;

  auto const& trace_options = kTraceOptsFile.GetValue();
  std::lock_guard<std::mutex> lock(trace_options->m);

  if (log_level) {
    int level = std::stoi(*log_level);
    trace_options->log_level = level;
    trace_options->logging_enabled = (level > 0);
  }
  if (log_path) trace_options->log_path = *log_path;
  if (log_file_size) trace_options->max_file_size = *log_file_size;
  if (log_file_count) trace_options->max_file_count = *log_file_count;

  bool const initlize = TraceOptions::InitializeLogging(true);
}

std::string GetLogFileWithIndex(std::string const& log_path) {
  std::string base_dir = log_path;

  int file_index = 0;
  if (kTraceOptsFile.Ok()) {
    auto const& trace_opts = kTraceOptsFile.GetValue();
    file_index = trace_opts->current_file_index;
  }
  std::string separator =
      (!base_dir.empty() && base_dir.back() != kPathSeparator)
          ? std::string(1, kPathSeparator)
          : "";
  return absl::StrFormat("%s%s%s_%d.log", base_dir, separator,
                         kLogTraceFileName, file_index);
}

void FileLogSink::InitializeFileLog(
    std::shared_ptr<TraceOptions> const& trace_opts) {
  if (file_sink_ || !trace_opts) return;

  if (file_sink_) {
    absl::log_internal::RemoveLogSink(file_sink_.get());
    file_sink_ = nullptr;
  }

  file_sink_ = std::make_unique<FileLogSink>(trace_opts);
  absl::log_internal::AddLogSink(file_sink_.get());
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

bool TraceOptions::InitializeLogging(bool is_trace_override) {
  // suppress all stderr output
  std::call_once(absl_log_init_flag, []() { absl::InitializeLog(); });
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);

  if (!kTraceOptsFile.Ok()) return false;
  auto const& trace_opts = kTraceOptsFile.GetValue();

  // If logging is disabled, return false
  if (trace_opts->log_level <= 0) {
    trace_opts->logging_enabled = false;
    return false;
  }

  // Logging already initialized and no override requested
  if (trace_opts->logging_enabled && !is_trace_override) {
    return true;
  }

  // Override logging config if requested via connection string
  if (trace_opts->logging_enabled && is_trace_override) {
    auto log_severity =
        GetAbslSeverity(static_cast<LogLevel>(trace_opts->log_level));
    absl::SetMinLogLevel(static_cast<absl::LogSeverityAtLeast>(log_severity));
    FileLogSink::InitializeFileLog(trace_opts);
    return true;
  }

  // Initialize Abseil logging and custom file sink
  auto log_severity =
      GetAbslSeverity(static_cast<LogLevel>(trace_opts->log_level));
  absl::SetMinLogLevel(static_cast<absl::LogSeverityAtLeast>(log_severity));

  FileLogSink::InitializeFileLog(trace_opts);
  trace_opts->logging_enabled = true;
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
  int log_file_count = 50;
  int log_file_size = 20;
  bool logging_enabled = false;
  for (auto const& s : trace_sections) {
    if (s.first == kLogLevel && !s.second.empty()) {
      log_level = std::strtol(s.second.c_str(), nullptr, 10);
    } else if (s.first == kLogPath) {
      log_path = s.second;
    } else if (s.first == kLogFileCount) {
      log_file_count = std::strtol(s.second.c_str(), nullptr, 10);
    } else if (s.first == kLogFileSize) {
      log_file_size = std::strtol(s.second.c_str(), nullptr, 10);
    }
  }

  std::lock_guard<std::mutex> lk(mu_);
  if (options_file_ == nullptr) {
    // Cannot use std::make_shared because constructor is protected.
    options_file_ = std::shared_ptr<TraceOptions>(new TraceOptions());
  }

  options_file_->log_level = log_level;
  options_file_->log_path = log_path;
  options_file_->max_file_count = log_file_count;
  options_file_->max_file_size = log_file_size;
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

}  // namespace google::cloud::odbc_bq_driver_internal
