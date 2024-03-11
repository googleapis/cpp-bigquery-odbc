// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_testing_client_library_utils {

using bigquery_v2_minimal_internal::InsertJobRequest;
using bigquery_v2_minimal_internal::Job;
using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::JobConfiguration;
using bigquery_v2_minimal_internal::JobConfigurationQuery;
using google::cloud::internal::GetEnv;

StatusOr<std::string> InsertJob(JobClient job_client) {
  absl::optional<std::string> project_id =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  absl::optional<std::string> dataset_id =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  absl::optional<std::string> table_id =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  if (project_id->empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT environment "
                  "variable is not set");
  }
  if (dataset_id->empty()) {
    return Status(StatusCode::kInvalidArgument,
                  "CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET environment "
                  "variable is not set");
  }
  if (table_id->empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "CPP_BIGQUERY_ODBC_TEST_TABLE_NAME environment variable is not set");
  }

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string table_name = absl::StrCat(*dataset_id, ".", *table_id);
  job_configuration_query.query = absl::StrCat("SELECT * FROM ", table_name);
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys(
      {"statistics", "status", "labels", "destinationTable",
       "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
       "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
       "clustering", "destinationEncryptionConfiguration", "scriptOptions",
       "connectionProperties", "systemVariables", "structTypes", "structValues",
       "location"});

  StatusOr<Job> job_response = job_client.InsertJob(request);

  if (!job_response.ok()) {
    return job_response.status();
  }

  return job_response.value().job_reference.job_id;
}

}  // namespace google::cloud::odbc_testing_client_library_utils
