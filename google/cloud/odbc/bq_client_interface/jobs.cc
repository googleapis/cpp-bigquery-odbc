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

#include "google/cloud/odbc/bq_client_interface/job_utils.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::GetJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatJob;
using ::google::cloud::bigquery_v2_minimal_internal::ListJobsRequest;

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
  request.set_state_filter(convert(job_filter.state_filter));
  request.set_parent_job_id(job_filter.parent_job_id);
  request.set_projection(convert(job_filter.projection));

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

}  // namespace google::cloud::odbc_bigquery_client_interface
