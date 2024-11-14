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
#include "google/cloud/odbc/bq_client_interface/datasets.h"
#include "google/cloud/odbc/bq_client_interface/jobs.h"
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/projects.h"
#include "google/cloud/odbc/bq_client_interface/storage.h"
#include "google/cloud/odbc/bq_client_interface/tables.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_storage_v1::BigQueryReadClient;
using ::google::cloud::bigquery_storage_v1::MakeBigQueryReadConnection;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeDatasetConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeProjectConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeTableConnection;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;
using ::google::cloud::odbc_bigquery_client_interface::CreateCredentials;
using google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::serviceusage_v1::MakeServiceUsageConnection;
using ::google::cloud::serviceusage_v1::ServiceUsageClient;

StatusRecordOr<std::shared_ptr<ODBCBQClient>> ODBCBQClient::CreateBQClient(
    Oauth const& oauth) {
  StatusRecordOr<std::shared_ptr<Credentials>> credentials =
      CreateCredentials(oauth);
  if (!credentials) {
    return credentials.GetStatusRecord();
  }

  Options options =
      google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
          *credentials);

  DatasetClient dataset_client = DatasetClient(MakeDatasetConnection(options));
  JobClient job_client = JobClient(MakeBigQueryJobConnection(options));
  ProjectClient project_client = ProjectClient(MakeProjectConnection(options));
  TableClient table_client = TableClient(MakeTableConnection(options));
  std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> generator =
      ::google::cloud::oauth2::MakeAccessTokenGenerator(*(*credentials));
  // Disable background threads for BQ Read Connection so we don't end up
  // blocking the main thread with the shared driver library.
  // This needs to be done for GRPC clients, in this case storage read client
  // and resource manager client.
  CompletionQueue cq;
  options.set<GrpcCompletionQueueOption>(cq);
  BigQueryReadClient bigquery_read_client =
      BigQueryReadClient(MakeBigQueryReadConnection(options));

  // Create the resource manager project client.
  ::google::cloud::resourcemanager_v3::ProjectsClient project_rm_client =
      ::google::cloud::resourcemanager_v3::ProjectsClient(
          ::google::cloud::resourcemanager_v3::MakeProjectsConnection(options));

  // Create the service usage client.
  ServiceUsageClient service_usage_client =
      ServiceUsageClient(MakeServiceUsageConnection(options));

  return std::shared_ptr<ODBCBQClient>(new ODBCBQClient(
      dataset_client, job_client, project_client, project_rm_client,
      service_usage_client, table_client, generator, bigquery_read_client));
}

StatusRecordOr<AccessToken> ODBCBQClient::GetOAuth2Token() {
  return ::google::cloud::odbc_bigquery_client_interface::GetOAuth2Token(
      access_token_generator_);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::Project>
ODBCBQClient::GetProject(std::string const& project_id, Options const& options,
                         bool use_resource_mgr) {
  if (use_resource_mgr) {
    return ::google::cloud::odbc_bigquery_client_interface::GetProjectRM(
        project_rm_client_, project_id, options);
  }
  return ::google::cloud::odbc_bigquery_client_interface::GetProject(
      project_client_, project_id, options);
}
StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
ODBCBQClient::ListAllProjects(Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllProjects(
      project_client_, options);
}

StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
ODBCBQClient::FilterProjects(std::vector<std::string> const& project_ids,
                             Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::FilterProjects(
      project_client_, project_ids, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::Dataset>
ODBCBQClient::GetDataset(std::string const& project_id,
                         std::string const& dataset_id,
                         Options const& options) {
  return google::cloud::odbc_bigquery_client_interface::GetDataset(
      dataset_client_, project_id, dataset_id, options);
}

StatusRecordOr<std::vector<
    ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset>>
ODBCBQClient::ListAllDatasets(std::string const& project_id,
                              Options const& options) {
  return google::cloud::odbc_bigquery_client_interface::ListAllDatasets(
      dataset_client_, project_id, options);
}

StatusRecordOr<std::vector<
    ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset>>
ODBCBQClient::FilterDatasets(std::string const& project_id,
                             DatasetFilter const& dataset_filter,
                             ::google::cloud::Options const& options) {
  return google::cloud::odbc_bigquery_client_interface::FilterDatasets(
      dataset_client_, project_id, dataset_filter, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::Table>
ODBCBQClient::GetTable(std::string const& project_id,
                       std::string const& dataset_id,
                       std::string const& table_id,
                       TableFilter const& table_filter,
                       ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetTable(
      table_client_, project_id, dataset_id, table_id, table_filter, options);
}

StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatTable>>
ODBCBQClient::ListAllTables(std::string const& project_id,
                            std::string const& dataset_id,
                            ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllTables(
      table_client_, project_id, dataset_id, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::Job>
ODBCBQClient::GetJob(std::string const& project_id, std::string const& job_id,
                     std::string const& location,
                     ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetJob(
      job_client_, project_id, job_id, location, options);
}

StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
ODBCBQClient::ListAllJobs(std::string const& project_id,
                          ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllJobs(
      job_client_, project_id, options);
}

StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
ODBCBQClient::FilterJobs(std::string const& project_id,
                         JobFilter const& job_filter,
                         ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::FilterJobs(
      job_client_, project_id, job_filter, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::Job>
ODBCBQClient::InsertJob(
    std::string const& project_id,
    ::google::cloud::bigquery_v2_minimal_internal::Job const& job,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::InsertJob(
      job_client_, project_id, job, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::Job>
ODBCBQClient::CancelJob(std::string const& project_id,
                        std::string const& job_id, std::string const& location,
                        ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::CancelJob(
      job_client_, project_id, job_id, location, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::PostQueryResults>
ODBCBQClient::Query(
    std::string const& project_id,
    ::google::cloud::bigquery_v2_minimal_internal::QueryRequest const&
        query_request,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::Query(
      job_client_, project_id, query_request, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::PostQueryResults>
ODBCBQClient::PostQuery(
    ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
        post_query_request,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::PostQuery(
      job_client_, post_query_request, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
ODBCBQClient::GetAllQueryResults(std::string const& project_id,
                                 std::string const& job_id,
                                 std::string const& location,
                                 std::chrono::milliseconds timeout_ms,
                                 ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetAllQueryResults(
      job_client_, project_id, job_id, location, timeout_ms, options);
}

StatusRecordOr<::google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
ODBCBQClient::FilterQueryResults(
    std::string const& project_id, std::string const& job_id,
    std::string const& location,
    QueryResultsFilterParams const& query_results_filter,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::FilterQueryResults(
      job_client_, project_id, job_id, location, query_results_filter, options);
}

StatusRecordOr<::google::cloud::bigquery::storage::v1::ReadSession>
ODBCBQClient::CreateReadSession(
    ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest const&
        read_session_request,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::CreateReadSession(
      bigquery_read_client_, read_session_request, options);
}

StatusRecordOr<
    std::vector<google::cloud::bigquery::storage::v1::ReadRowsResponse>>
ODBCBQClient::ReadRows(
    ::google::cloud::bigquery::storage::v1::ReadRowsRequest const&
        read_rows_request,
    int max_read_responses, ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ReadRows(
      bigquery_read_client_, read_rows_request, max_read_responses, options);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
