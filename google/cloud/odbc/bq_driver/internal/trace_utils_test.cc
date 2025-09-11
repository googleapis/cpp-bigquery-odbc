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
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_utils::StatusRecordIs;

// Common Test Values.
Section const kDriverSection2{{"LogLevel", "0"}, {"LogPath", "/tmp"}};
Section const kDriverSection4{{"LogLevel", "4"}, {"LogPath", "/tmp"}};
Section const kDriverSection5{{"LogLevel", "1"}};

Sections const kConfigSections2{{"Driver", kDriverSection2}};
Sections const kConfigSections4{{"Driver", kDriverSection4}};
Sections const kConfigSections5{{"Driver", kDriverSection5}};

#ifdef _WIN32
Section const kWINDriverSection1{{"LogLevel", "1"}, {"LogPath", "C:\\b"}};
Sections const kWINConfigSections1{{"Driver", kWINDriverSection1}};
#endif  // _WIN32

std::shared_ptr<TraceOptions> test_opts_console =
    *(TraceOptions::CreateTraceOptionsConsole(true, 0));

void CreateTestFile(std::string const& filename, std::size_t size) {
  std::ofstream file(filename, std::ios::binary);
  file.seekp(size - 1);
  file.write("", 1);
}

void DeleteTestFile(std::string const& filename) {
  std::remove(filename.c_str());
}

TEST(TraceLoggingFile, TraceOptionsEmptyConfigs) {
  std::shared_ptr<Sections> config_sections = nullptr;
  auto opts = TraceOptions::CreateTraceOptionsFile(config_sections);
  EXPECT_THAT(
      opts, StatusRecordIs(SQLStates::k_HY000(), "Invalid ODBC Driver Config"));
}

TEST(TraceLogging, TraceLoggingDisabled) {
  auto config_sections = std::make_shared<Sections>(kConfigSections2);
  auto test_trace_opts_file =
      TraceOptions::CreateTraceOptionsFile(config_sections);

  test_opts_console->logging_enabled = false;

  EXPECT_FALSE((test_opts_console)->logging_enabled);

  test_opts_console->logging_enabled = true;
}

TEST(GetAbslSeverity, MapLogLevelWithAbslSeverity) {
  EXPECT_EQ(GetAbslSeverity(LogLevel::kLogInfo), absl::LogSeverity::kInfo);
  EXPECT_EQ(GetAbslSeverity(LogLevel::kLogWarning),
            absl::LogSeverity::kWarning);
  EXPECT_EQ(GetAbslSeverity(LogLevel::kLogError), absl::LogSeverity::kError);
}

TEST(GetAbslSeverity, InvalidAbslSeverity) {
  auto invalid = static_cast<LogLevel>(-1);

  EXPECT_EQ(GetAbslSeverity(invalid), static_cast<absl::LogSeverity>(100));
}

TEST(GetLogFileWithIndex, CustomLogPath) {
  std::string log_path = "/custom/path/to/log/file";
  std::string expected =
      "/custom/path/to/log/file/" + kLogTraceFileName + "_0.log";

  auto actual = GetLogFileWithIndex(log_path);
  EXPECT_EQ(actual, expected);
}

TEST(CanWriteToFile, AllSecnarios) {
  std::string file_name = "sample.log";

  // File doesn't exist
  EXPECT_TRUE(CanWriteToFile(file_name, 100, 1000));

  // File exist and can store more data
  CreateTestFile(file_name, 200);
  EXPECT_TRUE(CanWriteToFile(file_name, 100, 500));

  // File exists and writing would exceed limit
  CreateTestFile(file_name, 900);
  EXPECT_FALSE(CanWriteToFile(file_name, 100, 900));

  // File exists and exactly fills the limit
  CreateTestFile(file_name, 800);
  EXPECT_TRUE(CanWriteToFile(file_name, 200, 1000));

  DeleteTestFile(file_name);
}

#ifdef WIN32
// TODO(b/375112496) enable this function after trace registry work is done
// TEST(TraceLoggingFile, WINTraceOptionsFromConfigTraceEnabled) {
// #ifdef _WIN64
//   auto sections_status =
//       ParseConfig("SOFTWARE\\Google\\ODBC Driver for Google BigQuery");
// #else
//   auto sections_status = ParseConfig(
//       "SOFTWARE\\WOW6432Node\\Google\\ODBC Driver for Google "
//       "BigQuery");
// #endif  // _WIN64
//   ASSERT_STATUS_RECORD_OK(sections_status);
//   auto sections = *sections_status;

//   for (auto const& it_outer : kWINConfigSections1) {
//     std::string section_name = it_outer.first;
//     Section sample_ini_section = it_outer.second;
//     for (auto& it_inner : sample_ini_section) {
//       std::string property = it_inner.first;
//       EXPECT_EQ(sample_ini_section[property],
//                 (*(sections))[section_name][property]);
//     }
//   }
// }
#endif  // _WIN32

}  // namespace google::cloud::odbc_bq_driver_internal
