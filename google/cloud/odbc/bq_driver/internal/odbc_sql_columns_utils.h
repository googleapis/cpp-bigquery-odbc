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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_COLUMNS_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_COLUMNS_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

// Defines the fixed value columns for SQLColumns when no datasource value is
// configured. The values are different based on the data type.
struct FixedColumnMetadata {
  std::int64_t precision;
  std::int64_t scale;
  std::int64_t buf_len;
  std::int64_t char_octet_len;
};

// Internal Helper functions used in SQLColumns API implementation.
odbc_internal::StatusRecordOr<FixedColumnMetadata> GetFixedColumnMetadata(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema
        field_schema);

odbc_internal::StatusRecordOr<SQLINTEGER> GetColSize(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema
        field_schema);

odbc_internal::StatusRecordOr<SQLINTEGER> GetBufferLen(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema
        field_schema);

odbc_internal::StatusRecordOr<SQLINTEGER> GetCharOctetLen(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema
        field_schema);

odbc_internal::StatusRecordOr<SQLSMALLINT> GetDecimalDigits(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema
        field_schema);

odbc_internal::StatusRecordOr<SQLSMALLINT> GetRadix(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema
        field_schema);

odbc_internal::StatusRecordOr<SQLSMALLINT> GetSQLDateTimeSub(
    SQLSMALLINT sql_data_type, SQLSMALLINT data_type);

odbc_internal::StatusRecordOr<SQLSMALLINT> GetSQLDataType(
    SQLSMALLINT data_type);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_COLUMNS_UTILS_H
