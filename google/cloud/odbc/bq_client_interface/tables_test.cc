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
#include "google/cloud/mocks/mock_stream_range.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_v2_minimal_internal::GetTableRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatTable;
using ::google::cloud::bigquery_v2_minimal_internal::ListTablesRequest;
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
  TableFilter table_filter{.selected_fields = {"filed_1"},
                           .view =
                               ::google::cloud::bigquery_v2_minimal_internal::
                                   TableMetadataView::Full()};
  Table table{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetTable).WillOnce([&](GetTableRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(dataset_id, request.dataset_id());
    EXPECT_EQ(table_id, request.table_id());
    EXPECT_EQ(table_filter.selected_fields, request.selected_fields());
    EXPECT_EQ(table_filter.view.value, request.view().value);
    return make_status_or(table);
  });
  TableClient table_client(std::move(mock));

  StatusOr<Table> actual = GetTable(table_client, project_id, dataset_id,
                                    table_id, table_filter, options);

  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->id, table.id);
}

TEST(GetTable, GetTable_EmptyInputParams) {
  Options options;
  std::string project_id;
  std::string dataset_id;
  std::string table_id;
  TableFilter table_filter{.selected_fields = {}};
  Table table{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetTable).WillOnce([&](GetTableRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(dataset_id, request.dataset_id());
    EXPECT_EQ(table_id, request.table_id());
    EXPECT_EQ(table_filter.selected_fields, request.selected_fields());
    EXPECT_EQ(table_filter.view.value, request.view().value);
    return make_status_or(table);
  });
  TableClient table_client(std::move(mock));

  StatusOr<Table> actual = GetTable(table_client, project_id, dataset_id,
                                    table_id, table_filter, options);

  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->id, table.id);
}

TEST(GetTable, GetTable_InvalidFilterParameters) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  std::string table_id = "table_id";
  auto metadata_view =
      ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView::Full();
  metadata_view.value = "invalid-value";
  TableFilter table_filter{.selected_fields = {}, .view = metadata_view};
  Table table{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetTable).WillOnce([&](GetTableRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(dataset_id, request.dataset_id());
    EXPECT_EQ(table_id, request.table_id());
    EXPECT_EQ(table_filter.selected_fields, request.selected_fields());
    EXPECT_EQ(table_filter.view.value, request.view().value);
    return make_status_or(table);
  });
  TableClient table_client(std::move(mock));

  StatusOr<Table> actual = GetTable(table_client, project_id, dataset_id,
                                    table_id, table_filter, options);

  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(actual->id, table.id);
}

TEST(GetTable, GetTableFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  std::string table_id = "table_id";
  TableFilter table_filter{.selected_fields = {}};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetTable).WillOnce([&](GetTableRequest const& request) {
    EXPECT_EQ(project_id, request.project_id());
    EXPECT_EQ(dataset_id, request.dataset_id());
    EXPECT_EQ(table_id, request.table_id());
    EXPECT_EQ(table_filter.selected_fields, request.selected_fields());
    EXPECT_EQ(table_filter.view.value, request.view().value);
    return Status(StatusCode::kUnauthenticated, "denied");
  });
  TableClient table_client(std::move(mock));

  StatusOr<Table> actual = GetTable(table_client, project_id, dataset_id,
                                    table_id, table_filter, options);

  EXPECT_THAT(actual, StatusIs(StatusCode::kUnauthenticated, StrEq("denied")));
}

TEST(ListAllTables, ListZeroTablesSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListTables)
      .WillOnce([&](ListTablesRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return mocks::MakeStreamRange<ListFormatTable>({});
      });
  TableClient table_client(std::move(mock));

  StatusOr<std::vector<ListFormatTable>> tables =
      ListAllTables(table_client, project_id, dataset_id, options);

  ASSERT_STATUS_OK(tables);
  EXPECT_EQ(0, tables->size());
}

TEST(ListAllTables, ListAllTablesSuccess) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  ListFormatTable expected{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListTables)
      .WillOnce([&](ListTablesRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return mocks::MakeStreamRange<ListFormatTable>({expected});
      });
  TableClient table_client(std::move(mock));

  StatusOr<std::vector<ListFormatTable>> tables =
      ListAllTables(table_client, project_id, dataset_id, options);

  ASSERT_STATUS_OK(tables);
  EXPECT_EQ(1, tables->size());
  EXPECT_EQ(expected.id, tables->at(0).id);
}

TEST(ListAllTables, ListAllTablesSuccess_EmptyInputParams) {
  Options options;
  std::string project_id;
  std::string dataset_id;
  ListFormatTable expected{.id = "table_id"};
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListTables)
      .WillOnce([&](ListTablesRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return mocks::MakeStreamRange<ListFormatTable>({expected});
      });
  TableClient table_client(std::move(mock));

  StatusOr<std::vector<ListFormatTable>> tables =
      ListAllTables(table_client, project_id, dataset_id, options);

  ASSERT_STATUS_OK(tables);
  EXPECT_EQ(1, tables->size());
  EXPECT_EQ(expected.id, tables->at(0).id);
}

TEST(ListAllTables, ListAllTablesFailure_UnauthenticatedRequest) {
  Options options;
  std::string project_id = "project_id";
  std::string dataset_id = "dataset_id";
  auto mock = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListTables)
      .WillOnce([&](ListTablesRequest const& request) {
        EXPECT_EQ(project_id, request.project_id());
        EXPECT_EQ(dataset_id, request.dataset_id());
        return mocks::MakeStreamRange<ListFormatTable>(
            {}, Status(StatusCode::kUnauthenticated, "denied"));
      });
  TableClient table_client(std::move(mock));

  StatusOr<std::vector<ListFormatTable>> tables =
      ListAllTables(table_client, project_id, dataset_id, options);

  EXPECT_THAT(tables, StatusIs(StatusCode::kUnauthenticated, StrEq("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
