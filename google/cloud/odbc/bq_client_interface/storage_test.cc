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

#include "google/cloud/odbc/bq_client_interface/storage.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/bigquery/storage/v1/mocks/mock_bigquery_read_connection.h"
#include "google/cloud/mocks/mock_stream_range.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest;
using ::google::cloud::bigquery::storage::v1::ReadRowsRequest;
using ::google::cloud::bigquery::storage::v1::ReadRowsResponse;
using ::google::cloud::bigquery::storage::v1::ReadSession;
using ::google::cloud::bigquery_storage_v1::BigQueryReadClient;
using ::google::cloud::bigquery_storage_v1_mocks::MockBigQueryReadConnection;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using google::cloud::odbc_internal::StatusRecordOr;
    using google::cloud::odbc_internal::SQLStates;
using ::testing::HasSubstr;

TEST(CreateReadSession, CreateReadSessionSuccess) {
  auto mock = std::make_shared<MockBigQueryReadConnection>();
  Options options;
  ReadSession read_session;
  CreateReadSessionRequest create_read_session_request;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, CreateReadSession)
      .WillOnce([&](CreateReadSessionRequest const&) {
        return make_status_or(read_session);
      });
  BigQueryReadClient mocked_bigquery_read_client(std::move(mock));

  StatusRecordOr<ReadSession> actual = CreateReadSession(
      mocked_bigquery_read_client, create_read_session_request, options);

      ASSERT_STATUS_RECORD_OK(actual);
}

TEST(ReadRows, ReadRowsSuccessSuccess) {
  auto mock = std::make_shared<MockBigQueryReadConnection>();
  Options options;
  ReadRowsResponse read_rows_response;
  ReadRowsRequest read_rows_request;
  int max_read_responses = 1;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadRows).WillOnce([&](ReadRowsRequest const&) {
    return mocks::MakeStreamRange<ReadRowsResponse>({read_rows_response});
  });
  BigQueryReadClient mocked_bigquery_read_client(std::move(mock));

  StatusRecordOr<std::vector<ReadRowsResponse>> actual =
      ReadRows(mocked_bigquery_read_client, read_rows_request,
               max_read_responses, options);

      ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(1, (*actual).size());
}

TEST(ReadRows, MaxReadResponsesIsZero) {
  auto mock = std::make_shared<MockBigQueryReadConnection>();
  Options options;
  ReadRowsResponse read_rows_response;
  ReadRowsRequest read_rows_request;
  int max_read_responses = 0;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadRows).WillOnce([&](ReadRowsRequest const&) {
    return mocks::MakeStreamRange<ReadRowsResponse>({read_rows_response});
  });
  BigQueryReadClient mocked_bigquery_read_client(std::move(mock));

  StatusRecordOr<std::vector<ReadRowsResponse>> actual =
      ReadRows(mocked_bigquery_read_client, read_rows_request,
               max_read_responses, options);

  ASSERT_STATUS_RECORD_OK(actual);
  EXPECT_EQ(0, (*actual).size());
}

TEST(ReadRows, MaxReadResponsesIsNegative) {
  auto mock = std::make_shared<MockBigQueryReadConnection>();
  Options options;
  ReadRowsRequest read_rows_request;
  int max_read_responses = -1;
  EXPECT_CALL(*mock, options);
  BigQueryReadClient mocked_bigquery_read_client(std::move(mock));

  StatusRecordOr<std::vector<ReadRowsResponse>> actual =
      ReadRows(mocked_bigquery_read_client, read_rows_request,
               max_read_responses, options);

  EXPECT_THAT(actual,
              StatusRecordIs(SQLStates::k_HY000(),
                       HasSubstr("max_read_responses should be non-negative")));
}

TEST(ReadRows, UnauthenticatedRequest) {
  auto mock = std::make_shared<MockBigQueryReadConnection>();
  Options options;
  ReadRowsRequest read_rows_request;
  int max_read_responses = 1;
  EXPECT_CALL(*mock, options);
  EXPECT_CALL(*mock, ReadRows).WillOnce([&](ReadRowsRequest const&) {
    return mocks::MakeStreamRange<ReadRowsResponse>(
        {}, Status(StatusCode::kUnauthenticated, "denied"));
  });
  BigQueryReadClient mocked_bigquery_read_client(std::move(mock));

  StatusRecordOr<std::vector<ReadRowsResponse>> actual =
      ReadRows(mocked_bigquery_read_client, read_rows_request,
               max_read_responses, options);

  EXPECT_THAT(actual, StatusRecordIs(SQLStates::k_28000(), HasSubstr("denied")));
}

}  // namespace google::cloud::odbc_bigquery_client_interface
