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
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/resourcemanager/v3/projects_client.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_client_library_utils::
    CreateApplicationDefaultAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationBYOIDWorkforce;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationBYOIDWorkload;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationJSONFile;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalUserOauthBYOIDWorkforce;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalUserOauthBYOIDWorkload;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateUserAccountAuthentication;
using google::cloud::odbc_testing_utils::StatusIs;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using google::cloud::resourcemanager::v3::Project;
using ::google::cloud::resourcemanager_v3::MakeProjectsConnection;
using ::google::cloud::resourcemanager_v3::ProjectsClient;
using ::testing::HasSubstr;

std::string const kRMProjectWithoutPrefix = "bigquery-devtools-drivers";
std::string const kRMProjectWithPrefix = "projects/" + kRMProjectWithoutPrefix;

std::string const kParentFolder =
    "folders/329838888119";  // bq-partner-org.joonix.net/data
std::string const kParentInvalidFolder = "folders/1234";

#ifdef EXTERNAL_ACCOUNT_AUTH
TEST(ResourceManager, ExternalAccountAuth_GetProjectRM_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kExternalUser, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject(kRMProjectWithPrefix, *options, true);

  ASSERT_STATUS_RECORD_OK(projects_status);
  EXPECT_EQ((*projects_status).id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager, ExternalAccountAuth_GetProjectRM_BYOID_Workload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject(kRMProjectWithPrefix, *options, true);

  ASSERT_STATUS_RECORD_OK(projects_status);
  EXPECT_EQ((*projects_status).id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager, ExternalAccountAuth_GetProjectRM_BYOID_Workforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject(kRMProjectWithPrefix, *options, true);

  ASSERT_STATUS_RECORD_OK(projects_status);
  EXPECT_EQ((*projects_status).id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager, ExternalAccountAuth_ListProjectsRM_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kExternalUser, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentFolder, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  ASSERT_TRUE((*projects_status).empty());
}

TEST(ResourceManager, ExternalAccountAuth_ListProjectsRM_BYOID_Workload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentFolder, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  ASSERT_TRUE((*projects_status).empty());
}

TEST(ResourceManager, ExternalAccountAuth_ListProjectsRM_BYOID_Workforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentFolder, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  ASSERT_TRUE((*projects_status).empty());
}

TEST(ResourceManager, ExternalAccountAuth_SearchProjectsRM_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kExternalUser, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("state:ACTIVE", *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
}

TEST(ResourceManager, ExternalAccountAuth_SearchProjectsRM_BYOID_Workload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("state:ACTIVE", *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
}

TEST(ResourceManager, ExternalAccountAuth_SearchProjectsRM_BYOID_Workforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("state:ACTIVE", *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
}

TEST(ResourceManager, ExternalAccountAuth_FilterProjectsRMList_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kExternalUser, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMList(kParentFolder, project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager, ExternalAccountAuth_FilterProjectsRMList_BYOIDWorkload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMList(kParentFolder, project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager, ExternalAccountAuth_FilterProjectsRMList_BYOIDWorkforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMList(kParentFolder, project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager, ExternalAccountAuth_FilterProjectsRMSearch_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kExternalUser, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("state:ACTIVE", project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);

  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager,
     ExternalAccountAuth_FilterProjectsRMSearch_BYOIDWorkload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("state:ACTIVE", project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);

  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ResourceManager,
     ExternalAccountAuth_FilterProjectsRMSearch_BYOIDWorkforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);

  Oauth oauth = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("state:ACTIVE", project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);

  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

#endif  // EXTERNAL_ACCOUNT_AUTH

#ifdef USER_ACCOUNT_AUTH
TEST(ResourceManagerGetProject, UserAccountAuth_SuccessProjectWithPrefix) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StatusOr<Project> project =
      projects_client.GetProject(kRMProjectWithPrefix, *options);

  ASSERT_STATUS_OK(project);
  EXPECT_FALSE((*project).name().empty());
  EXPECT_FALSE((*project).parent().empty());
  EXPECT_EQ((*project).project_id(), kRMProjectWithoutPrefix);
  EXPECT_EQ((*project).display_name(), kRMProjectWithoutPrefix);
}

TEST(ResourceManagerGetProject, UserAccountAuth_Failure_InvalidArgument) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(
      ::google::cloud::resourcemanager_v3::MakeProjectsConnection(*options));

  StatusOr<Project> project = projects_client.GetProject("invalid", *options);

  EXPECT_THAT(project, StatusIs(StatusCode::kInvalidArgument,
                                HasSubstr("invalid argument")));
}

TEST(ResourceManagerGetProject, UserAccountAuth_Failure_ProjectNotFound) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StatusOr<Project> project =
      projects_client.GetProject("projects/invalid-proj-1", *options);

  EXPECT_THAT(project, StatusIs(StatusCode::kPermissionDenied,
                                HasSubstr("may not exist")));
}

TEST(ResourceManagerSearchProjects, UserAccountAuth_Success_WithQuery) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.SearchProjects("state:ACTIVE", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerSearchProjects, UserAccountAuth_Success_EmptyQuery) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range = projects_client.SearchProjects("", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerSearchProjects,
     UserAccountAuth_Failure_InvalidArgument_BadQuery) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.SearchProjects("status:ACTIVE", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Invalid filter query")));
  }
}

TEST(ResourceManagerListProjects, UserAccountAuth_Failure_Forbidden) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.ListProjects(kParentInvalidFolder, *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project,
                StatusIs(StatusCode::kPermissionDenied,
                         HasSubstr("The caller does not have permission")));
  }
}
/////////////////////////////////////////
// UserAccountAuth tests via ODBCBQClient
/////////////////////////////////////////

TEST(ODBCBQClient, UserAccountAuth_GetProjectRM_Success) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject(kRMProjectWithPrefix, *options, true);

  ASSERT_STATUS_RECORD_OK(projects_status);
  EXPECT_EQ((*projects_status).id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, UserAccountAuth_GetProjectRM_Failure_ProjectNotFound) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject("invalid", *options, true);

  EXPECT_THAT(projects_status,
              StatusRecordIs(SQLStates::k_42000(), HasSubstr("may not exist")));
}

TEST(ODBCBQClient, UserAccountAuth_ListProjectsRM_Success) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentFolder, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  ASSERT_TRUE((*projects_status).empty());
}

TEST(ODBCBQClient, UserAccountAuth_ListProjectsRM_Failure) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentInvalidFolder, *options);
  EXPECT_THAT(projects_status,
              StatusRecordIs(SQLStates::k_42000(),
                             HasSubstr("The caller does not have permission")));
}

TEST(ODBCBQClient, UserAccountAuth_SearchProjectsRM_Success) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("state:ACTIVE", *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
}

TEST(ODBCBQClient, UserAccountAuth_SearchProjectsRM_Failure) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("status:ACTIVE", *options);
  EXPECT_THAT(
      projects_status,
      StatusRecordIs(SQLStates::k_42000(), HasSubstr("Invalid filter query")));
}

TEST(ODBCBQClient, UserAccountAuth_FilterProjectsRMList_Success) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMList(kParentFolder, project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, UserAccountAuth_FilterProjectsRMSearch_Success) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("state:ACTIVE", project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);

  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, UserAccountAuth_FilterProjectsRMSearch_Failure) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("status:ACTIVE", project_ids, *options);
  EXPECT_THAT(
      projects_status,
      StatusRecordIs(SQLStates::k_42000(), HasSubstr("Invalid filter query")));
}

#else   // USER_ACCOUNT_AUTH

TEST(ResourceManagerGetProject, SuccessProjectWithPrefix) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StatusOr<Project> project =
      projects_client.GetProject(kRMProjectWithPrefix, *options);

  ASSERT_STATUS_OK(project);
  EXPECT_FALSE((*project).name().empty());
  EXPECT_FALSE((*project).parent().empty());
  EXPECT_EQ((*project).project_id(), kRMProjectWithoutPrefix);
  EXPECT_EQ((*project).display_name(), kRMProjectWithoutPrefix);
}

TEST(ResourceManagerGetProject, ADC_SuccessProjectWithPrefix) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StatusOr<Project> project =
      projects_client.GetProject(kRMProjectWithPrefix, *options);

  ASSERT_STATUS_OK(project);
  EXPECT_FALSE((*project).name().empty());
  EXPECT_FALSE((*project).parent().empty());
  EXPECT_EQ((*project).project_id(), kRMProjectWithoutPrefix);
  EXPECT_EQ((*project).display_name(), kRMProjectWithoutPrefix);
}

TEST(ResourceManagerGetProject, Failure_InvalidArgument) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(
      ::google::cloud::resourcemanager_v3::MakeProjectsConnection(*options));

  StatusOr<Project> project = projects_client.GetProject("invalid", *options);

  EXPECT_THAT(project, StatusIs(StatusCode::kInvalidArgument,
                                HasSubstr("invalid argument")));
}

TEST(ResourceManagerGetProject, ADC_Failure_InvalidArgument) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(
      ::google::cloud::resourcemanager_v3::MakeProjectsConnection(*options));

  StatusOr<Project> project = projects_client.GetProject("invalid", *options);

  EXPECT_THAT(project, StatusIs(StatusCode::kInvalidArgument,
                                HasSubstr("invalid argument")));
}

TEST(ResourceManagerGetProject, Failure_ProjectNotFound) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StatusOr<Project> project =
      projects_client.GetProject("projects/invalid-proj-1", *options);

  EXPECT_THAT(project, StatusIs(StatusCode::kPermissionDenied,
                                HasSubstr("may not exist")));
}

TEST(ResourceManagerGetProject, ADC_Failure_ProjectNotFound) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StatusOr<Project> project =
      projects_client.GetProject("projects/invalid-proj-1", *options);

  EXPECT_THAT(project, StatusIs(StatusCode::kPermissionDenied,
                                HasSubstr("may not exist")));
}

TEST(ResourceManagerSearchProjects, Success_WithQuery) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.SearchProjects("state:ACTIVE", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerSearchProjects, ADC_Success_WithQuery) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.SearchProjects("state:ACTIVE", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerSearchProjects, Success_EmptyQuery) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range = projects_client.SearchProjects("", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerSearchProjects, ADC_Success_EmptyQuery) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range = projects_client.SearchProjects("", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerSearchProjects, Failure_InvalidArgument_BadQuery) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.SearchProjects("status:ACTIVE", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Invalid filter query")));
  }
}

TEST(ResourceManagerSearchProjects, ADC_Failure_InvalidArgument_BadQuery) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.SearchProjects("status:ACTIVE", *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Invalid filter query")));
  }
}

TEST(ResourceManagerListProjects, Success_ParentIsFolder) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.ListProjects(kParentFolder, *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerListProjects, ADC_Success_ParentIsFolder) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.ListProjects(kParentFolder, *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ResourceManagerListProjects, Failure_Forbidden) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.ListProjects(kParentInvalidFolder, *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project,
                StatusIs(StatusCode::kPermissionDenied,
                         HasSubstr("The caller does not have permission")));
  }
}

TEST(ResourceManagerListProjects, ADC_Failure_Forbidden) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));

  StreamRange<Project> range =
      projects_client.ListProjects(kParentInvalidFolder, *options);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project,
                StatusIs(StatusCode::kPermissionDenied,
                         HasSubstr("The caller does not have permission")));
  }
}

// RM Integration tests via ODBCBQClient.
TEST(ODBCBQClient, GetProjectRM_Success) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject(kRMProjectWithPrefix, *options, true);

  ASSERT_STATUS_RECORD_OK(projects_status);
  EXPECT_EQ((*projects_status).id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, ADC_GetProjectRM_Success) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject(kRMProjectWithPrefix, *options, true);

  ASSERT_STATUS_RECORD_OK(projects_status);
  EXPECT_EQ((*projects_status).id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, GetProjectRM_Failure_ProjectNotFound) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject("invalid", *options, true);

  EXPECT_THAT(projects_status,
              StatusRecordIs(SQLStates::k_42000(), HasSubstr("may not exist")));
}

TEST(ODBCBQClient, ADC_GetProjectRM_Failure_ProjectNotFound) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects_status =
          (*odbc_bq_client)->GetProject("invalid", *options, true);

  EXPECT_THAT(projects_status,
              StatusRecordIs(SQLStates::k_42000(), HasSubstr("may not exist")));
}

TEST(ODBCBQClient, ListProjectsRM_Success) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentFolder, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  ASSERT_TRUE((*projects_status).empty());
}

TEST(ODBCBQClient, ADC_ListProjectsRM_Success) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentFolder, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  ASSERT_TRUE((*projects_status).empty());
}

TEST(ODBCBQClient, ListProjectsRM_Failure) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentInvalidFolder, *options);
  EXPECT_THAT(projects_status,
              StatusRecordIs(SQLStates::k_42000(),
                             HasSubstr("The caller does not have permission")));
}

TEST(ODBCBQClient, ADC_ListProjectsRM_Failure) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->ListAllProjectsRM(kParentInvalidFolder, *options);
  EXPECT_THAT(projects_status,
              StatusRecordIs(SQLStates::k_42000(),
                             HasSubstr("The caller does not have permission")));
}

TEST(ODBCBQClient, SearchProjectsRM_Success) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("state:ACTIVE", *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
}

TEST(ODBCBQClient, ADC_SearchProjectsRM_Success) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("state:ACTIVE", *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
}

TEST(ODBCBQClient, SearchProjectsRM_Failure) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("status:ACTIVE", *options);
  EXPECT_THAT(
      projects_status,
      StatusRecordIs(SQLStates::k_42000(), HasSubstr("Invalid filter query")));
}

TEST(ODBCBQClient, ADC_SearchProjectsRM_Failure) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  auto projects_status =
      (*odbc_bq_client)->SearchAllProjectsRM("status:ACTIVE", *options);
  EXPECT_THAT(
      projects_status,
      StatusRecordIs(SQLStates::k_42000(), HasSubstr("Invalid filter query")));
}

TEST(ODBCBQClient, FilterProjectsRMList_Success) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMList(kParentFolder, project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, ADC_FilterProjectsRMList_Success) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMList(kParentFolder, project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);
  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, FilterProjectsRMSearch_Success) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("state:ACTIVE", project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);

  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, ADC_FilterProjectsRMSearch_Success) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("state:ACTIVE", project_ids, *options);
  ASSERT_STATUS_RECORD_OK(projects_status);

  std::vector< ::google::cloud::bigquery_v2_minimal_internal::Project>
      projects = *projects_status;
  ASSERT_FALSE(projects.empty());
  EXPECT_EQ(projects[0].id, kRMProjectWithoutPrefix);
}

TEST(ODBCBQClient, FilterProjectsRMSearch_Failure) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("status:ACTIVE", project_ids, *options);
  EXPECT_THAT(
      projects_status,
      StatusRecordIs(SQLStates::k_42000(), HasSubstr("Invalid filter query")));
}

TEST(ODBCBQClient, ADC_FilterProjectsRMSearch_Failure) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(MakeProjectsConnection(*options));
  std::string path_to_file_with_credentials =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  ASSERT_FALSE(path_to_file_with_credentials.empty());

  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAccount, path_to_file_with_credentials});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::vector<std::string> project_ids = {"app1", kRMProjectWithoutPrefix};

  auto projects_status =
      (*odbc_bq_client)
          ->FilterProjectsRMSearch("status:ACTIVE", project_ids, *options);
  EXPECT_THAT(
      projects_status,
      StatusRecordIs(SQLStates::k_42000(), HasSubstr("Invalid filter query")));
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
