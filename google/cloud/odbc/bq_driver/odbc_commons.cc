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
#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;

SQLRETURN SQLFreeHandleInternal(SQLSMALLINT handle_type, SQLHANDLE in_handle) {
  if (!in_handle) {
    // TODO(#170): Add error tracing call here
    // TODO(#158): Add logging here
    return SQL_ERROR;
  }
  // Validate the handle
  auto* in_handle_wrapped = reinterpret_cast<HandleWrapped*>(in_handle);

  switch (handle_type) {
    case SQL_HANDLE_ENV:
      return FreeHandle<EnvironmentHandle>(HandleType::kEnvHandle,
                                           in_handle_wrapped);
    case SQL_HANDLE_DBC:
      return FreeHandle<ConnectionHandle>(HandleType::kConnHandle,
                                          in_handle_wrapped);
    case SQL_HANDLE_STMT:
      return FreeHandle<StatementHandle>(HandleType::kStatementHandle,
                                         in_handle_wrapped);
  }
  // TODO(#158): SQLGetDiagRec should handle this
  return SQL_INVALID_HANDLE;
}

}  // namespace google::cloud::odbc_bq_driver
