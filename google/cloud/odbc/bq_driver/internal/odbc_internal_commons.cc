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

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"

namespace google::cloud::odbc_bq_driver_internal {


using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetReference;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterType;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterValue;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

StatusRecordOr<ResultSet> ProcessResultSetRows(
    TableSchema const& schema, std::vector<RowData> const& rows) {
  ResultSet result_set;
  // Populate the schema for each row. The row schema
  // indicates how they should converted back for the application buffers in
  // SQLFetch.
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
  // Populate the data for each row.
  for (auto const& row : rows) {
    DSRow rs_row;
    int i = 0;
    for (auto const& col : row.columns) {
      BQDataType col_type = result_set.row_schema[i++].col_type;
      std::string data = col.value;
      if (!data.empty()) {
        DSValue row_val;
        switch (col_type) {
          case BQDataType::kString: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kInt64: {
            SQLBIGINT l_data = std::stoll(data);
            ArithmeticToDSValue<SQLBIGINT>(l_data, row_val);
            break;
          }
          default: {
            return StatusRecord{SQLStates::k_HY000(),
                                "Invalid or unsupported col BQ data type"};
          }
        }
        rs_row.emplace_back(row_val);
      }
    }
    result_set.rows.emplace_back(rs_row);
  }
  return result_set;
}

StatusRecordOr<ResultSet> ProcessPostQueryResults(
    PostQueryResults const& postQueryResults) {
  if (!postQueryResults.job_complete) {
    // If this method is being called then the assumption is PostQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(postQueryResults.schema, postQueryResults.rows);
}

StatusRecordOr<ResultSet> ProcessGetQueryResults(
    GetQueryResults const& getQueryResults) {
  if (!getQueryResults.job_complete) {
    // If this method is being called then the assumption is GetQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(getQueryResults.schema, getQueryResults.rows);
}

StatusRecordOr<ResultSet> ProcessQueryResults(
    DSResults const& queryResults) {
  if (absl::holds_alternative<PostQueryResults>(
          queryResults.data_source_results)) {
    return ProcessPostQueryResults(
        absl::get<PostQueryResults>(queryResults.data_source_results));
  }
  if (absl::holds_alternative<GetQueryResults>(
          queryResults.data_source_results)) {
    return ProcessGetQueryResults(
        absl::get<GetQueryResults>(queryResults.data_source_results));
  }
  return StatusRecord{SQLStates::k_HY000(), "Invalid query results object"};
}

StatusRecordOr<DSResults> FetchBQData(
    ConnectionHandle& conn_handle, PostQueryRequest const& postQueryResults) {
  // Validate the  connection handle.
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }
  // For now , we use default options.
  // We can set timeout here as needed later.
  Options options;
  auto pq_status = bq_client->PostQuery(postQueryResults, options);
  if (!pq_status) {
    return pq_status.GetStatusRecord();
  }
  DSResults results;
  if (pq_status->job_complete && pq_status->page_token.empty()) {
    // we have gotten all the results
    results.data_source_results = *pq_status;
  } else {
    // Call GetAllQueryResults to get all the query results.
    auto gq_status = bq_client->GetAllQueryResults(
        pq_status->job_reference.project_id, pq_status->job_reference.job_id,
        pq_status->job_reference.location, options);
    if (!gq_status) {
      return gq_status.GetStatusRecord();
    }
    results.data_source_results = *gq_status;
  }
  return results;
}

StatusRecordOr<BQDataType> ConvertDSType(
    std::string const& type) {
  if (type == "STRING") {
    return BQDataType::kString;
  }
  if (type == "INTEGER" || type == "INT64") {
    return BQDataType::kInt64;
  }
  if (type == "BOOL" || type == "BOOLEAN") {
    return BQDataType::kBool;
  }
  if (type == "FLOAT64" || type == "FLOAT") {
    return BQDataType::kFloat64;
  }
  if (type == "DECIMAL" || type == "NUMERIC") {
    return BQDataType::kNumeric;
  }
  if (type == "BYTES") {
    return BQDataType::kBytes;
  }
  if (type == "DATE") {
    return BQDataType::kDate;
  }
  if (type == "DATETIME") {
    return BQDataType::kDatetime;
  }
  if (type == "TIME") {
    return BQDataType::kTime;
  }
  if (type == "TIMESTAMP") {
    return BQDataType::kTimeStamp;
  }
  if (type == "BIGNUMERIC") {
    return BQDataType::kBigNumeric;
  }
  if (type == "RANGE") {
    return BQDataType::kRange;
  }
  if (type == "STRUCT") {
    return BQDataType::kStruct;
  }
  if (type == "JSON") {
    return BQDataType::kJson;
  }
  if (type == "NULL") {
    return BQDataType::kNull;
  }
  if (type == "INTERVAL") {
    return BQDataType::kInterval;
  }
  if (type == "GEOGRAPHY") {
    return BQDataType::kGeography;
  }
  if (type == "ARRAY") {
    return BQDataType::kArray;
  }
  std::string err_msg = "Invalid Data Type: ";
  err_msg.append(type);
  return StatusRecord{SQLStates::k_HY000(), err_msg};
}

/*
StatusRecordOr<std::vector<RowData>> ExecuteQuery(std::string query_str, std::string catalog_name) {
  std::string dataset_id = "abc";
  PostQueryRequest post_request;
  QueryRequest query_request;

  DatasetReference ds_ref;
  // Set dataset info.
  ds_ref.project_id = catalog_name;
  ds_ref.dataset_id = dataset_id;
  query_request.set_default_dataset(ds_ref);

  query_request.set_dry_run(false);
  query_request.set_query(query_str);
  query_request.set_use_legacy_sql(false);

  // Set billing info and query request.
  post_request.set_project_id(catalog_name);
  post_request.set_query_request(query_request);
  auto status_record_or = FetchBQData(conn_handle, post_request);
  if (!status_record_or) {
    return status_record_or.GetStatusRecord();
  }
  std::vector<RowData> results;
  return results;
}
*/

StatusRecordOr<std::vector<QueryParameter>>
ConstructStringQueryParameters(
    std::map<std::string, std::string> const& params) {
  std::vector<QueryParameter> query_params;
  for (auto const& entry : params) {
    std::string parameter_name = entry.first;
    std::string parameter_value = entry.second;

    if (parameter_name.empty()) {
      return StatusRecord{SQLStates::k_HY000(), "Invalid parameter name"};
    }
    if (parameter_value.empty()) {
      return StatusRecord{SQLStates::k_HY000(), "Invalid parameter value"};
    }

    QueryParameter query_param;
    QueryParameterType query_param_type;
    QueryParameterValue query_param_value;

    query_param_type.type = "STRING";
    query_param_value.value = parameter_value;
    query_param.name = parameter_name;
    query_param.parameter_type = query_param_type;
    query_param.parameter_value = query_param_value;

    query_params.emplace_back(query_param);
  }
  return query_params;
}

}  // namespace google::cloud::odbc_bq_driver_internal
