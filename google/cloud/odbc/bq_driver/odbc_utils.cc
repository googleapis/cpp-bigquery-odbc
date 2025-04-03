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
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::HandleType;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

static StatusRecord const kNullPointerStatusRecord =
    StatusRecord{SQLStates::k_HY000(), "Handle is null pointer"};
static StatusRecord const kInvalidTypeStatusRecord =
    StatusRecord{SQLStates::k_HY000(), "Invalid handle type"};

#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), \
                             apply_to = function)
StatusRecordOr<ConnectionHandle*> ValidateConnectionHandle(
    SQLHDBC connection_handle, bool check_if_connected) {
      std::cout<<"Inn "<<std::endl;
  if (connection_handle == nullptr) {
    std::cout<<"Inn iff "<<std::endl;
    return StatusRecordOr<ConnectionHandle*>(kNullPointerStatusRecord,
                                             SQL_INVALID_HANDLE);
  }
  std::cout<<"Inn1 "<<std::endl;
  auto* conn_handle_ptr =
      reinterpret_cast<ConnectionHandle*>(connection_handle);
      std::cout<<"Inn2 "<<std::endl;
  if (conn_handle_ptr->kType != HandleType::kConnHandle) {
    std::cout<<"Inn iff 2 "<<std::endl;
    return StatusRecordOr<ConnectionHandle*>(kInvalidTypeStatusRecord,
                                             SQL_INVALID_HANDLE);
  }
  std::cout<<"Inn3 "<<std::endl;
  conn_handle_ptr->GetDiagnostics().ClearDiagnostics();

  if (check_if_connected && !conn_handle_ptr->IsConnected()) {
    std::cout<<"Inn iff check "<<std::endl;
    StatusRecord status_record{
        SQLStates::k_08003(), "Connection handle not connected to data source"};
    conn_handle_ptr->GetDiagnostics().AddStatusRecord(status_record);
    return status_record;
  }
  std::cout<<"Inn4 "<<std::endl;
  return conn_handle_ptr;
}
#pragma clang attribute pop

#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), \
                             apply_to = function)
StatusRecordOr<EnvironmentHandle*> ValidateEnvironmentHandle(
    SQLHENV environment_handle) {
  if (environment_handle == nullptr) {
    return StatusRecordOr<EnvironmentHandle*>(kNullPointerStatusRecord,
                                              SQL_INVALID_HANDLE);
  }
  auto* env_handle_ptr =
      reinterpret_cast<EnvironmentHandle*>(environment_handle);
  if (env_handle_ptr->kType != HandleType::kEnvHandle) {
    return StatusRecordOr<EnvironmentHandle*>(kInvalidTypeStatusRecord,
                                              SQL_INVALID_HANDLE);
  }
  env_handle_ptr->GetDiagnostics().ClearDiagnostics();

  return env_handle_ptr;
}
#pragma clang attribute pop

#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), \
                             apply_to = function)
StatusRecordOr<StatementHandle*> ValidateStatementHandle(SQLHSTMT stmt_handle) {
  if (stmt_handle == nullptr) {
    return StatusRecordOr<StatementHandle*>(kNullPointerStatusRecord,
                                            SQL_INVALID_HANDLE);
  }
  auto* stmt_handle_ptr = reinterpret_cast<StatementHandle*>(stmt_handle);
  if (stmt_handle_ptr->kType != HandleType::kStmtHandle) {
    return StatusRecordOr<StatementHandle*>(kInvalidTypeStatusRecord,
                                            SQL_INVALID_HANDLE);
  }
  stmt_handle_ptr->GetDiagnostics().ClearDiagnostics();

  return stmt_handle_ptr;
}
#pragma clang attribute pop

#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), \
                             apply_to = function)
StatusRecordOr<DescriptorHandle*> ValidateDescriptorHandle(
    SQLHDESC desc_handle) {
  if (desc_handle == nullptr) {
    return StatusRecordOr<DescriptorHandle*>(kNullPointerStatusRecord,
                                             SQL_INVALID_HANDLE);
  }
  auto* desc_handle_ptr = reinterpret_cast<DescriptorHandle*>(desc_handle);
  if (desc_handle_ptr->kType != HandleType::kDescHandle) {
    return StatusRecordOr<DescriptorHandle*>(kInvalidTypeStatusRecord,
                                             SQL_INVALID_HANDLE);
  }
  desc_handle_ptr->GetDiagnostics().ClearDiagnostics();

  return desc_handle_ptr;
}
#pragma clang attribute pop

}  // namespace google::cloud::odbc_bq_driver
