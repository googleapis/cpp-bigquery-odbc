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
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;

StatusOr<ConnectionHandle*> ValidateConnectionHandle(
    SQLHDBC connection_handle) {
  // Validate nullness.
  if (!connection_handle) {
    return Status(StatusCode::kInvalidArgument, "Null connection handle");
  }
  // Common validation for internal members.
  auto* conn_handle_wrapped =
      reinterpret_cast<HandleWrapped*>(connection_handle);

  auto conn_handle_ptr_status = ValidateHandle<ConnectionHandle>(
      HandleType::kConnHandle, conn_handle_wrapped);
  if (!conn_handle_ptr_status.ok()) {
    return conn_handle_ptr_status.status();
  }

  auto* conn_handle_ptr = *conn_handle_ptr_status;

  if (!conn_handle_ptr->IsConnected()) {
    return Status(StatusCode::kInvalidArgument,
                  "Connection handle not connected to data source");
  }

  return conn_handle_ptr;
}

StatusOr<EnvironmentHandle*> ValidateEnvironmentHandle(
    SQLHENV environment_handle) {
  // Validate nullness.
  if (!environment_handle) {
    return Status(StatusCode::kInvalidArgument, "Null environment handle");
  }
  // Validate the internal members.
  auto* env_handle_wrapped =
      reinterpret_cast<HandleWrapped*>(environment_handle);

  return ValidateHandle<EnvironmentHandle>(HandleType::kEnvHandle,
                                           env_handle_wrapped);
}

}  // namespace google::cloud::odbc_bq_driver
