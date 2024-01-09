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
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/projects.h"
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
  auto credentials = CreateCredentials(oauth);
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

  return std::shared_ptr<ODBCBQClient>(new ODBCBQClient(
      dataset_client, job_client, project_client, table_client));
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

}  // namespace google::cloud::odbc_bigquery_client_interface
