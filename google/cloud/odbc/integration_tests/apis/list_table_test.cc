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

#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
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
  using bigquery_v2_minimal_internal::TableClient;
  using bigquery_v2_minimal_internal::MakeTableConnection;
  using bigquery_v2_minimal_internal::ListTablesRequest;

   void listAllTables(Options options) {
    auto table_client = TableClient(MakeTableConnection(std::move(options)));
    auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
    auto dataset_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
    ASSERT_TRUE(project_id_optional.has_value());
    ASSERT_TRUE(dataset_id_optional.has_value());
    ListTablesRequest request;
    request.set_project_id(project_id_optional.value());
    request.set_dataset_id(dataset_id_optional.value());

    auto range = table_client.ListTables(request);

    auto begin = range.begin();
    ASSERT_NE(begin, range.end());
    for (auto const& table : range) {
      ASSERT_STATUS_OK(table);
    }
  }

  TEST(ListAllTables, UserAccountAuth) {
    auto options = CreateUserAccountAuthentication();
    ASSERT_STATUS_OK(options);
    listAllTables(options.value());
  }

  TEST(ListAllTables, ServiceAccountAuthWithClientId) {
    auto options = CreateServiceAccountAuthWithClientIdAuthentication();
    ASSERT_STATUS_OK(options);
    listAllTables(options.value());
  }

  TEST(ListAllTables, DatasetNotExist) {
    auto options = CreateServiceAccountAuthWithClientIdAuthentication();
    ASSERT_STATUS_OK(options);
    auto table_client = TableClient(MakeTableConnection(std::move(options.value())));

    auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
    ASSERT_TRUE(project_id_optional.has_value());
    ListTablesRequest request;
    request.set_project_id(project_id_optional.value());
    request.set_dataset_id("Non_existing_dataset");

    auto range = table_client.ListTables(request);

    auto begin = range.begin();
    ASSERT_NE(begin, range.end());
    for (auto const& table : range) {
      ASSERT_STATUS_NOT_OK(table);
      EXPECT_THAT(table.status().message(), HasSubstr("Not found: Dataset"));
      EXPECT_EQ(table.status().code(), StatusCode::kNotFound);
    }
  }
}
}
}
