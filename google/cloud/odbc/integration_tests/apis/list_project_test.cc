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

#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/options.h"
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace odbc_bigquery_v2_tests {

    using google::cloud::internal::GetEnv;
    using bigquery_v2_minimal_internal::ProjectClient;
    using bigquery_v2_minimal_internal::MakeProjectConnection;
    using bigquery_v2_minimal_internal::ListProjectsRequest;

    void listAllProjects(Options options) {
      auto project_client = ProjectClient(MakeProjectConnection(std::move(options)));
      ListProjectsRequest request;

      auto range = project_client.ListProjects(request);

      auto begin = range.begin();
      ASSERT_NE(begin, range.end());
      for (auto const& project : range) {
        ASSERT_STATUS_OK(project);
      }
    }

    // Using only this account here as it has access to only one project.
    // ServiceAccountAuthWithClientId account timing out after 15 minutes because of a big number of available projects.
    TEST(ListAllProjects, UserAccountAuth) {
      std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_ACCOUNT_KEY").value_or("");
      ASSERT_NE(path_to_file_with_credentials, "");
      setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
      auto options = google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeGoogleDefaultCredentials());
      listAllProjects(options);
    }
}
}
}
