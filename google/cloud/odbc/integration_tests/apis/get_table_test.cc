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
using google::cloud::odbc_testing_util_internal::StatusIs;
using google::cloud::odbc_testing_util_internal::CreateUserAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_util_internal::CreateNoAccessAccountAuthentication;
using ::testing::HasSubstr;
using bigquery_v2_minimal_internal::TableClient;
using bigquery_v2_minimal_internal::MakeTableConnection;
using bigquery_v2_minimal_internal::GetTableRequest;
using bigquery_v2_minimal_internal::TableMetadataView;

TEST(GetTable, UserAccountAuth) {
  auto options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));
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

TEST(GetTable, ServiceAccountAuth) {
  auto options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));
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

TEST(GetTable, ServiceAccountAuthWithClientId) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));
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

TEST(GetTable, TableNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));

  auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id_optional.has_value());
  ASSERT_TRUE(dataset_id_optional.has_value());
  GetTableRequest request;
  request.set_project_id(project_id_optional.value());
  request.set_dataset_id(dataset_id_optional.value());
  request.set_table_id("Non_existing_table");

  auto table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Table")));
}

TEST(GetTable, DatasetNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));

  auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto table_name_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id_optional.has_value());
  ASSERT_TRUE(table_name_optional.has_value());
  GetTableRequest request;
  request.set_project_id(project_id_optional.value());
  request.set_dataset_id("Non_existing_dataset");
  request.set_table_id(table_name_optional.value());

  auto table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Dataset")));
}

TEST(GetTable, ProjectNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));

  auto dataset_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(dataset_id_optional.has_value());
  ASSERT_TRUE(table_name_optional.has_value());
  std::string project_id = "Non-existing-project";
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id_optional.value());
  request.set_table_id(table_name_optional.value());

  auto table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kInvalidArgument,
    HasSubstr("Invalid resource name projects/" + project_id + "; Project id")));
}

TEST(GetTable, SelectedFields) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));

  auto project_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name_optional = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id_optional.has_value());
  ASSERT_TRUE(dataset_id_optional.has_value());
  ASSERT_TRUE(table_name_optional.has_value());
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE");
  ASSERT_TRUE(column_name.has_value());

  GetTableRequest request;
  request.set_project_id(project_id_optional.value());
  request.set_dataset_id(dataset_id_optional.value());
  request.set_table_id(table_name_optional.value());
  request.set_selected_fields({column_name.value()});

  auto table = table_client.GetTable(request);

  ASSERT_STATUS_OK(table);
  EXPECT_EQ(table.value().schema.fields.size(), 1);
  EXPECT_EQ(table.value().schema.fields[0].name, column_name.value());
}

TEST(GetTable, SelectedFieldsNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));

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
  request.set_selected_fields({"not_existing-field"});

  auto table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kInvalidArgument, HasSubstr("Selected non-existent field")));
}

TEST(GetTable, SetView) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));

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
  request.set_view(TableMetadataView::Basic());

  auto table = table_client.GetTable(request);

  ASSERT_STATUS_OK(table);
  EXPECT_EQ(table.value().num_bytes, -1);
}

TEST(GetTable, NoAccessAccountAuth) {
  auto options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(options.value())));
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

  EXPECT_THAT(table, StatusIs(StatusCode::kPermissionDenied, HasSubstr("Access Denied: Table")));
}
}
}
}
