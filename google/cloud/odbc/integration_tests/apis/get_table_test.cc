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
#include "google/cloud/credentials.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"

namespace google {
namespace cloud {
namespace odbc_bigquery_v2_tests {

  using google::cloud::internal::GetEnv;
  using ::testing::HasSubstr;
  using bigquery_v2_minimal_internal::TableClient;
  using bigquery_v2_minimal_internal::MakeTableConnection;
  using bigquery_v2_minimal_internal::GetTableRequest;

  void getTable(Options options) {
    auto table_client = TableClient(MakeTableConnection(std::move(options)));
    auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
    auto dataset_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
    auto table_name_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
    ASSERT_TRUE(project_id_optional.has_value());
    ASSERT_TRUE(dataset_id_optional.has_value());
    ASSERT_TRUE(table_name_optional.has_value());
    GetTableRequest request;
    request.set_project_id(project_id_optional.value());
    request.set_dataset_id(dataset_id_optional.value());
    request.set_table_id(table_name_optional.value());

    auto table = table_client.GetTable(request);

    ASSERT_STATUS_OK(table);
  }

  TEST(GetTable, UserAccountAuth) {
    std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_ACCOUNT_KEY").value_or("");
    ASSERT_NE(path_to_file_with_credentials, "");
    setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
    auto options = google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials());
    getTable(options);
  }

  TEST(GetTable, ServiceAccountAuthWithClientId) {
    std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_CLIENT_ID_ACCOUNT_KEY").value_or("");
    ASSERT_NE(path_to_file_with_credentials, "");
    setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
    auto options = google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials());
    getTable(options);
  }

  TEST(GetTable, WrongTableName) {
    std::string path_to_file_with_credentials = GetEnv("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_ACCOUNT_KEY").value_or("");
    ASSERT_NE(path_to_file_with_credentials, "");
    setenv("GOOGLE_APPLICATION_CREDENTIALS", path_to_file_with_credentials.c_str(), 1);
    auto options = google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
        google::cloud::MakeGoogleDefaultCredentials());
    auto table_client = TableClient(MakeTableConnection(std::move(options)));
    auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
    auto dataset_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
    ASSERT_TRUE(project_id_optional.has_value());
    ASSERT_TRUE(dataset_id_optional.has_value());
    GetTableRequest request;
    request.set_project_id(project_id_optional.value());
    request.set_dataset_id(dataset_id_optional.value());
    request.set_table_id("Non_existing_table");

    auto table = table_client.GetTable(request);

    ASSERT_STATUS_NOT_OK(table);
    EXPECT_THAT(table.status().message(), HasSubstr("Not found: Table"));
    EXPECT_EQ(table.status().code(), StatusCode::kNotFound);
  }
}
}
}
