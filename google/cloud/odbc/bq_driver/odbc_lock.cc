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

#include "google/cloud/odbc/bq_driver/odbc_lock.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::HandleType;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;

SQLRETURN AcquireHandleMutex(SQLHANDLE handle, SQLSMALLINT handleType) {
  if (!handle) {
    TracePrintInternal(*(*kTraceOption), "NULL SQL Handle");
    return SQL_NULL_HANDLE;
  }
  switch (handleType) {
    case SQL_HANDLE_ENV: {
      auto* env_handle_ptr = reinterpret_cast<EnvironmentHandle*>(handle);
      if (env_handle_ptr->kType != HandleType::kEnvHandle) {
        TracePrintInternal(*(*kTraceOption),
                           "Invalid Environment Handle Acquire");
        return SQL_INVALID_HANDLE;
      }
      env_handle_ptr->GetMutex().try_lock();
      break;
    }
    case SQL_HANDLE_DBC: {
      auto* conn_handle_ptr = reinterpret_cast<ConnectionHandle*>(handle);
      if (conn_handle_ptr->kType != HandleType::kConnHandle) {
        std::cout << "Handle type " << std::endl;
        TracePrintInternal(*(*kTraceOption),
                           "Invalid Connection Handle Acquire");
        return SQL_INVALID_HANDLE;
      }

      conn_handle_ptr->GetMutex().try_lock();
      break;
    }
    case SQL_HANDLE_STMT: {
      auto* stmt_handle_ptr = reinterpret_cast<StatementHandle*>(handle);
      if (stmt_handle_ptr->kType != HandleType::kStmtHandle) {
        TracePrintInternal(*(*kTraceOption),
                           "Invalid Statement Handle Acquire");
        return SQL_INVALID_HANDLE;
      }
      stmt_handle_ptr->GetMutex().try_lock();
      break;
    }
    case SQL_HANDLE_DESC: {
      auto* desc_handle_ptr = reinterpret_cast<DescriptorHandle*>(handle);
      if (desc_handle_ptr->kType != HandleType::kDescHandle) {
        TracePrintInternal(*(*kTraceOption),
                           "Invalid Descriptor Handle Acquire");
        return SQL_INVALID_HANDLE;
      }
      desc_handle_ptr->GetMutex().try_lock();
      break;
    }
    default:
      TracePrintInternal(*(*kTraceOption), "Invalid SQL Handle Acquire");
      return SQL_INVALID_HANDLE;
  }
  return SQL_SUCCESS;
}

SQLRETURN ReleaseHandleMutex(SQLHANDLE handle, SQLSMALLINT handleType) {
  if (!handle) {
    TracePrintInternal(*(*kTraceOption), "NULL SQL Handle");
    return SQL_NULL_HANDLE;
  }
  switch (handleType) {
    case SQL_HANDLE_ENV: {
      auto* env_handle_ptr = reinterpret_cast<EnvironmentHandle*>(handle);
      env_handle_ptr->GetMutex().unlock();
      break;
    }
    case SQL_HANDLE_DBC: {
      auto* conn_handle_ptr = reinterpret_cast<ConnectionHandle*>(handle);
      conn_handle_ptr->GetMutex().unlock();
      break;
    }
    case SQL_HANDLE_STMT: {
      auto* stmt_handle_ptr = reinterpret_cast<StatementHandle*>(handle);
      stmt_handle_ptr->GetMutex().unlock();
      break;
    }
    case SQL_HANDLE_DESC: {
      auto* desc_handle_ptr = reinterpret_cast<DescriptorHandle*>(handle);
      desc_handle_ptr->GetMutex().unlock();
      break;
    }
    default:
      TracePrintInternal(*(*kTraceOption), "Invalid SQL Handle");
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
