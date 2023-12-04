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

#include "google/cloud/credentials.h"

#include "google/cloud/odbc/client_interface/odbc_bq_client.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

using google::cloud::odbc_bigquery_client_interface::CreateCredentials;
using google::cloud::bigquery_v2_minimal_internal::MakeDatasetConnection;
using google::cloud::bigquery_v2_minimal_internal::MakeBigQueryJobConnection;
using google::cloud::bigquery_v2_minimal_internal::MakeProjectConnection;
using google::cloud::bigquery_v2_minimal_internal::MakeTableConnection;

StatusOr<std::unique_ptr<ODBCBQClient>> ODBCBQClient::Create(Auth const& auth) {
  auto credentials = CreateCredentials(auth);
  if (!credentials) {
    return credentials.status();
  }
  auto options = google::cloud::Options{}
    .set<google::cloud::UnifiedCredentialsOption>(*credentials);
  DatasetClient dataset_client = DatasetClient(MakeDatasetConnection(options));
  JobClient job_client = JobClient(MakeBigQueryJobConnection(options));
  ProjectClient project_client = ProjectClient(MakeProjectConnection(options));
  TableClient table_client = TableClient(MakeTableConnection(options));

  return std::unique_ptr<ODBCBQClient>(new ODBCBQClient(
      dataset_client, job_client, project_client, table_client));
}

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
