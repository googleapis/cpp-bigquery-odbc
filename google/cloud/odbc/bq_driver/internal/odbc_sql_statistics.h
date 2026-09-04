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
#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_STATISTICS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_STATISTICS_H

#include "google/cloud/odbc/internal/status_record_or.h"


namespace google::cloud::odbc_bq_driver_internal {

odbc_internal::StatusRecord ValidateStatisticsParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* table_name, SQLSMALLINT table_name_len,
    SQLUSMALLINT index_type, SQLUSMALLINT reserved,
    SQLULEN metadata_id);














}

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_TABLES_H
