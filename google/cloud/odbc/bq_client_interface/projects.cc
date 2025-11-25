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

#include "google/cloud/odbc/bq_client_interface/projects.h"
#include "google/cloud/odbc/bq_client_interface/datasets.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <google/cloud/resourcemanager/v3/projects.pb.h>
#include <absl/log/log.h>
#include <chrono>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::api::serviceusage::v1::GetServiceRequest;
using ::google::api::serviceusage::v1::State;
using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::ListProjectsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::resourcemanager_v3::ProjectsClient;
using ::google::cloud::serviceusage_v1::ServiceUsageClient;

constexpr int kSmallProjectNum = 100;

StatusRecordOr<std::vector<Project>> FilterBQProjects(
    std::vector<std::string> const& project_ids,
    StreamRange<Project>& bq_projects) {
  std::vector<Project> projects;
  for (auto const& project : bq_projects) {
    if (!project) {
      LOG(ERROR) << "FilterBQProjects:: " << project.status().message();
      return StatusRecord::ConvertFrom(project.status());
    }
    if (std::find(project_ids.begin(), project_ids.end(), (*project).id) !=
        project_ids.end()) {
      projects.push_back(*project);
    }
  }

  return projects;
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
    LOG(ERROR) << "ConvertFrom:: "
               << "The project " << rm_project.project_id()
               << " was not found with valid project name";
    return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                        "The project " + rm_project.project_id() +
                            " was not found with valid project name"};
  }
  std::string s_numeric_id = rm_project.name().substr(index + 1);
  bq_project.numeric_id = std::stoll(s_numeric_id);
  LOG(INFO) << "ConvertFrom::Project:: Request body: "
            << GetJsonRegResp<Project>(bq_project);
  return bq_project;
}

bool IsProjectBQEnabled(std::string const& bq_project_id,
                        ServiceUsageClient& service_usage_client,
                        Options const& options) {
  std::string name =
      "projects/" + bq_project_id + "/services/bigquery.googleapis.com";
  GetServiceRequest request;
  request.set_name(name);
  auto const& bq_service = service_usage_client.GetService(request, options);
  if (!bq_service) {
    return false;
  }
  return ((*bq_service).state() == State::ENABLED);
}

StatusRecordOr<std::vector<Project>> ListAllProjects(
    ProjectClient& project_client, Options const& options) {
  auto start_time = std::chrono::steady_clock::now();
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);
  auto elapsed_time = std::chrono::steady_clock::now() - start_time;
  LOG(INFO) << "ListAllProjects:: Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms";

  std::vector<Project> projects;
  for (auto const& project : projects_response) {
    if (!project) {
      LOG(ERROR) << "ListAllProjects::Project:: " << project.status().message();
      return StatusRecord::ConvertFrom(project.status());
    }
    projects.push_back(*project);
  }

  return projects;
}

StatusRecordOr<Project> GetProject(ProjectClient& project_client,
                                   std::string const& project_id,
                                   Options const& options) {
  auto start_time = std::chrono::steady_clock::now();
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);
  auto elapsed_time = std::chrono::steady_clock::now() - start_time;
  LOG(INFO) << "GetProject:: Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms";

  for (auto const& project : projects_response) {
    if (!project) {
      LOG(ERROR) << "GetProject::Project:: " << project.status().message();
      return StatusRecord::ConvertFrom(project.status());
    }
    if ((*project).id == project_id) {
      return *project;
    }
  }
  LOG(ERROR) << "GetProject:: The project " << project_id << " was not found";
  return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                      "The project " + project_id + " was not found"};
}

StatusRecordOr<Project> GetProjectRM(
    ProjectsClient& projects_rm_client,
    ::google::cloud::serviceusage_v1::ServiceUsageClient& service_usage_client,
    std::string const& project_id, Options const& options) {
  auto start_time = std::chrono::steady_clock::now();
  if (project_id.empty()) {
    LOG(ERROR) << "GetProjectRM:: The project id cannot be empty";
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
  auto elapsed_time = std::chrono::steady_clock::now() - start_time;
  LOG(INFO) << "GetProjectRM:: Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms";

  if (!resp_rm_project) {
    LOG(ERROR) << "GetProjectRM::GetProject:: "
               << resp_rm_project.status().message();
    return StatusRecord::ConvertFrom(resp_rm_project.status());
  }

  // Validate we got the correct project back from the server.
  std::string resp_rm_project_id = "projects/";
  resp_rm_project_id.append((*resp_rm_project).project_id());

  if (resp_rm_project_id != req_rm_project_id) {
    LOG(ERROR) << "GetProjectRM:: The project " << project_id
               << " was not found";
    return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                        "The project " + project_id + " was not found"};
  }

  // Ensure RM project is BQ enabled.
  if (IsProjectBQEnabled((*resp_rm_project).project_id(), service_usage_client,
                         options)) {
    auto const& bq_project = ConvertFrom(*resp_rm_project);

    if (!bq_project) {
      LOG(ERROR) << "GetProjectRM:: " << bq_project.GetStatusRecord().message;
      return bq_project.GetStatusRecord();
    }
    return bq_project;
  }
  LOG(ERROR) << "GetProjectRM:: The project " << project_id
             << " is not enabled for BigQuery";
  return StatusRecord{
      odbc_internal::SQLStates::k_HY000(),
      "The project " + project_id + " is not enabled for BigQuery"};
}

StatusRecordOr<std::vector<Project>> SearchProjectsRM(
    ProjectsClient& projects_rm_client, ServiceUsageClient service_usage_client,
    std::string const& query, ::google::cloud::Options const& options) {
  auto start_time = std::chrono::steady_clock::now();
  std::string search_query = (query.empty()) ? "state:ACTIVE" : query;

  StreamRange<google::cloud::resourcemanager::v3::Project>
      rm_projects_response =
          projects_rm_client.SearchProjects(search_query, options);
  auto elapsed_time = std::chrono::steady_clock::now() - start_time;
  LOG(INFO) << "SearchProjectsRM:: Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms";

  std::vector<Project> bq_projects;
  for (auto const& rm_project : rm_projects_response) {
    if (!rm_project) {
      LOG(ERROR) << "SearchProjectsRM:: " << rm_project.status().message();
      return StatusRecord::ConvertFrom(rm_project.status());
    }
    // Filter by BQ enabled projects.
    if (IsProjectBQEnabled((*rm_project).project_id(), service_usage_client,
                           options)) {
      auto const& bq_project = ConvertFrom(*rm_project);
      if (!bq_project) {
        LOG(ERROR) << "SearchProjectsRM:: "
                   << bq_project.GetStatusRecord().message;
        return bq_project.GetStatusRecord();
      }
      bq_projects.push_back(*bq_project);
    }
  }

  return bq_projects;
}

StatusRecordOr<std::vector<Project>> ListAllProjectsRM(
    ProjectsClient& projects_rm_client, ServiceUsageClient service_usage_client,
    std::string const& parent, Options const& options) {
  if (parent.empty()) {
    LOG(ERROR)
        << "ListAllProjectsRM:: The parent resource cannot be null or empty";
    return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                        "The parent resource cannot be null or empty"};
  }
  StreamRange<google::cloud::resourcemanager::v3::Project>
      rm_projects_response = projects_rm_client.ListProjects(parent, options);

  std::vector<Project> bq_projects;
  for (auto const& rm_project : rm_projects_response) {
    if (!rm_project) {
      LOG(ERROR) << "ListAllProjectsRM:: " << rm_project.status().message();
      return StatusRecord::ConvertFrom(rm_project.status());
    }
    // Filter by BQ enabled projects.
    if (IsProjectBQEnabled((*rm_project).project_id(), service_usage_client,
                           options)) {
      auto const& bq_project = ConvertFrom(*rm_project);
      if (!bq_project) {
        LOG(ERROR) << "ListAllProjectsRM:: "
                   << bq_project.GetStatusRecord().message;
        return bq_project.GetStatusRecord();
      }
      bq_projects.push_back(*bq_project);
    }
  }

  return bq_projects;
}

StatusRecordOr<std::vector<Project>> FilterProjects(
    ProjectClient& project_client, std::vector<std::string> const& project_ids,
    Options const& options) {
  ListProjectsRequest request;

  StreamRange<Project> projects_response =
      project_client.ListProjects(request, options);

  return FilterBQProjects(project_ids, projects_response);
}

StatusRecordOr<std::vector<Project>> FilterProjectsRMList(
    ProjectsClient& projects_rm_client,
    ServiceUsageClient& service_usage_client, std::string const& parent,
    std::vector<std::string> const& project_ids, Options const& options) {
  // If we have a small list of projects then we can use GetProject instead
  // of list projects.
  if (project_ids.size() <= kSmallProjectNum) {
    std::vector<Project> projects;
    for (auto const& project_id : project_ids) {
      auto project_status = GetProjectRM(
          projects_rm_client, service_usage_client, project_id, options);
      if (!project_status) {
        // We skip any projects we cannot get via Resource Manager.
        continue;
      }
      projects.push_back(*project_status);
    }
    return projects;
  }

  StatusRecordOr<std::vector<Project>> bq_all_projects = ListAllProjectsRM(
      projects_rm_client, service_usage_client, parent, options);

  if (!bq_all_projects) {
    LOG(ERROR) << "FilterProjectsRMList::ListAllProjectsRM:: "
               << bq_all_projects.GetStatusRecord().message;
    return bq_all_projects.GetStatusRecord();
  }

  std::vector<Project> projects;
  for (auto const& project : *bq_all_projects) {
    if (std::find(project_ids.begin(), project_ids.end(), project.id) !=
        project_ids.end()) {
      projects.push_back(project);
    }
  }

  return projects;
}

StatusRecordOr<std::vector<Project>> FilterProjectsRMSearch(
    ProjectsClient& projects_rm_client,
    ServiceUsageClient& service_usage_client, std::string const& query,
    std::vector<std::string> const& project_ids, Options const& options) {
  StatusRecordOr<std::vector<Project>> bq_all_projects = SearchProjectsRM(
      projects_rm_client, service_usage_client, query, options);

  if (!bq_all_projects) {
    LOG(ERROR) << "FilterProjectsRMSearch::SearchProjectsRM:: "
               << bq_all_projects.GetStatusRecord().message;
    return bq_all_projects.GetStatusRecord();
  }

  std::vector<Project> projects;
  for (auto const& project : *bq_all_projects) {
    if (std::find(project_ids.begin(), project_ids.end(), project.id) !=
        project_ids.end()) {
      projects.push_back(project);
    }
  }

  return projects;
}

}  // namespace google::cloud::odbc_bigquery_client_interface
