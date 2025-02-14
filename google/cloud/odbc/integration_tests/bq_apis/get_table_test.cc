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

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_client_interface/tables.h"
#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::GetTableRequest;
using bigquery_v2_minimal_internal::MakeTableConnection;
using bigquery_v2_minimal_internal::Table;
using bigquery_v2_minimal_internal::TableClient;
using bigquery_v2_minimal_internal::TableMetadataView;
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bigquery_client_interface::TableFilter;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_client_library_utils::
    CreateApplicationDefaultAuthentication;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationBYOIDWorkforce;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationBYOIDWorkload;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalAuthenticationJSONFile;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalUserOauthBYOIDWorkforce;
using google::cloud::odbc_testing_client_library_utils::
    CreateExternalUserOauthBYOIDWorkload;
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

#ifdef EXTERNAL_ACCOUNT_AUTH
TEST(GetTable, ExternalAccountAuth_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY");

  // Retrieving table via ODBCBQClient.
  TableFilter filter{{}, TableMetadataView::Full()};
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kExternalUser;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Table> table_response =
      (*odbc_bq_client)
          ->GetTable(project_id, dataset_id, table_name, filter,
                     std::move(*options));
  ASSERT_STATUS_RECORD_OK(table_response);

  Table table = *table_response;
  EXPECT_EQ(table.table_reference.project_id, project_id);
  EXPECT_EQ(table.table_reference.dataset_id, dataset_id);
  EXPECT_EQ(table.table_reference.table_id, table_name);
}

TEST(GetTable, ExternalAccountAuth_BYOID_Workload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  // Retrieving table via ODBCBQClient.
  TableFilter filter{{}, TableMetadataView::Full()};
  Oauth oauth = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Table> table_response =
      (*odbc_bq_client)
          ->GetTable(project_id, dataset_id, table_name, filter,
                     std::move(*options));
  ASSERT_STATUS_RECORD_OK(table_response);

  Table table = *table_response;
  EXPECT_EQ(table.table_reference.project_id, project_id);
  EXPECT_EQ(table.table_reference.dataset_id, dataset_id);
  EXPECT_EQ(table.table_reference.table_id, table_name);
}

TEST(GetTable, ExternalAccountAuth_BYOID_Workforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  // Retrieving table via ODBCBQClient.
  TableFilter filter{{}, TableMetadataView::Full()};
  Oauth oauth = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Table> table_response =
      (*odbc_bq_client)
          ->GetTable(project_id, dataset_id, table_name, filter,
                     std::move(*options));
  ASSERT_STATUS_RECORD_OK(table_response);

  Table table = *table_response;
  EXPECT_EQ(table.table_reference.project_id, project_id);
  EXPECT_EQ(table.table_reference.dataset_id, dataset_id);
  EXPECT_EQ(table.table_reference.table_id, table_name);
}
#endif  // EXTERNAL_ACCOUNT_AUTH

#ifdef USER_ACCOUNT_AUTH
TEST(GetTable, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);

  StatusOr<Table> table = table_client.GetTable(request);

  ASSERT_STATUS_OK(table);
}

TEST(ODBCBQClient_GetTable, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY");

  // Retrieving table via ODBCBQClient.
  TableFilter filter{{}, TableMetadataView::Full()};
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kServiceAndUserAccount;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Table> table_response =
      (*odbc_bq_client)
          ->GetTable(project_id, dataset_id, table_name, filter,
                     std::move(*options));
  ASSERT_STATUS_RECORD_OK(table_response);

  Table table = *table_response;
  EXPECT_EQ(table.table_reference.project_id, project_id);
  EXPECT_EQ(table.table_reference.dataset_id, dataset_id);
  EXPECT_EQ(table.table_reference.table_id, table_name);
}

#else  // USER_ACCOUNT_AUTH

TEST(GetTable, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);

  StatusOr<Table> table = table_client.GetTable(request);

  ASSERT_STATUS_OK(table);
}

TEST(GetTable, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);

  StatusOr<Table> table = table_client.GetTable(request);

  ASSERT_STATUS_OK(table);
}

TEST(ODBCBQClient_GetTable, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  TableFilter filter{{}, TableMetadataView::Full()};
  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kApplicationDefault});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<Table> table_response =
      (*odbc_bq_client)
          ->GetTable(project_id, dataset_id, table_name, filter,
                     std::move(*options));
  ASSERT_STATUS_RECORD_OK(table_response);

  Table table = *table_response;
  EXPECT_EQ(table.table_reference.project_id, project_id);
  EXPECT_EQ(table.table_reference.dataset_id, dataset_id);
  EXPECT_EQ(table.table_reference.table_id, table_name);
}

TEST(GetTable, TableNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id("Non_existing_table");

  StatusOr<Table> table = table_client.GetTable(request);

  EXPECT_THAT(table,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Table")));
}

TEST(GetTable, DatasetNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("Non_existing_dataset");
  request.set_table_id(table_name);

  StatusOr<Table> table = table_client.GetTable(request);

  EXPECT_THAT(table,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Dataset")));
}

TEST(GetTable, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));

  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string project_id = std::string(kNameForNonExistingProject);
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);

  StatusOr<Table> table = table_client.GetTable(request);

  EXPECT_THAT(table,
              StatusIs(StatusCode::kNotFound,
                       HasSubstr("Project " + project_id + " is not found")));
}

TEST(GetTable, SelectedFields) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE");

  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);
  request.set_selected_fields({column_name});

  StatusOr<Table> table = table_client.GetTable(request);

  ASSERT_STATUS_OK(table);
  EXPECT_EQ(table.value().schema.fields.size(), 1);
  EXPECT_EQ(table.value().schema.fields[0].name, column_name);
}

TEST(GetTable, SelectedFieldsNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);
  request.set_selected_fields({"not_existing-field"});

  StatusOr<Table> table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kInvalidArgument,
                              HasSubstr("Selected non-existent field")));
}

TEST(GetTable, SetView) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));

  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);
  request.set_view(TableMetadataView::Basic());

  StatusOr<Table> table = table_client.GetTable(request);

  ASSERT_STATUS_OK(table);
  EXPECT_EQ(table.value().num_bytes, -1);
}

TEST(GetTable, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  GetTableRequest request;
  request.set_project_id("");
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_name);

  StatusOr<Table> table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kNotFound,
                              HasSubstr("Request couldn't be served")));
}

TEST(GetTable, DatasetIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("");
  request.set_table_id(table_name);

  StatusOr<Table> table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kNotFound,
                              HasSubstr("Request couldn't be served")));
}

TEST(GetTable, TableIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id("");

  StatusOr<Table> table = table_client.GetTable(request);

  EXPECT_THAT(table, StatusIs(StatusCode::kInternal,
                              HasSubstr("Not a valid Json Table object")));
}

#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
