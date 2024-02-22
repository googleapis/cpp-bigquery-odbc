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

#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_statement_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::TracePrintInternal;

TraceOptions& opts = *(*kTraceOptsConsole);

SQLRETURN SQLBindColInternal(SQLHSTMT statement_handle,
                             SQLUSMALLINT column_number,
                             SQLSMALLINT target_c_type, SQLPOINTER target_value,
                             SQLLEN target_value_buffer_len,
                             SQLLEN* target_value_str_len) {
  StatusOr<std::shared_ptr<StatementHandle>> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result.ok()) {
    TracePrintInternal(
        opts, "Invalid Statement handle: " + handle_result.status().message());
    return SQL_INVALID_HANDLE;
  }
  StatementHandle handle = *(handle_result.value());
  handle.GetDiagnostics().ClearDiagnostics();

  return handle.BindColumn(column_number, target_c_type, target_value,
                           target_value_buffer_len, target_value_str_len);
}

// NOLINTBEGIN(misc-unused-parameters)

SQLRETURN SQLFetchInternal(SQLHSTMT statementHandle) { return SQL_SUCCESS; }

// NOLINTEND(misc-unused-parameters)

}  // namespace google::cloud::odbc_bq_driver
