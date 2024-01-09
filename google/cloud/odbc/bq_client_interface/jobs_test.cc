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
#include "google/cloud/bigquery/v2/minimal/mocks/mock_job_connection.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::GetJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::MockBigQueryJobConnection;
using ::google::cloud::bigquery_v2_minimal_internal::Projection;
using ::google::cloud::bigquery_v2_minimal_internal::StateFilter;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::StrEq;

TEST(GetJob, GetJobSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  Job job{.id = "job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetJob).WillOnce([&](GetJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusOr<Job> actual =
      GetJob(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->id, job.id);
}

TEST(GetJob, EmptyStrings) {
  Options options;
  std::string project_id;
  std::string job_id;
  std::string location;
  Job job{.id = "job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetJob).WillOnce([&](GetJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusOr<Job> actual =
      GetJob(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->id, job.id);
}

TEST(GetJob, GetJobFailure) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetJob).WillOnce([&](GetJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return Status(StatusCode::kUnauthenticated, "denied");
  });
  JobClient job_client(std::move(mock));

  StatusOr<Job> actual =
      GetJob(job_client, project_id, job_id, location, options);

  EXPECT_THAT(actual, StatusIs(StatusCode::kUnauthenticated, StrEq("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
