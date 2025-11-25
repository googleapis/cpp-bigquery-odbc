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
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"
#include <absl/log/log.h>
#include <chrono>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::Dataset;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::GetDatasetRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListDatasetsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

#pragma clang attribute push(__attribute__((no_sanitize("memory"))), \
                             apply_to = function)
StatusRecordOr<Dataset> GetDataset(DatasetClient& dataset_client,
                                   std::string const& project_id,
                                   std::string const& dataset_id,
                                   Options const& options) {
  auto start_time = std::chrono::steady_clock::now();
  GetDatasetRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  LOG(INFO) << "GetDataSet:: Request body: " << request.DebugString("");

  auto response = dataset_client.GetDataset(request, options);
  auto elapsed_time = std::chrono::steady_clock::now() - start_time;
  LOG(INFO) << "GetDataset:: Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms";

  if (!response.ok()) {
    LOG(WARNING) << "GetDataSet:: Request failed: " << response.status();
    return StatusRecordOr<Dataset>::ConvertFromStatusOr(response.status());
  }
  LOG(INFO) << "GetDataSet:: Response body: "
            << GetJsonRegResp<Dataset>(*response);
  return StatusRecordOr<Dataset>::ConvertFromStatusOr(*response);
}
#pragma clang attribute pop

#pragma clang attribute push(__attribute__((no_sanitize("memory"))), \
                             apply_to = function)
StatusRecordOr<std::vector<ListFormatDataset>> ListAllDatasets(
    DatasetClient& dataset_client, std::string const& project_id,
    Options const& options) {
  auto start_time = std::chrono::steady_clock::now();
  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_all_datasets(true);
  LOG(INFO) << "ListAllDatasets:: Request body: " << request.DebugString("");
  StreamRange<ListFormatDataset> datasets_response =
      dataset_client.ListDatasets(request, options);
  auto elapsed_time = std::chrono::steady_clock::now() - start_time;
  LOG(INFO) << "ListAllDatasets:: Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms";

  std::vector<ListFormatDataset> datasets;
  for (auto const& dataset : datasets_response) {
    if (!dataset) {
      LOG(ERROR) << "ListAllDatasets:: " << dataset.status().message();
      return StatusRecord::ConvertFrom(dataset.status());
    }
    LOG(INFO) << "ListAllDatasets:: Response body: "
              << GetJsonRegResp<ListFormatDataset>(*dataset);
    datasets.push_back(*dataset);
  }

  return datasets;
}
#pragma clang attribute pop

#pragma clang attribute push(__attribute__((no_sanitize("memory"))), \
                             apply_to = function)
StatusRecordOr<std::vector<ListFormatDataset>> FilterDatasets(
    DatasetClient& dataset_client, std::string const& project_id,
    DatasetFilter const& dataset_filter, Options const& options) {
  auto start_time = std::chrono::steady_clock::now();
  ListDatasetsRequest request;
  request.set_project_id(project_id);
  request.set_all_datasets(dataset_filter.all);
  request.set_filter(dataset_filter.filter);
  LOG(INFO) << "FilterDatasets:: Request body: " << request.DebugString("");

  StreamRange<ListFormatDataset> datasets_response =
      dataset_client.ListDatasets(request, options);
  auto elapsed_time = std::chrono::steady_clock::now() - start_time;
  LOG(INFO) << "FilterDatasets:: Elapsed time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count()
            << "ms";

  std::vector<ListFormatDataset> datasets;
  for (auto const& dataset : datasets_response) {
    if (!dataset) {
      LOG(ERROR) << "FilterDatasets:: " << dataset.status().message();
      return StatusRecord::ConvertFrom(dataset.status());
    }
    LOG(INFO) << "FilterDatasets:: Response body: "
              << GetJsonRegResp<ListFormatDataset>(*dataset);
    datasets.push_back(*dataset);
  }

  return datasets;
}
#pragma clang attribute pop

}  // namespace google::cloud::odbc_bigquery_client_interface
