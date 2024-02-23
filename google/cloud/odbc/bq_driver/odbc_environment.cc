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

#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;

SQLRETURN SQLAllocEnvHandle(SQLHANDLE* out_env_handle) {
  auto* env_handle = new EnvironmentHandle();
  auto* wrapped_handle = new HandleWrapped(HandleType::kEnvHandle, env_handle);
  *out_env_handle = wrapped_handle;
  return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetEnvAttrInternal(SQLHENV environment_handle,
                                        SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER val_str_len) {
  TraceOptions& opts = *(*kTraceOptsConsole);

  StatusOr<EnvironmentHandle*> env_handle_status =
      ValidateEnvironmentHandle(environment_handle);

  if (!env_handle_status.ok()) {
    TracePrintInternal(opts, env_handle_status.status().message());
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
    return SQL_ERROR;
  }

  if (value == nullptr) {
    TracePrintInternal(
        opts,
        "Input attribute value argument for SQLSetEnvAttr cannot be null");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
    return SQL_ERROR;
  }

  EnvironmentHandle* env_handle = *env_handle_status;

  return env_handle->SetAttribute(attribute, value, &val_str_len);
}

SQLRETURN SQL_API SQLGetEnvAttrInternal(SQLHENV environment_handle,
                                        SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER /*value_buffer_len*/,
                                        SQLINTEGER* val_str_len) {
  TraceOptions& opts = *(*kTraceOptsConsole);

  StatusOr<EnvironmentHandle*> env_handle_status =
      ValidateEnvironmentHandle(environment_handle);

  if (!env_handle_status.ok()) {
    TracePrintInternal(opts, env_handle_status.status().message());
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
    return SQL_ERROR;
  }

  if (value == nullptr) {
    TracePrintInternal(
        opts,
        "Output attribute value argument for SQLGetEnvAttr cannot be null");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
    return SQL_ERROR;
  }

  EnvironmentHandle* env_handle = *env_handle_status;

  return env_handle->GetAttribute(attribute, value, &val_str_len);
}

}  // namespace google::cloud::odbc_bq_driver
