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

#include "google/cloud/odbc/bq_client_interface/jobs.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_request.h"
#include <thread>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::CancelJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::InsertJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatJob;
using ::google::cloud::bigquery_v2_minimal_internal::ListJobsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Projection;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::internal::ExponentialBackoffPolicy;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using chrono_ms = std::chrono::milliseconds;

constexpr int kMaxChildJobsResults = 1000;

// When 'Job' object is created, all members are created with default values,
// usually empty strings. Client library doesn't provide any validation around
// it, even if BQ API returns an error. We use 'json_filter_keys' to filter out
// such information from the json in the request. Most of the time we simply
// don't need this data, but if some field is not empty, we should leave it in
// json and not populate it in json_filter_keys.
std::vector<std::string> CreateKeysToFilterOut(Job const& job) {
  std::vector<std::string> default_filtered_keys{
      "statistics",        "status",     "timePartitioning",
      "rangePartitioning", "clustering", "systemVariables"};
  if (job.configuration.query.default_dataset.project_id.empty() &&
      job.configuration.query.default_dataset.dataset_id.empty()) {
    default_filtered_keys.emplace_back("defaultDataset");
  }
  if (job.configuration.query.destination_table.project_id.empty() &&
      job.configuration.query.destination_table.dataset_id.empty() &&
      job.configuration.query.destination_table.table_id.empty()) {
    default_filtered_keys.emplace_back("destinationTable");
  }
  if (job.configuration.query.maximum_bytes_billed <= 0) {
    default_filtered_keys.emplace_back("maximumBytesBilled");
  }
  if (job.configuration.query.script_options.key_result_statement.value
          .empty()) {
    default_filtered_keys.emplace_back("keyResultStatement");
  }
  if (job.job_reference.project_id.empty() &&
      job.job_reference.job_id.empty()) {
    default_filtered_keys.emplace_back("jobReference");
  } else if (job.job_reference.location.empty()) {
    default_filtered_keys.emplace_back("location");
  }
  if (job.configuration.query.destination_encryption_configuration.kms_key_name
          .empty()) {
    default_filtered_keys.emplace_back("destinationEncryptionConfiguration");
  }
  return default_filtered_keys;
}

// When 'QueryRequest' object is created, all members are created with default
// values, usually empty strings. Client library doesn't provide any validation
// around it, even if BQ API returns an error. We use 'json_filter_keys' to
// filter out such information from the json in the request. Most of the time we
// simply don't need this data, but if some field is not empty, we should leave
// it in json and not populate it in json_filter_keys.
std::vector<std::string> CreateKeysToFilterOut(
    QueryRequest const& query_request) {
  std::vector<std::string> default_filtered_keys{"preserveNulls"};
  if (query_request.default_dataset().project_id.empty() &&
      query_request.default_dataset().dataset_id.empty()) {
    default_filtered_keys.emplace_back("defaultDataset");
  }
  if (query_request.maximum_bytes_billed() <= 0) {
    default_filtered_keys.emplace_back("maximumBytesBilled");
  }
  if (query_request.max_results() <= 0) {
    default_filtered_keys.emplace_back("maxResults");
  }
  if (query_request.timeout() == chrono_ms(0)) {
    default_filtered_keys.emplace_back("timeoutMs");
  }
  return default_filtered_keys;
}

StatusRecordOr<Job> GetJob(JobClient& job_client, std::string const& project_id,
                           std::string const& job_id,
                           std::string const& location,
                           Options const& options) {
  GetJobRequest get_job_request;
  get_job_request.set_project_id(project_id);
  get_job_request.set_job_id(job_id);
  get_job_request.set_location(location);

  return StatusRecordOr<Job>::ConvertFromStatusOr(
      job_client.GetJob(get_job_request, options));
}

StatusRecordOr<std::vector<ListFormatJob>> ListAllJobs(
    JobClient& job_client, std::string const& project_id,
    std::string const& parent_job_id, Options const& options) {
  // Validate inputs
  if (project_id.empty()) {
    return StatusRecord::ConvertFrom(
        Status(StatusCode::kInvalidArgument, "project_id cannot be empty"));
  }
  if (parent_job_id.empty()) {
    return StatusRecord::ConvertFrom(
        Status(StatusCode::kInvalidArgument, "parent_job_id cannot be empty"));
  }

  ListJobsRequest request;
  request.set_project_id(project_id);
  request.set_parent_job_id(parent_job_id);
  request.set_all_users(false);
  request.set_max_results(kMaxChildJobsResults);
  request.set_projection(Projection::Full());

  StreamRange<ListFormatJob> jobs_response =
      job_client.ListJobs(request, options);

  std::vector<ListFormatJob> jobs;
  for (auto const& job : jobs_response) {
    if (!job) {
      return StatusRecord::ConvertFrom(job.status());
    }
    jobs.push_back(*job);
  }

  return jobs;
}

StatusRecordOr<std::vector<ListFormatJob>> ListAllJobs(
    JobClient& job_client, std::string const& project_id,
    Options const& options) {
  ListJobsRequest request;
  request.set_project_id(project_id);
  request.set_all_users(false);
  request.set_max_results(kMaxChildJobsResults);
  request.set_projection(Projection::Full());

  StreamRange<ListFormatJob> jobs_response =
      job_client.ListJobs(request, options);

  std::vector<ListFormatJob> jobs;
  for (auto const& job : jobs_response) {
    if (!job) {
      return StatusRecord::ConvertFrom(job.status());
    }
    jobs.push_back(*job);
  }

  return jobs;
}

StatusRecordOr<std::vector<ListFormatJob>> FilterJobs(
    JobClient& job_client, std::string const& project_id,
    JobFilter const& job_filter, Options const& options) {
  ListJobsRequest request;
  request.set_project_id(project_id);
  request.set_all_users(job_filter.allUsers);
  request.set_max_creation_time(job_filter.max_creation_time);
  request.set_min_creation_time(job_filter.min_creation_time);
  request.set_state_filter(job_filter.state_filter);
  request.set_parent_job_id(job_filter.parent_job_id);
  request.set_projection(job_filter.projection);

  StreamRange<ListFormatJob> jobs_response =
      job_client.ListJobs(request, options);

  std::vector<ListFormatJob> jobs;
  for (auto const& job : jobs_response) {
    if (!job) {
      return StatusRecord::ConvertFrom(job.status());
    }
    jobs.push_back(*job);
  }

  return jobs;
}

StatusRecordOr<Job> InsertJob(JobClient& job_client,
                              std::string const& project_id, Job const& job,
                              Options const& options) {
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(CreateKeysToFilterOut(job));

  return StatusRecordOr<Job>::ConvertFromStatusOr(
      job_client.InsertJob(request, options));
}

StatusRecordOr<Job> CancelJob(JobClient& job_client,
                              std::string const& project_id,
                              std::string const& job_id,
                              std::string const& location,
                              Options const& options) {
  CancelJobRequest request;
  request.set_project_id(project_id);
  request.set_job_id(job_id);
  // Location may not be supplied for multi-region jobs.
  // e.g.
  // https://cloud.google.com/bigquery/docs/reference/rest/v2/jobs/cancel#query-parameters
  if (!location.empty()) {
    request.set_location(location);
  }

  return StatusRecordOr<Job>::ConvertFromStatusOr(
      job_client.CancelJob(request, options));
}

StatusRecordOr<PostQueryResults> Query(JobClient& job_client,
                                       std::string const& project_id,
                                       QueryRequest const& query_request,
                                       Options const& options) {
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(CreateKeysToFilterOut(query_request));

  return StatusRecordOr<PostQueryResults>::ConvertFromStatusOr(
      job_client.Query(post_query_request, options));
}

StatusRecordOr<PostQueryResults> PostQuery(
    JobClient& job_client, PostQueryRequest const& post_query_request,
    Options const& options) {
  return Query(job_client, post_query_request.project_id(),
               post_query_request.query_request(), options);
}

StatusRecordOr<GetQueryResults> GetAllQueryResults(
    JobClient& job_client, std::string const& project_id,
    std::string const& job_id, std::string const& location,
    chrono_ms timeout_ms, Options const& options) {
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_location(location);
  get_query_results_request.set_timeout(timeout_ms);

  GetQueryResults get_query_results;
  ExponentialBackoffPolicy backoff(chrono_ms(10), chrono_ms(500), 2);
  auto start_time = std::chrono::system_clock::now();

  while (true) {
    if (timeout_ms.count() > 0 &&
        std::chrono::system_clock::now() > start_time + timeout_ms) {
      std::string message = "The query timeout period of " +
                            std::to_string(timeout_ms.count()) +
                            "ms has expired";
      return StatusRecord{SQLStates::k_HYT00(), message};
    }
    std::this_thread::sleep_for(backoff.OnCompletion());
    StatusOr<GetQueryResults> get_query_results_partial =
        job_client.QueryResults(get_query_results_request, options);

    if (!get_query_results_partial) {
      return StatusRecord::ConvertFrom(get_query_results_partial.status());
    }

    // If job_complete is false, there would be no rows and we should wait for
    // job completion
    if (!get_query_results_partial->job_complete &&
        get_query_results_partial->rows.empty()) {
      continue;
    }
    if (get_query_results.rows.empty()) {
      // It's the first response. Copy it.
      get_query_results = *get_query_results_partial;
    } else {
      get_query_results.rows.insert(get_query_results.rows.end(),
                                    get_query_results_partial->rows.begin(),
                                    get_query_results_partial->rows.end());
    }

    if (get_query_results_partial->page_token.empty()) {
      get_query_results.page_token = "";
      break;
    }
    get_query_results_request.set_page_token(
        get_query_results_partial->page_token);
  }

  return get_query_results;
}

StatusRecordOr<GetQueryResults> FilterQueryResults(
    JobClient& job_client, std::string const& project_id,
    std::string const& job_id, std::string const& location,
    QueryResultsFilterParams const& query_results_filter,
    Options const& options) {
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_location(location);
  get_query_results_request.set_start_index(query_results_filter.start_index);
  get_query_results_request.set_timeout(
      chrono_ms(query_results_filter.query_timeout_ms));
  get_query_results_request.set_max_results(query_results_filter.max_results);
  get_query_results_request.set_page_token(query_results_filter.page_token);

  return StatusRecordOr<GetQueryResults>::ConvertFromStatusOr(
      job_client.QueryResults(get_query_results_request, options));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
