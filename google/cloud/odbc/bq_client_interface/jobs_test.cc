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
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_job_connection.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::CancelJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::InsertJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::JobConfiguration;
using ::google::cloud::bigquery_v2_minimal_internal::JobConfigurationQuery;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatJob;
using ::google::cloud::bigquery_v2_minimal_internal::ListJobsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::MockBigQueryJobConnection;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Projection;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ScriptOptions;
using ::google::cloud::bigquery_v2_minimal_internal::StateFilter;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::Contains;
using ::testing::HasSubstr;

TEST(GetJob, GetJobSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  Job job{"j-kind", "j-etag", "job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetJob).WillOnce([&](GetJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual =
      GetJob(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(actual->id, job.id);
}

TEST(GetJob, GetJob_EmptyInputParams) {
  Options options;
  std::string project_id;
  std::string job_id;
  std::string location;
  Job job{"j-kind", "j-etag", "job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetJob).WillOnce([&](GetJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual =
      GetJob(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(actual->id, job.id);
}

TEST(GetJob, GetJobFailure_UnauthenticatedRequest) {
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

  StatusRecordOr<Job> actual =
      GetJob(job_client, project_id, job_id, location, options);

  EXPECT_THAT(actual,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(ListAllJobs, ListZeroJobsSuccess) {
  Options options;
  std::string project_id = "project_id";
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    return mocks::MakeStreamRange<ListFormatJob>({});
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      ListAllJobs(job_client, project_id, options);

  ASSERT_STATUS_RECORD_OK(jobs);
  EXPECT_EQ(0, jobs->size());
}

TEST(ListAllJobs, ListAllJobsSuccess) {
  Options options;
  std::string project_id = "project_id";
  ListFormatJob expected{"job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    return mocks::MakeStreamRange<ListFormatJob>({expected});
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      ListAllJobs(job_client, project_id, options);

  ASSERT_STATUS_RECORD_OK(jobs);
  EXPECT_EQ(1, jobs->size());
  EXPECT_EQ(expected.id, jobs->at(0).id);
}

TEST(ListAllJobs, ListAllJobs_EmptyInputParams) {
  Options options;
  std::string project_id;
  ListFormatJob expected{"job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    return mocks::MakeStreamRange<ListFormatJob>({expected});
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      ListAllJobs(job_client, project_id, options);

  ASSERT_STATUS_RECORD_OK(jobs);
  EXPECT_EQ(1, jobs->size());
  EXPECT_EQ(expected.id, jobs->at(0).id);
}

TEST(ListAllJobs, ListAllJobsFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    return mocks::MakeStreamRange<ListFormatJob>(
        {}, Status(StatusCode::kUnauthenticated, "denied"));
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      ListAllJobs(job_client, project_id, options);

  EXPECT_THAT(jobs, StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(FilterJobs, FilterZeroJobsSuccess) {
  Options options;
  std::string project_id = "project_id";
  JobFilter job_filter;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_filter.allUsers, request.all_users());
    EXPECT_EQ(job_filter.min_creation_time, request.min_creation_time());
    EXPECT_EQ(job_filter.max_creation_time, request.max_creation_time());
    EXPECT_EQ(job_filter.state_filter.value, request.state_filter().value);
    EXPECT_EQ(job_filter.parent_job_id, request.parent_job_id());
    EXPECT_EQ(job_filter.projection.value, request.projection().value);
    return mocks::MakeStreamRange<ListFormatJob>({});
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      FilterJobs(job_client, project_id, job_filter, options);

  ASSERT_STATUS_RECORD_OK(jobs);
  EXPECT_EQ(0, jobs->size());
}

TEST(FilterJobs, FilterJobsSuccess) {
  Options options;
  std::string project_id = "project_id";
  JobFilter job_filter;
  job_filter.allUsers = true;
  job_filter.min_creation_time = std::chrono::system_clock::now();
  job_filter.max_creation_time = std::chrono::system_clock::now();
  job_filter.state_filter = StateFilter::Done();
  job_filter.parent_job_id = "parent_job_id";
  job_filter.projection = Projection::Full();
  ListFormatJob expected{"job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_filter.allUsers, request.all_users());
    EXPECT_EQ(job_filter.min_creation_time, request.min_creation_time());
    EXPECT_EQ(job_filter.max_creation_time, request.max_creation_time());
    EXPECT_EQ(job_filter.state_filter.value, request.state_filter().value);
    EXPECT_EQ(job_filter.parent_job_id, request.parent_job_id());
    EXPECT_EQ(job_filter.projection.value, request.projection().value);
    return mocks::MakeStreamRange<ListFormatJob>({expected});
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      FilterJobs(job_client, project_id, job_filter, options);

  ASSERT_STATUS_RECORD_OK(jobs);
  EXPECT_EQ(1, jobs->size());
  EXPECT_EQ(expected.id, jobs->at(0).id);
}

TEST(FilterJobs, FilterJobs_EmptyInputParams) {
  Options options;
  std::string project_id;
  JobFilter job_filter;
  ListFormatJob expected{"job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_filter.allUsers, request.all_users());
    EXPECT_EQ(job_filter.min_creation_time, request.min_creation_time());
    EXPECT_EQ(job_filter.max_creation_time, request.max_creation_time());
    EXPECT_EQ(job_filter.state_filter.value, request.state_filter().value);
    EXPECT_EQ(job_filter.parent_job_id, request.parent_job_id());
    EXPECT_EQ(job_filter.projection.value, request.projection().value);
    return mocks::MakeStreamRange<ListFormatJob>({expected});
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      FilterJobs(job_client, project_id, job_filter, options);

  ASSERT_STATUS_RECORD_OK(jobs);
  EXPECT_EQ(1, jobs->size());
  EXPECT_EQ(expected.id, jobs->at(0).id);
}

TEST(FilterJobs, FilterJobsFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  JobFilter job_filter;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListJobs).WillOnce([&](ListJobsRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_filter.allUsers, request.all_users());
    EXPECT_EQ(job_filter.min_creation_time, request.min_creation_time());
    EXPECT_EQ(job_filter.max_creation_time, request.max_creation_time());
    EXPECT_EQ(job_filter.state_filter.value, request.state_filter().value);
    EXPECT_EQ(job_filter.parent_job_id, request.parent_job_id());
    EXPECT_EQ(job_filter.projection.value, request.projection().value);
    return mocks::MakeStreamRange<ListFormatJob>(
        {}, Status(StatusCode::kUnauthenticated, "denied"));
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatJob>> jobs =
      FilterJobs(job_client, project_id, job_filter, options);

  EXPECT_THAT(jobs, StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(InsertJob, InsertJobSuccess) {
  Options options;
  std::string project_id = "project_id";
  Job job{"j-kind", "j-etag", "job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertJob).WillOnce([&](InsertJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_FALSE(request.json_filter_keys().empty());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual = InsertJob(job_client, project_id, job, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(actual->id, job.id);
}

TEST(InsertJob, InsertJobSuccess_EmptyInputParams) {
  Options options;
  std::string project_id;
  Job job{"j-kind", "j-etag", "job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertJob).WillOnce([&](InsertJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_FALSE(request.json_filter_keys().empty());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual = InsertJob(job_client, project_id, job, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(actual->id, job.id);
}

TEST(InsertJob, InsertJobFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  Job job{"j-kind", "j-etag", "job_id"};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertJob).WillOnce([&](InsertJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_FALSE(request.json_filter_keys().empty());
    return Status(StatusCode::kUnauthenticated, "denied");
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual = InsertJob(job_client, project_id, job, options);

  EXPECT_THAT(actual,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(InsertJob, InsertJobSuccess_JobObjectIsEmpty) {
  Options options;
  std::string project_id = "project_id";
  Job job;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertJob).WillOnce([&](InsertJobRequest const& request) {
    EXPECT_THAT(request.json_filter_keys(), Contains("defaultDataset"));
    EXPECT_THAT(request.json_filter_keys(), Contains("destinationTable"));
    EXPECT_THAT(request.json_filter_keys(), Contains("maximumBytesBilled"));
    EXPECT_THAT(request.json_filter_keys(), Contains("keyResultStatement"));
    EXPECT_THAT(request.json_filter_keys(), Contains("jobReference"));
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual = InsertJob(job_client, project_id, job, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(InsertJob, InsertJobSuccess_JobObjectIsFull) {
  Options options;
  std::string project_id = "project_id";
  JobConfigurationQuery job_configuration_query;
  job_configuration_query.maximum_bytes_billed = 111;
  job_configuration_query.default_dataset = {"dataset_id", "project_id"};
  job_configuration_query.destination_table = {"project_id", "dataset_id",
                                               "table_id"};
  ScriptOptions script_options;
  script_options.key_result_statement = ::google::cloud::
      bigquery_v2_minimal_internal::KeyResultStatementKind::UnSpecified();
  job_configuration_query.script_options = script_options;
  JobConfiguration job_configuration;
  job_configuration.query = job_configuration_query;
  Job job{"j-kind", "j-etag", "job_id"};
  job.job_reference = {"project_id", "job_id"};
  job.configuration = job_configuration;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, InsertJob).WillOnce([&](InsertJobRequest const& request) {
    EXPECT_FALSE(request.json_filter_keys().empty());
    EXPECT_THAT(request.json_filter_keys(),
                Contains("defaultDataset").Times(0));
    EXPECT_THAT(request.json_filter_keys(),
                Contains("destinationTable").Times(0));
    EXPECT_THAT(request.json_filter_keys(),
                Contains("maximumBytesBilled").Times(0));
    EXPECT_THAT(request.json_filter_keys(),
                Contains("keyResultStatement").Times(0));
    EXPECT_THAT(request.json_filter_keys(), Contains("jobReference").Times(0));
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual = InsertJob(job_client, project_id, job, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(CancelJob, CancelJobSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  Job job;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CancelJob).WillOnce([&](CancelJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual =
      CancelJob(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(CancelJob, CancelJob_EmptyInputParams) {
  Options options;
  std::string project_id;
  std::string job_id;
  std::string location;
  Job job;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CancelJob).WillOnce([&](CancelJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return make_status_or(job);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual =
      CancelJob(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(CancelJob, CancelJobFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CancelJob).WillOnce([&](CancelJobRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(job_id, request.job_id());
    EXPECT_EQ(location, request.location());
    return Status(StatusCode::kUnauthenticated, "denied");
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<Job> actual =
      CancelJob(job_client, project_id, job_id, location, options);

  EXPECT_THAT(actual,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(Query, QuerySuccess) {
  Options options;
  std::string project_id = "project_id";
  QueryRequest query_request;
  query_request.set_query("SELECT *");
  PostQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Query).WillOnce([&](PostQueryRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_FALSE(request.json_filter_keys().empty());
    return make_status_or(expected);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<PostQueryResults> actual =
      Query(job_client, project_id, query_request, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(Query, QuerySuccess_EmptyInputParams) {
  Options options;
  std::string project_id;
  QueryRequest query_request;
  PostQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Query).WillOnce([&](PostQueryRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_FALSE(request.json_filter_keys().empty());
    return make_status_or(expected);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<PostQueryResults> actual =
      Query(job_client, project_id, query_request, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(Query, QueryFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  QueryRequest query_request;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Query).WillOnce([&](PostQueryRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_FALSE(request.json_filter_keys().empty());
    return Status(StatusCode::kUnauthenticated, "denied");
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<PostQueryResults> actual =
      Query(job_client, project_id, query_request, options);

  EXPECT_THAT(actual,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(Query, QuerySuccess_QueryRequestObjectIsEmpty) {
  Options options;
  std::string project_id = "project_id";
  QueryRequest query_request;
  PostQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Query).WillOnce([&](PostQueryRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_THAT(request.json_filter_keys(), Contains("defaultDataset"));
    EXPECT_THAT(request.json_filter_keys(), Contains("maximumBytesBilled"));
    return make_status_or(expected);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<PostQueryResults> actual =
      Query(job_client, project_id, query_request, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(Query, QuerySuccess_QueryRequestObjectIsFull) {
  Options options;
  std::string project_id = "project_id";
  QueryRequest query_request;
  query_request.set_maximum_bytes_billed(1);
  query_request.set_default_dataset(
      {.dataset_id = "dataset_id", .project_id = "project_id"});
  PostQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, Query).WillOnce([&](PostQueryRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_THAT(request.json_filter_keys(),
                Contains("defaultDataset").Times(0));
    EXPECT_THAT(request.json_filter_keys(),
                Contains("maximumBytesBilled").Times(0));
    return make_status_or(expected);
  });
  JobClient job_client(std::move(mock));

  StatusRecordOr<PostQueryResults> actual =
      Query(job_client, project_id, query_request, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(GetAllQueryResults, GetAllQueryResultsSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  GetQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(job_id, request.job_id());
        EXPECT_EQ(location, request.location());
        return make_status_or(expected);
      });
  JobClient job_client(std::move(mock));

  StatusRecordOr<GetQueryResults> actual =
      GetAllQueryResults(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(GetAllQueryResults, GetAllQueryResultsSuccess_UsePagination) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  GetQueryResults expected_1;
  expected_1.page_token = "token";
  expected_1.rows = {{{{"1", {"value_1"}}}}};
  GetQueryResults expected_2;
  expected_2.rows = {{{{"1", {"value_2"}}}}};
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_TRUE(request.page_token().empty());
        return make_status_or(expected_1);
      })
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_EQ(expected_1.page_token, request.page_token());
        return make_status_or(expected_2);
      });
  JobClient job_client(std::move(mock));

  StatusRecordOr<GetQueryResults> actual =
      GetAllQueryResults(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(2, actual->rows.size());
  EXPECT_TRUE(actual->page_token.empty());
}

TEST(GetAllQueryResults, GetAllQueryResultsSuccess_EmptyInputParams) {
  Options options;
  std::string project_id;
  std::string job_id;
  std::string location;
  GetQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(job_id, request.job_id());
        EXPECT_EQ(location, request.location());
        return make_status_or(expected);
      });
  JobClient job_client(std::move(mock));

  StatusRecordOr<GetQueryResults> actual =
      GetAllQueryResults(job_client, project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(GetAllQueryResults, GetAllQueryResultsFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  GetQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(job_id, request.job_id());
        EXPECT_EQ(location, request.location());
        return Status(StatusCode::kUnauthenticated, "denied");
      });
  JobClient job_client(std::move(mock));

  StatusRecordOr<GetQueryResults> actual =
      GetAllQueryResults(job_client, project_id, job_id, location, options);

  EXPECT_THAT(actual,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(FilterQueryResults, FilterQueryResultsSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  QueryResultsFilterParams query_results_filter_params;
  query_results_filter_params.start_index = 1;
  query_results_filter_params.query_timeout_ms = 100;
  query_results_filter_params.max_results = 15;
  query_results_filter_params.page_token = "token";
  GetQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(job_id, request.job_id());
        EXPECT_EQ(location, request.location());
        EXPECT_EQ(query_results_filter_params.start_index,
                  request.start_index());
        EXPECT_EQ(query_results_filter_params.query_timeout_ms,
                  request.timeout().count());
        EXPECT_EQ(query_results_filter_params.max_results,
                  request.max_results());
        EXPECT_EQ(query_results_filter_params.page_token, request.page_token());
        return make_status_or(expected);
      });
  JobClient job_client(std::move(mock));

  StatusRecordOr<GetQueryResults> actual =
      FilterQueryResults(job_client, project_id, job_id, location,
                         query_results_filter_params, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(FilterQueryResults, FilterQueryResultsSuccess_EmptyInputParams) {
  Options options;
  std::string project_id;
  std::string job_id;
  std::string location;
  QueryResultsFilterParams query_results_filter_params;
  GetQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(job_id, request.job_id());
        EXPECT_EQ(location, request.location());
        EXPECT_EQ(query_results_filter_params.start_index,
                  request.start_index());
        EXPECT_EQ(query_results_filter_params.query_timeout_ms,
                  request.timeout().count());
        EXPECT_EQ(query_results_filter_params.max_results,
                  request.max_results());
        EXPECT_EQ(query_results_filter_params.page_token, request.page_token());
        return make_status_or(expected);
      });
  JobClient job_client(std::move(mock));

  StatusRecordOr<GetQueryResults> actual =
      FilterQueryResults(job_client, project_id, job_id, location,
                         query_results_filter_params, options);

  ASSERT_STATUS_RECORD_OK(actual);
}

TEST(FilterQueryResults, FilterQueryResultsFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  QueryResultsFilterParams query_results_filter_params;
  GetQueryResults expected;
  auto mock = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, QueryResults)
      .WillOnce([&](GetQueryResultsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(job_id, request.job_id());
        EXPECT_EQ(location, request.location());
        EXPECT_EQ(query_results_filter_params.start_index,
                  request.start_index());
        EXPECT_EQ(query_results_filter_params.query_timeout_ms,
                  request.timeout().count());
        return Status(StatusCode::kUnauthenticated, "denied");
      });
  JobClient job_client(std::move(mock));

  StatusRecordOr<GetQueryResults> actual =
      FilterQueryResults(job_client, project_id, job_id, location,
                         query_results_filter_params, options);

  EXPECT_THAT(actual,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
