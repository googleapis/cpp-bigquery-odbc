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
#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::ListFormatTable;
using bigquery_v2_minimal_internal::ListTablesRequest;
using bigquery_v2_minimal_internal::MakeTableConnection;
using bigquery_v2_minimal_internal::TableClient;
using google::cloud::odbc_bigquery_client_interface::ConnProps;
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

TEST(ListAllTables, ExternalAccountAuth_JSONFile) {
  StatusOr<Options> options = CreateExternalAuthenticationJSONFile();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_EXTERNAL_ACCOUNT_AUTH_KEY");

  // Retrieving tables via ODBCBQClient.
  ConnProps conn_props;
  conn_props.auth_mechanism = OauthMechanism::kExternalUser;
  conn_props.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(conn_props);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<ListFormatTable> > tables_response =
      (*odbc_bq_client)
          ->ListAllTables(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(tables_response);

  std::vector<ListFormatTable> tables = (*tables_response);

  ASSERT_FALSE(tables.empty());
  bool expected_table = false;
  for (auto const& table : tables) {
    expected_table = (table.table_reference.project_id == project_id &&
                      table.table_reference.dataset_id == dataset_id);
    if (!expected_table) break;
  }
  ASSERT_TRUE(expected_table);
}

TEST(ListAllTables, ExternalAccountAuth_BYOID_Workload) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkload();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  // Retrieving tables via ODBCBQClient.
  ConnProps conn_props = CreateExternalUserOauthBYOIDWorkload();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(conn_props);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<ListFormatTable> > tables_response =
      (*odbc_bq_client)
          ->ListAllTables(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(tables_response);

  std::vector<ListFormatTable> tables = (*tables_response);

  ASSERT_FALSE(tables.empty());
  bool expected_table = false;
  for (auto const& table : tables) {
    expected_table = (table.table_reference.project_id == project_id &&
                      table.table_reference.dataset_id == dataset_id);
    if (!expected_table) break;
  }
  ASSERT_TRUE(expected_table);
}

TEST(ListAllTables, ExternalAccountAuth_BYOID_Workforce) {
  StatusOr<Options> options = CreateExternalAuthenticationBYOIDWorkforce();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  // Retrieving tables via ODBCBQClient.
  ConnProps conn_props = CreateExternalUserOauthBYOIDWorkforce();
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(conn_props);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<ListFormatTable> > tables_response =
      (*odbc_bq_client)
          ->ListAllTables(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(tables_response);

  std::vector<ListFormatTable> tables = (*tables_response);

  ASSERT_FALSE(tables.empty());
  bool expected_table = false;
  for (auto const& table : tables) {
    expected_table = (table.table_reference.project_id == project_id &&
                      table.table_reference.dataset_id == dataset_id);
    if (!expected_table) break;
  }
  ASSERT_TRUE(expected_table);
}
#endif  // EXTERNAL_ACCOUNT_AUTH

#ifdef USER_ACCOUNT_AUTH
TEST(ListAllTables, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StreamRange<ListFormatTable> range = table_client.ListTables(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& table : range) {
    ASSERT_STATUS_OK(table);
  }
}

TEST(ODBCBQClient_ListAllTables, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY");

  // Retrieving tables via ODBCBQClient.
  ConnProps conn_props;
  conn_props.auth_mechanism = OauthMechanism::kServiceAccount;
  conn_props.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(conn_props);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<ListFormatTable> > tables_response =
      (*odbc_bq_client)
          ->ListAllTables(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(tables_response);

  std::vector<ListFormatTable> tables = (*tables_response);

  ASSERT_FALSE(tables.empty());
  bool expected_table = false;
  for (auto const& table : tables) {
    expected_table = (table.table_reference.project_id == project_id &&
                      table.table_reference.dataset_id == dataset_id);
    if (!expected_table) break;
  }
  ASSERT_TRUE(expected_table);
}

#else  // USER_ACCOUNT_AUTH

TEST(ListAllTables, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StreamRange<ListFormatTable> range = table_client.ListTables(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& table : range) {
    ASSERT_STATUS_OK(table);
  }
}

TEST(ListAllTables, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StreamRange<ListFormatTable> range = table_client.ListTables(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& table : range) {
    ASSERT_STATUS_OK(table);
  }
}

TEST(ODBCBQClient_ListAllTables, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kApplicationDefault});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  StatusRecordOr<std::vector<ListFormatTable> > tables_response =
      (*odbc_bq_client)
          ->ListAllTables(project_id, dataset_id, std::move(*options));
  ASSERT_STATUS_RECORD_OK(tables_response);

  std::vector<ListFormatTable> tables = (*tables_response);

  ASSERT_FALSE(tables.empty());
  bool expected_table = false;
  for (auto const& table : tables) {
    expected_table = (table.table_reference.project_id == project_id &&
                      table.table_reference.dataset_id == dataset_id);
    if (!expected_table) break;
  }
  ASSERT_TRUE(expected_table);
}

TEST(ListAllTables, DatasetNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("Non_existing_dataset");

  StreamRange<ListFormatTable> range = table_client.ListTables(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& table : range) {
    EXPECT_THAT(table, StatusIs(StatusCode::kNotFound,
                                HasSubstr("Not found: Dataset")));
  }
}

TEST(ListAllTables, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));

  std::string project_id = std::string(kNameForNonExistingProject);
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StreamRange<ListFormatTable> range = table_client.ListTables(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& table : range) {
    EXPECT_THAT(table,
                StatusIs(StatusCode::kNotFound,
                         HasSubstr("Project " + project_id + " is not found")));
  }
}

TEST(ListAllTables, ProjectIdIEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  ListTablesRequest request;
  request.set_project_id("");
  request.set_dataset_id(dataset_id);

  StreamRange<ListFormatTable> range = table_client.ListTables(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& table : range) {
    EXPECT_THAT(table, StatusIs(StatusCode::kNotFound,
                                HasSubstr("Request couldn't be served")));
  }
}

TEST(ListAllTables, DatasetIdIEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto table_client = TableClient(MakeTableConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id("");

  StreamRange<ListFormatTable> range = table_client.ListTables(request);

  auto begin = range.begin();
  ASSERT_NE(begin, range.end());
  for (auto const& table : range) {
    EXPECT_THAT(table, StatusIs(StatusCode::kNotFound,
                                HasSubstr("Request couldn't be served")));
  }
}

#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
