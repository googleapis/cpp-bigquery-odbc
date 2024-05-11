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

#include <iostream>
#include <memory>
#include <mutex>
#include <sql.h>
#include <sqlext.h>
#include <thread>

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// ODBC locking:
//
// AcquireMutex (based on handle type)
// ReleaseMutex (based on handle type)
///////////////////////////////////////////////////////////

namespace google::cloud::odbc_bq_driver {

class ODBCLock {
 public:
  // Disallow Copy and Assignment.
  ODBCLock(ODBCLock& other) = delete;
  void operator=(ODBCLock const&) = delete;

  ///////////////////////////////////////////////////////////
  // Get ODBCLock Instance
  //
  // Returns a singleton object for ODBCLock
  ///////////////////////////////////////////////////////////
  static std::shared_ptr<ODBCLock> GetODBCLockInstance();

  static void AcquireHandleMutex(SQLSMALLINT handleType);

  static void ReleaseHandleMutex(SQLSMALLINT handleType);

 private:
  ODBCLock() = default;
  static std::shared_ptr<ODBCLock> odbc_lock_;

  // Declared separate mutex for each handle type
  static std::mutex environment_handle_mutex_;
  static std::mutex connection_handle_mutex_;
  static std::mutex statement_handle_mutex_;
  static std::mutex descriptor_handle_mutex_;
  static std::mutex instance_mutex_;
};

static std::shared_ptr<ODBCLock> const kOdbcLock =
    ODBCLock::GetODBCLockInstance();

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_LOCK_H
