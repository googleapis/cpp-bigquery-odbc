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
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

SQLRETURN
SQLAllocEnvHandle(SQLHANDLE* out_env_handle) {
  auto* env_handle = new EnvironmentHandle();
  *out_env_handle = env_handle;
  return SQL_SUCCESS;
}

SQLRETURN SQL_API SQLSetEnvAttrInternal(SQLHENV environment_handle,
                                        SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER val_str_len) {
  StatusRecordOr<EnvironmentHandle*> env_handle_status =
      ValidateEnvironmentHandle(environment_handle);

  if (!env_handle_status) {
    TracePrintInternal(**kTraceOption,
                       env_handle_status.GetStatusRecord().message);
    return env_handle_status.GetCalculatedReturnCode();
  }

  EnvironmentHandle* env_handle = *env_handle_status;

  return env_handle->SetAttribute(attribute, value, &val_str_len);
}

// value_buffer_len is not used since there is no character environment
// attribute or the driver currently does not support any environment
// attribute with character data. Per spec, this parameter can be ignored for
// non character attribute value.
SQLRETURN SQL_API SQLGetEnvAttrInternal(SQLHENV environment_handle,
                                        SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER /*value_buffer_len*/,
                                        SQLINTEGER* val_str_len) {
  StatusRecordOr<EnvironmentHandle*> env_handle_status =
      ValidateEnvironmentHandle(environment_handle);

  if (!env_handle_status) {
    TracePrintInternal(**kTraceOption,
                       env_handle_status.GetStatusRecord().message);
    return env_handle_status.GetCalculatedReturnCode();
  }

  EnvironmentHandle* env_handle = *env_handle_status;

  if (value == nullptr) {
    auto status_record =
        StatusRecord{SQLStates::k_HY092(), "Null attribute value"};
    return LogAndReturnCode(*env_handle, status_record);
  }

  return env_handle->GetAttribute(attribute, value, &val_str_len);
}

}  // namespace google::cloud::odbc_bq_driver
