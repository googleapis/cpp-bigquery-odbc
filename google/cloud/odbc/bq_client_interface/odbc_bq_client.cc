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
#include "google/cloud/odbc/bq_client_interface/jobs.h"
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/projects.h"
#include "google/cloud/odbc/bq_client_interface/tables.h"
#include "google/cloud/credentials.h"

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeDatasetConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeProjectConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MakeTableConnection;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;
using ::google::cloud::odbc_bigquery_client_interface::CreateCredentials;

StatusOr<std::shared_ptr<ODBCBQClient>> ODBCBQClient::CreateBQClient(
    Oauth const& oauth) {
  StatusOr<std::shared_ptr<Credentials>> credentials = CreateCredentials(oauth);
  if (!credentials) {
    return credentials.status();
  }
  auto options =
      google::cloud::Options{}.set<google::cloud::UnifiedCredentialsOption>(
          *credentials);
  DatasetClient dataset_client = DatasetClient(MakeDatasetConnection(options));
  JobClient job_client = JobClient(MakeBigQueryJobConnection(options));
  ProjectClient project_client = ProjectClient(MakeProjectConnection(options));
  TableClient table_client = TableClient(MakeTableConnection(options));
  std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator> generator =
      ::google::cloud::oauth2::MakeAccessTokenGenerator(*(*credentials));

  return std::shared_ptr<ODBCBQClient>(new ODBCBQClient(
      dataset_client, job_client, project_client, table_client, generator));
}

StatusOr<AccessToken> ODBCBQClient::GetOAuth2Token() {
  return ::google::cloud::odbc_bigquery_client_interface::GetOAuth2Token(
      access_token_generator_);
}

StatusOr<::google::cloud::bigquery_v2_minimal_internal::Project>
ODBCBQClient::GetProject(std::string const& project_id,
                         Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetProject(
      project_client_, project_id, options);
}
StatusOr<std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
ODBCBQClient::ListAllProjects(Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllProjects(
      project_client_, options);
}

StatusOr<std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
ODBCBQClient::FilterProjects(std::vector<std::string> const& project_ids,
                             Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::FilterProjects(
      project_client_, project_ids, options);
}

StatusOr<::google::cloud::bigquery_v2_minimal_internal::Table>
ODBCBQClient::GetTable(std::string const& project_id,
                       std::string const& dataset_id,
                       std::string const& table_id,
                       ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetTable(
      table_client_, project_id, dataset_id, table_id, options);
}

StatusOr<::google::cloud::bigquery_v2_minimal_internal::Job>
ODBCBQClient::GetJob(std::string const& project_id, std::string const& job_id,
                     std::string const& location,
                     ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::GetJob(
      job_client_, project_id, job_id, location, options);
}

StatusOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
ODBCBQClient::ListAllJobs(std::string const& project_id,
                          ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::ListAllJobs(
      job_client_, project_id, options);
}

StatusOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
ODBCBQClient::FilterJobs(std::string const& project_id,
                         JobFilter const& job_filter,
                         ::google::cloud::Options const& options) {
  return ::google::cloud::odbc_bigquery_client_interface::FilterJobs(
      job_client_, project_id, job_filter, options);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
