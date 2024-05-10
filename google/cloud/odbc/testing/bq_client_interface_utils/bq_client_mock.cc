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

#include "google/cloud/odbc/testing/bq_client_interface_utils/bq_client_mock.h"

namespace google::cloud::odbc_testing_bq_client_interface_utils {

using ::google::cloud::bigquery_storage_v1::BigQueryReadClient;
using ::google::cloud::bigquery_storage_v1_mocks::MockBigQueryReadConnection;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetClient;
using ::google::cloud::bigquery_v2_minimal_internal::JobClient;
using ::google::cloud::bigquery_v2_minimal_internal::MockBigQueryJobConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MockDatasetConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MockProjectConnection;
using ::google::cloud::bigquery_v2_minimal_internal::MockTableConnection;
using ::google::cloud::bigquery_v2_minimal_internal::ProjectClient;
using ::google::cloud::bigquery_v2_minimal_internal::TableClient;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClientMockBuilder;

BQClientMocks CreateBQClientMocks() {
  Options options;

  auto mock_dataset_connection = std::make_shared<MockDatasetConnection>();
  EXPECT_CALL(*mock_dataset_connection, options);
  DatasetClient mock_dataset_client(mock_dataset_connection);

  auto mock_job_connection = std::make_shared<MockBigQueryJobConnection>();
  EXPECT_CALL(*mock_job_connection, options);
  JobClient mock_job_client(mock_job_connection);

  auto mock_project_connection = std::make_shared<MockProjectConnection>();
  EXPECT_CALL(*mock_project_connection, options);
  ProjectClient mock_project_client(mock_project_connection);

  auto mock_table_connection = std::make_shared<MockTableConnection>();
  EXPECT_CALL(*mock_table_connection, options);
  TableClient mock_table_client(mock_table_connection);

  auto mock_bq_read_connection = std::make_shared<MockBigQueryReadConnection>();
  EXPECT_CALL(*mock_bq_read_connection, options);
  BigQueryReadClient mock_bq_read_client(mock_bq_read_connection);

  ODBCBQClient mock_bq_client = ODBCBQClientMockBuilder::CreateBQClientMock(
      mock_dataset_client, mock_job_client, mock_project_client,
      mock_table_client, nullptr, mock_bq_read_client);

  return {mock_bq_client,        mock_dataset_connection,
          mock_job_connection,   mock_project_connection,
          mock_table_connection, mock_bq_read_connection};
}

}  // namespace google::cloud::odbc_testing_bq_client_interface_utils
