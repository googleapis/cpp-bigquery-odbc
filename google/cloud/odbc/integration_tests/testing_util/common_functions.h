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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTEGRATION_TESTS_TESTING_UTIL_COMMON_FUNCTIONS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTEGRATION_TESTS_TESTING_UTIL_COMMON_FUNCTIONS_H

#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/status_or.h"

namespace google::cloud::odbc_integration_tests_testing_util {

// Inserts some basic job to BQ
// Returns the job_id
StatusOr<std::string> InsertJob(bigquery_v2_minimal_internal::JobClient job_client);

} // namespace google::cloud::odbc_integration_tests_testing_util

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTEGRATION_TESTS_TESTING_UTIL_COMMON_FUNCTIONS_H
