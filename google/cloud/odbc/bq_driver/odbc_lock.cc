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

namespace {
std::mutex g_driver_mutex;  // Global driver mutex for driver-wide operations
}
using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::HandleType;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;

SQLRETURN AcquireHandleMutex(SQLHANDLE handle, SQLSMALLINT handle_type,
                             bool is_global) {
  if (!handle) {
    LOG(ERROR) << "AcquireHandleMutex::NULL SQL Handle";
    return SQL_NULL_HANDLE;
  }

  if (is_global) {
    g_driver_mutex.lock();
    return SQL_SUCCESS;
  }
  switch (handle_type) {
    case SQL_HANDLE_ENV: {
      auto* env_handle_ptr = reinterpret_cast<EnvironmentHandle*>(handle);
      if (env_handle_ptr->kType != HandleType::kEnvHandle) {
        LOG(ERROR) << "AcquireHandleMutex::Invalid Environment Handle Acquire";
        return SQL_INVALID_HANDLE;
      }
      env_handle_ptr->GetMutex().lock();
      break;
    }
    case SQL_HANDLE_DBC: {
      auto* conn_handle_ptr = reinterpret_cast<ConnectionHandle*>(handle);
      if (conn_handle_ptr->kType != HandleType::kConnHandle) {
        LOG(ERROR) << "AcquireHandleMutex::Invalid Connection Handle Acquire";
        return SQL_INVALID_HANDLE;
      }

      conn_handle_ptr->GetMutex().lock();
      break;
    }
    case SQL_HANDLE_STMT: {
      auto* stmt_handle_ptr = reinterpret_cast<StatementHandle*>(handle);
      if (stmt_handle_ptr->kType != HandleType::kStmtHandle) {
        LOG(ERROR) << "AcquireHandleMutex::Invalid Statement Handle Acquire";
        return SQL_INVALID_HANDLE;
      }
      stmt_handle_ptr->GetMutex().lock();
      break;
    }
    case SQL_HANDLE_DESC: {
      auto* desc_handle_ptr = reinterpret_cast<DescriptorHandle*>(handle);
      if (desc_handle_ptr->kType != HandleType::kDescHandle) {
        LOG(ERROR) << "AcquireHandleMutex::Invalid Descriptor Handle Acquire";
        return SQL_INVALID_HANDLE;
      }
      desc_handle_ptr->GetMutex().lock();
      break;
    }
    default:
      LOG(ERROR) << "AcquireHandleMutex::Invalid SQL Handle Acquire";
      return SQL_INVALID_HANDLE;
  }
  return SQL_SUCCESS;
}

SQLRETURN ReleaseHandleMutex(SQLHANDLE handle, SQLSMALLINT handle_type,
                             bool is_global) {
  if (!handle) {
    LOG(ERROR) << "ReleaseHandleMutex::NULL SQL Handle";
    return SQL_NULL_HANDLE;
  }

  if (is_global) {
    g_driver_mutex.unlock();
    return SQL_SUCCESS;
  }
  switch (handle_type) {
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
      LOG(ERROR) << "ReleaseHandleMutex::Invalid SQL Handle";
  }
  return SQL_SUCCESS;
}

SQLRETURN GetParentHandles(SQLHANDLE& handle, SQLSMALLINT& handle_type,
                           bool& is_global) {
  if (!handle) {
    LOG(ERROR) << "GetParentHandles::NULL SQL Handle";
    return SQL_NULL_HANDLE;
  }

  is_global = false;

  switch (handle_type) {
    case SQL_HANDLE_ENV:
      is_global = true;
      return SQL_SUCCESS;

    case SQL_HANDLE_DBC: {
      auto* conn_handle_ptr = reinterpret_cast<ConnectionHandle*>(handle);
      if (!conn_handle_ptr ||
          conn_handle_ptr->kType != HandleType::kConnHandle) {
        LOG(ERROR) << "GetParentHandles::Invalid Connection Handle Acquire";
        return SQL_INVALID_HANDLE;
      }
      handle = conn_handle_ptr->GetEnvironmentHandle();
      handle_type = SQL_HANDLE_ENV;
      return SQL_SUCCESS;
    }

    case SQL_HANDLE_STMT: {
      auto* stmt_handle_ptr = reinterpret_cast<StatementHandle*>(handle);
      if (!stmt_handle_ptr ||
          stmt_handle_ptr->kType != HandleType::kStmtHandle) {
        LOG(ERROR) << "GetParentHandles::Invalid Statement Handle Acquire";
        return SQL_INVALID_HANDLE;
      }
      handle = stmt_handle_ptr->GetConnectionHandle();
      handle_type = SQL_HANDLE_DBC;
      return SQL_SUCCESS;
    }

    case SQL_HANDLE_DESC: {
      auto* desc_handle_ptr = reinterpret_cast<DescriptorHandle*>(handle);
      if (!desc_handle_ptr ||
          desc_handle_ptr->kType != HandleType::kDescHandle) {
        LOG(ERROR) << "GetParentHandles::Invalid Descriptor Handle Acquire";
        return SQL_INVALID_HANDLE;
      }

      auto stmt_handles = desc_handle_ptr->GetAssociatedStatementHandles();
      for (auto const& pair : stmt_handles) {
        if (pair.second == desc_handle_ptr->GetType()) {
          handle = pair.first;
          handle_type = SQL_HANDLE_STMT;
          return SQL_SUCCESS;
        }
      }
      handle = desc_handle_ptr->GetConnectionHandle();
      handle_type = SQL_HANDLE_DBC;
      return SQL_SUCCESS;
    }

    default:
      LOG(ERROR) << "GetParentHandles::Invalid SQL Handle Acquire";
      return SQL_INVALID_HANDLE;
  }
}

HandleLock::HandleLock(SQLHANDLE handle, SQLSMALLINT handle_type,
                       bool lock_parent)
    : handle_(handle), handle_type_(handle_type) {
  Acquire(lock_parent);
}

HandleLock::~HandleLock() { Release(); }

void HandleLock::Acquire(bool lock_parent) {
  if (lock_parent) {
    SQLRETURN result = GetParentHandles(handle_, handle_type_, is_global_);
    if (result != SQL_SUCCESS) {
      LOG(ERROR) << "HandleLock::Acquire::GetParentHandles::Failed to get "
                    "parent handles";
      return;
    }
  }

  SQLRETURN result = AcquireHandleMutex(handle_, handle_type_, is_global_);
  if (result == SQL_SUCCESS) {
    locked_ = true;
  } else {
    LOG(ERROR) << "HandleLock::Acquire::AcquireHandleMutex::Failed to acquire "
                  "handle mutex";
  }
}

void HandleLock::Release() {
  if (locked_) {
    ReleaseHandleMutex(handle_, handle_type_, is_global_);
    locked_ = false;
  }
}

}  // namespace google::cloud::odbc_bq_driver

// namespace google::cloud::odbc_bq_driver
