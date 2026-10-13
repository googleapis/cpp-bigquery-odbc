// Copyright 2024 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_opentelemetry.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {
namespace {

TEST(OpenTelemetryTest, RedactSqlRemovesStringLiterals) {
  std::string sql = "SELECT * FROM table WHERE name = 'John Doe'";
  std::string redacted = RedactSql(sql);
  EXPECT_EQ(redacted, "SELECT * FROM table WHERE name = ?");
}

TEST(OpenTelemetryTest, RedactSqlRemovesNumericLiterals) {
  std::string sql =
      "SELECT * FROM table WHERE id = 123 AND val = 3.14 AND e = 1e-5";
  std::string redacted = RedactSql(sql);
  EXPECT_EQ(redacted, "SELECT * FROM table WHERE id = ? AND val = ? AND e = ?");
}

TEST(OpenTelemetryTest, RedactSqlIgnoresNonLiterals) {
  std::string sql = "SELECT col1, col2 FROM my_project.my_dataset.my_table";
  std::string redacted = RedactSql(sql);
  EXPECT_EQ(redacted, sql);
}

TEST(OpenTelemetryTest, InitializationNoopWhenDisabled) {
  OtelConfig config;
  config.enable_telemetry = false;

  // This should be a safe no-op.
  OdbcOpenTelemetry::Initialize(config);
  OdbcOpenTelemetry::Release(config);
}

TEST(OpenTelemetryTest, OdbcSpanScopeEndWithOdbcStatusSafe) {
  // Test that mapping functions don't crash
  {
    OdbcSpanScope scope("test_success");
    scope.EndWithOdbcStatus(SQL_SUCCESS);
  }
  {
    OdbcSpanScope scope("test_success_with_info");
    scope.EndWithOdbcStatus(SQL_SUCCESS_WITH_INFO);
  }
  {
    OdbcSpanScope scope("test_no_data");
    scope.EndWithOdbcStatus(SQL_NO_DATA);
  }
  {
    OdbcSpanScope scope("test_error");
    scope.EndWithOdbcStatus(SQL_ERROR);
  }
  {
    OdbcSpanScope scope("test_invalid_handle");
    scope.EndWithOdbcStatus(SQL_INVALID_HANDLE);
  }
  {
    OdbcSpanScope scope("test_still_executing");
    scope.EndWithOdbcStatus(SQL_STILL_EXECUTING);
  }
}

}  // namespace
}  // namespace google::cloud::odbc_bq_driver_internal
