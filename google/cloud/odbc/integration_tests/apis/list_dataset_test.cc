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

#include <gtest/gtest.h>

#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace odbc_bigquery_v2_tests {

  using google::cloud::internal::GetEnv;
  using bigquery_v2_minimal_internal::DatasetClient;
  using bigquery_v2_minimal_internal::MakeDatasetConnection;
  using bigquery_v2_minimal_internal::ListDatasetsRequest;

  void listAllDatasets(Options options) {
    auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(options)));
    auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
    ASSERT_TRUE(project_id_optional.has_value());
    std::string project_id = project_id_optional.value();
    ListDatasetsRequest request;
    request.set_project_id(project_id);

    auto range = dataset_client.ListDatasets(request);

    auto begin = range.begin();
    ASSERT_NE(begin, range.end());
    for (auto const& dataset : range) {
      ASSERT_STATUS_OK(dataset);
    }
  }

  TEST(ListAllDatasets, UserAccountAuth) {
    std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_ACCOUNT_KEY").value_or("");
    ASSERT_NE(path_to_file_with_credentials, "");
    setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
    auto options = google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials());
    listAllDatasets(options);
  }

  TEST(ListAllDatasets, ServiceAccountAuthWithClientId) {
    std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_ACCOUNT_KEY").value_or("");
    ASSERT_NE(path_to_file_with_credentials, "");
    setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
    auto options = google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials());
    listAllDatasets(options);
  }

    void listDatasetsByFilter(Options options) {
      auto dataset_client = DatasetClient(MakeDatasetConnection(std::move(options)));
      auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
      ASSERT_TRUE(project_id_optional.has_value());
      std::string project_id = project_id_optional.value();
      ListDatasetsRequest request;
      request.set_project_id(project_id);
      request.set_filter("labels.key_label:value_label"); // Such dataset was created on GCP

      auto range = dataset_client.ListDatasets(request);

      auto begin = range.begin();
      ASSERT_NE(begin, range.end());
      for (auto const& dataset : range) {
        ASSERT_STATUS_OK(dataset);
      }
    }

    TEST(ListDatasetsByFilter, UserAccountAuth) {
      std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_ACCOUNT_KEY").value_or("");
      ASSERT_NE(path_to_file_with_credentials, "");
      setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
      auto options = google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeGoogleDefaultCredentials());
      listDatasetsByFilter(options);
    }
} // google
} // cloud
} // odbc_bigquery_v2_tests
