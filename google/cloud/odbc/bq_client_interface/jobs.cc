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
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"

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
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;

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
  return default_filtered_keys;
}

StatusOr<Job> GetJob(JobClient& job_client, std::string const& project_id,
                     std::string const& job_id, std::string const& location,
                     Options const& options) {
  GetJobRequest get_job_request;
  get_job_request.set_project_id(project_id);
  get_job_request.set_job_id(job_id);
  get_job_request.set_location(location);

  return job_client.GetJob(get_job_request, options);
}

StatusOr<std::vector<ListFormatJob>> ListAllJobs(JobClient& job_client,
                                                 std::string const& project_id,
                                                 Options const& options) {
  ListJobsRequest request;
  request.set_project_id(project_id);

  StreamRange<ListFormatJob> jobs_response =
      job_client.ListJobs(request, options);

  std::vector<ListFormatJob> jobs;
  for (auto const& job : jobs_response) {
    if (!job) {
      return job.status();
    }
    jobs.push_back(*job);
  }

  return jobs;
}

StatusOr<std::vector<ListFormatJob>> FilterJobs(JobClient& job_client,
                                                std::string const& project_id,
                                                JobFilter const& job_filter,
                                                Options const& options) {
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
      return job.status();
    }
    jobs.push_back(*job);
  }

  return jobs;
}

StatusOr<Job> InsertJob(JobClient& job_client, std::string const& project_id,
                        Job const& job, Options const& options) {
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(CreateKeysToFilterOut(job));

  return job_client.InsertJob(request, options);
}

StatusOr<Job> CancelJob(JobClient& job_client, std::string const& project_id,
                        std::string const& job_id, std::string const& location,
                        Options const& options) {
  CancelJobRequest request;
  request.set_project_id(project_id);
  request.set_job_id(job_id);
  request.set_location(location);

  return job_client.CancelJob(request, options);
}

StatusOr<PostQueryResults> Query(JobClient& job_client,
                                 std::string const& project_id,
                                 QueryRequest const& query_request,
                                 Options const& options) {
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(CreateKeysToFilterOut(query_request));

  return job_client.Query(post_query_request, options);
}

StatusOr<GetQueryResults> GetAllQueryResults(JobClient& job_client,
                                             std::string const& project_id,
                                             std::string const& job_id,
                                             std::string const& location,
                                             Options const& options) {
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_location(location);

  return job_client.QueryResults(get_query_results_request, options);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
