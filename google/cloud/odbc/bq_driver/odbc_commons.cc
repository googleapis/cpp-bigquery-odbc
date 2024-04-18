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

#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::HandleType;
using google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_internal::StatusRecordOr;

SQLRETURN SQLFreeHandleInternal(SQLSMALLINT handle_type, SQLHANDLE in_handle) {
  switch (handle_type) {
    case SQL_HANDLE_ENV: {
      StatusRecordOr<EnvironmentHandle*> handle_result =
          ValidateEnvironmentHandle(in_handle);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOptsConsole),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      (*handle_result)->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    case SQL_HANDLE_DBC: {
      StatusRecordOr<ConnectionHandle*> handle_result =
          ValidateConnectionHandle(in_handle, false);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOptsConsole),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      (*handle_result)->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    case SQL_HANDLE_STMT: {
      StatusRecordOr<StatementHandle*> handle_result =
          ValidateStatementHandle(in_handle);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOptsConsole),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      // TODO(b/332812254) free the four automatically allocated descriptors
      // associated with that handle
      (*handle_result)->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    case SQL_HANDLE_DESC: {
      StatusRecordOr<DescriptorHandle*> handle_result =
          ValidateDescriptorHandle(in_handle);
      if (!handle_result) {
        TracePrintInternal(*(*kTraceOptsConsole),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
      }
      // TODO(b/332812714) all statements that the freed handle had been
      // associated with should be reverted to their respective automatically
      // allocated descriptor handles
      (*handle_result)->kType = HandleType::kUnspecified;
      delete *handle_result;
      break;
    }
    default:
      return SQL_INVALID_HANDLE;
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
