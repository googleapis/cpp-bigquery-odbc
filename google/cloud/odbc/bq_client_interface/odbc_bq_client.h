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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_ODBC_BQ_CLIENT_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_ODBC_BQ_CLIENT_H

#include "google/cloud/odbc/bq_client_interface/datasets.h"
#include "google/cloud/odbc/bq_client_interface/jobs.h"
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"
#include "google/cloud/odbc/bq_client_interface/tables.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/storage/v1/bigquery_read_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/job_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
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
  static odbc_internal::StatusRecordOr<std::shared_ptr<ODBCBQClient>>
  CreateBQClient(Oauth const& oauth);
  ~ODBCBQClient() = default;

  ODBCBQClient(ODBCBQClient const&) = default;
  ODBCBQClient& operator=(ODBCBQClient const&) = default;

  ODBCBQClient(ODBCBQClient&&) = default;
  ODBCBQClient& operator=(ODBCBQClient&&) = default;

  odbc_internal::StatusRecordOr<AccessToken> GetOAuth2Token();

  ///////////////
  // Project APIs
  ///////////////

  // Get detailed project information for the project passed in.
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::Project>
  GetProject(std::string const& project_id,
             ::google::cloud::Options const& options);

  // Lists all projects for the user.
  odbc_internal::StatusRecordOr<
      std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
  ListAllProjects(::google::cloud::Options const& options);

  // Filter projects for the user, based on project_ids.
  odbc_internal::StatusRecordOr<
      std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
  FilterProjects(std::vector<std::string> const& project_ids,
                 ::google::cloud::Options const& options);

  ///////////////
  // Dataset APIs
  ///////////////

  // Returns detailed info for a specific Dataset.
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::Dataset>
  GetDataset(std::string const& project_id, std::string const& dataset_id,
             ::google::cloud::Options const& options);

  // Returns all Datasets in a Project.
  odbc_internal::StatusRecordOr<std::vector<
      ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset>>
  ListAllDatasets(std::string const& project_id,
                  ::google::cloud::Options const& options);

  // Returns filtered list of datasets in a Project, based on the dataset
  // filters passed in.
  odbc_internal::StatusRecordOr<std::vector<
      ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset>>
  FilterDatasets(std::string const& project_id,
                 DatasetFilter const& dataset_filter,
                 ::google::cloud::Options const& options);

  ///////////////
  // Table APIs
  ///////////////

  // Returns detailed info for a specific Table
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::Table>
  GetTable(std::string const& project_id, std::string const& dataset_id,
           std::string const& table_id, TableFilter const& table_filter,
           ::google::cloud::Options const& options);

  // Returns all Tables in a Dataset
  odbc_internal::StatusRecordOr<std::vector<
      ::google::cloud::bigquery_v2_minimal_internal::ListFormatTable>>
  ListAllTables(std::string const& project_id, std::string const& dataset_id,
                ::google::cloud::Options const& options);

  ///////////////
  // Job APIs
  ///////////////

  // Returns detailed info for a specific Job
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::Job>
  GetJob(std::string const& project_id, std::string const& job_id,
         std::string const& location, ::google::cloud::Options const& options);

  // Returns all Jobs in a Project
  odbc_internal::StatusRecordOr<
      std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
  ListAllJobs(std::string const& project_id,
              ::google::cloud::Options const& options);

  // Returns a filtered list of Jobs in a Project, based on the job filters
  // passed in
  odbc_internal::StatusRecordOr<
      std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatJob>>
  FilterJobs(std::string const& project_id, JobFilter const& job_filter,
             ::google::cloud::Options const& options);

  // Inserts a BQ job for execution
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::Job>
  InsertJob(std::string const& project_id,
            ::google::cloud::bigquery_v2_minimal_internal::Job const& job,
            ::google::cloud::Options const& options);

  // Cancels an already running BQ Job
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::Job>
  CancelJob(std::string const& project_id, std::string const& job_id,
            std::string const& location,
            ::google::cloud::Options const& options);

  // Runs a BQ SQL query synchronously and returns query
  // results if the query completes within a specified timeout.
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults>
  Query(std::string const& project_id,
        ::google::cloud::bigquery_v2_minimal_internal::QueryRequest const&
            query_request,
        ::google::cloud::Options const& options);

  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults>
  PostQuery(
      ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
          query_request,
      ::google::cloud::Options const& options);

  // Gets all the query results of a previously run query job.
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
  GetAllQueryResults(std::string const& project_id, std::string const& job_id,
                     std::string const& location,
                     ::google::cloud::Options const& options);

  // Gets query results, based on the filter passed in.
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
  FilterQueryResults(std::string const& project_id, std::string const& job_id,
                     std::string const& location,
                     QueryResultsFilterParams const& query_results_filter,
                     ::google::cloud::Options const& options);

  ///////////////
  // Storage APIs
  ///////////////

  // Creates a new read session for dividing BQ Table contents into one or more
  // streams, to be read later.
  odbc_internal::StatusRecordOr<
      ::google::cloud::bigquery::storage::v1::ReadSession>
  CreateReadSession(
      ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest const&
          read_session_request,
      ::google::cloud::Options const& options);

  // Reads rows from streams, in the format prescribed by the read session.
  // Allows to limit the number of read responses returned via the
  // max_read_responses parameter. By default, all read responses is returned
  odbc_internal::StatusRecordOr<
      std::vector<google::cloud::bigquery::storage::v1::ReadRowsResponse>>
  ReadRows(::google::cloud::bigquery::storage::v1::ReadRowsRequest const&
               read_rows_request,
           int max_read_responses, ::google::cloud::Options const& options);

 private:
  ODBCBQClient(
      ::google::cloud::bigquery_v2_minimal_internal::DatasetClient
          dataset_client,
      ::google::cloud::bigquery_v2_minimal_internal::JobClient job_client,
      ::google::cloud::bigquery_v2_minimal_internal::ProjectClient
          project_client,
      ::google::cloud::bigquery_v2_minimal_internal::TableClient table_client,
      std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator>
          access_token_generator,
      ::google::cloud::bigquery_storage_v1::BigQueryReadClient
          bigquery_read_client)
      : dataset_client_(std::move(dataset_client)),
        job_client_(std::move(job_client)),
        project_client_(std::move(project_client)),
        table_client_(std::move(table_client)),
        access_token_generator_(std::move(access_token_generator)),
        bigquery_read_client_(std::move(bigquery_read_client)) {}

  ::google::cloud::bigquery_v2_minimal_internal::DatasetClient dataset_client_;
  ::google::cloud::bigquery_v2_minimal_internal::JobClient job_client_;
  ::google::cloud::bigquery_v2_minimal_internal::ProjectClient project_client_;
  ::google::cloud::bigquery_v2_minimal_internal::TableClient table_client_;
  std::shared_ptr<::google::cloud::oauth2::AccessTokenGenerator>
      access_token_generator_;
  ::google::cloud::bigquery_storage_v1::BigQueryReadClient
      bigquery_read_client_;
};

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_ODBC_BQ_CLIENT_H
