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

#include "google/cloud/odbc/bq_client_interface/projects.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_project_connection.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_v2_minimal_internal::ListProjectsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::MockProjectConnection;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using google::cloud::odbc_bigquery_client_interface::ListAllProjects;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(ListAllProjects, ListZeroProjects) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects).WillOnce([](ListProjectsRequest const&) {
    return mocks::MakeStreamRange<Project>({});
  });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> projects =
      ListAllProjects(mocked_project_client, options);

  EXPECT_EQ(0, projects->size());
}

TEST(ListAllProjects, ListOneProject) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  Project expected{"p-kind", "p-id"};
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce([expected](ListProjectsRequest const&) {
        return mocks::MakeStreamRange<Project>({expected});
      });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> projects =
      ListAllProjects(mocked_project_client, options);

  EXPECT_EQ(1, projects->size());
  EXPECT_EQ(expected.id, projects->at(0).id);
}

TEST(ListAllProjects, ListProjectsFailure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects).WillOnce([](ListProjectsRequest const&) {
    return mocks::MakeStreamRange<Project>(
        {}, Status(StatusCode::kUnauthenticated, "denied"));
  });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> projects =
      ListAllProjects(mocked_project_client, options);

  EXPECT_THAT(projects, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                       HasSubstr("denied")));
}

TEST(GetProjects, GetOneProject) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  Project expected{"p-kind", "p-id"};
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce([expected](ListProjectsRequest const&) {
        return mocks::MakeStreamRange<Project>({expected});
      });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<Project> project =
      GetProject(mocked_project_client, expected.id, options);

  EXPECT_EQ(expected.id, project->id);
}

TEST(GetProjects, GetProjectFailure_ProjectNotFound) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  Project expected{"p-kind", "p-id"};
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce([expected](ListProjectsRequest const&) {
        return mocks::MakeStreamRange<Project>({expected});
      });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<Project> project =
      GetProject(mocked_project_client, "unknown-id", options);

  EXPECT_THAT(project, StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                                      HasSubstr("not found")));
}

TEST(GetProjects, GetProjectFailure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects).WillOnce([](ListProjectsRequest const&) {
    return mocks::MakeStreamRange<Project>(
        {}, Status(StatusCode::kUnauthenticated, "denied"));
  });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<Project> project =
      GetProject(mocked_project_client, "id", options);

  EXPECT_THAT(project, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                      HasSubstr("denied")));
}

TEST(FilterProjects, FilterZeroProjects) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects).WillOnce([](ListProjectsRequest const&) {
    return mocks::MakeStreamRange<Project>({});
  });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjects(mocked_project_client, {"id_1", "id_2"}, options);

  EXPECT_EQ(0, projects->size());
}

TEST(FilterProjects, FilterOneProject) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  Project response_1{"p-kind", "p-id-1"};
  Project response_2{"p-kind", "p-id-2"};
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce([response_1, response_2](ListProjectsRequest const&) {
        return mocks::MakeStreamRange<Project>({response_1, response_2});
      });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjects(mocked_project_client, {response_1.id, "id_2"}, options);

  EXPECT_EQ(1, projects->size());
  EXPECT_EQ(response_1.id, projects->at(0).id);
}

TEST(FilterProjects, FilterProjectsFailure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockProjectConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects).WillOnce([](ListProjectsRequest const&) {
    return mocks::MakeStreamRange<Project>(
        {}, Status(StatusCode::kUnauthenticated, "denied"));
  });
  ProjectClient mocked_project_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjects(mocked_project_client, {"id_1", "id_2"}, options);

  EXPECT_THAT(projects, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                       HasSubstr("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
