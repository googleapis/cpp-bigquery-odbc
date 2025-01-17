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
#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/internal/getenv.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::ListProjectsRequest;
using bigquery_v2_minimal_internal::MakeProjectConnection;
using bigquery_v2_minimal_internal::Project;
using bigquery_v2_minimal_internal::ProjectClient;
using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_client_library_utils::
    CreateApplicationDefaultAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateNoAccessAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateUserAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateWrongAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateWrongPathToAuthFileAuthentication;
using google::cloud::odbc_testing_utils::GetRequiredEnvVar;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef USER_ACCOUNT_AUTH
TEST(ListAllProjects, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));
  ListProjectsRequest request;

  StreamRange<Project> range = project_client.ListProjects(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ODBCBQClient_ListAllProjects, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));

  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY");

  // List projects via ODBC BQ Client
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kServiceAndUserAccount;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<Project>> projects_response =
      (*odbc_bq_client)->ListAllProjects(std::move(*options));
  ASSERT_STATUS_RECORD_OK(projects_response);
  ASSERT_FALSE((*projects_response).empty());
}

#else   // USER_ACCOUNT_AUTH

TEST(ListAllProjects, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));
  ListProjectsRequest request;

  StreamRange<Project> range = project_client.ListProjects(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ListAllProjects, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));
  ListProjectsRequest request;

  StreamRange<Project> range = project_client.ListProjects(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    ASSERT_STATUS_OK(project);
  }
}

TEST(ODBCBQClient_ListAllProjects, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));

  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kApplicationDefault});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<Project>> projects_response =
      (*odbc_bq_client)->ListAllProjects(std::move(*options));
  ASSERT_STATUS_RECORD_OK(projects_response);
  ASSERT_FALSE((*projects_response).empty());
}

TEST(ListAllProjects, WrongPathToAuthFile) {
  StatusOr<Options> options = CreateWrongPathToAuthFileAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));
  ListProjectsRequest request;

  StreamRange<Project> range = project_client.ListProjects(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project, StatusIs(StatusCode::kUnknown,
                                  HasSubstr("Cannot open credentials file")));
  }
}

TEST(ListAllProjects, WrongAuthntication) {
  StatusOr<Options> options = CreateWrongAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));
  ListProjectsRequest request;

  StreamRange<Project> range = project_client.ListProjects(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& project : range) {
    EXPECT_THAT(project, StatusIs(StatusCode::kUnauthenticated,
                                  HasSubstr("The OAuth client was not found")));
  }
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
