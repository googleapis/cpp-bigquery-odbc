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

#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "absl/strings/str_cat.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::GetQueryResults;
using bigquery_v2_minimal_internal::GetQueryResultsRequest;
using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using bigquery_v2_minimal_internal::PostQueryRequest;
using bigquery_v2_minimal_internal::PostQueryResults;
using bigquery_v2_minimal_internal::QueryParameter;
using bigquery_v2_minimal_internal::QueryRequest;
using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_client_library_utils::
    CreateApplicationDefaultAuthentication;
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

static std::vector<std::string> const kKeysToFilter{
    "preserveNulls", "defaultDataset", "maximumBytesBilled"};

#ifdef USER_ACCOUNT_AUTH
TEST(Query, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows,
            query_response.value().total_rows);
}

TEST(ODBCBQClient_Query, UserAccountAuth) {
  StatusOr<Options> options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  std::string path_to_file_with_credentials =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_USER_ACCOUNT_AUTH_KEY");

  // Query via ODBCBQClient
  Oauth oauth;
  oauth.auth_mechanism = OauthMechanism::kServiceAndUserAccount;
  oauth.credentials_file_path = path_to_file_with_credentials;
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(oauth);
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusRecordOr<PostQueryResults> query_response =
      (*odbc_bq_client)->Query(project_id, query_request, *options);

  ASSERT_STATUS_RECORD_OK(query_response);
  EXPECT_TRUE((*query_response).job_complete);
  EXPECT_EQ((*query_response).schema.fields.size(), 1);
  EXPECT_GT((*query_response).total_rows, 1);

  // Getting results of previous Query
  std::string job_id = (*query_response).job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusRecordOr<GetQueryResults> query_results_response =
      (*odbc_bq_client)
          ->GetAllQueryResults(project_id, job_id, "",
                               std::chrono::milliseconds(1000), *options);

  ASSERT_STATUS_RECORD_OK(query_results_response);
  EXPECT_TRUE((*query_results_response).job_complete);
  EXPECT_EQ((*query_results_response).total_rows, (*query_response).total_rows);
}
#else   // USER_ACCOUNT_AUTH

TEST(Query, ServiceAccountAuth) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows,
            query_response.value().total_rows);
}

TEST(Query, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows,
            query_response.value().total_rows);
}

TEST(ODBCBQClient_Query, ApplicationDefaultCredentials) {
  StatusOr<Options> options = CreateApplicationDefaultAuthentication();
  ASSERT_STATUS_OK(options);
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);

  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kApplicationDefault});
  ASSERT_STATUS_RECORD_OK(odbc_bq_client);

  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusRecordOr<PostQueryResults> query_response =
      (*odbc_bq_client)->Query(project_id, query_request, *options);

  ASSERT_STATUS_RECORD_OK(query_response);
  EXPECT_TRUE((*query_response).job_complete);
  EXPECT_EQ((*query_response).schema.fields.size(), 1);
  EXPECT_GT((*query_response).total_rows, 1);

  // Getting results of previous Query
  std::string job_id = (*query_response).job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusRecordOr<GetQueryResults> query_results_response =
      (*odbc_bq_client)
          ->GetAllQueryResults(project_id, job_id, "",
                               std::chrono::milliseconds(1000), *options);

  ASSERT_STATUS_RECORD_OK(query_results_response);
  EXPECT_TRUE((*query_results_response).job_complete);
  EXPECT_EQ((*query_results_response).total_rows, (*query_response).total_rows);
}

TEST(Query, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(std::string(kNameForNonExistingProject));
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  EXPECT_THAT(query_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Error in non-idempotent operation: Cannot "
                                 "parse  as CloudRegion")));
}

TEST(Query, DatasetNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  std::string full_table_name =
      absl::StrCat("Not_existing_dataset.", table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  EXPECT_THAT(query_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Dataset")));
}

TEST(Query, TableNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");

  std::string table_name = absl::StrCat(dataset_id, ".Not_existing_table");
  std::string query_statement = absl::StrCat("SELECT * FROM ", table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  EXPECT_THAT(query_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Table")));
}

TEST(Query, ColumnNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT not_existing_column FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  EXPECT_THAT(query_response, StatusIs(StatusCode::kInvalidArgument,
                                       HasSubstr("Unrecognized name")));
}

TEST(Query, SelectZeroRows) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT * FROM ", full_table_name, " WHERE 1 = 2");
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().total_rows, 0);
}

TEST(Query, PageTokens) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  query_request.set_max_results(1);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_FALSE(query_response.value().page_token.empty());
  EXPECT_GT(query_response.value().total_rows, 1);
  EXPECT_EQ(query_response.value().rows.size(), 1);

  // Getting the rest results from previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_page_token(query_response.value().page_token);
  get_query_results_request.set_max_results(1);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().rows.size(), 1);
  EXPECT_FALSE(
      query_results_response.value()
          .page_token
          .empty());  // There are more results, skipping it for this test
}

TEST(QueryResukts, JobNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");

  std::string job_id = "Not_existing_job";
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(QueryResults, LocationNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_location("Not_existing_location");

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Invalid value for location")));
}

TEST(QueryResults, WrongLocation) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_location("asia-south2");

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(Query, WithQueryParameters) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name, " WHERE ",
                   column_name, " > @min_age");
  QueryRequest query_request;
  query_request.set_query(query_statement);
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  query_request.set_query_parameters({query_parameter});
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows,
            query_response.value().total_rows);
}

TEST(Query, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id("");
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  EXPECT_THAT(
      query_response,
      StatusIs(StatusCode::kNotFound, HasSubstr("Request couldn't be served")));
}

TEST(QueryResults, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id("");
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(
      query_results_response,
      StatusIs(StatusCode::kNotFound, HasSubstr("Request couldn't be served")));
}

TEST(Query, QueryResultsPagination) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");

  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  std::string query_statement =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys(kKeysToFilter);

  StatusOr<PostQueryResults> query_response =
      job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_max_results(1);

  GetQueryResults query_results_response;

  while (true) {
    StatusOr<GetQueryResults> query_results_response_partial =
        job_client.QueryResults(get_query_results_request);
    ASSERT_STATUS_OK(query_results_response_partial);

    if (query_results_response.rows.empty()) {
      // It's the first response. Copy it.
      query_results_response = *query_results_response_partial;
    } else {
      query_results_response.rows.insert(
          query_results_response.rows.end(),
          query_results_response_partial->rows.begin(),
          query_results_response_partial->rows.end());
    }

    if (query_results_response_partial->page_token.empty()) {
      query_results_response.page_token = "";
      break;
    }
    get_query_results_request.set_page_token(
        query_results_response_partial->page_token);
  }

  EXPECT_TRUE(query_results_response.job_complete);
  EXPECT_EQ(query_results_response.total_rows,
            query_response.value().total_rows);
}
#endif  // USER_ACCOUNT_AUTH

}  // namespace google::cloud::odbc_integration_tests_apis
