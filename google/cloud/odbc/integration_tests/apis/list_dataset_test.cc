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

#include <stdio.h>
#include <iostream>
#include "google/cloud/bigquery/v2/minimal/internal/dataset_request.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_options.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_connection.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_rest_stub.h"

#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/example_driver.h"
#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"

#include <gmock/gmock.h>

#include <fstream>
#include <iostream>

namespace google {
namespace cloud {
namespace bigquery_v2_minimal_internal {

void ExplicitADCs(std::vector<std::string> const& argv) {
  std::cout << "Running: " << std::endl;
  using ::google::cloud::internal::GetEnv;
  if (argv.size() == 1 && argv[0] == "--help") {
    throw google::cloud::testing_util::Usage{
        "explicit-adcs"};
  }
  //! [explicit-adcs]
  auto options =
      google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeGoogleDefaultCredentials());
  auto dataset_client = DatasetClient(MakeDatasetConnection(options));

  std::cout << "Creating request: " << std::endl;
  ListDatasetsRequest request;
  std::string project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT").value_or("");
  request.set_project_id(project_id);
  std::cout << "Project ID: " << project_id << std::endl;
  std::cout << "Before request: " << std::endl;
  auto range = dataset_client.ListDatasets(request);
  std::cout << "Response is achieved: " << std::endl;
  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  std::vector<std::string> actual_dataset_ids;
  for (auto const& dataset : range) {
    ASSERT_STATUS_OK(dataset);
    actual_dataset_ids.push_back(dataset->id);
  }
  for (auto dataset_id: actual_dataset_ids) {
    std::cout << "Dataset: " << dataset_id << std::endl;
  }
}

void WithServiceAccount(std::vector<std::string> const& argv) {
  using ::google::cloud::internal::GetEnv;
  if (argv.size() != 1 || argv[0] == "--help") {
    throw google::cloud::testing_util::Usage{"with-service-account <keyfile>"};
  }
  //! [with-service-account]
  [](std::string const& keyfile) {
    auto is = std::ifstream(keyfile);
    is.exceptions(std::ios::badbit);  // Minimal error handling in examples
    auto contents = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
    auto options =
        google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
            google::cloud::MakeServiceAccountCredentials(contents));
    auto dataset_client = DatasetClient(MakeDatasetConnection(options));
    ListDatasetsRequest request;
    std::string project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT").value_or("");
    request.set_project_id(project_id);
    auto range = dataset_client.ListDatasets(request);
    auto begin = range.begin();
    ASSERT_NE(begin, range.end());
    std::vector<std::string> actual_dataset_ids;
    for (auto const& dataset : range) {
      ASSERT_STATUS_OK(dataset);
      actual_dataset_ids.push_back(dataset->id);
    }
  }
  //! [with-service-account]
  (argv.at(0));
}

}  // namespace bigquery_v2_minimal_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  std::cout << "START: " << std::endl;
  google::cloud::testing_util::Example example({
      {"explicit-adcs", google::cloud::bigquery_v2_minimal_internal::ExplicitADCs},
      {"with-service-account", google::cloud::bigquery_v2_minimal_internal::WithServiceAccount},
  });
  return example.Run(argc, argv);
}
