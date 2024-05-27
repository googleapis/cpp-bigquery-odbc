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
#include <iostream>

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// ODBC locking:
//
// AcquireMutex (based on handle type)
// ReleaseMutex (based on handle type)
///////////////////////////////////////////////////////////

namespace google::cloud::odbc_bq_driver {
void AcquireHandleMutex(SQLHANDLE handle, SQLSMALLINT handleType);

void ReleaseHandleMutex(SQLHANDLE handle, SQLSMALLINT handleType);
}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_LOCK_H
