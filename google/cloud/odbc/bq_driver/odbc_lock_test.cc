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

#include "google/cloud/odbc/bq_driver/odbc_lock.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::internal::GetEnv;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bq_driver_internal::Authentication;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_internal::StatusRecord;

void CreateConnection(Authentication auth) {
  auto* conn_handle = new ConnectionHandle();
  StatusRecord status = conn_handle->Connect(auth);
  EXPECT_EQ(status.ok(), false);
  EXPECT_FALSE(conn_handle->IsConnected());
  delete conn_handle;
}
TEST(MultithreadingHandleTest, ConnectionHandle_Connect) {
  std::string credentials_file_path =
      GetEnv("CPP_BIGQUERY_ODBC_TEST_SERVICE_ACCOUNT_AUTH_KEY").value_or("");
  Authentication auth = {OauthMechanism::kServiceAccount,
                         credentials_file_path};

  std::vector<std::thread> threads;

  for (int i = 0; i < 4; i++) {
    std::thread t(CreateConnection, auth);
    threads.push_back(std::move(t));
  }

  for (auto& t : threads) {
    t.join();
    std::cout << "Running thread '"
              << "\n";
  }
}

}  // namespace google::cloud::odbc_bq_driver
