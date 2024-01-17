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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_STORAGE_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_STORAGE_H

#include "google/cloud/bigquery/storage/v1/bigquery_read_client.h"

namespace google::cloud::odbc_bigquery_client_interface {

StatusOr<::google::cloud::bigquery::storage::v1::ReadSession> CreateReadSession(
    ::google::cloud::bigquery_storage_v1::BigQueryReadClient&
        bigquery_read_client,
    ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest const&
        read_session_request,
    ::google::cloud::Options const& options);

StatusOr<std::vector<google::cloud::bigquery::storage::v1::ReadRowsResponse>>
ReadRows(::google::cloud::bigquery_storage_v1::BigQueryReadClient&
             bigquery_read_client,
         ::google::cloud::bigquery::storage::v1::ReadRowsRequest const&
             read_rows_request,
         int max_read_responses, ::google::cloud::Options const& options);

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_CLIENT_INTERFACE_BQ_STORAGE_H
