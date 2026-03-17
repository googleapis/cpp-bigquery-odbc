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

inline std::string const kLogLevel = "LogLevel";
inline std::string const kLogPath = "LogPath";
inline std::string const kLogFileCount = "LogFileCount";
inline std::string const kLogFileSize = "LogFileSize";
inline std::string const kMaxThreadsParam = "MaxThreads";
inline std::uint32_t const kDefaultMaxThreads = 8;

/////////////////////////////////////////////////////////////////////////////////
// TraceOptions facilitates ODBC tracing.
// Multiple instances of this class is forbidden.
//
// Usage:
//   auto options = CreateTraceOptionsConsole(true, 0);
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
  static std::shared_ptr<TraceOptions> GetTraceOption();

  static bool InitializeLogging(bool is_trace_override = false);

  // Shared members.
  bool logging_enabled;
  bool is_file_closed;
  int log_level{0};
  int max_file_count{50};   // max number of log files (50).
  int max_file_size{2000};  // max file size of a single file(2000 KB)
  std::uint32_t max_threads = kDefaultMaxThreads;
  int current_file_index{0};
  std::string log_path;
  std::ofstream trace_file;
  // Used for guarding any logging operations with file or stdout.
  std::mutex m;

 private:
  TraceOptions() = default;
  static std::shared_ptr<TraceOptions> options_file_;
  // used for guarding update of internal options members.
  static std::mutex mu_;
};

// Default log file name
inline std::string const kLogTraceFileName = "odbcdriverforbigquery";

enum class LogLevel {
  kLogOff = 0,
  kLogError = 1,
  kLogWarning = 2,
  kLogInfo = 3,
};

class FileLogSink : public absl::LogSink {
 public:
  explicit FileLogSink(std::shared_ptr<TraceOptions> opts);
  ~FileLogSink() override;

  void Send(absl::LogEntry const& entry) override;
  [[nodiscard]] int GetLogLevel() const { return opts_->log_level; }

  static void InitializeFileLog(
      std::shared_ptr<TraceOptions> const& trace_opts);

 private:
  static std::unique_ptr<FileLogSink> file_sink_;

  std::shared_ptr<TraceOptions> opts_;
  std::string current_file_;
  std::size_t current_file_size_;
  std::mutex log_mutex_;
  FILE* fp_ = nullptr;
};

// Get abseil severity as per internal driver log levels
absl::LogSeverity GetAbslSeverity(LogLevel level);

///////////////////////////////////////////
// Convenience Helper Methods.
////////////////////////////////////////////

void ClearOldLogFiles(std::string const& base_dir, int next_index,
                      int max_file_count);

void UpdateTraceOption(std::optional<int> log_level,
                       std::optional<std::string> log_path,
                       std::optional<int> log_file_size,
                       std::optional<int> log_file_count,
                       std::optional<std::uint32_t> max_threads);

std::string GetLogFileWithIndex(std::string const& log_path);
std::string GetFormattedMsg(absl::LogEntry const& entry);

////////////////////////////////////////////////////////////////////
// Additional Helper methods for validating and formatting strings
// based on parameter types.
////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////
// To Be Implemented:
// 1) Unicode types.
///////////////////////////////////////////////

/////////////////////////////////////////////
// Unicode Types
/////////////////////////////////////////////

/////////////////////////////////////////////
// Struct types.
/////////////////////////////////////////////

static odbc_internal::StatusRecordOr<std::shared_ptr<TraceOptions>> const
    kTraceOptsFile =
        TraceOptions::CreateTraceOptionsFile(GetOdbcTraceConfigPath());

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_TRACE_UTILS_H
