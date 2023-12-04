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

#include <gtest/gtest.h>

#include "google/cloud/odbc/client_interface/odbc_bq_client.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;

  TEST(ODBCBQClient, Create) {
    std::string refresh_token = "my-token";

    auto odbc_bq_client = ODBCBQClient::Create({OauthMechanism::kAuthorizedUser, "", refresh_token});

    EXPECT_TRUE(odbc_bq_client);
  }

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
