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

#include "google/cloud/odbc/bq_client_interface/datasets.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::GetDatasetRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListDatasetsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;

StatusOr<Dataset> GetDataset(DatasetClient& dataset_client,
                             std::string const& project_id,
                             std::string const& dataset_id,
                             Options const& options) {
  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  return dataset_client.GetDataset(request, options);
}

StatusOr<std::vector<ListFormatDataset>> ListAllDatasets(
    DatasetClient& dataset_client, std::string const& project_id,
    Options const& options) {
  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_all_datasets(true);

  StreamRange<ListFormatDataset> datasets_response =
      dataset_client.ListDatasets(request, options);

  std::vector<ListFormatDataset> datasets;
  for (auto const& dataset : datasets_response) {
    if (!dataset) {
      return dataset.status();
    }
    datasets.push_back(*dataset);
  }

  return datasets;
}

StatusOr<std::vector<ListFormatDataset>> FilterDatasets(
    DatasetClient& dataset_client, std::string const& project_id,
    DatasetFilter const& dataset_filter, Options const& options) {
  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_all_datasets(dataset_filter.all);
  request.set_filter(dataset_filter.filter);

  StreamRange<ListFormatDataset> datasets_response =
      dataset_client.ListDatasets(request, options);

  std::vector<ListFormatDataset> datasets;
  for (auto const& dataset : datasets_response) {
    if (!dataset) {
      return dataset.status();
    }
    datasets.push_back(*dataset);
  }

  return datasets;
}

}  // namespace google::cloud::odbc_bigquery_client_interface
