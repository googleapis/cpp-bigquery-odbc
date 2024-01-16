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

#include "google/cloud/odbc/bq_client_interface/tables.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_table_connection.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_v2_minimal_internal::GetTableRequest;
using ::google::cloud::bigquery_v2_minimal_internal::MockTableConnection;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;
using google::cloud::odbc_bigquery_client_interface::GetTable;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::StrEq;

TEST(GetTable, GetTableSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  std::string table_id = "table_id";
  Table table{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetTable).WillOnce([&](GetTableRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(dataset_id, request.dataset_id());
    EXPECT_EQ(table_id, request.table_id());
    return make_status_or(table);
  });
  TableClient table_client(std::move(mock));

  StatusOr<Table> actual =
      GetTable(table_client, project_id, dataset_id, table_id, options);

  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->id, table.id);
}

TEST(GetTable, UseEmptyStringsForInputParameters) {
  Options options;
  std::string project_id;
  std::string dataset_id;
  std::string table_id;
  Table table{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetTable).WillOnce([&](GetTableRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(dataset_id, request.dataset_id());
    EXPECT_EQ(table_id, request.table_id());
    return make_status_or(table);
  });
  TableClient table_client(std::move(mock));

  StatusOr<Table> actual =
      GetTable(table_client, project_id, dataset_id, table_id, options);

  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->id, table.id);
}

TEST(GetTable, UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  std::string table_id = "table_id";
  Table table{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetTable).WillOnce([&](GetTableRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(dataset_id, request.dataset_id());
    EXPECT_EQ(table_id, request.table_id());
    return Status(StatusCode::kUnauthenticated, "denied");
  });
  TableClient table_client(std::move(mock));

  StatusOr<Table> actual =
      GetTable(table_client, project_id, dataset_id, table_id, options);

  EXPECT_THAT(actual, StatusIs(StatusCode::kUnauthenticated, StrEq("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
