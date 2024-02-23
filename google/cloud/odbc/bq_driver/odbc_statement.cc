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

#include "google/cloud/odbc/bq_driver/odbc_statement.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;

SQLRETURN SQLAllocStmtHandle(SQLHDBC in_handle, SQLHANDLE* out_conn_handle) {
  if (!in_handle) {
    // TODO(#170): Add error tracing call here
    // TODO(#158): Add logging here
    return SQL_ERROR;
  }
  // Validate the handle
  auto* in_handle_wrapped = reinterpret_cast<HandleWrapped*>(in_handle);
  if (in_handle_wrapped->handle_type != HandleType::kConnHandle) {
    // TODO(#158): SQLGetDiagRec should handle this
    return SQL_INVALID_HANDLE;
  }

  ConnectionHandle conn_handle =
      *reinterpret_cast<ConnectionHandle*>(in_handle_wrapped->handle_ref);

  auto* stmt_handle = new StatementHandle();
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kStatementHandle, stmt_handle);
  *out_conn_handle = wrapped_handle;
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
