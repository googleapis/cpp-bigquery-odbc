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
#include "google/cloud/odbc/testing/bq_client_interface_utils/bq_client_mock.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::GetDatasetRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetJobRequest;
using ::google::cloud::bigquery_v2_minimal_internal::GetTableRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::ListProjectsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using google::cloud::internal::GetEnv;
using ::google::cloud::mocks::MakeStreamRange;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::odbc_testing_bq_client_interface_utils::BQClientMocks;
using ::google::cloud::odbc_testing_bq_client_interface_utils::
    CreateBQClientMocks;

TEST(ODBCBQClient, CreateBQClient) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path =
      test_data_path + "service_account_auth_keys.json";

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, credentials_file_path});

  ASSERT_STATUS_RECORD_OK(odbc_bq_client);
}

TEST(ODBCBQClientMock, GetDataset) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  Dataset expected{"d-kind", "d-etag", dataset_id};
  BQClientMocks mocks = CreateBQClientMocks();
  EXPECT_CALL(*mocks.mock_dataset_connection, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return make_status_or(expected);
      });

  StatusRecordOr<Dataset> actual =
      mocks.mock_bq_client.GetDataset(project_id, expected.id, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(expected.id, actual->id);
}

TEST(ODBCBQClientMock, GetJob) {
  Options options;
  std::string project_id = "project_id";
  std::string job_id = "job_id";
  std::string location = "location";
  Job expected{"j-kind", "j-etag", "job_id"};
  BQClientMocks mocks = CreateBQClientMocks();
  EXPECT_CALL(*mocks.mock_job_connection, GetJob)
      .WillOnce([&](GetJobRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(job_id, request.job_id());
        EXPECT_EQ(location, request.location());
        return make_status_or(expected);
      });

  StatusRecordOr<Job> actual =
      mocks.mock_bq_client.GetJob(project_id, job_id, location, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(expected.id, actual->id);
}

TEST(ODBCBQClientMock, ListOneProject) {
  Options options;
  Project expected{"p-kind", "p-id"};
  BQClientMocks mocks = CreateBQClientMocks();
  EXPECT_CALL(*mocks.mock_project_connection, ListProjects)
      .WillOnce([expected](ListProjectsRequest const&) {
        return MakeStreamRange<Project>({expected});
      });

  StatusRecordOr<std::vector<Project>> projects =
      mocks.mock_bq_client.ListAllProjects(options);

  EXPECT_EQ(1, projects->size());
  EXPECT_EQ(expected.id, projects->at(0).id);
}

TEST(ODBCBQClientMock, GetTable) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  std::string table_id = "table_id";
  TableFilter table_filter{{}};
  Table table{"t-kind", "t-etag", "table_id"};
  BQClientMocks mocks = CreateBQClientMocks();
  EXPECT_CALL(*mocks.mock_table_connection, GetTable)
      .WillOnce([&](GetTableRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        EXPECT_EQ(table_id, request.table_id());
        return make_status_or(table);
      });

  StatusRecordOr<Table> actual = mocks.mock_bq_client.GetTable(
      project_id, dataset_id, table_id, table_filter, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(actual->id, table.id);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
