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
#include "google/cloud/options.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/authentication.h"
#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace odbc_bigquery_v2_tests {

  using google::cloud::internal::GetEnv;
  using google::cloud::odbc_testing_util_internal::CreateUserAccountAuthentication;
  using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthWithClientIdAuthentication;
  using ::testing::HasSubstr;
  using bigquery_v2_minimal_internal::DatasetClient;
  using bigquery_v2_minimal_internal::MakeDatasetConnection;
  using bigquery_v2_minimal_internal::GetDatasetRequest;

  void getDataset(Options options) {
    auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(options)));
    auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
    auto dataset_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
    ASSERT_TRUE(project_id_optional.has_value());
    ASSERT_TRUE(dataset_id_optional.has_value());
    GetDatasetRequest request;
    request.set_project_id(project_id_optional.value());
    request.set_dataset_id(dataset_id_optional.value());

    auto dataset = dataset_client.GetDataset(request);

    ASSERT_STATUS_OK(dataset);
  }

  TEST(GetDataset, UserAccountAuth) {
    auto options = CreateUserAccountAuthentication();
    ASSERT_STATUS_OK(options);
    getDataset(options.value());
  }

  TEST(GetDataset, ServiceAccountAuthWithClientId) {
    auto options = CreateServiceAccountAuthWithClientIdAuthentication();
    ASSERT_STATUS_OK(options);
    getDataset(options.value());
  }

  TEST(GetNonExistingDataset, UserAccountAuth) {
    auto options = CreateServiceAccountAuthWithClientIdAuthentication();
    ASSERT_STATUS_OK(options);
    auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(options.value())));

    auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
    ASSERT_TRUE(project_id_optional.has_value());
    GetDatasetRequest request;
    request.set_project_id(project_id_optional.value());
    request.set_dataset_id("Non_existing_dataset");

    auto dataset = dataset_client.GetDataset(request);


    ASSERT_STATUS_NOT_OK(dataset);
    EXPECT_THAT(dataset.status().message(), HasSubstr("Not found"));
    EXPECT_EQ(dataset.status().code(), StatusCode::kNotFound);
  }
}
}
}
