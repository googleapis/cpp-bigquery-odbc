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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_STATISTICS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_STATISTICS_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <string>

namespace google::cloud::odbc_bq_driver_internal {

static std::map<std::string, ColumnSchema> const kStatisticsMap = {
    {kTableCatColName, WithIndex(0, kTableCatSchema)},
    {kTableSchemaColName, WithIndex(1, kTableSchemaSchema)},
    {kTableNameColName, WithIndex(2, kTableNameSchema)},
    {kNonUniqueColName, WithIndex(3, kNonUniqueSchema)},
    {kIndexQualifierColName, WithIndex(4, kIndexQualifierSchema)},
    {kIndexNameColName, WithIndex(5, kIndexNameSchema)},
    {kTypeColName, WithIndex(6, kTypeSchema)},
    {kOrdinalPositionColName, WithIndex(7, kOrdinalPositionSchema)},
    {kColumnNameColName, WithIndex(8, kColumnNameSchema)},
    {kAscOrDescColName, WithIndex(9, kAscOrDescSchema)},
    {kCardinalityColName, WithIndex(10, kCardinalitySchema)},
    {kPagesColName, WithIndex(11, kPagesSchema)},
    {kFilterConditionColName, WithIndex(12, kFilterConditionSchema)},
};

StatusRecordOr<ResultSet> FetchStatisticsResultSet(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    int catalog_name_len, std::string const& schema_name, int schema_name_len,
    std::string const& table_name, int table_name_len, SQLUSMALLINT unique,
    SQLUSMALLINT reserved);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_STATISTICS_H
