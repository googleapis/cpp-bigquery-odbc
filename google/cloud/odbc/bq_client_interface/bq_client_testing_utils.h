// Copyright 2024 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_BQ_CLIENT_TESTING_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_BQ_CLIENT_TESTING_UTILS_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/bigquery/storage/v1/mocks/mock_bigquery_read_connection.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_dataset_connection.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_job_connection.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_project_connection.h"
#include "google/cloud/bigquery/v2/minimal/mocks/mock_table_connection.h"

namespace google::cloud::odbc_bigquery_client_interface {

struct BQClientMocks {
  ODBCBQClient mock_bq_client;
  std::shared_ptr<
      ::google::cloud::bigquery_v2_minimal_internal::MockDatasetConnection>
      mock_dataset_connection;
  std::shared_ptr<
      ::google::cloud::bigquery_v2_minimal_internal::MockBigQueryJobConnection>
      mock_job_connection;
  std::shared_ptr<
      ::google::cloud::bigquery_v2_minimal_internal::MockProjectConnection>
      mock_project_connection;
  std::shared_ptr<
      ::google::cloud::bigquery_v2_minimal_internal::MockTableConnection>
      mock_table_connection;
  std::shared_ptr<
      ::google::cloud::bigquery_storage_v1_mocks::MockBigQueryReadConnection>
      mock_bq_read_connection;
};

BQClientMocks CreateBQClientMocks();

}  // namespace google::cloud::odbc_bigquery_client_interface

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_CLIENT_INTERFACE_BQ_CLIENT_TESTING_UTILS_H
