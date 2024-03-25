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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_statement_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

template <typename T>
odbc_internal::StatusRecordOr<T*> CastToHandle(HandleType handle_type,
                                               SQLHANDLE input_handle) {
  if (input_handle == nullptr) {
    return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                       "Handle is null pointer"};
  }
  auto* handle_wrapped = reinterpret_cast<HandleWrapped*>(input_handle);
  if (handle_type != handle_wrapped->handle_type) {
    return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                       "Invalid handle type"};
  }
  if (handle_wrapped->handle_ref == nullptr) {
    return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                       "Null internal handle reference"};
  }
  return reinterpret_cast<T*>(handle_wrapped->handle_ref);
}

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::ConnectionHandle*>
ValidateConnectionHandle(SQLHDBC connection_handle);

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::EnvironmentHandle*>
ValidateEnvironmentHandle(SQLHENV environment_handle);

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::StatementHandle*>
ValidateStatementHandle(SQLHSTMT stmt_handle);

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::DescriptorHandle*>
ValidateDescriptorHandle(SQLHDESC desc_handle);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
