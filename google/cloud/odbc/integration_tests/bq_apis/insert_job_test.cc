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

#include "google/cloud/odbc/testing/client_library_utils/authentication.h"
#include "google/cloud/odbc/testing/client_library_utils/util_constants.h"
#include "google/cloud/odbc/testing/utils/env_vars.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_integration_tests_apis {

using bigquery_v2_minimal_internal::GetQueryResults;
using bigquery_v2_minimal_internal::GetQueryResultsRequest;
using bigquery_v2_minimal_internal::InsertJobRequest;
using bigquery_v2_minimal_internal::Job;
using bigquery_v2_minimal_internal::JobClient;
using bigquery_v2_minimal_internal::JobConfiguration;
using bigquery_v2_minimal_internal::JobConfigurationQuery;
using bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using bigquery_v2_minimal_internal::QueryParameter;
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
    "statistics",         "status",         "destinationTable",
    "maximumBytesBilled", "defaultDataset", "timePartitioning",
    "rangePartitioning",  "clustering",     "keyResultStatement",
    "systemVariables",    "location"};

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(InsertJob, UserAccountAuth) {
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
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE");
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name, " WHERE ",
                   column_name, " > @min_age");
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  job_configuration_query.query_parameters = {query_parameter};
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);

  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().schema.fields.size(), 1);
  EXPECT_EQ(query_results_response.value().total_rows, 1);
}
#endif  // USER_ACCOUNT_AUTH

TEST(InsertJob, ServiceAccountAuth) {
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
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name, " WHERE ",
                   column_name, " > @min_age");
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  job_configuration_query.query_parameters = {query_parameter};
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().schema.fields.size(), 1);
  EXPECT_EQ(query_results_response.value().total_rows, 1);
}

#ifdef USER_ACCOUNT_AUTH  // TODO(b/333011414) Enable tests
TEST(InsertJob, ServiceAccountAuthWithClientId) {
  StatusOr<Options> options =
      CreateServiceAccountAuthWithClientIdAuthentication();
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
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name, " WHERE ",
                   column_name, " > @min_age");
  QueryParameter query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  job_configuration_query.query_parameters = {query_parameter};
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);
  EXPECT_EQ(query_results_response.value().schema.fields.size(), 1);
  EXPECT_EQ(query_results_response.value().total_rows, 1);
}
#endif  // USER_ACCOUNT_AUTH

TEST(InsertJob, ProjectNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(std::string(kNameForNonExistingProject));
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Error in non-idempotent operation: ProjectId "
                                 "and DatasetId must be non-empty")));
}

TEST(InsertJob, DatasetNotExist) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name =
      absl::StrCat("Not_existing_dataset.", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_FALSE(job_response.value().status.errors.empty());
  EXPECT_FALSE(job_response.value().status.error_result.message.empty());

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response,
              StatusIs(StatusCode::kNotFound, HasSubstr("Not found: Dataset")));
}

TEST(InsertJob, NoQueryParameters) {
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

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name, " WHERE ",
                   column_name, " > @min_age");
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_FALSE(job_response.value().status.errors.empty());
  EXPECT_FALSE(job_response.value().status.error_result.message.empty());

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Query parameter 'min_age' not found")));
}

TEST(InsertJob, NoJobConfiguration) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  Job job;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Required parameter is missing: query")));
}

TEST(InsertJob, NoJobConfigurationQuery) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  Job job;
  JobConfiguration job_configuration;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Required parameter is missing: query")));
}

#ifdef USER_ACCOUNT_AUTH  // TODO: b/309605217 - Enable once the bug is fixed
TEST(InsertJob, NoAccessAccountAuth) {
  StatusOr<Options> options = CreateNoAccessAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  EXPECT_THAT(job_response,
              StatusIs(StatusCode::kPermissionDenied,
                       HasSubstr("User does not have bigquery.jobs.create "
                                 "permission in project")));
}
#endif  // USER_ACCOUNT_AUTH

#ifdef USER_ACCOUNT_AUTH  // TODO(b/333011414) Enable tests
TEST(InsertJob, DifferentAccount) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");

  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT * FROM ", full_table_name);

  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job using another account
  StatusOr<Options> options_with_user_account =
      CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client_with_user_account = JobClient(
      MakeBigQueryJobConnection(std::move(*options_with_user_account)));

  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client_with_user_account.QueryResults(get_query_results_request);

  EXPECT_THAT(query_results_response,
              StatusIs(StatusCode::kPermissionDenied,
                       HasSubstr("User does not have permission to access "
                                 "results of another user's job")));
}
#endif  // USER_ACCOUNT_AUTH

TEST(InsertJob, CreateTableAndInsertRow) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string project_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string table_name = absl::StrCat(dataset_id, ".Test_Table_Runtime");
  job_configuration_query.query =
      absl::StrCat("CREATE TABLE IF NOT EXISTS ", table_name, " (id INT) ");
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id(project_id);
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  ASSERT_STATUS_OK(job_response);
  EXPECT_EQ(job_response.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id = job_response.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(project_id);
  get_query_results_request.set_job_id(job_id);

  StatusOr<GetQueryResults> query_results_response =
      job_client.QueryResults(get_query_results_request);

  ASSERT_STATUS_OK(query_results_response);
  EXPECT_TRUE(query_results_response.value().job_complete);

  // Inserting a row in that table
  Job job_dml;
  JobConfiguration job_configuration_dml;
  JobConfigurationQuery job_configuration_query_dml;
  job_configuration_query_dml.query =
      absl::StrCat("INSERT INTO ", table_name, " VALUES(1)");
  job_configuration_dml.query = job_configuration_query_dml;
  job_dml.configuration = job_configuration_dml;
  InsertJobRequest request_dml;
  request_dml.set_project_id(project_id);
  request_dml.set_job(job_dml);
  request_dml.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response_dml = job_client.InsertJob(request_dml);

  ASSERT_STATUS_OK(job_response_dml);
  EXPECT_EQ(job_response_dml.value().configuration.job_type, "QUERY");

  // Getting results of previous Job
  std::string job_id_dml = job_response_dml.value().job_reference.job_id;
  GetQueryResultsRequest get_query_results_request_dml;
  get_query_results_request_dml.set_project_id(project_id);
  get_query_results_request_dml.set_job_id(job_id_dml);

  StatusOr<GetQueryResults> query_results_response_dml =
      job_client.QueryResults(get_query_results_request_dml);

  ASSERT_STATUS_OK(query_results_response_dml);
  EXPECT_TRUE(query_results_response_dml.value().job_complete);
}

TEST(InsertJob, ProjectIdIsEmpty) {
  StatusOr<Options> options = CreateServiceAccountAuthentication();
  ASSERT_STATUS_OK(options);
  auto job_client = JobClient(MakeBigQueryJobConnection(std::move(*options)));
  std::string dataset_id =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_BIGQUERY_DATASET");
  std::string table_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_TABLE_NAME");
  std::string column_name =
      GetRequiredEnvVar("CPP_BIGQUERY_ODBC_TEST_COLUMN_NAME_AGE");
  Job job;
  JobConfiguration job_configuration;
  JobConfigurationQuery job_configuration_query;
  std::string full_table_name = absl::StrCat(dataset_id, ".", table_name);
  job_configuration_query.query =
      absl::StrCat("SELECT ", column_name, " FROM ", full_table_name);
  job_configuration.query = job_configuration_query;
  job.configuration = job_configuration;
  InsertJobRequest request;
  request.set_project_id("");
  request.set_job(job);
  request.set_json_filter_keys(kKeysToFilter);

  StatusOr<Job> job_response = job_client.InsertJob(request);

  // BQ API error
  EXPECT_THAT(job_response, StatusIs(StatusCode::kNotFound,
                                     HasSubstr("Request couldn't be served")));
}

}  // namespace google::cloud::odbc_integration_tests_apis
