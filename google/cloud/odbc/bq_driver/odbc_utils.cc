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

#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;

StatusOr<std::shared_ptr<ConnectionHandle>> ValidateConnectionHandle(
    SQLHDBC connection_handle) {
  // Validate nullness.
  if (!connection_handle) {
    return Status(StatusCode::kInvalidArgument, "Null connection handle");
  }
  // Validate the connection handle type.
  auto* conn_handle_wrapped =
      reinterpret_cast<HandleWrapped*>(connection_handle);
  if (conn_handle_wrapped->handle_type != HandleType::kConnHandle) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid connection handle type");
  }

  auto* handle =
      reinterpret_cast<ConnectionHandle*>(conn_handle_wrapped->handle_ref);
  // Ensure the handle validity.
  if (!handle->IsConnected()) {
    return Status(StatusCode::kInvalidArgument, "Invalid connection handle");
  }

  return std::make_shared<ConnectionHandle>(*handle);
}

StatusOr<std::shared_ptr<StatementHandle>> ValidateStatementHandle(
    SQLHDBC statement_handle) {
  // Validate nullness.
  if (!statement_handle) {
    return Status(StatusCode::kInvalidArgument, "Null statement handle");
  }
  // Validate the connection handle type.
  auto* stmt_handle_wrapped =
      reinterpret_cast<HandleWrapped*>(statement_handle);
  if (stmt_handle_wrapped->handle_type != HandleType::kStatementHandle) {
    return Status(StatusCode::kInvalidArgument,
                  "Invalid statement handle type");
  }

  auto* handle =
      reinterpret_cast<StatementHandle*>(stmt_handle_wrapped->handle_ref);
  return std::make_shared<StatementHandle>(*handle);
}

}  // namespace google::cloud::odbc_bq_driver
