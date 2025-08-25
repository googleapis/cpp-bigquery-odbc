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
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_tables.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/optional.h"

namespace google::cloud::odbc_bq_driver_internal {

// Defines the fixed value columns for SQLColumns when no datasource value is
// configured. The values are different based on the data type.
struct FixedColumnMetadata {
  optional<std::int64_t> precision;
  optional<std::int64_t> scale;
  optional<std::int64_t> buf_len;
  optional<std::int64_t> char_octet_len;
  optional<std::int64_t> radix;
};

// Internal Helper functions used in SQLColumns API implementation.
odbc_internal::StatusRecordOr<FixedColumnMetadata> GetFixedColumnMetadata(
    std::string const& type, std::uint32_t column_size = 16384);

odbc_internal::StatusRecordOr<::google::cloud::optional<SQLINTEGER>> GetColSize(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema const&
        field_schema,
    std::uint32_t column_size = 16384, bool is_array = false);

odbc_internal::StatusRecordOr<::google::cloud::optional<SQLINTEGER>>
GetBufferLen(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema const&
        field_schema,
    std::uint32_t column_size = 16384, bool is_array = false);

odbc_internal::StatusRecordOr<::google::cloud::optional<SQLINTEGER>>
GetCharOctetLen(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema const&
        field_schema,
    std::uint32_t column_size = 16384, bool is_array = false);

odbc_internal::StatusRecordOr<::google::cloud::optional<SQLSMALLINT>>
GetDecimalDigits(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema const&
        field_schema,
    std::uint32_t column_size = 16384, bool is_array = false);

odbc_internal::StatusRecordOr<::google::cloud::optional<SQLSMALLINT>> GetRadix(
    ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema const&
        field_schema,
    std::uint32_t column_size = 16384, bool is_array = false);

odbc_internal::StatusRecordOr<std::string> GetTypeDescription(
    std::string const& field_schema_type);

odbc_internal::StatusRecordOr<::google::cloud::optional<SQLSMALLINT>>
GetSQLDateTimeSub(SQLSMALLINT sql_data_type, SQLSMALLINT data_type);

odbc_internal::StatusRecordOr<::google::cloud::optional<SQLSMALLINT>>
GetSQLDataType(SQLSMALLINT data_type);

odbc_internal::StatusRecord ValidateColumnParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* table_name, SQLSMALLINT table_name_len,
    const SQLCHAR* column_name, SQLSMALLINT column_name_len,
    SQLULEN metadata_id);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_COLUMNS_UTILS_H
