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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_LOCK_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_LOCK_H

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver {

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// ODBC locking:
//
// AcquireMutex (based on handle type)
// ReleaseMutex (based on handle type)
///////////////////////////////////////////////////////////

SQLRETURN AcquireHandleMutex(SQLHANDLE handle, SQLSMALLINT handle_type,
                             bool is_global = false);
SQLRETURN ReleaseHandleMutex(SQLHANDLE handle, SQLSMALLINT handle_type,
                             bool is_global = false);
SQLRETURN GetParentHandles(SQLHANDLE& handle, SQLSMALLINT& handle_type,
                           bool& is_global);

class HandleLock {
 public:
  HandleLock(SQLHANDLE handle, SQLSMALLINT handle_type,
             bool lock_parent = false);
  ~HandleLock();

  [[nodiscard]] bool isLocked() const { return locked_; }

  // Prevent copying
  HandleLock(HandleLock const&) = delete;
  HandleLock& operator=(HandleLock const&) = delete;

 private:
  void Acquire(bool lock_parent);
  void Release();

  SQLHANDLE handle_;
  SQLSMALLINT handle_type_;
  bool is_global_{false};
  bool locked_{false};
};

void HandleLockError(SQLSMALLINT handleType,
                            SQLHANDLE inputHandle,
                            const std::string& context);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_LOCK_H
