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

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/client_library_utils/common_functions.h"
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::CancelJobRequest;
using bigquery_v2_minimal_internal::InsertJobRequest;
using bigquery_v2_minimal_internal::Job;
using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::JobConfiguration;
using bigquery_v2_minimal_internal::JobConfigurationQuery;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_client_library_utils::
    CreateApplicationDefaultAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateUserAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::InsertJob;
using google::cloud::odbc_testing_client_library_utils::
    kNameForNonExistingProject;
using google::cloud::odbc_testing_utils::GetRequiredEnvVar;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef USER_ACCOUNT_AUTH
TEST(CancelJob, UserAccountAuth) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);

  ASSERT_STATUS_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response.value().status.state, "DONE");
}

TEST(ODBCBQClient_CancelJob, UserAccountAuth) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);

  auto job_client = JobClient(MakeBigQueryJobConnection(*options));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY");

  // Cancelling previous Job via ODBC BQ Client
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kServiceAndUserAccount;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Job> cancel_job_response =
      (*odbc_bq_client)
          ->CancelJob(project_id, *job_id, "", std::move(*options));
  ASSERT_STATUS_RECORD_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response->status.state, "DONE");
}

#else

TEST(CancelJob, ApplicationDefaultCredentials) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);

  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);
  ASSERT_STATUS_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response.value().status.state, "DONE");
}

TEST(ODBCBQClient_CancelJob, ApplicationDefaultCredentials) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);

  auto job_client = JobClient(MakeBigQueryJobConnection(*options));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  // Cancelling previous Job via ODBC BQ Client
  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kApplicationDefault});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Job> cancel_job_response =
      (*odbc_bq_client)
          ->CancelJob(project_id, *job_id, "", std::move(*options));
  ASSERT_STATUS_RECORD_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response->status.state, "DONE");
}

TEST(CancelJob, ServiceAccountAuth) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);

  ASSERT_STATUS_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response.value().status.state, "DONE");
}

TEST(CancelJob, DifferentAccount) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  // Cancelling previous Job with another account
  StatusOr<Options> options_with_user_account =
      CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options_with_user_account);
  auto job_client_with_user_account = JobClient(
      MakeBigQueryJobConnection(std::move(*options_with_user_account)));
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());

  StatusOr<Job> cancel_job_response =
      job_client_with_user_account.CancelJob(cancel_job_request);

  ASSERT_STATUS_OK(cancel_job_response);
  EXPECT_EQ(cancel_job_response.value().status.state, "DONE");
}

TEST(CancelJob, WrongLocation) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());
  cancel_job_request.set_location("asia-south2");

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(CancelJob, LocationNotExist) {
  // First we create a job, so later we could 'cancel' it
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  StatusOr<std::string> job_id = InsertJob(job_client);
  ASSERT_FALSE(job_id->empty()) << job_id.status().message();
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  // Cancelling previous Job
  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id(job_id.value());
  cancel_job_request.set_location("Not_existing_location");

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid value for location")));
}

TEST(CancelJob, JobNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(project_id);
  cancel_job_request.set_job_id("Not_existing_job");

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(CancelJob, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));

  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id(std::string(kNameForNonExistingProject));
  cancel_job_request.set_job_id("Not_existing_job");

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);

  EXPECT_THAT(cancel_job_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Project")));
}

TEST(CancelJob, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));

  CancelJobRequest cancel_job_request;
  cancel_job_request.set_project_id("");
  cancel_job_request.set_job_id("job_id");

  StatusOr<Job> cancel_job_response = job_client.CancelJob(cancel_job_request);

  // BQ API error
  EXPECT_THAT(
      cancel_job_response,
      StatusIs(StatusCode::kNotFound, HasSubstr("Request couldn't be served")));
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
