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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_DATASETS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_DATASETS_H

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include <optional>
#include <regex>

namespace google::cloud::odbc_bigquery_client_interface {

// Filters used for filtering a list of Datasets
// returned in Dataset response.
struct DatasetFilter {
  // Allows filtering of datasets by labels.
  std::string filter;
  // Whether to include hidden datasets in results.
  bool all = false;
};

// Returns detailed info for a specific Dataset.
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Dataset>
GetDataset(::google::cloud::bigquery_v2_minimal_internal::DatasetClient&
               dataset_client,
           std::string const& project_id, std::string const& dataset_id,
           ::google::cloud::Options const& options);

// Returns all Datasets in a Project.
odbc_internal::StatusRecordOr<std::vector<
    ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset>>
ListAllDatasets(::google::cloud::bigquery_v2_minimal_internal::DatasetClient&
                    dataset_client,
                std::string const& project_id,
                ::google::cloud::Options const& options);

// Returns filtered list of datasets in a Project, based on the dataset
// filters passed in.
odbc_internal::StatusRecordOr<std::vector<
    ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset>>
FilterDatasets(::google::cloud::bigquery_v2_minimal_internal::DatasetClient&
                   dataset_client,
               std::string const& project_id,
               DatasetFilter const& dataset_filter,
               ::google::cloud::Options const& options);

// Returns list of dataset ids for a Project, filtered based on regex for
// dataset_id If regex is empty - returns all
odbc_internal::StatusRecordOr<std::vector<std::string>> FilterDatasetIds(
    ::google::cloud::bigquery_v2_minimal_internal::DatasetClient&
        dataset_client,
    std::string const& project_id,
    std::optional<std::regex> const& regex_filter,
    ::google::cloud::Options const& options);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_DATASETS_H
