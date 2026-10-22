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

#include "google/cloud/odbc/internal/odbc_opentelemetry.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google::cloud::odbc_internal {
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
  auto tracer = OdbcOpenTelemetry::Initialize(config);
  EXPECT_EQ(tracer.get(), nullptr);
  OdbcOpenTelemetry::Release(config);
}

TEST(OpenTelemetryTest, GlobalProviderFallback) {
  OtelConfig config;
  config.enable_telemetry = true;
  config.use_global_otel = true;

  auto tracer = OdbcOpenTelemetry::Initialize(config);
  EXPECT_NE(tracer.get(), nullptr);
  OdbcOpenTelemetry::Release(config);
}

TEST(OpenTelemetryTest, IsolatedTracerProvidersAndRefCount) {
#ifdef NO_OPENTELEMETRY_EXPORTERS
  GTEST_SKIP()
      << "Skipping isolated tracer provider test when exporters are disabled";
#else
  OtelConfig config1;
  config1.enable_telemetry = true;
  config1.endpoint = "localhost:4317";
  config1.service_name = "service-a";

  OtelConfig config2;
  config2.enable_telemetry = true;
  config2.endpoint = "localhost:4318";
  config2.service_name = "service-b";

  auto tracer1 = OdbcOpenTelemetry::Initialize(config1);
  EXPECT_NE(tracer1.get(), nullptr);

  auto tracer2 = OdbcOpenTelemetry::Initialize(config2);
  EXPECT_NE(tracer2.get(), nullptr);

  // Both should be valid isolated tracers
  EXPECT_NE(tracer1.get(), tracer2.get());

  // Second init on config1 increments ref_count and returns valid tracer
  auto tracer1_dup = OdbcOpenTelemetry::Initialize(config1);
  EXPECT_NE(tracer1_dup.get(), nullptr);

  // Release config1 twice
  OdbcOpenTelemetry::Release(config1);
  OdbcOpenTelemetry::Release(config1);

  // Release config2 once
  OdbcOpenTelemetry::Release(config2);
#endif
}

TEST(OpenTelemetryTest, OdbcSpanScopeWithActiveTracer) {
  OtelConfig config;
  config.enable_telemetry = true;
  config.use_global_otel = true;

  auto tracer = OdbcOpenTelemetry::Initialize(config);
  ASSERT_NE(tracer.get(), nullptr);

  {
    OdbcSpanScope scope("test_span", tracer);
    EXPECT_NE(scope.GetSpan().get(), nullptr);
    scope.SetError("something went wrong");
    scope.EndWithOdbcStatus(SQL_ERROR);
  }
}

TEST(OpenTelemetryTest, OdbcSpanScopeEndWithOdbcStatusSafe) {
  // Test that mapping functions don't crash when tracer is null
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
}  // namespace google::cloud::odbc_internal
