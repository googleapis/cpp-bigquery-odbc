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
#include "google/cloud/resourcemanager/v3/mocks/mock_projects_connection.h"
#include "google/cloud/resourcemanager/v3/projects_client.h"
#include "google/cloud/serviceusage/v1/mocks/mock_service_usage_connection.h"
#include <google/cloud/resourcemanager/v3/projects.pb.h>
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::api::serviceusage::v1::GetServiceRequest;
using ::google::api::serviceusage::v1::Service;
using ::google::api::serviceusage::v1::State;
using ::google::cloud::bigquery_v2_minimal_internal::ListProjectsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::MockProjectConnection;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using google::cloud::odbc_bigquery_client_interface::ListAllProjects;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::google::cloud::resourcemanager_v3::ProjectsClient;
using ::google::cloud::resourcemanager_v3_mocks::MockProjectsConnection;
using ::google::cloud::serviceusage_v1::ServiceUsageClient;
using ::google::cloud::serviceusage_v1_mocks::MockServiceUsageConnection;
using ::testing::AtLeast;
using ::testing::HasSubstr;

std::string const kParentFolder = "folders/123";
std::string const kQuery = "state:ACTIVE";
std::string const kParentOrganization = "organizations/123";

namespace {

ProjectsClient GetMockResourceProjectsClient(
    google::cloud::resourcemanager::v3::Project const& expected_rm_project) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetProject)
      .WillOnce(
          [expected_rm_project](
              google::cloud::resourcemanager::v3::GetProjectRequest const&) {
            return make_status_or(expected_rm_project);
            ;
          });
  ProjectsClient mocked_projects_client(std::move(mock));
  return mocked_projects_client;
}

void VerifyResourceProjectResults(
    std::string const& kind, std::int64_t const& numeric_id,
    google::cloud::resourcemanager::v3::Project const& expected_rm_project,
    Project const& actual_bq_project) {
  EXPECT_EQ(kind, actual_bq_project.kind);
  EXPECT_EQ(numeric_id, actual_bq_project.numeric_id);
  EXPECT_EQ(expected_rm_project.project_id(), actual_bq_project.id);
  EXPECT_EQ(expected_rm_project.display_name(),
            actual_bq_project.friendly_name);
  EXPECT_EQ(expected_rm_project.project_id(),
            actual_bq_project.project_reference.project_id);
}

ProjectsClient GetMockListProjectsClientSuccess(
    google::cloud::resourcemanager::v3::Project const& expected_rm_project) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce(
          [expected_rm_project](
              google::cloud::resourcemanager::v3::ListProjectsRequest const&) {
            return mocks::MakeStreamRange<
                google::cloud::resourcemanager::v3::Project>(
                {expected_rm_project});
          });
  ProjectsClient mocked_projects_client(std::move(mock));
  return mocked_projects_client;
}

ProjectsClient GetMockSearchProjectsClientSuccess(
    google::cloud::resourcemanager::v3::Project const& expected_rm_project) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SearchProjects)
      .WillOnce([expected_rm_project](google::cloud::resourcemanager::v3::
                                          SearchProjectsRequest const&) {
        return mocks::MakeStreamRange<
            google::cloud::resourcemanager::v3::Project>({expected_rm_project});
      });
  ProjectsClient mocked_projects_client(std::move(mock));
  return mocked_projects_client;
}

ProjectsClient GetMockSearchProjectsClientFailure(
    Status const& expected_status) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SearchProjects)
      .WillOnce([expected_status](google::cloud::resourcemanager::v3::
                                      SearchProjectsRequest const&) {
        return mocks::MakeStreamRange<
            google::cloud::resourcemanager::v3::Project>({}, expected_status);
      });
  ProjectsClient mocked_projects_client(std::move(mock));
  return mocked_projects_client;
}

ProjectsClient GetMockListProjectsClientFailure(Status const& expected_status) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce(
          [expected_status](
              google::cloud::resourcemanager::v3::ListProjectsRequest const&) {
            return mocks::MakeStreamRange<
                google::cloud::resourcemanager::v3::Project>({},
                                                             expected_status);
          });
  ProjectsClient mocked_projects_client(std::move(mock));
  return mocked_projects_client;
}

ServiceUsageClient GetMockServiceUsageClient(Service const& expected_service) {
  auto mock = std::make_shared<MockServiceUsageConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetService)
      .WillOnce([expected_service](GetServiceRequest const&) {
        return make_status_or(expected_service);
        ;
      });
  ServiceUsageClient mock_service_usage_client(std::move(mock));
  return mock_service_usage_client;
}

void TestSearchProjectsRMSuccess(State const& expected_state,
                                 std::string const& search_query,
                                 int expected_projects_size) {
  Options options;

  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("test");
  expected_rm_project.set_display_name("test");

  ProjectsClient mocked_projects_client =
      GetMockSearchProjectsClientSuccess(expected_rm_project);

  Service expected_service;
  expected_service.set_state(expected_state);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  StatusRecordOr<std::vector<Project>> bq_projects =
      SearchProjectsRM(mocked_projects_client, mocked_service_usage_client,
                       search_query, options);
  ASSERT_STATUS_RECORD_OK(bq_projects);
  if (expected_state == State::ENABLED) {
    ASSERT_FALSE((*bq_projects).empty());
    ASSERT_EQ((*bq_projects).size(), expected_projects_size);
    if (expected_projects_size == 1) {
      ASSERT_EQ(expected_rm_project.project_id(), (*bq_projects)[0].id);
    }
  } else {
    ASSERT_TRUE((*bq_projects).empty());
  }
}

void TestListProjectsRMSuccess(State const& expected_state,
                               std::string const& parent,
                               int expected_projects_size) {
  Options options;

  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("test");
  expected_rm_project.set_display_name("test");

  ProjectsClient mocked_projects_client =
      GetMockListProjectsClientSuccess(expected_rm_project);

  Service expected_service;
  expected_service.set_state(expected_state);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  StatusRecordOr<std::vector<Project>> bq_projects = ListAllProjectsRM(
      mocked_projects_client, mocked_service_usage_client, parent, options);
  ASSERT_STATUS_RECORD_OK(bq_projects);
  if (expected_state == State::ENABLED) {
    ASSERT_FALSE((*bq_projects).empty());
    ASSERT_EQ((*bq_projects).size(), expected_projects_size);
    if (expected_projects_size == 1) {
      ASSERT_EQ(expected_rm_project.project_id(), (*bq_projects)[0].id);
    }
  } else {
    ASSERT_TRUE((*bq_projects).empty());
  }
}

}  // namespace

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

TEST(GetResourceManagerProject, SuccessWithProjectsPrefix) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("test");
  expected_rm_project.set_display_name("test");

  ProjectsClient mocked_projects_client =
      GetMockResourceProjectsClient(expected_rm_project);

  Service expected_service;
  expected_service.set_state(State::ENABLED);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  StatusRecordOr<Project> actual_bq_project =
      GetProjectRM(mocked_projects_client, mocked_service_usage_client,
                   "projects/test", options);
  ASSERT_STATUS_RECORD_OK(actual_bq_project);

  VerifyResourceProjectResults(/*kind*/ "bigquery#project",
                               /*numeric_id*/ 1234, expected_rm_project,
                               *actual_bq_project);
}

TEST(GetResourceManagerProject, SuccessWithoutProjectsPrefix) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("test");
  expected_rm_project.set_display_name("test");

  ProjectsClient mocked_projects_client =
      GetMockResourceProjectsClient(expected_rm_project);

  Service expected_service;
  expected_service.set_state(State::ENABLED);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  StatusRecordOr<Project> actual_bq_project = GetProjectRM(
      mocked_projects_client, mocked_service_usage_client, "test", options);
  ASSERT_STATUS_RECORD_OK(actual_bq_project);

  VerifyResourceProjectResults(/*kind*/ "bigquery#project",
                               /*numeric_id*/ 1234, expected_rm_project,
                               *actual_bq_project);
}

TEST(GetResourceManagerProject, Fail_ProjectNotEnabledForBQ) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("test");
  expected_rm_project.set_display_name("test");

  ProjectsClient mocked_projects_client =
      GetMockResourceProjectsClient(expected_rm_project);

  Service expected_service;
  expected_service.set_state(State::DISABLED);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  StatusRecordOr<Project> actual_bq_project = GetProjectRM(
      mocked_projects_client, mocked_service_usage_client, "test", options);

  EXPECT_THAT(actual_bq_project,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("not enabled for BigQuery")));
}

TEST(GetResourceManagerProject, Fail_EmptyProjectId) {
  Options options;
  auto mock = std::make_shared<MockProjectsConnection>();
  ProjectsClient mocked_projects_client(std::move(mock));

  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<Project> actual_bq_project = GetProjectRM(
      mocked_projects_client, mocked_service_usage_client, "", options);

  EXPECT_THAT(actual_bq_project,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("cannot be empty")));
}

TEST(GetResourceManagerProject, Fail_ProjectNotFound) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("test");
  expected_rm_project.set_display_name("test");

  ProjectsClient mocked_projects_client =
      GetMockResourceProjectsClient(expected_rm_project);

  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<Project> actual_bq_project = GetProjectRM(
      mocked_projects_client, mocked_service_usage_client, "test123", options);

  EXPECT_THAT(actual_bq_project,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("not found")));
}

TEST(GetResourceManagerProject, Fail_InvalidProjectName) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects-1234");
  expected_rm_project.set_project_id("test");
  expected_rm_project.set_display_name("test");

  ProjectsClient mocked_projects_client =
      GetMockResourceProjectsClient(expected_rm_project);

  Service expected_service;
  expected_service.set_state(State::ENABLED);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  StatusRecordOr<Project> actual_bq_project = GetProjectRM(
      mocked_projects_client, mocked_service_usage_client, "test", options);

  EXPECT_THAT(actual_bq_project,
              StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                             HasSubstr("not found with valid project name")));
}

TEST(SearchProjectsRM, Success_EmptyQuery_EnabledState) {
  TestSearchProjectsRMSuccess(State::ENABLED, /* query */ "",
                              /*projects size*/ 1);
}

TEST(SearchProjectsRM, Success_EmptyQuery_DisabledState) {
  TestSearchProjectsRMSuccess(State::DISABLED, /* query */ "",
                              /*projects size*/ 0);
}

TEST(SearchProjectsRM, Success_EmptyQuery_UnSpecifiedState) {
  TestSearchProjectsRMSuccess(State::STATE_UNSPECIFIED, /* query */ "",
                              /*projects size*/ 0);
}

TEST(SearchProjectsRM, Success_WithQuery_EnabledState) {
  TestSearchProjectsRMSuccess(State::ENABLED, /* query */ "state:ACTIVE",
                              /*projects size*/ 1);
}

TEST(SearchProjectsRM, Success_WithQuery_DisabledState) {
  TestSearchProjectsRMSuccess(State::DISABLED, /* query */ "state:ACTIVE",
                              /*projects size*/ 0);
}

TEST(SearchProjectsRM, Success_WithQuery_UnSpecifiedState) {
  TestSearchProjectsRMSuccess(State::STATE_UNSPECIFIED,
                              /* query */ "state:ACTIVE",
                              /*projects size*/ 0);
}

TEST(SearchProjectsRM, Failure_Invalid_Argument) {
  auto expected_status = Status(StatusCode::kInvalidArgument, "Bad Argument");
  Options options;

  ProjectsClient mocked_projects_client =
      GetMockSearchProjectsClientFailure(expected_status);

  auto mock = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> bq_projects = SearchProjectsRM(
      mocked_projects_client, mocked_service_usage_client, "", options);
  EXPECT_THAT(bq_projects,
              StatusRecordIs(odbc_internal::SQLStates::k_42000(),
                             HasSubstr(expected_status.message())));
}

TEST(ListProjectsRM, Success_ParentIsFolder_EnabledState) {
  TestListProjectsRMSuccess(State::ENABLED, kParentFolder,
                            /*projects size*/ 1);
}

TEST(ListProjectsRM, Success_ParentIsFolder_DisabledState) {
  TestListProjectsRMSuccess(State::DISABLED, kParentFolder,
                            /*projects size*/ 0);
}

TEST(ListProjectsRM, Success_ParentIsFolder_UnSpecifiedState) {
  TestListProjectsRMSuccess(State::STATE_UNSPECIFIED, kParentFolder,
                            /*projects size*/ 0);
}

TEST(ListProjectsRM, Success_ParentIsOrganization_EnabledState) {
  TestListProjectsRMSuccess(State::ENABLED, kParentOrganization,
                            /*projects size*/ 1);
}

TEST(ListProjectsRM, Success_ParentIsOrganization_DisabledState) {
  TestListProjectsRMSuccess(State::DISABLED, kParentOrganization,
                            /*projects size*/ 0);
}

TEST(ListProjectsRM, Success_ParentIsOrganization_UnSpecifiedState) {
  TestListProjectsRMSuccess(State::STATE_UNSPECIFIED, kParentOrganization,
                            /*projects size*/ 0);
}

TEST(ListProjectsRM, Failure_Forbidden) {
  auto expected_status = Status(StatusCode::kPermissionDenied,
                                "The caller does not have permission");
  Options options;

  ProjectsClient mocked_projects_client =
      GetMockListProjectsClientFailure(expected_status);

  auto mock = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock));

  StatusRecordOr<std::vector<Project>> bq_projects =
      ListAllProjectsRM(mocked_projects_client, mocked_service_usage_client,
                        "folders/1234", options);
  EXPECT_THAT(bq_projects,
              StatusRecordIs(odbc_internal::SQLStates::k_42000(),
                             HasSubstr(expected_status.message())));
}

TEST(ListProjectsRM, Failure_EmptyParent) {
  Options options;
  auto mock = std::make_shared<MockProjectsConnection>();
  ProjectsClient mocked_projects_client(std::move(mock));

  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<std::vector<Project>> bq_projects = ListAllProjectsRM(
      mocked_projects_client, mocked_service_usage_client, "", options);
  EXPECT_THAT(
      bq_projects,
      StatusRecordIs(odbc_internal::SQLStates::k_HY000(),
                     HasSubstr("parent resource cannot be null or empty")));
}

TEST(FilterProjectsRMList, FilterZeroProjects_NoRMProjects) {
  Options options;
  auto mock = std::make_shared<MockProjectsConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce(
          [](google::cloud::resourcemanager::v3::ListProjectsRequest const&) {
            return mocks::MakeStreamRange<
                google::cloud::resourcemanager::v3::Project>({});
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  std::vector<std::string> project_ids;
  for (int i = 0; i <= 110; i++) {
    std::string id = "ids_";
    id.append(std::to_string(i));
    project_ids.push_back(id);
  }

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjectsRMList(mocked_projects_client, mocked_service_usage_client,
                           kParentFolder, project_ids, options);

  EXPECT_EQ(0, projects->size());
}

TEST(FilterProjectsRMList,
     FilterZeroProjects_NoRMProjects_ProjectIdsLessThan100) {
  Options options;
  auto mock = std::make_shared<MockProjectsConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetProject)
      .Times(AtLeast(1))
      .WillRepeatedly(
          [](google::cloud::resourcemanager::v3::GetProjectRequest const&) {
            return make_status_or<google::cloud::resourcemanager::v3::Project>(
                {});
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjectsRMList(mocked_projects_client, mocked_service_usage_client,
                           kParentFolder, {"ids_1", "ids_2"}, options);

  EXPECT_EQ(0, projects->size());
}

TEST(FilterProjectsRMSearch, FilterZeroProjects_NoRMProjects) {
  Options options;
  auto mock = std::make_shared<MockProjectsConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SearchProjects)
      .WillOnce(
          [](google::cloud::resourcemanager::v3::SearchProjectsRequest const&) {
            return mocks::MakeStreamRange<
                google::cloud::resourcemanager::v3::Project>({});
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<std::vector<Project>> projects = FilterProjectsRMSearch(
      mocked_projects_client, mocked_service_usage_client, kQuery,
      {"id_1", "id_2"}, options);

  EXPECT_EQ(0, projects->size());
}

TEST(FilterProjectsRMList, FilterZeroProjects_NoBQEnabledProjects) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("ids_111");
  expected_rm_project.set_display_name("test");
  ProjectsClient mocked_projects_client =
      GetMockListProjectsClientSuccess(expected_rm_project);

  Service expected_service;
  expected_service.set_state(State::DISABLED);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  std::vector<std::string> project_ids;
  for (int i = 0; i <= 110; i++) {
    std::string id = "ids_";
    id.append(std::to_string(i));
    project_ids.push_back(id);
  }
  project_ids.push_back("ids_111");

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjectsRMList(mocked_projects_client, mocked_service_usage_client,
                           kParentFolder, project_ids, options);
  EXPECT_EQ(0, projects->size());
}

TEST(FilterProjectsRMList,
     FilterZeroProjects_NoBQEnabledProjects_ProjectIdsLessThan100) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("ids_1");
  expected_rm_project.set_display_name("test");

  auto mock = std::make_shared<MockProjectsConnection>();
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, GetProject)
      .Times(AtLeast(1))
      .WillRepeatedly(
          [expected_rm_project](
              google::cloud::resourcemanager::v3::GetProjectRequest const&) {
            return make_status_or(expected_rm_project);
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  Service expected_service;
  expected_service.set_state(State::DISABLED);
  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  EXPECT_CALL(*mock_su, options);
  EXPECT_CALL(*mock_su, GetService)
      .Times(AtLeast(1))
      .WillRepeatedly([expected_service](GetServiceRequest const&) {
        return make_status_or(expected_service);
      });
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjectsRMList(mocked_projects_client, mocked_service_usage_client,
                           kParentFolder, {"ids_1", "ids_2"}, options);

  EXPECT_EQ(0, projects->size());
}

TEST(FilterProjectsRMSearch, FilterZeroProjects_NoBQEnabledProjects) {
  Options options;
  google::cloud::resourcemanager::v3::Project expected_rm_project;
  expected_rm_project.set_name("projects/1234");
  expected_rm_project.set_project_id("id_1");
  expected_rm_project.set_display_name("test");
  ProjectsClient mocked_projects_client =
      GetMockSearchProjectsClientSuccess(expected_rm_project);

  Service expected_service;
  expected_service.set_state(State::DISABLED);
  ServiceUsageClient mocked_service_usage_client =
      GetMockServiceUsageClient(expected_service);

  StatusRecordOr<std::vector<Project>> projects = FilterProjectsRMSearch(
      mocked_projects_client, mocked_service_usage_client, kQuery,
      {"id_1", "id_2"}, options);

  EXPECT_EQ(0, projects->size());
}

TEST(FilterProjectsRMList, FilterOneProject) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  google::cloud::resourcemanager::v3::Project expected_rm_project_1;
  expected_rm_project_1.set_name("projects/1234");
  expected_rm_project_1.set_project_id("test1");
  expected_rm_project_1.set_display_name("test1");
  google::cloud::resourcemanager::v3::Project expected_rm_project_2;
  expected_rm_project_2.set_name("projects/8901");
  expected_rm_project_2.set_project_id("test2");
  expected_rm_project_2.set_display_name("test2");
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce(
          [expected_rm_project_1, expected_rm_project_2](
              google::cloud::resourcemanager::v3::ListProjectsRequest const&) {
            return mocks::MakeStreamRange<
                google::cloud::resourcemanager::v3::Project>(
                {expected_rm_project_1, expected_rm_project_2});
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  Service expected_service;
  expected_service.set_state(State::ENABLED);
  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  EXPECT_CALL(*mock_su, options);
  EXPECT_CALL(*mock_su, GetService)
      .Times(AtLeast(1))
      .WillRepeatedly([expected_service](GetServiceRequest const&) {
        return make_status_or(expected_service);
      });
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  std::vector<std::string> project_ids;
  for (int i = 0; i <= 110; i++) {
    std::string id = "ids_";
    id.append(std::to_string(i));
    project_ids.push_back(id);
  }
  project_ids.push_back(expected_rm_project_1.project_id());

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjectsRMList(mocked_projects_client, mocked_service_usage_client,
                           kParentFolder, project_ids, options);

  EXPECT_EQ(1, projects->size());
  EXPECT_EQ(expected_rm_project_1.project_id(), projects->at(0).id);
}

TEST(FilterProjectsRMSearch, FilterOneProject) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  google::cloud::resourcemanager::v3::Project expected_rm_project_1;
  expected_rm_project_1.set_name("projects/1234");
  expected_rm_project_1.set_project_id("test1");
  expected_rm_project_1.set_display_name("test1");
  google::cloud::resourcemanager::v3::Project expected_rm_project_2;
  expected_rm_project_2.set_name("projects/8901");
  expected_rm_project_2.set_project_id("test2");
  expected_rm_project_2.set_display_name("test2");

  EXPECT_CALL(*mock, SearchProjects)
      .WillOnce([expected_rm_project_1,
                 expected_rm_project_2](google::cloud::resourcemanager::v3::
                                            SearchProjectsRequest const&) {
        return mocks::MakeStreamRange<
            google::cloud::resourcemanager::v3::Project>(
            {expected_rm_project_1, expected_rm_project_2});
      });
  ProjectsClient mocked_projects_client(std::move(mock));

  Service expected_service;
  expected_service.set_state(State::ENABLED);
  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  EXPECT_CALL(*mock_su, options);
  EXPECT_CALL(*mock_su, GetService)
      .Times(AtLeast(1))
      .WillRepeatedly([expected_service](GetServiceRequest const&) {
        return make_status_or(expected_service);
      });
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<std::vector<Project>> projects = FilterProjectsRMSearch(
      mocked_projects_client, mocked_service_usage_client, kQuery,
      {expected_rm_project_1.project_id(), "id_2"}, options);

  EXPECT_EQ(1, projects->size());
  EXPECT_EQ(expected_rm_project_1.project_id(), projects->at(0).id);
}

TEST(FilterProjectsRMList, FilterOneProject_ProjectIdsLessThan100) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  google::cloud::resourcemanager::v3::Project expected_rm_project_1;
  expected_rm_project_1.set_name("projects/1234");
  expected_rm_project_1.set_project_id("test1");
  expected_rm_project_1.set_display_name("test1");
  google::cloud::resourcemanager::v3::Project expected_rm_project_2;
  expected_rm_project_2.set_name("projects/8901");
  expected_rm_project_2.set_project_id("test2");
  expected_rm_project_2.set_display_name("test2");
  EXPECT_CALL(*mock, GetProject)
      .WillRepeatedly(
          [expected_rm_project_1](
              google::cloud::resourcemanager::v3::GetProjectRequest const&) {
            return make_status_or(expected_rm_project_1);
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  Service expected_service;
  expected_service.set_state(State::ENABLED);
  auto mock_su = std::make_shared<MockServiceUsageConnection>();
  EXPECT_CALL(*mock_su, options);
  EXPECT_CALL(*mock_su, GetService)
      .Times(AtLeast(1))
      .WillRepeatedly([expected_service](GetServiceRequest const&) {
        return make_status_or(expected_service);
      });
  ServiceUsageClient mocked_service_usage_client(std::move(mock_su));

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjectsRMList(mocked_projects_client, mocked_service_usage_client,
                           kParentFolder, {"test1", "test2"}, options);

  EXPECT_EQ(1, projects->size());
  EXPECT_EQ(expected_rm_project_1.project_id(), projects->at(0).id);
}

TEST(FilterProjectsRMList, Failure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ListProjects)
      .WillOnce(
          [](google::cloud::resourcemanager::v3::ListProjectsRequest const&) {
            return mocks::MakeStreamRange<
                google::cloud::resourcemanager::v3::Project>(
                {}, Status(StatusCode::kUnauthenticated, "denied"));
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  auto mock_2 = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_2));

  std::vector<std::string> project_ids;
  for (int i = 0; i <= 110; i++) {
    std::string id = "ids_";
    id.append(std::to_string(i));
    project_ids.push_back(id);
  }

  StatusRecordOr<std::vector<Project>> projects =
      FilterProjectsRMList(mocked_projects_client, mocked_service_usage_client,
                           kParentFolder, project_ids, options);

  EXPECT_THAT(projects, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                       HasSubstr("denied")));
}

TEST(FilterProjectsRMSearch, Failure_UnauthenticatedRequest) {
  auto mock = std::make_shared<MockProjectsConnection>();
  Options options;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, SearchProjects)
      .WillOnce(
          [](google::cloud::resourcemanager::v3::SearchProjectsRequest const&) {
            return mocks::MakeStreamRange<
                google::cloud::resourcemanager::v3::Project>(
                {}, Status(StatusCode::kUnauthenticated, "denied"));
          });
  ProjectsClient mocked_projects_client(std::move(mock));

  auto mock_2 = std::make_shared<MockServiceUsageConnection>();
  ServiceUsageClient mocked_service_usage_client(std::move(mock_2));

  StatusRecordOr<std::vector<Project>> projects = FilterProjectsRMSearch(
      mocked_projects_client, mocked_service_usage_client, kQuery,
      {"id_1", "id_2"}, options);

  EXPECT_THAT(projects, StatusRecordIs(odbc_internal::SQLStates::k_28000(),
                                       HasSubstr("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
