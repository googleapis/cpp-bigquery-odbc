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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_ODBC_OPENTELEMETRY_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_ODBC_OPENTELEMETRY_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/scope.h>
#include <opentelemetry/trace/span.h>
#include <memory>
#include <string>

namespace google::cloud::odbc_internal {

struct OtelConfig {
  bool enable_telemetry = false;
  bool use_global_otel = false;
  std::string endpoint;
  std::string protocol;
  std::string service_name;
  bool opentelemetry_capture_sql = false;
};

class OdbcOpenTelemetry {
 public:
  static opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
  Initialize(OtelConfig const& config);
  static void Release(OtelConfig const& config);
  static opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
  GetTracer();
};

class OdbcSpanScope {
 public:
  explicit OdbcSpanScope(
      std::string const& span_name,
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> const&
          tracer =
              opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>(),
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const&
          parent =
              opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>());
  ~OdbcSpanScope();

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> GetSpan() {
    return span_;
  }

  void EndWithOdbcStatus(SQLRETURN odbc_return);
  void SetError(std::string const& message);

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  std::unique_ptr<opentelemetry::trace::Scope> scope_;
};

std::string RedactSql(std::string const& sql);

}  // namespace google::cloud::odbc_internal

namespace google::cloud::odbc_bq_driver_internal {
using ::google::cloud::odbc_internal::OdbcOpenTelemetry;
using ::google::cloud::odbc_internal::OdbcSpanScope;
using ::google::cloud::odbc_internal::OtelConfig;
using ::google::cloud::odbc_internal::RedactSql;
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_ODBC_OPENTELEMETRY_H
