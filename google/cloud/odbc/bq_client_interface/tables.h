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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_TABLES_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_TABLES_H

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/table_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

// Filters used for filtering Table details returned in Table response.
struct TableFilter {
  // Allows selected fields to be returned in the Table details.
  std::vector<std::string> const& selected_fields;
  // Allows filtering of table information by view.
  ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView view;
};

// Returns detailed info for a specific Table
odbc_internal::StatusRecordOr<
    ::google::cloud::bigquery_v2_minimal_internal::Table>
GetTable(
    ::google::cloud::bigquery_v2_minimal_internal::TableClient& table_client,
    std::string const& project_id, std::string const& dataset_id,
    std::string const& table_id, TableFilter const& table_filter,
    ::google::cloud::Options const& options);

// Returns all Tables in a Dataset
odbc_internal::StatusRecordOr<
    std::vector<::google::cloud::bigquery_v2_minimal_internal::ListFormatTable>>
ListAllTables(
    ::google::cloud::bigquery_v2_minimal_internal::TableClient& table_client,
    std::string const& project_id, std::string const& dataset_id,
    ::google::cloud::Options const& options);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_TABLES_H
