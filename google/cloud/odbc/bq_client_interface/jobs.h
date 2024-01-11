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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_JOBS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_JOBS_H

#include "google/cloud/odbc/bq_client_interface/job_utils.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

StatusOr<::google::cloud::bigquery_v2_minimal_internal::Job> GetJob(
    ::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
    std::string const& project_id, std::string const& job_id,
    std::string const& location, ::google::cloud::Options const& options);

StatusOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
ListAllJobs(
    ::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
    std::string const& project_id, ::google::cloud::Options const& options);

StatusOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
FilterJobs(::google::cloud::bigquery_v2_minimal_internal::JobClient& job_client,
           std::string const& project_id, JobFilter const& job_filter,
           ::google::cloud::Options const& options);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_JOBS_H
