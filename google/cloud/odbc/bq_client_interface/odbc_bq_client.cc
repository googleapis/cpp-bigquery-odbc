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
#include <grpcpp/security/tls_credentials_options.h>
#include <algorithm>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_storage_v1::BigQueryReadClient;
using ::google::cloud::bigquery_storage_v1::MakeBigQueryReadConnection;
using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatJob;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatTable;
using ::google::cloud::bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeDatasetConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeProjectConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeTableConnection;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
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

  std::string proxy_host = oauth.proxy_options.hostname;
  std::string proxy_port = oauth.proxy_options.port;
  if (!proxy_host.empty() && !proxy_port.empty()) {
    std::string proxy_env = "http://" + proxy_host + ":" + proxy_port;
    std::string proxy_username = oauth.proxy_options.username;
    std::string proxy_pass = oauth.proxy_options.password;
    if (!proxy_username.empty()) {
      proxy_env = "http://" + proxy_username + ":" + proxy_pass + "@" +
                  proxy_host + ":" + proxy_port;
    }
#ifdef _WIN32
    _putenv_s("https_proxy", proxy_env.c_str());
#else
    setenv("https_proxy", proxy_env.c_str(), 1);
#endif
  }

  Options options =
      google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
          *credentials);

  std::string pem_file = oauth.ssl_credentials.pem_root_certs;
  if (!pem_file.empty()) {
    options.set<google::cloud::CARootsFilePathOption>(pem_file);
  }

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

  if (!pem_file.empty()) {
    grpc::SslCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = pem_file;
    auto ssl_creds = grpc::SslCredentials(ssl_opts);
    options.set<google::cloud::GrpcCredentialOption>(ssl_creds);
  }
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

StatusRecordOr<Project> ODBCBQClient::GetProject(std::string const& project_id,
                                                 Options const& options,
                                                 bool use_resource_mgr) {
  if (use_resource_mgr) {
    return ::google::cloud::odbc_bigquery_client_interface::GetProjectRM(
        project_rm_client_, service_usage_client_, project_id, options);
  }
  return ::google::cloud::odbc_bigquery_client_interface::GetProject(
      project_client_, project_id, options);
}

StatusRecordOr<std::vector<Project>> ODBCBQClient::ListAllProjects(
    Options const& options) {
  // RM List API using the provided parent.
  if (!GetListProjectsParent().empty()) {
    return ListAllProjectsRM(GetListProjectsParent(), options);
  }
  // BQ Projects API.
  return ListAllProjectsInternal(options, /*parent*/ "", /*query*/ "", false);
}

StatusRecordOr<std::vector<Project>> ODBCBQClient::ListAllProjectsRM(
    std::string const& parent, Options const& options) {
  return ListAllProjectsInternal(options, parent, /*query*/ "", true);
}

StatusRecordOr<std::vector<Project>> ODBCBQClient::SearchAllProjectsRM(
    std::string const& query, Options const& options) {
  auto all_projects_status =
      ListAllProjectsInternal(options, /*parent*/ "", query, true);
  if (!all_projects_status) {
    return all_projects_status.GetStatusRecord();
  }
  std::vector<Project> all_projects = *all_projects_status;
  // Search API returns results in unspecified order.
  std::sort(all_projects.begin(), all_projects.end(),
            [](Project const& p1, Project const& p2) {
              return p1.numeric_id < p2.numeric_id;
            });
  return all_projects;
}

StatusRecordOr<std::vector<Project>> ODBCBQClient::ListAllProjectsInternal(
    Options const& options, std::string const& parent, std::string const& query,
    bool use_resource_manager) {
  // Calls BQ API
  if (!use_resource_manager) {
    return ::google::cloud::odbc_bigquery_client_interface::ListAllProjects(
        project_client_, options);
  }
  // Calls RM projects.List
  if (!parent.empty()) {
    return ::google::cloud::odbc_bigquery_client_interface::ListAllProjectsRM(
        project_rm_client_, service_usage_client_, parent, options);
  }
  // Calls RM projects.Search
  return ::google::cloud::odbc_bigquery_client_interface::SearchProjectsRM(
      project_rm_client_, service_usage_client_, query, options);
}

StatusRecordOr<std::vector<Project>> ODBCBQClient::FilterProjects(
    std::vector<std::string> const& project_ids, Options const& options) {
  // Filters projects via RM List API using the provided parent.
  if (!GetListProjectsParent().empty()) {
    return FilterProjectsRMList(GetListProjectsParent(), project_ids, options);
  }
  // Calls BQ API
  return ::google::cloud::odbc_bigquery_client_interface::FilterProjects(
      project_client_, project_ids, options);
}

StatusRecordOr<Dataset> ODBCBQClient::GetDataset(std::string const& project_id,
                                                 std::string const& dataset_id,
                                                 Options const& options) {
  return google::cloud::odbc_bigquery_client_interface::GetDataset(
      dataset_client_, project_id, dataset_id, options);
}

StatusRecordOr<std::vector<ListFormatDataset>> ODBCBQClient::ListAllDatasets(
    std::string const& project_id, Options const& options) {
  return google::cloud::odbc_bigquery_client_interface::ListAllDatasets(
      dataset_client_, project_id, options);
}

StatusRecordOr<std::vector<ListFormatDataset>> ODBCBQClient::FilterDatasets(
    std::string const& project_id, DatasetFilter const& dataset_filter,
    ::google::cloud::Options const& options) {
  return google::cloud::odbc_bigquery_client_interface::FilterDatasets(
      dataset_client_, project_id, dataset_filter, options);
}

StatusRecordOr<Table> ODBCBQClient::GetTable(
    std::string const& project_id, std::string const& dataset_id,
    std::string const& table_id, TableFilter const& table_filter,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetTable(
      table_client_, project_id, dataset_id, table_id, table_filter, options);
}

StatusRecordOr<std::vector<ListFormatTable>> ODBCBQClient::ListAllTables(
    std::string const& project_id, std::string const& dataset_id,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllTables(
      table_client_, project_id, dataset_id, options);
}

StatusRecordOr<Job> ODBCBQClient::GetJob(
    std::string const& project_id, std::string const& job_id,
    std::string const& location, ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetJob(
      job_client_, project_id, job_id, location, options);
}

StatusRecordOr<std::vector<ListFormatJob>> ODBCBQClient::ListAllJobs(
    std::string const& project_id, std::string const& parent_job_id,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllJobs(
      job_client_, project_id, parent_job_id, options);
}

StatusRecordOr<std::vector<ListFormatJob>> ODBCBQClient::ListAllJobs(
    std::string const& project_id, ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllJobs(
      job_client_, project_id, options);
}

StatusRecordOr<std::vector<ListFormatJob>> ODBCBQClient::FilterJobs(
    std::string const& project_id, JobFilter const& job_filter,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::FilterJobs(
      job_client_, project_id, job_filter, options);
}

StatusRecordOr<Job> ODBCBQClient::InsertJob(
    std::string const& project_id, Job const& job,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::InsertJob(
      job_client_, project_id, job, options);
}

StatusRecordOr<Job> ODBCBQClient::CancelJob(
    std::string const& project_id, std::string const& job_id,
    std::string const& location, ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::CancelJob(
      job_client_, project_id, job_id, location, options);
}

StatusRecordOr<PostQueryResults> ODBCBQClient::Query(
    std::string const& project_id, QueryRequest const& query_request,
    ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::Query(
      job_client_, project_id, query_request, options);
}

StatusRecordOr<PostQueryResults> ODBCBQClient::PostQuery(
    PostQueryRequest const& post_query_request,
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

StatusRecordOr<GetQueryResults> ODBCBQClient::FilterQueryResults(
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

// Filter projects for the user, based on project_ids, using RM List API.
StatusRecordOr<std::vector<Project>> ODBCBQClient::FilterProjectsRMList(
    std::string const& parent, std::vector<std::string> const& project_ids,
    Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::FilterProjectsRMList(
      project_rm_client_, service_usage_client_, parent, project_ids, options);
}
// Filter projects for the user, based on project_ids, using RM Search API.
StatusRecordOr<std::vector<Project>> ODBCBQClient::FilterProjectsRMSearch(
    std::string const& query, std::vector<std::string> const& project_ids,
    Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::
      FilterProjectsRMSearch(project_rm_client_, service_usage_client_, query,
                             project_ids, options);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
