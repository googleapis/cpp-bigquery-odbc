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

#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"
#include "google/cloud/resourcemanager/v3/projects_client.h"
#include <google/cloud/resourcemanager/v3/projects.pb.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::ListProjectsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::resourcemanager_v3::ProjectsClient;

StatusRecordOr<std::vector<Project>> ListAllProjects(
    ProjectClient& project_client, Options const& options) {
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);

  std::vector<Project> projects;
  for (auto const& project : projects_response) {
    if (!project) {
      return StatusRecord::ConvertFrom(project.status());
    }
    projects.push_back(*project);
  }

  return projects;
}

StatusRecordOr<Project> GetProject(ProjectClient& project_client,
                                   std::string const& project_id,
                                   Options const& options) {
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);

  for (auto const& project : projects_response) {
    if (!project) {
      return StatusRecord::ConvertFrom(project.status());
    }
    if ((*project).id == project_id) {
      return *project;
    }
  }

  return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                      "The project " + project_id + " was not found"};
}

StatusRecordOr<Project> ConvertFrom(
    google::cloud::resourcemanager::v3::Project const& rm_project) {
  Project bq_project;
  bq_project.kind = "bigquery#project";
  bq_project.id = rm_project.project_id();
  bq_project.friendly_name = rm_project.display_name();
  bq_project.project_reference.project_id = rm_project.project_id();
  auto index = rm_project.name().find('/');
  if (index == std::string::npos) {
    return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                        "The project " + rm_project.project_id() +
                            " was not found with valid project name"};
  }

  bq_project.numeric_id = std::stoi(rm_project.name().substr(index + 1));
  return bq_project;
}

StatusRecordOr<Project> GetProjectRM(ProjectsClient& projects_rm_client,
                                     std::string const& project_id,
                                     Options const& options) {
  if (project_id.empty()) {
    return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                        "The project id cannot be empty"};
  }
  std::string req_rm_project_id = "projects/";
  if (!absl::StartsWith(project_id, "projects/")) {
    req_rm_project_id.append(project_id);
  } else {
    req_rm_project_id = project_id;
  }

  StatusOr<google::cloud::resourcemanager::v3::Project> resp_rm_project =
      projects_rm_client.GetProject(req_rm_project_id, options);
  if (!resp_rm_project) {
    return StatusRecord::ConvertFrom(resp_rm_project.status());
  }

  // Validate we got the correct project back from the server.
  std::string resp_rm_project_id = "projects/";
  resp_rm_project_id.append((*resp_rm_project).project_id());

  if (resp_rm_project_id != req_rm_project_id) {
    return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                        "The project " + project_id + " was not found"};
  }

  return ConvertFrom(*resp_rm_project);
}

StatusRecordOr<std::vector<Project>> FilterProjects(
    ProjectClient& project_client, std::vector<std::string> const& project_ids,
    Options const& options) {
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);

  std::vector<Project> projects;
  for (auto const& project : projects_response) {
    if (!project) {
      return StatusRecord::ConvertFrom(project.status());
    }
    if (std::find(project_ids.begin(), project_ids.end(), (*project).id) !=
        project_ids.end()) {
      projects.push_back(*project);
    }
  }

  return projects;
}

}  // namespace google::cloud::odbc_bigquery_client_interface
