// Copyright 2025 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/driver_log_form.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

class LogTraceDialogTest : public ::testing::Test {
 protected:
  HWND mock_hwnd_;
  HWND mock_edit_;
  void SetUp() override {
    attributes_map_["log_level"] = "LOG_TRACE";
    attributes_map_["log_file_path"] = "C:\\temp\\log.txt";
  }
  LogTraceDialog log_trace_dialog_;
  Section attributes_map_;
};

TEST(LogTraceDialogTest, SetValues_ValidAttributes) {
  LogTraceDialog log_trace_dialog;
  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "LOG_OFF");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "");

  Section attributes_map;
  attributes_map["LogLevel"] = "6";
  attributes_map["LogFile"] = "C:\\temp\\log.txt";

  log_trace_dialog.SetValues(attributes_map);
  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "LOG_TRACE");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "C:\\temp\\log.txt");
}

TEST(LogTraceDialogTest, SetValues_InvalidLogLevel) {
  LogTraceDialog log_trace_dialog;
  Section attributes_map;

  attributes_map["LogLevel"] = "999";  // Invalid level
  attributes_map["LogFile"] = "C:\\temp\\log.txt";
  log_trace_dialog.SetValues(attributes_map);

  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "LOG_OFF");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "C:\\temp\\log.txt");
}

TEST(LogTraceDialogTest, SetValues_EmptyAttributes) {
  LogTraceDialog log_trace_dialog;
  Section attributes_map;
  log_trace_dialog.SetValues(attributes_map);

  ASSERT_EQ(log_trace_dialog.GetLogLevel(), "");
  ASSERT_EQ(log_trace_dialog.GetLogFilePath(), "");
}

TEST(LogTraceDialogTest, MockFolderPathProvided) {
  HWND parent_hwnd = CreateWindow("STATIC", "MockParent", WS_OVERLAPPED, 0, 0,
                                  100, 100, NULL, NULL, NULL, NULL);
  HWND mock_edit = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE, 0, 0, 100,
                                20, parent_hwnd, NULL, NULL, NULL);

  // Mock folder path
  char const* mock_folder_path = "C:\\mock\\path";
  OpenFolderDialog(parent_hwnd, mock_edit, mock_folder_path);
  char buffer[MAX_PATH] = {0};
  GetWindowText(mock_edit, buffer, MAX_PATH);

  ASSERT_STREQ(buffer, mock_folder_path);
  DestroyWindow(mock_edit);
  DestroyWindow(parent_hwnd);
}
}  // namespace google::cloud::odbc_bq_driver_internal
