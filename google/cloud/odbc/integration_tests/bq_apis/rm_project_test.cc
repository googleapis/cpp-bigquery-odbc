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
#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/resourcemanager/v3/projects_client.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_utils::StatusIs;
using google::cloud::resourcemanager::v3::Project;
using ::google::cloud::resourcemanager_v3::MakeProjectsConnection;
using ::google::cloud::resourcemanager_v3::ProjectsClient;
using ::testing::HasSubstr;

std::string const kRMProjectWithoutPrefix = "bigquery-devtools-drivers";
std::string const kRMProjectWithPrefix = "projects/" + kRMProjectWithoutPrefix;

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

TEST(ResourceManagerGetProject, Failure_InvalidArgument) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto projects_client = ProjectsClient(
      ::google::cloud::resourcemanager_v3::MakeProjectsConnection(*options));

  StatusOr<Project> project =
      projects_client.GetProject(kRMProjectWithoutPrefix, *options);

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

}  // namespace google::cloud::odbc_integration_tests_apis
