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

#include <fstream>
#include <gtest/gtest.h>

#include "google/cloud/internal/getenv.h"

#include "google/cloud/odbc/client_interface/odbc_authentication.h"

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bigquery_client_interface {

using google::cloud::internal::GetEnv;

TEST(UserAuthentication, HappyPath) {
  std::string refresh_token = "my-token";

  auto credentials = CreateCredentials(OauthMechanism::kAuthorizedUser, "", refresh_token);

  EXPECT_TRUE(credentials);
  auto file_path = GetEnv("GOOGLE_APPLICATION_CREDENTIALS");
  EXPECT_TRUE(file_path);
  auto is = std::ifstream(AuthorizedUserFilePath());
  is.exceptions(std::ios::badbit);  // Minimal error handling
  auto content = std::string(std::istreambuf_iterator<char>(is.rdbuf()), {});
  EXPECT_TRUE(content.find(refresh_token) != std::string::npos);
}

}  // namespace odbc_bigquery_client_interface
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)
