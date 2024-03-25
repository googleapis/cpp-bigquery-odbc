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
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

StatusRecordOr<ConnectionHandle*> ValidateConnectionHandle(
    SQLHDBC connection_handle) {
  auto conn_handle_ptr_status = CastToHandle<ConnectionHandle>(
      HandleType::kConnHandle, connection_handle);
  if (!conn_handle_ptr_status) {
    return StatusRecordOr<ConnectionHandle*>(
        conn_handle_ptr_status.GetStatusRecord(), SQL_INVALID_HANDLE);
  }

  auto* conn_handle_ptr = *conn_handle_ptr_status;
  conn_handle_ptr->GetDiagnostics().ClearDiagnostics();

  if (!conn_handle_ptr->IsConnected()) {
    StatusRecord status_record{
        SQLStates::k_08003(), "Connection handle not connected to data source"};
    conn_handle_ptr->GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }

  return conn_handle_ptr;
}

StatusRecordOr<EnvironmentHandle*> ValidateEnvironmentHandle(
    SQLHENV environment_handle) {
  auto env_handle_ptr_status = CastToHandle<EnvironmentHandle>(
      HandleType::kEnvHandle, environment_handle);
  if (!env_handle_ptr_status) {
    return StatusRecordOr<EnvironmentHandle*>(
        env_handle_ptr_status.GetStatusRecord(), SQL_INVALID_HANDLE);
  }
  (*env_handle_ptr_status)->GetDiagnostics().ClearDiagnostics();

  return *env_handle_ptr_status;
}

StatusRecordOr<StatementHandle*> ValidateStatementHandle(SQLHSTMT stmt_handle) {
  auto stmt_handle_ptr_status =
      CastToHandle<StatementHandle>(HandleType::kStatementHandle, stmt_handle);
  if (!stmt_handle_ptr_status) {
    return StatusRecordOr<StatementHandle*>(
        stmt_handle_ptr_status.GetStatusRecord(), SQL_INVALID_HANDLE);
  }
  (*stmt_handle_ptr_status)->GetDiagnostics().ClearDiagnostics();

  return *stmt_handle_ptr_status;
}

StatusRecordOr<DescriptorHandle*> ValidateDescriptorHandle(
    SQLHDESC desc_handle) {
  auto desc_handle_ptr_status = CastToHandle<DescriptorHandle>(
      HandleType::kDescriptorHandle, desc_handle);
  if (!desc_handle_ptr_status) {
    return StatusRecordOr<DescriptorHandle*>(
        desc_handle_ptr_status.GetStatusRecord(), SQL_INVALID_HANDLE);
  }
  (*desc_handle_ptr_status)->GetDiagnostics().ClearDiagnostics();

  return *desc_handle_ptr_status;
}

}  // namespace google::cloud::odbc_bq_driver
