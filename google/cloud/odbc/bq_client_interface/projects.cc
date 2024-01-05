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

#include "google/cloud/bigquery/v2/minimal/internal/project_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::ListProjectsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;

StatusOr<std::vector<Project>> ListAllProjects(ProjectClient& project_client,
                                               Options const& options) {
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);

  std::vector<Project> projects;
  for (auto const& project : projects_response) {
    if (!project) {
      return project.status();
    }
    projects.push_back(*project);
  }

  return projects;
}

StatusOr<Project> GetProject(ProjectClient& project_client,
                             std::string const& project_id,
                             Options const& options) {
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);

  for (auto const& project : projects_response) {
    if (!project) {
      return project.status();
    }
    if ((*project).id == project_id) {
      return *project;
    }
  }

  return Status(StatusCode::kNotFound,
                "The project " + project_id + " was not found");
}

StatusOr<std::vector<Project>> FilterProjects(
    ProjectClient& project_client, std::vector<std::string> const& project_ids,
    Options const& options) {
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);

  std::vector<Project> projects;
  for (auto const& project : projects_response) {
    if (!project) {
      return project.status();
    }
    if (std::find(project_ids.begin(), project_ids.end(), (*project).id) !=
        project_ids.end()) {
      projects.push_back(*project);
    }
  }

  return projects;
}

}  // namespace google::cloud::odbc_bigquery_client_interface
