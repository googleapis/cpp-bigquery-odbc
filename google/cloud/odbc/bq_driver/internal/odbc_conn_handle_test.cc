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

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

TEST(ConnectionHandle, Connect) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path =
      test_data_path + "service_account_auth_keys.json";

  Authentication auth = {AuthMechanism::kServiceAuth, "",
                         credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  Status status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), true);
}

TEST(ConnectionHandle, ConnectWithInvalidFile) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path = test_data_path + "random_file.json";

  Authentication auth = {AuthMechanism::kServiceAuth, "",
                         credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  Status status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
}

TEST(ConnectionHandle, ConnectWithUnImplementedAuth) {
  std::string test_data_path =
      google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_DRIVER_TEST_DATA_PATH")
          .value_or("");
  std::string credentials_file_path =
      test_data_path + "service_account_auth_keys.json";

  Authentication auth = {AuthMechanism::kUserAuth, "", credentials_file_path};
  auto* conn_handle = new ConnectionHandle();
  Status status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_EQ(status.code(), StatusCode::kUnimplemented);
}

}  // namespace google::cloud::odbc_bq_driver_internal
