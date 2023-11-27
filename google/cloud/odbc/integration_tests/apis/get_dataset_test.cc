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

#include <gmock/gmock.h>

#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/authentication.h"
#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"
#include "google/cloud/odbc/integration_tests/testing_util/util_constants.h"

namespace google {
namespace cloud {
namespace odbc_bigquery_v2_tests {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_testing_util_internal::StatusIs;
using google::cloud::odbc_testing_util_internal::CreateUserAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_util_internal::CreateNoAccessAccountAuthentication;
using google::cloud::odbc_testing_util_internal::kNameForNonExistingProject;
using ::testing::HasSubstr;
using bigquery_v2_minimal_internal::DatasetClient;
using bigquery_v2_minimal_internal::MakeDatasetConnection;
using bigquery_v2_minimal_internal::GetDatasetRequest;

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(GetDataset, UserAccountAuth) {
  auto options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);

  GetDatasetRequest request;
  request.set_project_id(*project_id);
  request.set_dataset_id(*dataset_id);

  auto dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}
#endif // USER_ACCOUNT_AUTH

TEST(GetDataset, ServiceAccountAuth) {
  auto options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);

  GetDatasetRequest request;
  request.set_project_id(*project_id);
  request.set_dataset_id(*dataset_id);

  auto dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}

TEST(GetDataset, ServiceAccountAuthWithClientId) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);

  GetDatasetRequest request;
  request.set_project_id(*project_id);
  request.set_dataset_id(*dataset_id);

  auto dataset = dataset_client.GetDataset(request);

  ASSERT_STATUS_OK(dataset);
}

TEST(GetDataset, DatasetNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);
  GetDatasetRequest request;
  request.set_project_id(*project_id);
  request.set_dataset_id("Non_existing_dataset");

  auto dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound, HasSubstr("Not found")));
}

TEST(GetDataset, ProjectNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(dataset_id);
  std::string project_id = std::string(kNameForNonExistingProject);
  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(*dataset_id);

  auto dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound,
    HasSubstr("Project " + project_id + " is not found")));
}

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(GetDataset, NoAccessAccountAuth) {
  auto options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  GetDatasetRequest request;
  request.set_project_id(*project_id);
  request.set_dataset_id(*dataset_id);

  auto dataset = dataset_client.GetDataset(request);

  EXPECT_THAT(dataset, StatusIs(StatusCode::kPermissionDenied, HasSubstr("Access Denied: Dataset")));
}
#endif // USER_ACCOUNT_AUTH
}
}
}
