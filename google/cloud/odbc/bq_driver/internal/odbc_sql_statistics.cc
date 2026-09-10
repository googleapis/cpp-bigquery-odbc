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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_statistics.h"
#include "google/cloud/odbc/bq_client_interface/utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

// Returns a ResultSet containing table-level statistics for the given BigQuery
// table. BigQuery does not support traditional indexes, so only a
// SQL_TABLE_STAT row (TYPE = 0) is returned with the row count in CARDINALITY.
// All index-specific columns are set to NULL.
//
// Per the ODBC spec:
//  - If the table does not exist or catalog/schema arguments do not identify a
//    table, the function returns SQL_SUCCESS with an empty result set.
//  - CARDINALITY is the number of rows in the table (num_rows from BQ metadata).
//  - The unique argument is ignored since BigQuery has no traditional indexes.
//  - When reserved = SQL_QUICK, CARDINALITY and PAGES may be NULL; we always
//    return num_rows from BQ metadata since it is cheap to retrieve.
//
// See:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlstatistics-function
StatusRecordOr<ResultSet> FetchStatisticsResultSet(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    int catalog_name_len, std::string const& schema_name, int schema_name_len,
    std::string const& table_name, int table_name_len,
    SQLUSMALLINT unique, SQLUSMALLINT reserved) {

  if (unique != SQL_INDEX_UNIQUE && unique != SQL_INDEX_ALL) {
    LOG(ERROR) << "FetchStatisticsResultSet:: Invalid fUnique option: "
               << unique;
    return StatusRecord{SQLStates::k_HY100(), "Invalid fUnique option."};
  }

  if (reserved != SQL_ENSURE && reserved != SQL_QUICK) {
    LOG(ERROR) << "FetchStatisticsResultSet:: Invalid fAccuracy option: "
               << reserved;
    return StatusRecord{SQLStates::k_HY101(), "Invalid fAccuracy option."};
  }

  if (!stmt_handle.GetConnectionHandle()) {
    LOG(ERROR) << "FetchStatisticsResultSet:: Connection handle is null.";
    return StatusRecord{SQLStates::k_HY013(),
                        "Internal connection handle is null."};
  }

  // Initialize the result set schema.
  // Per the spec, SQLStatistics always returns the 13-column schema.
  ResultSet result_set;
  result_set.row_schema.resize(kStatisticsMap.size());
  for (auto const& [_, schema] : kStatisticsMap) {
    result_set.row_schema[schema.col_index] = schema;
  }

  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
