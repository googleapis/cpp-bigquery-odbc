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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_statistics.h"
#include "google/cloud/odbc/bq_client_interface/utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView;
using ::google::cloud::odbc_bigquery_client_interface::MaxRetriesOption;
using ::google::cloud::odbc_bigquery_client_interface::TableFilter;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

// Returns a ResultSet containing table-level statistics for the given BigQuery
// table. BigQuery does not support traditional indexes, so only a
// SQL_TABLE_STAT row (TYPE = 0) is returned with the row count in CARDINALITY.
// All index-specific columns are set to NULL.
//
// Per the ODBC spec:
//  - If the table does not exist or catalog/schema arguments do not identify a
//    table, the function returns SQL_SUCCESS with an empty result set.
//  - CARDINALITY is the number of rows in the table (num_rows from BQ
//  metadata).
//  - The unique argument is ignored since BigQuery has no traditional indexes.
//  - When reserved = SQL_QUICK, CARDINALITY and PAGES may be NULL; we always
//    return num_rows from BQ metadata since it is cheap to retrieve.
//
// See:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlstatistics-function
StatusRecordOr<ResultSet> FetchStatisticsResultSet(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    std::string const& schema_name, std::string const& table_name,
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

  // Per the ODBC spec, table_name is a required identifier (not a search
  // pattern). If it is empty or contains wildcard characters, return an empty
  // result set.
  std::cout<<"table name is "<<table_name<<std::endl;
  if (table_name.empty() ||
    absl::StrContains(table_name, "%") ||
    absl::StrContains(table_name, "\\")) {
    return result_set;
  }

  // Fetch table metadata from BigQuery to populate the SQL_TABLE_STAT row.
  auto bq_client = stmt_handle.GetConnectionHandle()->GetClient();
  if (!bq_client) {
    LOG(ERROR) << "FetchStatisticsResultSet:: Invalid or null BQ Client.";
    return StatusRecord{SQLStates::k_HY000(), "Invalid or null BQ Client."};
  }

  Options options;
  options.set<MaxRetriesOption>(
      stmt_handle.GetConnectionHandle()->GetDsn().max_retries);

  TableFilter filter{{}, TableMetadataView::Full()};

  auto table_status = bq_client->GetTable(catalog_name, schema_name,
                                          table_name, filter, options);
  if (!table_status) {
    // Per the ODBC spec: if the table is not found, return SQL_SUCCESS with
    // an empty result set.
    if (table_status.GetStatusRecord().native_error_code == 404) {
      LOG(INFO) << "FetchStatisticsResultSet:: Table not found, returning "
                   "empty result set.";
      return result_set;
    }
    LOG(ERROR) << "FetchStatisticsResultSet::GetTable:: "
               << table_status.GetStatusRecord().message;
    return table_status.GetStatusRecord();
  }

  auto const& table = *table_status;

  // Build the single SQL_TABLE_STAT row per the ODBC spec.
  // Columns: TABLE_CAT, TABLE_SCHEM, TABLE_NAME, NON_UNIQUE, INDEX_QUALIFIER,
  //          INDEX_NAME, TYPE, ORDINAL_POSITION, COLUMN_NAME, ASC_OR_DESC,
  //          CARDINALITY, PAGES, FILTER_CONDITION
  DSRow ds_row;

  // 1: TABLE_CAT
  DSValue ds_table_cat = kNullValue;
  if (!catalog_name.empty()) StringToDSValue(catalog_name, ds_table_cat);
  ds_row.push_back(ds_table_cat);

  // 2: TABLE_SCHEM
  DSValue ds_table_schem = kNullValue;
  if (!schema_name.empty()) StringToDSValue(schema_name, ds_table_schem);
  ds_row.push_back(ds_table_schem);

  // 3: TABLE_NAME
  DSValue ds_table_name = kNullValue;
  if (!table_name.empty()) StringToDSValue(table_name, ds_table_name);
  ds_row.push_back(ds_table_name);

  // 4: NON_UNIQUE — NULL for SQL_TABLE_STAT rows
  ds_row.push_back(kNullValue);

  // 5: INDEX_QUALIFIER — NULL for SQL_TABLE_STAT rows
  ds_row.push_back(kNullValue);

  // 6: INDEX_NAME — NULL for SQL_TABLE_STAT rows
  ds_row.push_back(kNullValue);

  // 7: TYPE — SQL_TABLE_STAT (0) indicates this is a table statistics row
  DSValue ds_type;
  ArithmeticToDSValue<SQLSMALLINT>(static_cast<SQLSMALLINT>(SQL_TABLE_STAT), ds_type);
  ds_row.push_back(ds_type);

  // 8: ORDINAL_POSITION — NULL for SQL_TABLE_STAT rows
  ds_row.push_back(kNullValue);

  // 9: COLUMN_NAME — NULL for SQL_TABLE_STAT rows
  ds_row.push_back(kNullValue);

  // 10: ASC_OR_DESC — NULL for SQL_TABLE_STAT rows
  ds_row.push_back(kNullValue);

  // 11: CARDINALITY — number of rows in the table
  DSValue ds_cardinality;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(table.num_rows), ds_cardinality);
  ds_row.push_back(ds_cardinality);

  // 12: PAGES — NULL (BigQuery has no concept of pages)
  ds_row.push_back(kNullValue);

  // 13: FILTER_CONDITION — NULL (only for filtered indexes)
  ds_row.push_back(kNullValue);

  result_set.rows.push_back(std::move(ds_row));
  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
