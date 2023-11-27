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
using bigquery_v2_minimal_internal::ListDatasetsRequest;

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(ListDatasets, UserAccountAuth) {
  auto options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == *dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}
#endif // USER_ACCOUNT_AUTH

TEST(ListDatasets, ServiceAccountAuth) {
  auto options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == *dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}

TEST(ListDatasets, ServiceAccountAuthWithClientId) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == *dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}

TEST(ListDatasets, UsingFilter) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);
  request.set_filter("labels.dataset_label_to_filter:dataset_label_value_to_filter");

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == *dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}

TEST(ListDatasets, UsingFilterNoDatasets) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);
  request.set_filter("labels.dataset_label_to_filter:zero_datasets_for_such_filter");

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  EXPECT_EQ(begin, range.end());
}

TEST(ListDatasets, WrongFilter) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);
  request.set_filter("not-valid-filter");

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& dataset : range) {
    EXPECT_THAT(dataset, StatusIs(StatusCode::kInvalidArgument, HasSubstr("Unsupported field")));
  }
}

// Hidden datasets are datasets which starts with underscore.
// More about it https://cloud.google.com/bigquery/docs/datasets#hidden_datasets
TEST(ListDatasets, HiddenDatasets) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);
  request.set_all_datasets(true);

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == *dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}

TEST(ListDatasets, ProjectNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  ListDatasetsRequest request;
  request.set_project_id(std::string(kNameForNonExistingProject));

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& dataset : range) {
    EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Project")));
  }
}

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(ListDatasets, NoAccessAccountAuth) {
  auto options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(*options)));

  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);
  ListDatasetsRequest request;
  request.set_project_id(*project_id);

  auto range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  EXPECT_EQ(begin, range.end());
}
#endif // USER_ACCOUNT_AUTH
} // google
} // cloud
} // odbc_bigquery_v2_tests
