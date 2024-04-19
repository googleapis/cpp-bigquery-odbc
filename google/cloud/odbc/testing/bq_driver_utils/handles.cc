// Copyright 2024 Google LLC
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

#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/bq_driver/odbc_connection.h"
#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/bq_driver/odbc_statement.h"

namespace google::cloud::odbc_testing_bq_driver_utils {

using google::cloud::odbc_bq_driver::SQLAllocConnHandle;
using google::cloud::odbc_bq_driver::SQLAllocEnvHandle;
using google::cloud::odbc_bq_driver::SQLAllocStmtHandle;
using google::cloud::odbc_bq_driver::SQLFreeHandleInternal;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::StatementHandle;

SQLRETURN AllocateHandles(SQLHENV* env_handle_ref, SQLHDBC* conn_handle_ref) {
  SQLRETURN status = SQLAllocEnvHandle(env_handle_ref);
  // SQLRETURN status = SQLAllocHandle(SQL_HANDLE_ENV, NULL, env_handle_ref);
  if (status != SQL_SUCCESS) {
    return status;
  }
  return SQLAllocConnHandle(*env_handle_ref, conn_handle_ref);
  // return SQLAllocHandle(SQL_HANDLE_DBC, *env_handle_ref, conn_handle_ref);
}

SQLRETURN FreeHandles(SQLHENV env_handle, SQLHDBC conn_handle) {
  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DBC, conn_handle);
  // SQLRETURN status = SQLFreeHandle(SQL_HANDLE_DBC, conn_handle);
  if (status != SQL_SUCCESS) {
    return status;
  }
  return SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle);
  // return SQLFreeHandle(SQL_HANDLE_ENV, env_handle);
}

class FakeConnectionHandle : public ConnectionHandle {
 public:
  explicit FakeConnectionHandle() = default;
  void SetConnected() { is_connected_ = true; }
};

ConnectionHandle CreateConnectionHandle(bool is_connected = true) {
  FakeConnectionHandle conn_handle;
  if (is_connected) {
    conn_handle.SetConnected();
  }
  return conn_handle;
}

StatementHandle CreateStatementHandle() {
  DescriptorHandle ard(DescriptorType::kARD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ird(DescriptorType::kIRD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);
  return StatementHandle(nullptr, {ard, apd, ird, ipd});
}

}  // namespace google::cloud::odbc_testing_bq_driver_utils
