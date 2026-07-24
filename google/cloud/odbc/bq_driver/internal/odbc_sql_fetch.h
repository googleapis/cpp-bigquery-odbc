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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FETCH_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FETCH_H

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"

namespace google::cloud::odbc_bq_driver_internal {

// Writes rowset_size number of rows to the columns bound by the application
google::cloud::odbc_internal::StatusRecord WriteRowset(
    StatementHandle const& stmt_handle, ResultSet const& result_set,
    int rowset_size, DescriptorHandle& ard, DescriptorHandle& ird);

// Fetches the next batch of ResultSet rows
google::cloud::odbc_internal::StatusRecord FetchNextResultSet(
    StatementHandle& stmt_handle);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FETCH_H
