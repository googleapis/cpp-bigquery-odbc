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
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::Dataset;
using bigquery_v2_minimal_internal::DatasetClient;
using bigquery_v2_minimal_internal::GetDatasetRequest;
using bigquery_v2_minimal_internal::MakeDatasetConnection;
using google::cloud::odbc_integration_tests_testing_util::
    CreateNoAccessAccountAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    CreateUserAccountAuthentication;
using google::cloud::odbc_integration_tests_testing_util::
    kNameForNonExistingProject;
using google::cloud::odbc_testing_utils::GetRequiredEnvVar;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(GetDataset, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}
#endif  // USER_ACCOUNT_AUTH

TEST(GetDataset, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}

TEST(GetDataset, ServiceAccountAuthWithClientId) {
  StatusOr<Options> options =
      CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}

TEST(GetDataset, DatasetNotExist) {
  StatusOr<Options> options =
      CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("Non_existing_dataset");

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound, HasSubstr("Not found")));
}

TEST(GetDataset, ProjectNotExist) {
  StatusOr<Options> options =
      CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string project_id = std::string(kNameForNonExistingProject);

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset,
              StatusIs(StatusCode::kNotFound,
                       HasSubstr("Project " + project_id + " is not found")));
}

TEST(GetDataset, ProjectIdIsEmpty) {
  StatusOr<Options> options =
      CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id("");
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  // BQ API error
  EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound,
                                HasSubstr("Request couldn't be served")));
}

TEST(GetDataset, DatasetIdIsEmpty) {
  StatusOr<Options> options =
      CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("");

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  // BQ API error
  EXPECT_THAT(dataset, StatusIs(StatusCode::kInternal,
                                HasSubstr("Not a valid Json Dataset object")));
}

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(GetDataset, NoAccessAccountAuth) {
  StatusOr<Options> options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StatusOr<Dataset> dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset, StatusIs(StatusCode::kPermissionDenied,
                                HasSubstr("Access Denied: Dataset")));
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
