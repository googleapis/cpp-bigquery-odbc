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
inline StatusOr<T*> ValidateHandle(HandleType handle_type,
                                   HandleWrapped* handle_wrapped) {
  if (!handle_wrapped) {
    return Status(StatusCode::kInvalidArgument, "Null handle");
  }
  if (handle_type != handle_wrapped->handle_type) {
    return Status(StatusCode::kInvalidArgument, "Invalid handle type");
  }
  T* internal_handle_ptr = reinterpret_cast<T*>(handle_wrapped->handle_ref);
  if (!internal_handle_ptr) {
    return Status(StatusCode::kInvalidArgument,
                  "Null internal handle reference");
  }
  return internal_handle_ptr;
}

StatusOr<google::cloud::odbc_bq_driver_internal::ConnectionHandle*>
ValidateConnectionHandle(SQLHDBC connection_handle);

StatusOr<google::cloud::odbc_bq_driver_internal::EnvironmentHandle*>
ValidateEnvironmentHandle(SQLHENV environment_handle);

StatusOr<google::cloud::odbc_bq_driver_internal::StatementHandle*>
ValidateStatementHandle(SQLHENV stmt_handle);

}  // namespace google::cloud::odbc_bq_driver

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
