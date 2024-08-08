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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_BQ_DRIVER_UTILS_HANDLES_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_BQ_DRIVER_UTILS_HANDLES_H

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_testing_bq_driver_utils {

SQLRETURN AllocateHandles(SQLHENV* env_handle_ref, SQLHDBC* conn_handle_ref);

SQLRETURN FreeHandles(SQLHENV env_handle, SQLHDBC conn_handle);

odbc_bq_driver_internal::ConnectionHandle CreateConnectionHandle(
    bool is_connected = true);

odbc_bq_driver_internal::StatementHandle CreateStatementHandle();
odbc_bq_driver_internal::StatementHandle CreatePreparedStatementHandle();
odbc_bq_driver_internal::StatementHandle CreateExecutedStatementHandle();

odbc_bq_driver_internal::DescriptorHandle CreateExplicitDescriptor();

odbc_bq_driver_internal::DescriptorRecord CreateDescRecordWithRandomValues(
    SQLSMALLINT concise_type);

}  // namespace google::cloud::odbc_testing_bq_driver_utils

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_BQ_DRIVER_UTILS_HANDLES_H
