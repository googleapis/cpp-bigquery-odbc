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

#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::GetTableRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatTable;
using ::google::cloud::bigquery_v2_minimal_internal::ListTablesRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;

StatusOr<Table> GetTable(TableClient& table_client,
                         std::string const& project_id,
                         std::string const& dataset_id,
                         std::string const& table_id, Options const& options) {
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_id);

  return table_client.GetTable(request, options);
}

StatusOr<std::vector<ListFormatTable>> ListAllTables(
    TableClient& table_client, std::string const& project_id,
    std::string const& dataset_id, Options const& options) {
  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  StreamRange<ListFormatTable> tables_response =
      table_client.ListTables(request, options);

  std::vector<ListFormatTable> tables;
  for (auto const& table : tables_response) {
    if (!table) {
      return table.status();
    }
    tables.push_back(*table);
  }

  return tables;
}

}  // namespace google::cloud::odbc_bigquery_client_interface
