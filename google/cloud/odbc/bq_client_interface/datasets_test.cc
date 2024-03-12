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
#include "google/cloud/mocks/mock_stream_range.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::GetDatasetRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListDatasetsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;
using ::google::cloud::bigquery_v2_minimal_internal::MockDatasetConnection;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(GetDataset, GetDatasetSuccess) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  EXPECT_CALL(*mock, options);
  Dataset expected{"d-kind", "d-etag", dataset_id};
  EXPECT_CALL(*mock, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return make_status_or(expected);
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<Dataset> dataset =
      GetDataset(mocked_dataset_client, project_id, expected.id, options);

  ASSERT_STATUS_RECORD_OK(dataset);
  EXPECT_EQ(expected.id, dataset->id);
}

TEST(GetDataset, GetDataset_EmptyInputParams) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id;
  std::string dataset_id;
  EXPECT_CALL(*mock, options);
  Dataset expected{"d-kind", "d-etag", dataset_id};
  EXPECT_CALL(*mock, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return make_status_or(expected);
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<Dataset> dataset =
      GetDataset(mocked_dataset_client, project_id, expected.id, options);

  ASSERT_STATUS_RECORD_OK(dataset);
  EXPECT_EQ(expected.id, dataset->id);
}

TEST(GetDataset, GetDatasetFailure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  EXPECT_CALL(*mock, options);
  Dataset expected{"d-kind", "d-etag", dataset_id};
  EXPECT_CALL(*mock, GetDataset)
      .WillOnce([&](GetDatasetRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return Status(StatusCode::kUnauthenticated, "denied");
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<Dataset> dataset =
      GetDataset(mocked_dataset_client, project_id, expected.id, options);

  EXPECT_THAT(dataset,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(ListAllDatasets, ListZeroDatasetsSuccess) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        return mocks::MakeStreamRange<ListFormatDataset>({});
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets =
      ListAllDatasets(mocked_dataset_client, project_id, options);

  ASSERT_STATUS_RECORD_OK(datasets);
  EXPECT_EQ(0, datasets->size());
}

TEST(ListAllDatasets, ListAllDatasetsSuccess) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  ListFormatDataset expected{"d-kind", "dataset_id"};
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        return mocks::MakeStreamRange<ListFormatDataset>({expected});
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets =
      ListAllDatasets(mocked_dataset_client, project_id, options);

  ASSERT_STATUS_RECORD_OK(datasets);
  EXPECT_EQ(1, datasets->size());
  EXPECT_EQ(expected.id, datasets->at(0).id);
}

TEST(ListAllDatasets, ListAllDatasets_EmptyInputParams) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id;
  ListFormatDataset expected{"d-kind", "dataset_id"};
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        return mocks::MakeStreamRange<ListFormatDataset>({expected});
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets =
      ListAllDatasets(mocked_dataset_client, project_id, options);

  ASSERT_STATUS_RECORD_OK(datasets);
  EXPECT_EQ(1, datasets->size());
  EXPECT_EQ(expected.id, datasets->at(0).id);
}

TEST(ListAllDatasets, ListAllDatasetsFailure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  ListFormatDataset expected{"d-kind", "dataset_id"};
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        return mocks::MakeStreamRange<ListFormatDataset>(
            {}, Status(StatusCode::kUnauthenticated, "denied"));
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets =
      ListAllDatasets(mocked_dataset_client, project_id, options);

  EXPECT_THAT(datasets,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

TEST(FilterDatasets, FilterZeroDatasetsSuccess) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  DatasetFilter dataset_filter{"filtering", true};
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_filter.filter, request.filter());
        EXPECT_EQ(dataset_filter.all, request.all_datasets());
        return mocks::MakeStreamRange<ListFormatDataset>({});
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets = FilterDatasets(
      mocked_dataset_client, project_id, dataset_filter, options);

  ASSERT_STATUS_RECORD_OK(datasets);
  EXPECT_EQ(0, datasets->size());
}

TEST(FilterDatasets, FilterAllDatasetsSuccess) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  DatasetFilter dataset_filter{.filter = "filtering", .all = true};
  ListFormatDataset expected{"d-kind", "dataset_id"};
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_filter.filter, request.filter());
        EXPECT_EQ(dataset_filter.all, request.all_datasets());
        return mocks::MakeStreamRange<ListFormatDataset>({expected});
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets = FilterDatasets(
      mocked_dataset_client, project_id, dataset_filter, options);

  ASSERT_STATUS_RECORD_OK(datasets);
  EXPECT_EQ(1, datasets->size());
  EXPECT_EQ(expected.id, datasets->at(0).id);
}

TEST(FilterDatasets, FilterDatasets_EmptyInputParams) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id;
  DatasetFilter dataset_filter;
  ListFormatDataset expected{"d-kind", "dataset_id"};
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_filter.filter, request.filter());
        EXPECT_EQ(dataset_filter.all, request.all_datasets());
        return mocks::MakeStreamRange<ListFormatDataset>({expected});
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets = FilterDatasets(
      mocked_dataset_client, project_id, dataset_filter, options);

  ASSERT_STATUS_RECORD_OK(datasets);
  EXPECT_EQ(1, datasets->size());
  EXPECT_EQ(expected.id, datasets->at(0).id);
}

TEST(FilterDatasets, FilterDatasetsFailure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockDatasetConnection>();
  Options options;
  std::string project_id = "project_id";
  DatasetFilter dataset_filter;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListDatasets)
      .WillOnce([&](ListDatasetsRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_filter.filter, request.filter());
        EXPECT_EQ(dataset_filter.all, request.all_datasets());
        return mocks::MakeStreamRange<ListFormatDataset>(
            {}, Status(StatusCode::kUnauthenticated, "denied"));
      });
  DatasetClient mocked_dataset_client(std::move(mock));

  StatusRecordOr<std::vector<ListFormatDataset>> datasets = FilterDatasets(
      mocked_dataset_client, project_id, dataset_filter, options);

  EXPECT_THAT(datasets,
              StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
