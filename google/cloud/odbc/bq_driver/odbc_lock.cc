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

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;

void AcquireHandleMutex(SQLHANDLE handle, SQLSMALLINT handleType) {
  switch (handleType) {
    case SQL_HANDLE_ENV: {
      auto* Env_Handle_ptr = reinterpret_cast<EnvironmentHandle*>(handle);
      Env_Handle_ptr->environment_handle_mutex_.lock();
      break;
    }
    case SQL_HANDLE_DBC: {
      auto* Conn_Handle_ptr = reinterpret_cast<ConnectionHandle*>(handle);
      Conn_Handle_ptr->connection_handle_mutex_.lock();
      break;
    }
    case SQL_HANDLE_STMT: {
      auto* Stmt_Handle_ptr = reinterpret_cast<StatementHandle*>(handle);
      Stmt_Handle_ptr->statement_handle_mutex_.lock();
      break;
    }
    case SQL_HANDLE_DESC: {
      auto* Desc_Handle_ptr = reinterpret_cast<DescriptorHandle*>(handle);
      Desc_Handle_ptr->descriptor_handle_mutex_.lock();
      break;
    }
    default:
      std::cout << "Unknown Handle Type to Acquire the lock" << std::endl;
  }
}

void ReleaseHandleMutex(SQLHANDLE handle, SQLSMALLINT handleType) {
  switch (handleType) {
    case SQL_HANDLE_ENV: {
      auto* Env_Handle_ptr = reinterpret_cast<EnvironmentHandle*>(handle);
      Env_Handle_ptr->environment_handle_mutex_.unlock();
      break;
    }
    case SQL_HANDLE_DBC: {
      auto* Conn_Handle_ptr = reinterpret_cast<ConnectionHandle*>(handle);
      Conn_Handle_ptr->connection_handle_mutex_.unlock();
      break;
    }
    case SQL_HANDLE_STMT: {
      auto* Stmt_Handle_ptr = reinterpret_cast<StatementHandle*>(handle);
      Stmt_Handle_ptr->statement_handle_mutex_.unlock();
      break;
    }
    case SQL_HANDLE_DESC: {
      auto* Desc_Handle_ptr = reinterpret_cast<DescriptorHandle*>(handle);
      Desc_Handle_ptr->descriptor_handle_mutex_.unlock();
      break;
    }
    default:
      std::cout << "Unknown Handle Type to Release the lock" << std::endl;
  }
}

}  // namespace google::cloud::odbc_bq_driver
