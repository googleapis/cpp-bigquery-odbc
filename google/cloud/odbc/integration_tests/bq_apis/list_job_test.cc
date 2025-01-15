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
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::ListFormatJob;
using bigquery_v2_minimal_internal::ListJobsRequest;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using bigquery_v2_minimal_internal::Projection;
using bigquery_v2_minimal_internal::StateFilter;
using google::cloud::odbc_bigquery_client_interface::CreateCredentials;
using ::google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using ::google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_client_library_utils::
    CreateApplicationDefaultAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateNoAccessAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateUserAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    kNameForNonExistingProject;
using google::cloud::odbc_testing_utils::GetRequiredEnvVar;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef USER_ACCOUNT_AUTH

TEST(ListJobs, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListJobsRequest request;
  request.set_project_id(project_id);
  // Listing jobs only for the last 1 day to make the test faster
  auto hours_before = std::chrono::system_clock::now() - std::chrono::hours(1);
  request.set_min_creation_time(hours_before);
  request.set_max_creation_time(std::chrono::system_clock::now());

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}

// Caution: This test lists all jobs for the user account for the project
// and maybe very slow if the user account has a lot of jobs. Hence the test
// is disabled by default. The test has been verified to be working.
// Please only run incase its necessary for troubleshooting.
TEST(ODBCBQClient_ListJobs, DISABLED_UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY");
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kServiceAndUserAccount;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<ListFormatJob>> list_jobs_response =
      (*odbc_bq_client)->ListAllJobs(project_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(list_jobs_response);

  std::vector<ListFormatJob> jobs = (*list_jobs_response);
  ASSERT_TRUE(jobs.empty());
}

#else

TEST(ListJobs, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListJobsRequest request;
  request.set_project_id(project_id);
  // Listing jobs only for a single hour so it can be completed quicker
  auto hour_before = std::chrono::system_clock::now() - std::chrono::hours(1);
  request.set_min_creation_time(hour_before);
  request.set_max_creation_time(std::chrono::system_clock::now());

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  // We can't guarantee the # of jobs that  would be returned
  // so we can't assert for non-emptiness. It can be empty as well.
  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}

TEST(ListJobs, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListJobsRequest request;
  request.set_project_id(project_id);
  // Listing jobs only for a single hour so it can be completed quicker
  auto hour_before = std::chrono::system_clock::now() - std::chrono::hours(1);
  request.set_min_creation_time(hour_before);
  request.set_max_creation_time(std::chrono::system_clock::now());

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  // We can't guarantee the # of jobs that  would be returned
  // so we can't assert for non-emptiness. It can be empty as well.
  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}

// Caution: This test lists all jobs for the service account for the project
// and maybe very slow hence is disabled by default. Please only run incase
// its necessary.
TEST(ODBCBQClient_ListJobs, DISABLED_ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kApplicationDefault});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<ListFormatJob>> list_jobs_response =
      (*odbc_bq_client)->ListAllJobs(project_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(list_jobs_response);

  std::vector<ListFormatJob> jobs = (*list_jobs_response);
  ASSERT_FALSE(jobs.empty());
}

TEST(ListJobs, MoreRequestArguments) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListJobsRequest request;
  request.set_project_id(project_id);
  // Listing jobs only for the last hour to make the test faster
  auto hour_before = std::chrono::system_clock::now() - std::chrono::hours(1);
  request.set_min_creation_time(hour_before);
  request.set_max_creation_time(std::chrono::system_clock::now());
  request.set_projection(Projection::Full());
  request.set_all_users(true);
  request.set_state_filter(StateFilter::Done());

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  // We can't guarantee the # of jobs that  would be returned
  // so we can't assert for non-emptiness. It can be empty as well.
  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}

TEST(ListJobs, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));

  ListJobsRequest request;
  request.set_project_id(std::string(kNameForNonExistingProject));

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    EXPECT_THAT(
        job, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Project")));
  }
}

TEST(ListJobs, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));

  ListJobsRequest request;
  request.set_project_id("");

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    // BQ API error
    EXPECT_THAT(job, StatusIs(StatusCode::kNotFound,
                              HasSubstr("Request couldn't be served")));
  }
}

TEST(ListJobs, FilterStateIsWrong) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListJobsRequest request;
  request.set_project_id(project_id);
  StateFilter state_filter;
  state_filter.value = "not-valid-state";
  request.set_state_filter(state_filter);

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    // BQ API error
    EXPECT_THAT(job, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("Invalid value at 'state_filter'")));
  }
}

TEST(ListJobs, FilterProjectionIsWrong) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListJobsRequest request;
  request.set_project_id(project_id);
  Projection projection;
  projection.value = "not-valid-projection";
  request.set_projection(projection);

  StreamRange<ListFormatJob> range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    // BQ API error
    EXPECT_THAT(job, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("Invalid value at 'projection'")));
  }
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
