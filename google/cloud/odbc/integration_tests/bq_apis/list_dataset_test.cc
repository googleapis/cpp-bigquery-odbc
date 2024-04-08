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

using bigquery_v2_minimal_internal::DatasetClient;
using bigquery_v2_minimal_internal::ListDatasetsRequest;
using bigquery_v2_minimal_internal::ListFormatDataset;
using bigquery_v2_minimal_internal::MakeDatasetConnection;
using google::cloud::odbc_testing_client_library_utils::
    CreateNoAccessAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateUserAccountAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    kNameForNonExistingProject;
using google::cloud::odbc_testing_utils::GetRequiredEnvVar;
using google::cloud::odbc_testing_utils::StatusIs;
using ::testing::HasSubstr;

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(ListDatasets, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListDatasetsRequest request;
  request.set_project_id(project_id);

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}
#endif  // USER_ACCOUNT_AUTH

TEST(ListDatasets, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListDatasetsRequest request;
  request.set_project_id(project_id);

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}

#ifdef USER_ACCOUNT_AUTH  // TODO(b/333011414) Enable tests
TEST(ListDatasets, ServiceAccountAuthWithClientId) {
  StatusOr<Options> options =
      CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListDatasetsRequest request;
  request.set_project_id(project_id);

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}
#endif  // USER_ACCOUNT_AUTH

TEST(ListDatasets, UsingFilter) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_filter(
      "labels.dataset_label_to_filter:dataset_label_value_to_filter");

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}

TEST(ListDatasets, UsingFilterNoDatasets) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_filter(
      "labels.dataset_label_to_filter:zero_datasets_for_such_filter");

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  EXPECT_EQ(begin, range.end());
}

TEST(ListDatasets, WrongFilter) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_filter("not-valid-filter");

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& dataset : range) {
    EXPECT_THAT(dataset, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Unsupported field")));
  }
}

// Hidden datasets are datasets which starts with underscore.
// More about it https://cloud.google.com/bigquery/docs/datasets#hidden_datasets
TEST(ListDatasets, HiddenDatasets) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_all_datasets(true);

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  bool found = false;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    found = dataset.value().dataset_reference.dataset_id == dataset_id;
    if (found) break;
  }
  ASSERT_EQ(found, true);
}

TEST(ListDatasets, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));

  ListDatasetsRequest request;
  request.set_project_id(std::string(kNameForNonExistingProject));

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& dataset : range) {
    EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound,
                                  HasSubstr("Not found: Project")));
  }
}

TEST(ListDatasets, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));

  ListDatasetsRequest request;
  request.set_project_id("");

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& dataset : range) {
    // BQ API error
    EXPECT_THAT(dataset, StatusIs(StatusCode::kNotFound,
                                  HasSubstr("Request couldn't be served")));
  }
}

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(ListDatasets, NoAccessAccountAuth) {
  StatusOr<Options> options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto dataset_client =
      DatasetClient(MakeDatasetConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListDatasetsRequest request;
  request.set_project_id(project_id);

  StreamRange<ListFormatDataset> range = dataset_client.ListDatasets(request);

  auto begin = range.begin();
  EXPECT_EQ(begin, range.end());
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
