// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_BQ_DRIVER_UTILS_STATUS_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_BQ_DRIVER_UTILS_STATUS_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"

namespace google::cloud::odbc_testing_bq_driver_utils {
odbc_internal::StatusRecord GetLastStatusRecord(
    google::cloud::odbc_bq_driver_internal::EnvironmentHandle& handle);
odbc_internal::StatusRecord GetLastStatusRecord(
    google::cloud::odbc_bq_driver_internal::StatementHandle& handle);
odbc_internal::StatusRecord GetLastStatusRecord(
    google::cloud::odbc_bq_driver_internal::ConnectionHandle& handle);
}  // namespace google::cloud::odbc_testing_bq_driver_utils

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_BQ_DRIVER_UTILS_STATUS_UTILS_H
