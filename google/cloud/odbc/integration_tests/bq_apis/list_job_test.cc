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

#include "google/cloud/odbc/integration_tests/testing_util/authentication.h"
#include "google/cloud/odbc/integration_tests/testing_util/util_constants.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/internal/getenv.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::ListJobsRequest;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using bigquery_v2_minimal_internal::Projection;
using bigquery_v2_minimal_internal::StateFilter;
using google::cloud::internal::GetEnv;
using google::cloud::odbc_integration_tests_testing_util::
    CreateNoAccessAccountAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    CreateUserAccountAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    kNameForNonExistingProject;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(ListJobs, UserAccountAuth) {
  auto options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  ListJobsRequest request;
  request.set_project_id(*project_id);
  // Listing jobs only for the last week to make the test faster
  auto week_before =
      std::chrono::system_clock::now() - std::chrono::hours(7 * 24);
  request.set_min_creation_time(week_before);
  request.set_max_creation_time(std::chrono::system_clock::now());

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}
#endif  // USER_ACCOUNT_AUTH

TEST(ListJobs, ServiceAccountAuth) {
  auto options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  ListJobsRequest request;
  request.set_project_id(*project_id);
  // Listing jobs only for the last week to make the test faster
  auto week_before =
      std::chrono::system_clock::now() - std::chrono::hours(7 * 24);
  request.set_min_creation_time(week_before);
  request.set_max_creation_time(std::chrono::system_clock::now());

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}

TEST(ListJobs, ServiceAccountAuthWithClientId) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  ListJobsRequest request;
  request.set_project_id(*project_id);
  // Listing jobs only for the last week to make the test faster
  auto week_before =
      std::chrono::system_clock::now() - std::chrono::hours(7 * 24);
  request.set_min_creation_time(week_before);
  request.set_max_creation_time(std::chrono::system_clock::now());

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}

TEST(ListJobs, MoreRequestArguments) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  ListJobsRequest request;
  request.set_project_id(*project_id);
  // Listing jobs only for the last week to make the test faster
  auto week_before =
      std::chrono::system_clock::now() - std::chrono::hours(7 * 24);
  request.set_min_creation_time(week_before);
  request.set_max_creation_time(std::chrono::system_clock::now());
  request.set_projection(Projection::Full());
  request.set_all_users(true);
  request.set_state_filter(StateFilter::Done());

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    ASSERT_STATUS_OK(job);
  }
}

TEST(ListJobs, ProjectNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));

  ListJobsRequest request;
  request.set_project_id(std::string(kNameForNonExistingProject));

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    EXPECT_THAT(
        job, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Project")));
  }
}

TEST(ListJobs, ProjectIdIsEmpty) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));

  ListJobsRequest request;
  request.set_project_id("");

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    EXPECT_THAT(job, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("Project Id is empty")));
  }
}

TEST(ListJobs, FilterStateIsWrong) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  ListJobsRequest request;
  request.set_project_id(*project_id);
  StateFilter state_filter;
  state_filter.value = "not-valid-state";
  request.set_state_filter(state_filter);

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    EXPECT_THAT(job, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("Invalid value at 'state_filter'")));
  }
}

TEST(ListJobs, FilterProjectionIsWrong) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  ListJobsRequest request;
  request.set_project_id(*project_id);
  Projection projection;
  projection.value = "not-valid-projection";
  request.set_projection(projection);

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    EXPECT_THAT(job, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("Invalid value at 'projection'")));
  }
}

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(ListJobs, NoAccessAccountAuth) {
  auto options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  ListJobsRequest request;
  request.set_project_id(*project_id);

  auto range = job_client.ListJobs(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& job : range) {
    EXPECT_THAT(job, StatusIs(StatusCode::kPermissionDenied,
                              HasSubstr("User does not have bigquery.jobs.list "
                                        "permission in project")));
  }
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
