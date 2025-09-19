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

#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/storage/v1/bigquery_read_client.h"
#include <absl/log/log.h>

namespace google::cloud::odbc_bigquery_client_interface {

using ::google::cloud::Options;
using ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest;
using ::google::cloud::bigquery::storage::v1::ReadRowsRequest;
using ::google::cloud::bigquery::storage::v1::ReadRowsResponse;
using ::google::cloud::bigquery::storage::v1::ReadSession;
using ::google::cloud::bigquery_storage_v1::BigQueryReadClient;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

StatusRecordOr<ReadSession> CreateReadSession(
    BigQueryReadClient& bigquery_read_client,
    CreateReadSessionRequest const& read_session_request,
    Options const& options) {
  return StatusRecordOr<ReadSession>::ConvertFromStatusOr(
      bigquery_read_client.CreateReadSession(read_session_request, options));
}

StatusRecordOr<std::vector<ReadRowsResponse>> ReadRows(
    BigQueryReadClient& bigquery_read_client,
    ReadRowsRequest const& read_rows_request, int max_read_responses,
    Options const& options) {
  if (max_read_responses < 0) {
    LOG(ERROR) << "ReadRows:: max_read_responses should be non-negative";
    return StatusRecord{odbc_internal::SQLStates::k_HY000(),
                        "max_read_responses should be non-negative"};
  }
  StreamRange<ReadRowsResponse> read_rows_response_range =
      bigquery_read_client.ReadRows(read_rows_request, options);

  std::vector<ReadRowsResponse> read_rows_responses;
  for (auto const& read_rows_response : read_rows_response_range) {
    if (read_rows_responses.size() == max_read_responses) {
      break;
    }
    if (!read_rows_response) {
      return StatusRecord::ConvertFrom(read_rows_response.status());
    }
    read_rows_responses.push_back(*read_rows_response);
  }

  return read_rows_responses;
}

}  // namespace google::cloud::odbc_bigquery_client_interface
