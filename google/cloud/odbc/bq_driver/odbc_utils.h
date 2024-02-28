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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_connection.h"
#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/bq_driver/odbc_statement.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/status_or.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver {

template <typename T>
T* CastToInternalHandle(SQLHANDLE input_handle, HandleType handle_type) {
  if (input_handle == nullptr) {
    TracePrintInternal(
        *(*google::cloud::odbc_bq_driver_internal::kTraceOptsConsole),
        "Handle is null pointer");
    return nullptr;
  }
  auto* handle_wrapped = reinterpret_cast<HandleWrapped*>(input_handle);
  if (handle_wrapped->handle_ref == nullptr) {
    TracePrintInternal(
        *(*google::cloud::odbc_bq_driver_internal::kTraceOptsConsole),
        "Null internal handle reference");
    return nullptr;
  }
  if (handle_type != handle_wrapped->handle_type) {
    TracePrintInternal(
        *(*google::cloud::odbc_bq_driver_internal::kTraceOptsConsole),
        "Invalid handle type");
    return nullptr;
  }

  return reinterpret_cast<T*>(handle_wrapped->handle_ref);
}

google::cloud::odbc_bq_driver_internal::EnvironmentHandle*
ValidateEnvironmentHandle(SQLHENV environment_handle);

google::cloud::odbc_bq_driver_internal::ConnectionHandle*
ValidateConnectionHandle(SQLHDBC connection_handle);

google::cloud::odbc_bq_driver_internal::StatementHandle*
ValidateStatementHandle(SQLHSTMT statement_handle);

}  // namespace google::cloud::odbc_bq_driver

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
