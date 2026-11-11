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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_EXECUTE_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_EXECUTE_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/data_translation_inv.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
#include "google/cloud/bigquery/storage/v1/bigquery_read_options.h"
#endif

namespace google::cloud::odbc_bq_driver_internal {

// Updates the list of `QueryParameter`s with the value for those parameters
// based on the C data type and SQL data type fetched from apd and ipd.
// We are passing `basic_query_params` as reference because we also need to
// read the BQ Data type of those params.
odbc_internal::StatusRecord ConstructPositionalQueryParams(
    DescriptorHandle& apd, DescriptorHandle& ipd,
    std::vector<::google::cloud::bigquery_v2_minimal_internal::QueryParameter>&
        basic_query_params,
    bool is_data_buff_req = false, int paramset_index = 0);

/*
 * @brief Executes a script (SQL query) using the given statement and connection
 * handles.
 *
 * This function validates the connection handle, retrieves the BigQuery client,
 * and executes the query. It then processes the results, retrieves job
 * information, and fetches query results if applicable. DML statistics are
 * updated based on the statement type, and session information is stored if
 * necessary.
 *
 * @param stmt_handle Reference to the statement handle, used to store job data
 * and results.
 * @param conn_handle Reference to the connection handle, used to validate
 * connection and retrieve BigQuery client.
 * @param post_query_request The request object containing query details and
 * execution options.
 *
 * @return StatusRecordOr<DSResults> Returns query execution results or an error
 * status if execution fails.
 *
 * Error Cases:
 * - Returns an error if the connection to the data source is broken.
 * - Returns an error if the BigQuery client is null or invalid.
 * - Returns an error if posting the query or fetching query results fails.
 */
odbc_internal::StatusRecordOr<DSResults> ExecuteScript(
    StatementHandle& stmt_handle,
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
        post_query_request);

/*
 * @brief If the executed query was a multi-statement script, populates child
 * jobs into the statement handle for SQLMoreResults and retrieves results of
 * the first statement.
 */
odbc_internal::StatusRecord PopulateScriptChildJobs(
    StatementHandle& stmt_handle, DSResults& results,
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
        post_query_request);

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
/*
 * @brief Identifies whether a stream error is transient (e.g., network
 * interruptions, proxy timeouts, RST_STREAM, connection resets).
 */
bool IsTransientStreamError(google::cloud::Status const& status);

/*
 * @brief Custom retry policy for BigQuery Storage Read API (HTAPI) streams
 * that classifies transient HTTP/2 and proxy disconnects (such as RST_STREAM,
 * Stream removed, and socket/connection resets) as retryable so
 * google-cloud-cpp's ResumableStreamingReadRpc automatically reconnects and
 * resumes from the last read row offset.
 */
class HtapiStreamRetryPolicy
    : public ::google::cloud::bigquery_storage_v1::BigQueryReadRetryPolicy {
 public:
  explicit HtapiStreamRetryPolicy(int maximum_failures)
      : maximum_failures_(maximum_failures < 0 ? 0 : maximum_failures) {}

  bool OnFailure(google::cloud::Status const& status) override {
    if (status.ok()) {
      return true;
    }
    if (!IsTransientStreamError(status)) {
      failure_count_ = maximum_failures_ + 1;
      return false;
    }
    ++failure_count_;
    return failure_count_ <= maximum_failures_;
  }

  bool IsExhausted() const override {
    return failure_count_ > maximum_failures_;
  }

  bool IsPermanentFailure(google::cloud::Status const& status) const override {
    return !status.ok() && !IsTransientStreamError(status);
  }

  std::unique_ptr<::google::cloud::bigquery_storage_v1::BigQueryReadRetryPolicy>
  clone() const override {
    return std::make_unique<HtapiStreamRetryPolicy>(maximum_failures_);
  }

 private:
  int failure_count_ = 0;
  int maximum_failures_;
};

/*
 * @brief Reads the next set of rows from the stream cached in the statement
 * handle
 */
StatusRecord ReadNextResultsFromStream(StatementHandle& stmt_handle);

#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

odbc_internal::StatusRecordOr<DSResults> FetchBQData(
    StatementHandle& stmt_handle,
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
        post_query_request,
    bool with_htapi = false);

odbc_internal::StatusRecord FetchNextPageResultSet(
    StatementHandle& stmt_handle);
odbc_internal::StatusRecordOr<
    google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
FetchNextPageOfQueryResults(
    StatementHandle& stmt_handle,
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
        post_query_request);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_EXECUTE_UTILS_H
