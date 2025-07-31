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

#include "google/cloud/odbc/bq_client_interface/tables.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"
#include <absl/log/log.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::GetTableRequest;
using ::google::cloud::bigquery_v2_minimal_internal::ListFormatTable;
using ::google::cloud::bigquery_v2_minimal_internal::ListTablesRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

#pragma clang attribute push(__attribute__((no_sanitize("memory"))), \
                             apply_to = function)
StatusRecordOr<Table> GetTable(TableClient& table_client,
                               std::string const& project_id,
                               std::string const& dataset_id,
                               std::string const& table_id,
                               TableFilter const& table_filter,
                               ::google::cloud::Options const& options) {
  GetTableRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);
  request.set_table_id(table_id);
  request.set_selected_fields(table_filter.selected_fields);
  request.set_view(table_filter.view);
  LOG(INFO) << "GetTable:: Request body: " << request.DebugString("");
  auto response = table_client.GetTable(request, options);
  if (!response.ok()) {
    LOG(WARNING) << "GetTable:: Request failed: " << response.status();
    return StatusRecordOr<Table>::ConvertFromStatusOr(response.status());
  }
  nlohmann::json resp;
  to_json(resp, *response);
  LOG(INFO) << "GetTable:: Response body: " << resp.dump(4);
  return StatusRecordOr<Table>::ConvertFromStatusOr(*response);
}
#pragma clang attribute pop

#pragma clang attribute push(__attribute__((no_sanitize("memory"))), \
                             apply_to = function)
StatusRecordOr<std::vector<ListFormatTable>> ListAllTables(
    TableClient& table_client, std::string const& project_id,
    std::string const& dataset_id, Options const& options) {
  ListTablesRequest request;
  request.set_project_id(project_id);
  request.set_dataset_id(dataset_id);

  LOG(INFO) << "ListAllTables:: Request body: " << request.DebugString("");
  StreamRange<ListFormatTable> tables_response =
      table_client.ListTables(request, options);

  std::vector<ListFormatTable> tables;
  for (auto const& table : tables_response) {
    if (!table) {
      LOG(ERROR) << "ListAllTables:: " << table.status().message();
      return StatusRecord::ConvertFrom(table.status());
    }
    nlohmann::json resp;
    to_json(resp, *table);
    LOG(INFO) << "ListAllTables:: Response body: " << resp.dump(4);
    tables.push_back(*table);
  }

  return tables;
}
#pragma clang attribute pop

}  // namespace google::cloud::odbc_bigquery_client_interface
