// Copyright 2023 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_JOBS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_JOBS_H

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

// Filters used for filtering a list of Jobs.
// returned in Job response.
struct JobFilter {
  // Whether to include jobs by all users.
  bool allUsers = false;
  // Minimum point in time for job creation time.
  std::chrono::system_clock::time_point min_creation_time;
  // Maximum point in time for job creation time.
  std::chrono::system_clock::time_point max_creation_time =
      std::chrono::system_clock::now();
  // Filtering by Job state: DONE, PENDING or RUNNING.
  ::google::cloud::bigquery_v2_minimal_internal::StateFilter state_filter;
  // Filters to return the child job of a specific parent.
  std::string parent_job_id;
  // Filtering based on specific Job fields: MINIMAL or FULL.
  ::google::cloud::bigquery_v2_minimal_internal::Projection projection;
};

// Allows filtering of query results.
struct QueryResultsFilterParams {
  // Zero based index of the starting row.
  std::uint64_t start_index = 0;
  // Maximum amount of time (in millis) the client is
  // willing to wait for the query completion.
  std::int64_t query_timeout_ms = 10000;  // 10s is the default value for BQ API
  // Maximum number of results to return.
  std::uint32_t max_results = 0;
  // Allows for pagination of results.
  std::string page_token;
};

// Returns detailed info for a specific Job
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Job>
GetJob(::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
       std::string const& project_id, std::string const& job_id,
       std::string const& location, ::google::cloud::Options const& options);

// Returns all Jobs in a Project
odbc_internal::StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
ListAllJobs(
    ::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
    std::string const& project_id, ::google::cloud::Options const& options);

// Returns a filtered list of Jobs in a Project, based on the job filters
// passed in
odbc_internal::StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
FilterJobs(::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
           std::string const& project_id, JobFilter const& job_filter,
           ::google::cloud::Options const& options);

// Inserts a BQ job for execution
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Job>
InsertJob(::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
          std::string const& project_id,
          ::google::cloud::bigquery_v2_minimal_internal::Job const& job,
          ::google::cloud::Options const& options);

// Cancels an already running BQ Job
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Job>
CancelJob(::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
          std::string const& project_id, std::string const& job_id,
          std::string const& location, ::google::cloud::Options const& options);

// Runs a BQ SQL query synchronously and returns query
// results if the query completes within a specified timeout.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults>
Query(::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
      std::string const& project_id,
      ::google::cloud::bigquery_v2_minimal_internal::QueryRequest const&
          query_request,
      ::google::cloud::Options const& options);

// Gets all the query results of a previously run query job.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
GetAllQueryResults(
    ::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
    std::string const& project_id, std::string const& job_id,
    std::string const& location, ::google::cloud::Options const& options);

// Gets query results, based on the filter passed in.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
FilterQueryResults(
    ::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
    std::string const& project_id, std::string const& job_id,
    std::string const& location,
    QueryResultsFilterParams const& query_results_filter,
    ::google::cloud::Options const& options);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_JOBS_H
