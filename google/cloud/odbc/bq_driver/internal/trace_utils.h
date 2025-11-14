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

  static bool InitializeLogging(bool is_trace_override = false);

  // Shared members.
  bool logging_enabled;
  bool is_file_closed;
  int log_level{0};
  int max_file_size{50};   // max number of log files (50).
  int max_file_count{50};  // max file size of a single file(50 MB)
  int current_file_index{0};
  std::string log_path;
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

// Default log file name
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
  ~FileLogSink() override;

  void Send(absl::LogEntry const& entry) override;
  [[nodiscard]] int GetLogLevel() const { return opts_->log_level; }

  static void InitializeFileLog(
      std::shared_ptr<TraceOptions> const& trace_opts);

 private:
  static std::unique_ptr<FileLogSink> file_sink_;

  std::shared_ptr<TraceOptions> opts_;
  std::string current_file_;
  std::mutex log_mutex_;
  FILE* fp_ = nullptr;
};

// Get abseil severity as per internal driver log levels
absl::LogSeverity GetAbslSeverity(LogLevel level);

///////////////////////////////////////////
// Convenience Helper Methods.
////////////////////////////////////////////

void UpdateTraceOption(std::optional<std::string> log_level,
                       std::optional<std::string> log_path,
                       std::optional<int> log_file_size,
                       std::optional<int> log_file_count);

bool CanWriteToFile(std::string const& log_file, std::size_t new_log_size,
                    std::uintmax_t max_file_size_bytes);

std::string GetLogFileWithIndex(std::string const& log_path);
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
