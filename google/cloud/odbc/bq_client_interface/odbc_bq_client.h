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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_CLIENT_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_CLIENT_H

#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/status_or.h"

namespace google::cloud::odbc_bigquery_client_interface {

/// ODBC BigQuery Client
///
/// This Client is responsible for interacting with BigQuery API.
/// It should be used for all kinds of such interactions.
///
/// @par Performance
///
/// Creating a new instance of this class is a relatively efficient operation,
/// as there is no connection with a service. Copy-construction,
/// move-construction, and the corresponding assignment operations are also
/// relatively efficient as the copies share all underlying resources.
///
/// @par Thread Safety
///
/// Concurrent access to different instances of this class, even if they compare
/// equal, is guaranteed to work. Two or more threads operating on the same
/// instance of this class is not guaranteed to work. Since copy-construction
/// and move-construction is a relatively efficient operation, consider using
/// such a copy when using this class from multiple threads.
///
class ODBCBQClient {
 public:
  static StatusOr<std::unique_ptr<ODBCBQClient>> CreateBQClient(
      Oauth const& oauth);
  ~ODBCBQClient() = default;

  ODBCBQClient(ODBCBQClient const&) = default;
  ODBCBQClient& operator=(ODBCBQClient const&) = default;

  ODBCBQClient(ODBCBQClient&&) = default;
  ODBCBQClient& operator=(ODBCBQClient&&) = default;

  ///////////////
  // Project APIs
  ///////////////

  // Get detailed project information for the project passed in.
  StatusOr<::google::cloud::bigquery_v2_minimal_internal::Project> GetProject(
      std::string const& project_id, ::google::cloud::Options const& options);

  // Lists all projects for the user.
  StatusOr<std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
  ListAllProjects(::google::cloud::Options const& options);

  // Filter projects for the user, based on project_ids.
  StatusOr<std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
  FilterProjects(std::vector<std::string> const& project_ids,
                 ::google::cloud::Options const& options);

 private:
  ODBCBQClient(
      ::google::cloud::bigquery_v2_minimal_internal::DatasetClient
          dataset_client,
      ::google::cloud::bigquery_v2_minimal_internal::JobClient job_client,
      ::google::cloud::bigquery_v2_minimal_internal::ProjectClient
          project_client,
      ::google::cloud::bigquery_v2_minimal_internal::TableClient table_client)
      : dataset_client_(std::move(dataset_client)),
        job_client_(std::move(job_client)),
        project_client_(std::move(project_client)),
        table_client_(std::move(table_client)) {}

  ::google::cloud::bigquery_v2_minimal_internal::DatasetClient dataset_client_;
  ::google::cloud::bigquery_v2_minimal_internal::JobClient job_client_;
  ::google::cloud::bigquery_v2_minimal_internal::ProjectClient project_client_;
  ::google::cloud::bigquery_v2_minimal_internal::TableClient table_client_;
};

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_CLIENT_H
