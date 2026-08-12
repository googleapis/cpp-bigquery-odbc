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
#include "absl/log/internal/globals.h"
#include "absl/strings/str_format.h"
#include <sstream>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace fs = std::filesystem;
constexpr int kKB = 1024;
constexpr int kCharBufSize2 = 256;

static std::once_flag absl_log_init_flag;
// Initialize the Singleton instance.
std::shared_ptr<TraceOptions> TraceOptions::options_file_ = nullptr;
std::mutex TraceOptions::mu_;

odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>> const
    kTraceOptsFile =
        TraceOptions::CreateTraceOptionsFile(GetOdbcTraceConfigPath());

#ifdef _WIN32
constexpr char kPathSeparator = '\\';
#else
constexpr char kPathSeparator = '/';
#endif  // _WIN32

std::unique_ptr<FileLogSink> FileLogSink::file_sink_ = nullptr;

FileLogSink::FileLogSink(std::shared_ptr<TraceOptions> opts)
    : opts_(std::move(opts)) {
  // File is created only when both log path and log level are provided
  if (opts_->log_level > 0 && !opts_->log_path.empty()) {
    // If file open fails, driver continues silently.
    fs::path log_dir(opts_->log_path);
    if (!fs::exists(log_dir) || !fs::is_directory(log_dir)) {
      return;
    }
    current_file_ = GetLogFileWithIndex(opts_->log_path);
    fp_ = fopen(current_file_.c_str(), "a");
    if (!fp_) {
      return;
    }
    if (std::filesystem::exists(current_file_)) {
      current_file_size_ = std::filesystem::file_size(current_file_);
    } else {
      current_file_size_ = 0;
    }
  }
}

FileLogSink::~FileLogSink() {
  // Close the file pointer if it was opened
  if (fp_ != nullptr) {
    fclose(fp_);
    fp_ = nullptr;
  }
}
// Required for writing to the driver's default log file
void FileLogSink::Send(absl::LogEntry const& entry) {
  std::lock_guard<std::mutex> lock(log_mutex_);
  // Logging disabled or never initialized
  if (!fp_ || !opts_) return;

  auto& opts = *opts_;
  auto formatted_msg = GetFormattedMsg(entry);

  std::size_t new_log_size = formatted_msg.size();
  std::uintmax_t max_file_size_bytes =
      opts.max_file_size * kKB;  // file size is in KB
  if (current_file_size_ + new_log_size >= max_file_size_bytes) {
    fclose(fp_);
    fp_ = nullptr;
    // Remove the oldest log file (if limit reached), then advance to the next
    // index.
    ClearOldLogFiles(opts.log_path, opts.current_file_index,
                     opts.max_file_count);
    ++opts.current_file_index;

    current_file_ = GetLogFileWithIndex(opts.log_path);
    fp_ = fopen(current_file_.c_str(), "a");
    if (!fp_) return;

    current_file_size_ = 0;
  }
  absl::FPrintF(fp_, "%s", formatted_msg.c_str());
  fflush(fp_);
  current_file_size_ += new_log_size;
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

// Formats log msg in format: [LOG_LEVEL] [TIME] [FILE:LINE] MSG
std::string GetFormattedMsg(absl::LogEntry const& entry) {
  static absl::TimeZone const kTimeZone = absl::LocalTimeZone();
  std::string time_str =
      absl::FormatTime("%Y-%m-%d %H:%M:%S", entry.timestamp(), kTimeZone);

  char const* log_tag = absl::LogSeverityName(entry.log_severity());
  absl::string_view full_path = entry.source_filename();

  size_t pos = full_path.rfind('/');
  if (pos == absl::string_view::npos) pos = full_path.rfind('\\');

  absl::string_view file_name =
      pos == absl::string_view::npos ? full_path : full_path.substr(pos + 1);

  std::string msg =
      absl::StrFormat("[%s] [%s] [%s:%d] %s\n", log_tag, time_str, file_name,
                      entry.source_line(), entry.text_message());
  return msg;
}

void ClearOldLogFiles(std::string const& base_dir, int next_index,
                      int max_file_count) {
  if (max_file_count < 1) return;

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

void UpdateTraceOption(std::optional<int> log_level,
                       std::optional<std::string> log_path,
                       std::optional<int> log_file_size,
                       std::optional<int> log_file_count,
                       std::optional<std::uint32_t> max_threads) {
  if (!kTraceOptsFile.Ok() || !(log_level || log_path || log_file_size ||
                                log_file_count || max_threads))
    return;

  auto const& trace_options = kTraceOptsFile.GetValue();
  std::lock_guard<std::mutex> lock(trace_options->m);

  if (log_level) {
    trace_options->log_level = *log_level;
    trace_options->logging_enabled = (*log_level > 0);
  }
  if (trace_options->logging_enabled) {
    if (log_path) trace_options->log_path = *log_path;
    if (log_file_size) trace_options->max_file_size = *log_file_size;
    if (log_file_count) trace_options->max_file_count = *log_file_count;
    if (max_threads) trace_options->max_threads = *max_threads;
  }

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

bool TraceOptions::InitializeLogging(bool is_trace_override) {
  // suppress all stderr output
  std::call_once(absl_log_init_flag, []() { absl::InitializeLog(); });
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);

  if (!kTraceOptsFile.Ok()) return false;
  auto const& trace_opts = kTraceOptsFile.GetValue();

  // If logging is disabled, return false
  if (trace_opts->log_level <= 0) {
    trace_opts->logging_enabled = false;
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfinity);
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

  std::lock_guard<std::mutex> lk(mu_);
  if (options_file_ == nullptr) {
    // Cannot use std::make_shared because constructor is protected.
    options_file_ = std::shared_ptr<TraceOptions>(new TraceOptions());
  }

  std::string log_path;
  int log_level = 0;
  int log_file_count;
  int log_file_size;
  std::uint32_t max_threads = 8;  // default max_threads
  for (auto const& s : trace_sections) {
    if (s.first == kLogLevel && !s.second.empty()) {
      log_level = std::strtol(s.second.c_str(), nullptr, 10);
    } else if (s.first == kLogPath) {
      log_path = s.second;
    } else if (s.first == kLogFileCount) {
      log_file_count = std::strtol(s.second.c_str(), nullptr, 10);
    } else if (s.first == kLogFileSize) {
      log_file_size = std::strtol(s.second.c_str(), nullptr, 10);
    } else if (s.first == kMaxThreadsParam) {
      max_threads = std::stoull(s.second);
#if !defined(_WIN32)
    } else if (s.first == kWcharEncoding) {
      SetWcharEncodingFromConfig(s.second);
#endif
    }
  }

  if (log_level > 0) {
    options_file_->log_level = log_level;
    options_file_->log_path = log_path;
    options_file_->max_file_count = log_file_count;
    options_file_->max_file_size = log_file_size;
    options_file_->max_threads = max_threads;
  }
  return options_file_;
}

std::shared_ptr<TraceOptions> TraceOptions::GetTraceOption() {
  return options_file_;
}

}  // namespace google::cloud::odbc_bq_driver_internal
