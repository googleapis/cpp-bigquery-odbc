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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_primary_keys.h"
#include <variant>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Struct;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using ::google::cloud::bigquery_v2_minimal_internal::Value;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {
StatusRecordOr<ResultSet> ProcessResultSetRows(
    TableSchema const& schema, std::vector<Struct> const& rows) {
  // Per the SQLPrimaryKeys design, query results returned by the data source
  // are as follows:
  // Col-0: Table Catalog name, STRING
  // Col-1: Table Dataset/Schema name, STRING
  // Col-2: Table Name, STRING
  // Col-3: Column Name, STRING
  // Col-4: Ordinal Position, INTEGER
  // Col-5: PK Constraint Name, STRING
  ResultSet result_set;
  // Populate the schema first
  for (int i = 0; i < schema.fields.size(); i++) {
    TableFieldSchema table_field_schema = schema.fields[i];
    StatusRecordOr<BQDataType> type_status_record =
        ConvertDSType(table_field_schema.type);
    if (!type_status_record.Ok()) {
      return type_status_record.GetStatusRecord();
    }
    ColumnSchema col_schema;
    col_schema.col_index = i;
    col_schema.col_type = *type_status_record;
    result_set.row_schema.emplace_back(col_schema);
  }
  // Now populate the data for each row. For SQLPrimaryKeys all column data are
  // stored as strings because of how the server returns them. The row schema
  // indicates how they should converted back for the application buffers in
  // SQLFetch.
  for (int i = 0; i < rows.size(); i++) {
    Struct struct_val = rows[i];
    DSRow rs_row;
    for (auto field_entry : struct_val.fields) {
      Value bq_val = field_entry.second;
      std::string data = absl::get<std::string>(bq_val.value_kind);
      if (!data.empty()) {
        DSValue row_val;
        StringToDSValue(data, row_val);
        rs_row.emplace_back(row_val);
      }
    }
    result_set.rows.emplace_back(rs_row);
  }
  return result_set;
}
}  // namespace

odbc_internal::StatusRecordOr<BQDataType> ConvertDSType(
    std::string const& type) {
  if (type == "STRING") {
    return BQDataType::kString;
  } else if (type == "INTEGER") {
    return BQDataType::kInt64;
  }
  std::string err_msg = "Invalid Data Type: ";
  err_msg.append(type);
  return StatusRecord{SQLStates::k_HY000(), err_msg};
}

StatusRecordOr<DSPrimaryKeysResults> FetchPrimaryKeysFromDataSource(
    std::string const& catalog_name, int catalogNameLen,
    std::string const& schemaName, int schemaNameLen,
    std::string const& tableName, int tableNameLen) {
  // Not Yet Implemented.
  return StatusRecord::Ok();
}

StatusRecordOr<ResultSet> ProcessPKPostQueryResults(
    PostQueryResults const& primaryKeysQueryResults) {
  if (!primaryKeysQueryResults.job_complete) {
    // If this method is being called then the assumption is PostQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(primaryKeysQueryResults.schema,
                              primaryKeysQueryResults.rows);
}

StatusRecordOr<ResultSet> ProcessPKGetQueryResults(
    GetQueryResults const& primaryKeysQueryResults) {
  if (!primaryKeysQueryResults.job_complete) {
    // If this method is being called then the assumption is GetQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(primaryKeysQueryResults.schema,
                              primaryKeysQueryResults.rows);
}

}  // namespace google::cloud::odbc_bq_driver_internal
