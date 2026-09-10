// Copyright 2026 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_SPECIAL_COLUMNS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_SPECIAL_COLUMNS_H

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <map>
#include <string>

namespace google::cloud::odbc_bq_driver_internal {

inline constexpr char const* kScopeColName = "SCOPE";
inline constexpr char const* kDataTypeColName = "DATA_TYPE";
inline constexpr char const* kTypeNameColName = "TYPE_NAME";
inline constexpr char const* kColumnSizeColName = "COLUMN_SIZE";
inline constexpr char const* kBufferLengthColName = "BUFFER_LENGTH";
inline constexpr char const* kDecimalDigitsColName = "DECIMAL_DIGITS";
inline constexpr char const* kPseudoColumnColName = "PSEUDO_COLUMN";

inline constexpr ColumnSchema kScopeSchema{0, BQDataType::kInt64};
inline constexpr ColumnSchema kDataTypeSchema{0, BQDataType::kInt64};
inline constexpr ColumnSchema kTypeNameSchema{0, BQDataType::kString};
inline constexpr ColumnSchema kColumnSizeSchema{0, BQDataType::kInt64};
inline constexpr ColumnSchema kBufferLengthSchema{0, BQDataType::kInt64};
inline constexpr ColumnSchema kDecimalDigitsSchema{0, BQDataType::kInt64};
inline constexpr ColumnSchema kPseudoColumnSchema{0, BQDataType::kInt64};

inline std::map<std::string, ColumnSchema> const kSpecialColumnsMap{
    {kScopeColName, WithIndex(0, kScopeSchema)},
    {kColumnNameColName, WithIndex(1, kColumnNameSchema)},
    {kDataTypeColName, WithIndex(2, kDataTypeSchema)},
    {kTypeNameColName, WithIndex(3, kTypeNameSchema)},
    {kColumnSizeColName, WithIndex(4, kColumnSizeSchema)},
    {kBufferLengthColName, WithIndex(5, kBufferLengthSchema)},
    {kDecimalDigitsColName, WithIndex(6, kDecimalDigitsSchema)},
    {kPseudoColumnColName, WithIndex(7, kPseudoColumnSchema)}};

odbc_internal::StatusRecordOr<ResultSet>
FetchSpecialColumnsResultSetFromTableMetaData(
    StatementHandle& stmt_handle, SQLUSMALLINT identifier_type,
    std::string const& catalog_name, int catalog_name_len,
    std::string const& schema_name, int schema_name_len,
    std::string const& table_name, int table_name_len,
    SQLUSMALLINT min_row_id_scope, SQLUSMALLINT col_nullable);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_SPECIAL_COLUMNS_H
