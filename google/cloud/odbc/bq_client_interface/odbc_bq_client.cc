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
#include "google/cloud/odbc/internal/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/log/log.h"
#include <grpcpp/security/tls_credentials_options.h>
#include <algorithm>

namespace google::cloud::odbc_bigquery_client_interface {

using google::cloud::StatusOr;
using ::google::cloud::bigquery_storage_v1::BigQueryReadClient;
using ::google::cloud::bigquery_storage_v1::MakeBigQueryReadConnection;
using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResultsRequest;
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

#ifdef _WIN32
#include <wincrypt.h>
#include <windows.h>

odbc_internal::StatusRecordOr<std::string> ExportWindowsSystemCertsToPem() {
  char temp_path[MAX_PATH];
  GetTempPathA(MAX_PATH, temp_path);
  std::string pem_file = std::string(temp_path) + "bqca_roots.pem";

  WIN32_FILE_ATTRIBUTE_DATA file_info;
  if (GetFileAttributesExA(pem_file.c_str(), GetFileExInfoStandard,
                           &file_info)) {
    if (file_info.nFileSizeHigh > 0 || file_info.nFileSizeLow > 0) {
      return pem_file;
    }
  }

  LOG(INFO)
      << "ExportWindowsSystemCertsToPem:: Calling CertOpenSystemStoreA...";
  HCERTSTORE h_store = CertOpenSystemStoreA(NULL, "ROOT");
  if (!h_store) {
    LOG(ERROR) << "Failed to open Windows ROOT certificate store.";
    return odbc_internal::StatusRecord{
        odbc_internal::SQLStates::k_HY000(),
        "Failed to open Windows ROOT certificate store."};
  }

  PCCERT_CONTEXT p_context = nullptr;
  std::string pem_data;

  while ((p_context = CertEnumCertificatesInStore(h_store, p_context)) !=
         nullptr) {
    DWORD size = 0;
    if (!CryptBinaryToStringA(p_context->pbCertEncoded,
                              p_context->cbCertEncoded,
                              CRYPT_STRING_BASE64HEADER, NULL, &size)) {
      continue;
    }

    std::vector<char> buffer(size);
    if (CryptBinaryToStringA(p_context->pbCertEncoded, p_context->cbCertEncoded,
                             CRYPT_STRING_BASE64HEADER, buffer.data(), &size)) {
      pem_data.append(buffer.data());
      pem_data.append("\n");
    }
  }

  CertCloseStore(h_store, 0);

  HANDLE h_file = CreateFileA(pem_file.c_str(), GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (h_file == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "Failed to create .pem file.";
    return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                       "Failed to create .pem file."};
  }

  DWORD bytes_written = 0;
  BOOL ok =
      WriteFile(h_file, pem_data.data(), static_cast<DWORD>(pem_data.size()),
                &bytes_written, NULL);

  CloseHandle(h_file);

  if (!ok || bytes_written != pem_data.size()) {
    LOG(ERROR) << "Failed to write certificate data to .pem file.";
    return odbc_internal::StatusRecord{
        odbc_internal::SQLStates::k_HY000(),
        "Failed to write certificate data to .pem file."};
  }

  return pem_file;
}

#endif

namespace {
google::cloud::ProxyConfig CreateProxyConfig(std::string hostname,
                                             std::string port,
                                             std::string username,
                                             std::string password,
                                             std::string scheme = "http") {
  google::cloud::ProxyConfig proxy_config;
  proxy_config.set_hostname(std::move(hostname))
      .set_port(std::move(port))
      .set_username(std::move(username))
      .set_password(std::move(password))
      .set_scheme(std::move(scheme));
  return proxy_config;
}

}  // namespace

StatusRecordOr<std::shared_ptr<ODBCBQClient>> ODBCBQClient::CreateBQClient(
    Oauth const& oauth) {
  // 1. Initialize Options and set Proxy/SSL settings FIRST
  google::cloud::Options options;

  std::string pem_file = oauth.ssl_credentials.pem_root_certs;
#ifdef _WIN32
  bool use_system_trust_store = oauth.ssl_credentials.use_system_trust_store;
  std::string pem_path;
  if (use_system_trust_store == true) {
    auto pem_path_or = ExportWindowsSystemCertsToPem();
    if (!pem_path_or) {
      return pem_path_or.GetStatusRecord();
    }
    pem_path = *pem_path_or;
    options.set<google::cloud::CARootsFilePathOption>(pem_path);
  } else {
    if (!pem_file.empty()) {
      options.set<google::cloud::CARootsFilePathOption>(pem_file);
    }
  }
#else
  // NON-WINDOWS (Linux, Mac): UseSystemTrustStore is ignored
  if (!pem_file.empty()) {
    options.set<google::cloud::CARootsFilePathOption>(pem_file);
  }
#endif

  // Set Proxy
  options.set<google::cloud::ProxyOption>(
      ProxyConfig()
          .set_hostname(oauth.proxy_options.hostname)
          .set_port(oauth.proxy_options.port)
          .set_username(oauth.proxy_options.username)
          .set_password(oauth.proxy_options.password)
          .set_scheme("http"));

  options.set<google::cloud::UserAgentProductsOption>(
      {"Google-Bigquery-ODBC/" + std::string(DRIVER_VERSION)});

  if(oauth.request_google_drive_scope) {
    options.set<google::cloud::ScopesOption>(
        {"https://www.googleapis.com/auth/bigquery",
         "https://www.googleapis.com/auth/drive.readonly"});
  } else {
  options.set<google::cloud::ScopesOption>(
    {"https://www.googleapis.com/auth/bigquery"}
  );
}
// if (!oauth.request_google_drive_scope) {
//     std::unordered_multimap<std::string, std::string> custom_headers;
    
//     // This tells Google's API Gateway to ignore full platform credential fallback 
//     // and strictly evaluate the request within the narrow perimeter of BigQuery.
//     custom_headers.insert({"X-Goog-Allowed-Resources", "bigquery.googleapis.com"});
    
//     // Alternatively, some Google endpoints accept context downscoping headers:
//     custom_headers.insert({"X-Google-Target-Scopes", "https://www.googleapis.com/auth/bigquery"});

//     options.set<google::cloud::CustomHeadersOption>(custom_headers);
//   }
  StatusRecordOr<std::shared_ptr<Credentials>> credentials =
      CreateCredentials(oauth, options);
  if (!credentials) {
    LOG(ERROR) << "CreateBQClient::CreateCredentials:: "
               << credentials.GetStatusRecord().message;
    return credentials.GetStatusRecord();
  }

  options.set<google::cloud::UnifiedCredentialsOption>(*credentials);

  if (oauth.tpc.enable_tpc && oauth.tpc.universe_domain != "googleapis.com") {
    options.set<google::cloud::internal::UniverseDomainOption>(
        oauth.tpc.universe_domain);
  }

  // Handle Private Service Connect URIs
  std::string bigquery_endpoint;
  std::string readapi_endpoint;

  if (!oauth.psc.empty()) {
    std::stringstream ss(oauth.psc);
    std::string token;
    while (std::getline(ss, token, ',')) {
      auto pos = token.find('=');
      if (pos != std::string::npos) {
        auto key = token.substr(0, pos);
        auto value = token.substr(pos + 1);
        if (key == "BIGQUERY") bigquery_endpoint = value;
        if (key == "READ_API") readapi_endpoint = value;
      }
    }
  }

  // REST client options (BIGQUERY PSC)
  Options rest_options = options;
  if (!bigquery_endpoint.empty()) {
    rest_options.set<google::cloud::EndpointOption>(bigquery_endpoint);
  }
 rest_options.set<google::cloud::ScopesOption>(
    {"https://www.googleapis.com/auth/bigquery"}
  );
  DatasetClient dataset_client =
      DatasetClient(MakeDatasetConnection(rest_options));
  JobClient job_client = JobClient(MakeBigQueryJobConnection(rest_options));
  ProjectClient project_client =
      ProjectClient(MakeProjectConnection(rest_options));
  TableClient table_client = TableClient(MakeTableConnection(rest_options));
  std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> generator =
      ::google::cloud::oauth2::MakeAccessTokenGenerator(*(*credentials));

  Options read_options = options;

  // Disable background threads for BQ Read Connection so we don't end up
  // blocking the main thread with the shared driver library.
  // This needs to be done for GRPC clients, in this case storage read client
  // and resource manager client.
  CompletionQueue cq;
  read_options.set<GrpcCompletionQueueOption>(cq);

  grpc::ChannelArguments channel_arguments;
  channel_arguments.SetUserAgentPrefix("Google-Bigquery-ODBC/" +
                                       std::string(DRIVER_VERSION));
  channel_arguments.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                           std::chrono::minutes(1).count());
  channel_arguments.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS,
                           std::chrono::seconds(10).count());

  if (!readapi_endpoint.empty()) {
    // Set Endpoint and Authority
    read_options.set<google::cloud::EndpointOption>(readapi_endpoint);
    read_options.set<google::cloud::AuthorityOption>(readapi_endpoint);

    channel_arguments.SetSslTargetNameOverride(readapi_endpoint);
  }

  read_options.set<google::cloud::GrpcChannelArgumentsNativeOption>(
      std::move(channel_arguments));
  grpc::SslCredentialsOptions ssl_opts;

#ifdef _WIN32
  if (use_system_trust_store) {
    auto pem_path_or = ExportWindowsSystemCertsToPem();
    if (!pem_path_or) {
      return pem_path_or.GetStatusRecord();
    }
    std::string pem_path = *pem_path_or;
    ssl_opts.pem_root_certs = pem_path;
    auto ssl_creds = grpc::SslCredentials(ssl_opts);
    read_options.set<google::cloud::GrpcCredentialOption>(ssl_creds);
  } else if (!pem_file.empty()) {
    ssl_opts.pem_root_certs = pem_file;
    auto ssl_creds = grpc::SslCredentials(ssl_opts);
    read_options.set<google::cloud::GrpcCredentialOption>(ssl_creds);
  }
#else
  if (!pem_file.empty()) {
    ssl_opts.pem_root_certs = pem_file;
    auto ssl_creds = grpc::SslCredentials(ssl_opts);
    read_options.set<google::cloud::GrpcCredentialOption>(ssl_creds);
  }
#endif
  BigQueryReadClient bigquery_read_client =
      BigQueryReadClient(MakeBigQueryReadConnection(read_options));

  // Create the resource manager project client.
  ::google::cloud::resourcemanager_v3::ProjectsClient project_rm_client =
      ::google::cloud::resourcemanager_v3::ProjectsClient(
          ::google::cloud::resourcemanager_v3::MakeProjectsConnection(
              rest_options));

  // Create the service usage client.
  ServiceUsageClient service_usage_client =
      ServiceUsageClient(MakeServiceUsageConnection(rest_options));

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
    LOG(ERROR) << "SearchAllProjectsRM::ListAllProjectsInternal:: "
               << all_projects_status.GetStatusRecord().message;
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

StatusOr<GetQueryResults> ODBCBQClient::GetQueryResults(
    GetQueryResultsRequest const& request,
    ::google::cloud::Options const& options) {
  return job_client_.QueryResults(request, options);
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

StreamRange<google::cloud::bigquery::storage::v1::ReadRowsResponse>
ODBCBQClient::GetReadRowsStream(
    ::google::cloud::bigquery::storage::v1::ReadRowsRequest const&
        read_rows_request,
    ::google::cloud::Options const& options) {
  return bigquery_read_client_.ReadRows(read_rows_request, options);
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
