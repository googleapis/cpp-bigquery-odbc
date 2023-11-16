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

#include <gmock/gmock.h>

#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/authentication.h"
#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"
#include "google/cloud/odbc/integration_tests/testing_util/common_functions.h"
#include "google/cloud/odbc/integration_tests/testing_util/util_constants.h"

namespace google {
namespace cloud {
namespace odbc_bigquery_v2_tests {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_testing_util_internal::StatusIs;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_util_internal::CreateUserAccountAuthentication;
using google::cloud::odbc_testing_util_internal::InsertJob;
using google::cloud::odbc_testing_util_internal::kNameForNonExistingProject;
using ::testing::HasSubstr;
using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using bigquery_v2_minimal_internal::InsertJobRequest;
using bigquery_v2_minimal_internal::Job;
using bigquery_v2_minimal_internal::JobConfiguration;
using bigquery_v2_minimal_internal::JobConfigurationQuery;
using bigquery_v2_minimal_internal::CancelJobRequest;

TEST(CancelJob, ServiceAccountAuthWithClientId) {
  // First we create a job, so later we could 'cancel' it
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(options.value())));
  auto job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT").value_or("");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());

  auto cancel_job_response = job_client.CancelJob(cancel_job_request);

  ASSERT_STATUS_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response.value().status.state, "DONE");
}

TEST(CancelJob, DifferentAccount) {
  // First we create a job, so later we could 'cancel' it
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(options.value())));
  auto job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT").value_or("");

  // Cancelling previous Job with another account
  auto options_with_user_account = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options_with_user_account);
  auto job_client_with_user_account = JobClient(MakeBigQueryJobConnection(std::move(options_with_user_account.value())));
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());

  auto cancel_job_response = job_client_with_user_account.CancelJob(cancel_job_request);

  ASSERT_STATUS_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response.value().status.state, "DONE");
}

TEST(CancelJob, WrongLocation) {
  // First we create a job, so later we could 'cancel' it
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(options.value())));
  auto job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT").value_or("");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());
  cancel_job_request.set_location("asia-south2");

  auto cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(CancelJob, LocationNotExist) {
  // First we create a job, so later we could 'cancel' it
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(options.value())));
  auto job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT").value_or("");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());
  cancel_job_request.set_location("Not_existing_location");

  auto cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response, StatusIs(StatusCode::kInvalidArgument, HasSubstr("Invalid value for location")));
}

TEST(CancelJob, JobNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(options.value())));
  auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id_optional.has_value());

  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id_optional.value());
  cancel_job_request.set_job_id("Not_existing_job");

  auto cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(CancelJob, ProjectNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(options.value())));
  auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id_optional.has_value());

  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(std::string(kNameForNonExistingProject));
  cancel_job_request.set_job_id("Not_existing_job");

  auto cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Project")));
}
}
}
}
