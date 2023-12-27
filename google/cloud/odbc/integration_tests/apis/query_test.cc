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
#include "absl/strings/str_cat.h"

#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/integration_tests/testing_util/authentication.h"
#include "google/cloud/odbc/integration_tests/testing_util/util_constants.h"
#include "google/cloud/odbc/testing_util/status_matchers.h"

namespace google::cloud::odbc_bigquery_v2_tests {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_testing_util_internal::StatusIs;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_util_internal::CreateUserAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateNoAccessAccountAuthentication;
using google::cloud::odbc_testing_util_internal::kNameForNonExistingProject;
using ::testing::HasSubstr;
using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using bigquery_v2_minimal_internal::PostQueryRequest;
using bigquery_v2_minimal_internal::QueryRequest;
using bigquery_v2_minimal_internal::GetQueryResultsRequest;
using bigquery_v2_minimal_internal::QueryParameter;

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(Query, UserAccountAuth) {
  auto options = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows, query_response.value().total_rows);
}
#endif // USER_ACCOUNT_AUTH

TEST(Query, ServiceAccountAuth) {
  auto options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows, query_response.value().total_rows);
}

TEST(Query, ServiceAccountAuthWithClientId) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().schema.fields.size(), 1);
  EXPECT_GT(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows, query_response.value().total_rows);
}

TEST(Query, ProjectNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(std::string(kNameForNonExistingProject));
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  EXPECT_THAT(query_response, StatusIs(StatusCode::kInvalidArgument,
    HasSubstr("Error in non-idempotent operation: Cannot parse  as CloudRegion")));
}

TEST(Query, DatasetNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(table_name);

  std::string full_table_name = absl::StrCat("Not_existing_dataset.", *table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  EXPECT_THAT(query_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Dataset")));
}

TEST(Query, TableNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);

  std::string table_name = absl::StrCat(*dataset_id, ".Not_existing_table");
  std::string query_statement = absl::StrCat("SELECT * FROM ", table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  EXPECT_THAT(query_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Table")));
}

TEST(Query, ColumnNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT not_existing_column FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  EXPECT_THAT(query_response, StatusIs(StatusCode::kInvalidArgument, HasSubstr("Unrecognized name")));
}

TEST(Query, SelectZeroRows) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name, " WHERE 1 = 2");
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().total_rows, 0);
}

TEST(Query, PageTokens) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  query_request.set_max_results(1);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_FALSE(query_response.value().page_token.empty());
  EXPECT_GT(query_response.value().total_rows, 1);
  EXPECT_EQ(query_response.value().rows.size(), 1);

  // Getting the rest results from previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_page_token(query_response.value().page_token);
  get_query_results_request.set_max_results(1);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().rows.size(), 1);
  EXPECT_FALSE(query_results_response.value().page_token.empty()); // There are more results, skipping it for this test
}

TEST(QueryResukts, JobNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  ASSERT_TRUE(project_id);

  std::string job_id = "Not_existing_job";
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(QueryResults, LocationNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_location("Not_existing_location");

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kInvalidArgument, HasSubstr("Invalid value for location")));
}

TEST(QueryResults, WrongLocation) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT * FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);
  get_query_results_request.set_location("asia-south2");

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Job")));
}

TEST(Query, WithQueryParameters) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name, " WHERE ", *column_name, " > @min_age");
  QueryRequest query_request;
  query_request.set_query(query_statement);
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  query_request.set_query_parameters({query_parameter});
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  EXPECT_TRUE(query_response.value().job_complete);
  EXPECT_EQ(query_response.value().total_rows, 1);

  // Getting results of previous Query
  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().total_rows, query_response.value().total_rows);
}

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(Query, NoAccessAccountAuth) {
  auto options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  EXPECT_THAT(query_response, StatusIs(StatusCode::kPermissionDenied,
    HasSubstr("User does not have bigquery.jobs.create permission in project")));
}
#endif // USER_ACCOUNT_AUTH

TEST(QueryResults, DifferentAccount) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);

  // Getting results of previous Query with another account
  auto options_with_user_account = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client_with_user_account = JobClient(MakeBigQueryJobConnection(
    std::move(*options_with_user_account)));

  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client_with_user_account.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kPermissionDenied,
    HasSubstr("User does not have permission to access results of another user's job")));
}

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(QueryResults, NoAccessAccountAuth) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);
  auto column_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_NAME");
  ASSERT_TRUE(column_name);

  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  std::string query_statement = absl::StrCat("SELECT ", *column_name, " FROM ", full_table_name);
  QueryRequest query_request;
  query_request.set_query(query_statement);
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(*project_id);
  post_query_request.set_query_request(query_request);
  post_query_request.set_json_filter_keys({"preserveNulls", "labels", "requestId",
                                           "queryParameters", "defaultDataset",
                                           "maximumBytesBilled", "formatOptions",
                                           "connectionProperties"});

  auto query_response = job_client.Query(post_query_request);

  ASSERT_STATUS_OK(query_response);
  EXPECT_TRUE(query_response.value().job_complete);

  // Getting results of previous Query with another account with no access permission
  auto options_with_user_account = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client_with_user_account = JobClient(MakeBigQueryJobConnection(
    std::move(*options_with_user_account)));

  std::string job_id = query_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client_with_user_account.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kPermissionDenied,
    HasSubstr("Permission bigquery.jobs.get denied on job")));
}
#endif // USER_ACCOUNT_AUTH

} // namespace google::cloud::odbc_bigquery_v2_tests
