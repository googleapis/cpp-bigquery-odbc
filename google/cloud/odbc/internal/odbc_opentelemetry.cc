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
#ifndef NO_OPENTELEMETRY_EXPORTERS
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#endif
#include <opentelemetry/trace/noop.h>
#include <opentelemetry/trace/provider.h>
#include <map>
#include <mutex>
#include <regex>
#include <tuple>

namespace google::cloud::odbc_internal {

#ifndef NO_OPENTELEMETRY_EXPORTERS
// SDK Provider Cache for multi-connection handling
struct SdkCacheKey {
  std::string endpoint;
  std::string protocol;
  std::string service_name;

  bool operator<(SdkCacheKey const& other) const {
    return std::tie(endpoint, protocol, service_name) <
           std::tie(other.endpoint, other.protocol, other.service_name);
  }
};

struct SdkCacheEntry {
  std::shared_ptr<opentelemetry::trace::TracerProvider> provider;
  int ref_count = 0;
};

static std::map<SdkCacheKey, SdkCacheEntry> sdk_cache;
static std::mutex sdk_cache_mutex;
#endif

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
OdbcOpenTelemetry::Initialize(OtelConfig const& config) {
  if (!config.enable_telemetry) {
    return opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>();
  }

  // Global provider uses opentelemetry::trace::Provider from API,
  // which is supported under both Bazel and CMake without exporters.
  if (config.use_global_otel) {
    return GetTracer();
  }

#ifdef NO_OPENTELEMETRY_EXPORTERS
  // Under Bazel builds where exporters are not linked, driver-managed
  // tracer provider creation is a safe no-op.
  return opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>();
#else

  std::lock_guard<std::mutex> lock(sdk_cache_mutex);

  SdkCacheKey key{config.endpoint, config.protocol, config.service_name};
  auto it = sdk_cache.find(key);
  if (it != sdk_cache.end()) {
    it->second.ref_count++;
    return it->second.provider->GetTracer("google-cloud-bigquery-odbc");
  }

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter;

  if (!config.endpoint.empty()) {
    opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
    opts.endpoint = config.endpoint;
    exporter =
        opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(opts);
  } else {
    exporter =
        opentelemetry::exporter::trace::OStreamSpanExporterFactory::Create();
  }

  opentelemetry::sdk::trace::BatchSpanProcessorOptions processor_options;
  auto processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
      std::move(exporter), processor_options);

  std::string service_name = config.service_name.empty()
                                 ? "google-cloud-bigquery-odbc"
                                 : config.service_name;
  auto resource = opentelemetry::sdk::resource::Resource::Create(
      {{"service.name", service_name}});

  auto sdk_provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(
      std::move(processor), resource);

  auto shared_provider = std::shared_ptr<opentelemetry::trace::TracerProvider>(
      std::move(sdk_provider));

  auto tracer = shared_provider->GetTracer("google-cloud-bigquery-odbc");

  sdk_cache[key] = {shared_provider, 1};
  return tracer;
#endif
}

void OdbcOpenTelemetry::Release(OtelConfig const& config) {
  if (!config.enable_telemetry) return;
  if (config.use_global_otel) return;

#ifndef NO_OPENTELEMETRY_EXPORTERS
  std::lock_guard<std::mutex> lock(sdk_cache_mutex);

  SdkCacheKey key{config.endpoint, config.protocol, config.service_name};
  auto it = sdk_cache.find(key);
  if (it == sdk_cache.end()) {
    return;
  }

  it->second.ref_count--;
  if (it->second.ref_count > 0) return;

  auto* sdk = static_cast<opentelemetry::sdk::trace::TracerProvider*>(
      it->second.provider.get());
  sdk->ForceFlush();
  sdk->Shutdown();

  sdk_cache.erase(it);
#endif
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
OdbcOpenTelemetry::GetTracer() {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("google-cloud-bigquery-odbc");
}

OdbcSpanScope::OdbcSpanScope(
    std::string const& span_name,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> const&
        tracer,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> const&
        parent) {
  if (!tracer) {
    return;
  }
  if (parent) {
    opentelemetry::trace::StartSpanOptions options;
    options.parent = parent->GetContext();
    span_ = tracer->StartSpan(span_name, options);
  } else {
    span_ = tracer->StartSpan(span_name);
  }
  scope_ = std::make_unique<opentelemetry::trace::Scope>(span_);
}

OdbcSpanScope::~OdbcSpanScope() {
  // Scope must be destroyed before the span is ended
  scope_.reset();
  if (span_) {
    span_->End();
  }
}

void OdbcSpanScope::EndWithOdbcStatus(SQLRETURN odbc_return) {
  if (!span_) return;

  switch (odbc_return) {
    case SQL_SUCCESS:
      // Spec: Leave status Unset on success
      break;
    case SQL_SUCCESS_WITH_INFO:
      span_->AddEvent("SQL_SUCCESS_WITH_INFO");
      break;
    case SQL_NO_DATA:
      span_->AddEvent("SQL_NO_DATA");
      break;
    case SQL_STILL_EXECUTING:
      break;
    case SQL_ERROR:
    case SQL_INVALID_HANDLE:
      span_->SetStatus(opentelemetry::trace::StatusCode::kError);
      break;
    default:
      span_->SetStatus(opentelemetry::trace::StatusCode::kError,
                       "Unknown ODBC Return Code");
      break;
  }
}

void OdbcSpanScope::SetError(std::string const& message) {
  if (span_) {
    span_->SetStatus(opentelemetry::trace::StatusCode::kError, message);
  }
}

std::string RedactSql(std::string const& sql) {
  std::regex string_literal(R"('(?:[^'\\]|\\.)*')");
  std::regex numeric_literal(R"(\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b)");

  std::string redacted = std::regex_replace(sql, string_literal, "?");
  redacted = std::regex_replace(redacted, numeric_literal, "?");
  return redacted;
}

}  // namespace google::cloud::odbc_internal
