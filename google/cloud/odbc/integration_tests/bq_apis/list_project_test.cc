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

#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
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
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
// We don't use ServiceAccountAuthWithClientIdAuthentication
// It's timing out after 15 minutes because of a big number of available
// projects.
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
#endif  // USER_ACCOUNT_AUTH

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

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(ListAllProjects, NoAccessAccountAuth) {
  StatusOr<Options> options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto project_client =
      ProjectClient(MakeProjectConnection(std::move(*options)));
  ListProjectsRequest request;

  StreamRange<Project> range = project_client.ListProjects(request);

  auto begin = range.begin();
  EXPECT_EQ(begin, range.end());
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
