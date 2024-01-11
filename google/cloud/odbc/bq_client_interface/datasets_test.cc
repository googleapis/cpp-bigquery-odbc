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

#include "google/cloud/odbc/bq_client_interface/datasets.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_dataset_connection.h"
// #include "google/cloud/mocks/mock_stream_range.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::GetDatasetRequest;
using ::google::cloud::bigquery_v2_minimal_internal::MockDatasetConnection;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::StrEq;

TEST(GetDataset, GetDatasetSuccess) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  EXPECT_CALL(*mock, options);
  Dataset expected{.kind = "d-kind", .id = dataset_id};
  EXPECT_CALL(*mock, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return make_status_or(expected);
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusOr<Dataset> dataset =
      GetDataset(mocked_dataset_client, project_id, expected.id, options);

  ASSERT_STATUS_OK(dataset);
  EXPECT_EQ(expected.id, (*dataset).id);
}

TEST(GetDataset, EmptyStrings) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id;
  std::string dataset_id;
  EXPECT_CALL(*mock, options);
  Dataset expected{.kind = "d-kind", .id = dataset_id};
  EXPECT_CALL(*mock, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return make_status_or(expected);
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusOr<Dataset> dataset =
      GetDataset(mocked_dataset_client, project_id, expected.id, options);

  ASSERT_STATUS_OK(dataset);
  EXPECT_EQ(expected.id, (*dataset).id);
}

TEST(GetDataset, GetDatasetFailure) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  EXPECT_CALL(*mock, options);
  Dataset expected{.kind = "d-kind", .id = dataset_id};
  EXPECT_CALL(*mock, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return Status(StatusCode::kUnauthenticated, "denied");
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusOr<Dataset> dataset =
      GetDataset(mocked_dataset_client, project_id, expected.id, options);

  EXPECT_THAT(dataset, StatusIs(StatusCode::kUnauthenticated, StrEq("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
