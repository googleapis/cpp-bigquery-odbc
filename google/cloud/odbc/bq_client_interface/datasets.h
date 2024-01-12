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

#include "google/cloud/bigquery/v2/minimal/internal/dataset_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

StatusOr<::google::cloud::bigquery_v2_minimal_internal::Dataset> GetDataset(
    ::google::cloud::bigquery_v2_minimal_internal::DatasetClient&
        dataset_client,
    std::string const& project_id, std::string const& dataset_id,
    ::google::cloud::Options const& options);

StatusOr<std::vector<
    ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset>>
ListAllDatasets(::google::cloud::bigquery_v2_minimal_internal::DatasetClient&
                    dataset_client,
                std::string const& project_id,
                ::google::cloud::Options const& options);

}  // namespace google::cloud::odbc_bigquery_client_interface
