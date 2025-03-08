// Copyright 2025 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"

//////////////////////////////////////////////////////////////////
// This file has query execution related utilities which can have
// statement or descriptor handles as arguments. We have some utils
// in `odbc_internal_commons` but those cannot include any handles
// except connection handle to avoid cyclic dependencies.
//////////////////////////////////////////////////////////////////

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DoubleStrToInt;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;

StatusRecord ConstructPositionalQueryParams(
    DescriptorHandle& apd, DescriptorHandle& ipd,
    std::vector<QueryParameter>& basic_query_params) {
  for (int param_ind = 0; param_ind < basic_query_params.size(); param_ind++) {
    if (!apd.HasDescriptorRecord(param_ind + 1)) {
      return StatusRecord{
          SQLStates::k_07002(),
          "Expected descriptor record does not exist during query execution."};
    }
    DescriptorRecord& apd_rec = apd.GetDescriptorRecord(param_ind + 1);
    // SQL_NULL_DATA implies the application wants to use empty data.
    if (apd_rec.indicator_ptr != nullptr &&
        *apd_rec.indicator_ptr == SQL_NULL_DATA) {
      continue;
    }
    if (apd_rec.data_ptr == nullptr) {
      return StatusRecord{SQLStates::k_HY009(),
                          "The bound param buffer was null"};
    }

    DataBuffer data = {apd_rec.concise_type, apd_rec.data_ptr,
                       apd_rec.octet_length, apd_rec.octet_length_ptr};

    DescriptorRecord& ipd_rec = ipd.GetDescriptorRecord(param_ind + 1);
    if (!ipd.HasDescriptorRecord(param_ind + 1)) {
      return StatusRecord{
          SQLStates::k_07002(),
          "Expected descriptor record does not exist during query execution."};
    }
    SQLSMALLINT sql_type = ipd_rec.concise_type;
    StatusRecordOr<std::string> conv_status = ConvertFromBuffer(data, sql_type);
    if (!conv_status) {
      return conv_status.GetStatusRecord();
    }
    std::string& value_str = *conv_status;
    // "INT64" is a special case where a string like "23.000" will not be
    // accepted by the BQ Server. For ex, this may occur when translating from
    // SQL_C_CHAR->SQL_DOUBLE.
    if (basic_query_params[param_ind].parameter_type.type == "INT64") {
      // Both integral and floating point values can be expressed as a double.
      // DoubleStrToInt will succeed for those but fail for non-arithmetic
      // value.
      StatusRecord status = DoubleStrToInt(value_str);
      if (!status.ok()) {
        return status;
      }
    }
    basic_query_params[param_ind].parameter_value.value = value_str;
  }
  return StatusRecord::Ok();
}

StatusRecordOr<DSResults> ExecuteScript(
    StatementHandle& stmt_handle, PostQueryRequest const& post_query_request) {
  ConnectionHandle* conn_handle = stmt_handle.GetConnectionHandle();
  if (!conn_handle) {
    return StatusRecord{SQLStates::k_HY009(), "Invalid statement handle"};
  }

  // Validate connection handle
  if (!conn_handle->IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle->GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  // Execute the query
  Options post_query_options;
  auto pq_status = bq_client->PostQuery(post_query_request, post_query_options);
  if (!pq_status) {
    return pq_status.GetStatusRecord();
  }

  DSResults results;
  results.dml_stats = pq_status->dml_stats;
  results.data_source_results = *pq_status;

  // Retrieve job information
  Options list_job_options;
  auto all_jobs_status =
      bq_client->ListAllJobs(pq_status->job_reference.project_id,
                             pq_status->job_reference.job_id, list_job_options);

  for (auto const& job_status : all_jobs_status.GetValue()) {
    if (job_status.statistics.job_query_stats.statement_type !=
        "CREATE_PROCEDURE") {
      stmt_handle.SetJobData(
          job_status.job_reference.job_id,
          job_status.statistics.job_query_stats.statement_type);
    }
  }

  // Fetch query results if job data is available
  if (!stmt_handle.HasJobData()) {
    return results;
  }

  auto job_data = stmt_handle.GetNextJobData();
  std::string job_id = job_data.first;
  std::string statement_type = job_data.second;

  Options query_results_options;
  if (pq_status->job_complete) {
    auto gq_status = bq_client->GetAllQueryResults(
        pq_status->job_reference.project_id, job_id,
        pq_status->job_reference.location,
        post_query_request.query_request().timeout(), query_results_options);

    if (!gq_status) {
      return gq_status.GetStatusRecord();
    }

    // Assign DML row counts
    std::int64_t dml_affected_rows = gq_status->num_dml_affected_rows;
    if (statement_type == "INSERT") {
      results.dml_stats.inserted_row_count = dml_affected_rows;
    } else if (statement_type == "UPDATE") {
      results.dml_stats.updated_row_count = dml_affected_rows;
    } else if (statement_type == "DELETE") {
      results.dml_stats.deleted_row_count = dml_affected_rows;
    }
    results.data_source_results = *gq_status;
  }
  stmt_handle.SetDSResults(results);

  if (!conn_handle->IsSessionStarted() &&
      !pq_status->session_info.session_id.empty()) {
    conn_handle->SetSessionId(pq_status->session_info.session_id);
  }

  return results;
}

}  // namespace google::cloud::odbc_bq_driver_internal
