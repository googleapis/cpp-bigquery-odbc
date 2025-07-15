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

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bigquery_client_interface {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(ODBCBQClient, CreateBQClientFailsWithInvalidCredentials) {
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAndUserAccount, ""});

  auto const sql_state = odbc_internal::SQLStates::k_HY000();
  auto const message_matcher = HasSubstr("The path to the file can't be empty");
  auto const status_matcher = StatusRecordIs(sql_state, message_matcher);

  EXPECT_THAT(odbc_bq_client, status_matcher);
}

}  // namespace google::cloud::odbc_bigquery_client_interface
