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
#include "google/cloud/odbc/integration_tests/testing_util/status_matchers.h"
#include "google/cloud/odbc/integration_tests/testing_util/util_constants.h"

namespace google {
namespace cloud {
namespace odbc_bigquery_v2_tests {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_testing_util_internal::StatusIs;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateServiceAccountAuthWithClientIdAuthentication;
using google::cloud::odbc_testing_util_internal::CreateUserAccountAuthentication;
using google::cloud::odbc_testing_util_internal::CreateNoAccessAccountAuthentication;
using google::cloud::odbc_testing_util_internal::kNameForNonExistingProject;
using ::testing::HasSubstr;
using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using bigquery_v2_minimal_internal::InsertJobRequest;
using bigquery_v2_minimal_internal::Job;
using bigquery_v2_minimal_internal::JobConfiguration;
using bigquery_v2_minimal_internal::JobConfigurationQuery;
using bigquery_v2_minimal_internal::GetQueryResultsRequest;
using bigquery_v2_minimal_internal::QueryParameter;

#ifdef USER_ACCOUNT_AUTH // TODO: b/309605217 - Enable once the bug is fixed
TEST(InsertJob, UserAccountAuth) {
  auto options = CreateUserAccountAuthentication();
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
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  job_configuration_query.query = absl::StrCat(
    "SELECT ", *column_name, " FROM ", full_table_name, " WHERE ", *column_name, " > @min_age");
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  job_configuration_query.query_parameters = {query_parameter};
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().schema.fields.size(), 1);
  EXPECT_EQ(query_results_response.value().total_rows, 1);
}
#endif // USER_ACCOUNT_AUTH

TEST(InsertJob, ServiceAccountAuth) {
  auto options = CreateServiceAccountAuthentication();
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
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  job_configuration_query.query = absl::StrCat(
      "SELECT ", *column_name, " FROM ", full_table_name, " WHERE ", *column_name, " > @min_age");
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  job_configuration_query.query_parameters = {query_parameter};
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().schema.fields.size(), 1);
  EXPECT_EQ(query_results_response.value().total_rows, 1);
}

TEST(InsertJob, ServiceAccountAuthWithClientId) {
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
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  job_configuration_query.query = absl::StrCat(
    "SELECT ", *column_name, " FROM ", full_table_name, " WHERE ", *column_name, " > @min_age");
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  job_configuration_query.query_parameters = {query_parameter};
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().schema.fields.size(), 1);
  EXPECT_EQ(query_results_response.value().total_rows, 1);
}

TEST(InsertJob, ProjectNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  job_configuration_query.query = absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(std::string(kNameForNonExistingProject));
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response, StatusIs(StatusCode::kInvalidArgument,
    HasSubstr("Error in non-idempotent operation: ProjectId and DatasetId must be non-empty")));
}

TEST(InsertJob, DatasetNotExist) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(table_name);

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat("Not_existing_dataset.", *table_name);
  job_configuration_query.query = absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_FALSE(job_response.value().status.errors.empty());
  EXPECT_FALSE(job_response.value().status.error_result.message.empty());

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Dataset")));
}

TEST(InsertJob, NoQueryParameters) {
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

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  job_configuration_query.query = absl::StrCat(
    "SELECT ", *column_name, " FROM ", full_table_name, " WHERE ", *column_name, " > @min_age");
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_FALSE(job_response.value().status.errors.empty());
  EXPECT_FALSE(job_response.value().status.error_result.message.empty());

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kInvalidArgument,
    HasSubstr("Query parameter 'min_age' not found")));
}

TEST(InsertJob, NoJobConfiguration) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  Job job;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response, StatusIs(StatusCode::kInvalidArgument, HasSubstr("Invalid Job object")));
}

TEST(InsertJob, NoJobConfigurationQuery) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  Job job;
  JobConfiguration job_configuration;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response, StatusIs(StatusCode::kInvalidArgument, HasSubstr("Invalid Job object")));
}

TEST(InsertJob, NoAccessAccountAuth) {
  auto options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  job_configuration_query.query = absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response, StatusIs(StatusCode::kPermissionDenied,
    HasSubstr("User does not have bigquery.jobs.create permission in project")));
}

TEST(InsertJob, DifferentAccount) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  auto table_name = GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  ASSERT_TRUE(table_name);

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(*dataset_id, ".", *table_name);
  job_configuration_query.query = absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job using another account
  auto options_with_user_account = CreateUserAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client_with_user_account = JobClient(MakeBigQueryJobConnection(
    std::move(*options_with_user_account)));

  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client_with_user_account.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response, StatusIs(StatusCode::kPermissionDenied,
    HasSubstr("User does not have permission to access results of another user's job")));
}

TEST(InsertJob, CreateTableAndInsertRow) {
  auto options = CreateServiceAccountAuthWithClientIdAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  auto project_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto dataset_id = GetEnv("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  ASSERT_TRUE(project_id);
  ASSERT_TRUE(dataset_id);
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string table_name = absl::StrCat(*dataset_id, ".Test_Table_Runtime");
  job_configuration_query.query = absl::StrCat("CREATE TABLE IF NOT EXISTS ", table_name, " (id INT) ");
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(*project_id);
  request.set_job(job);

  request.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                "connectionProperties", "systemVariables", "structTypes",
                                "structValues", "location"});

  auto job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(*project_id);
  get_query_results_request.set_job_id(job_id);

  auto query_results_response = job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);

  // Inserting a row in that table
  Job job_dml;
  JobConfiguration job_configuration_dml;
  JobConfigurationQuery job_configuration_query_dml;
  job_configuration_query_dml.query = absl::StrCat("INSERT INTO ", table_name, " VALUES(1)");
  job_configuration_dml.query = job_configuration_query_dml;
  job_dml.configuration = job_configuration_dml;
  InsertJobRequest request_dml;
  request_dml.set_project_id(*project_id);
  request_dml.set_job(job_dml);

  request_dml.set_json_filter_keys({"statistics", "status", "labels", "destinationTable",
                                    "maximumBytesBilled", "userDefinedFunctionResources", "defaultDataset",
                                    "schemaUpdateOptions", "timePartitioning", "rangePartitioning",
                                    "clustering", "destinationEncryptionConfiguration", "scriptOptions",
                                    "connectionProperties", "systemVariables", "structTypes",
                                    "structValues", "location"});

  auto job_response_dml = job_client.InsertJob(request_dml);

  ASSERT_STATUS_OK(job_response_dml);
  EXPECT_EQ(job_response_dml.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id_dml = job_response_dml.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request_dml;
  get_query_results_request_dml.set_project_id(*project_id);
  get_query_results_request_dml.set_job_id(job_id_dml);

  auto query_results_response_dml = job_client.QueryResults(get_query_results_request_dml);

  ASSERT_STATUS_OK(query_results_response_dml);
  EXPECT_TRUE(query_results_response_dml.value().job_complete);
}
}
}
}
