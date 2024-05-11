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

// Initialize the Singleton instance.
std::shared_ptr<ODBCLock> ODBCLock::odbc_lock_ = nullptr;
std::mutex ODBCLock::instance_mutex_;
std::mutex ODBCLock::environment_handle_mutex_;
std::mutex ODBCLock::connection_handle_mutex_;
std::mutex ODBCLock::statement_handle_mutex_;
std::mutex ODBCLock::descriptor_handle_mutex_;

void ODBCLock::AcquireHandleMutex(SQLSMALLINT handleType) {
  switch (handleType) {
    case SQL_HANDLE_ENV: {
      environment_handle_mutex_.lock();
      break;
    }
    case SQL_HANDLE_DBC: {
      connection_handle_mutex_.lock();
      break;
    }
    case SQL_HANDLE_STMT: {
      statement_handle_mutex_.lock();
      break;
    }
    case SQL_HANDLE_DESC: {
      descriptor_handle_mutex_.lock();
      break;
    }
    default:
      std::cout << "Unknown Acquire Handle Type" << std::endl;
  }
}

void ODBCLock::ReleaseHandleMutex(SQLSMALLINT handleType) {
  switch (handleType) {
    case SQL_HANDLE_ENV: {
      environment_handle_mutex_.unlock();
      break;
    }
    case SQL_HANDLE_DBC: {
      connection_handle_mutex_.unlock();
      break;
    }
    case SQL_HANDLE_STMT: {
      statement_handle_mutex_.unlock();
      break;
    }
    case SQL_HANDLE_DESC: {
      descriptor_handle_mutex_.unlock();
      break;
    }
    default:
      std::cout << "Release mutex" << std::endl;
  }
}

std::shared_ptr<ODBCLock> ODBCLock::GetODBCLockInstance() {
  std::lock_guard<std::mutex> lck(instance_mutex_);
  if (odbc_lock_ == nullptr) {
    // Cannot use std::make_shared because constructor is protected.
    odbc_lock_ = std::shared_ptr<ODBCLock>(new ODBCLock());
  }
  return odbc_lock_;
}

}  // namespace google::cloud::odbc_bq_driver
