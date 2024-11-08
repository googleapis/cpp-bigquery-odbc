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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_PROJECTS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_PROJECTS_H

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/resourcemanager/v3/projects_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

// Lists all projects for the user.
odbc_internal::StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
ListAllProjects(::google::cloud::bigquery_v2_minimal_internal::ProjectClient&
                    project_client,
                ::google::cloud::Options const& options);

// APIs for fetching detailed project information for the project passed in.

// Fetch project details via BQ Projects API.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Project>
GetProject(::google::cloud::bigquery_v2_minimal_internal::ProjectClient&
               project_client,
           std::string const& project_id,
           ::google::cloud::Options const& options);

// Fetch Project details via ResourceManager API.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Project>
GetProjectRM(
    ::google::cloud::resourcemanager_v3::ProjectsClient& projects_rm_client,
    std::string const& project_id, ::google::cloud::Options const& options);

// Converts ResourceManager project structure to BQ project structure.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Project>
ConvertFrom(google::cloud::resourcemanager::v3::Project const& rm_project);

// Filter projects for the user, based on project_ids.
odbc_internal::StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::Project>>
FilterProjects(::google::cloud::bigquery_v2_minimal_internal::ProjectClient&
                   project_client,
               std::vector<std::string> const& project_ids,
               ::google::cloud::Options const& options);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_PROJECTS_H
