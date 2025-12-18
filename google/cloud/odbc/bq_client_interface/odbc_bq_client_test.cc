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
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace google::cloud::odbc_bigquery_client_interface {

using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(ODBCBQClient, CreateBQClientFailsWithInvalidCredentials) {
  auto odbc_bq_client = ODBCBQClient::CreateBQClient(
      {OauthMechanism::kServiceAndUserAccount, ""});

  auto const expected_sql_state = odbc_internal::SQLStates::k_HY000();
  auto const expected_message =
      HasSubstr("The path to the file can't be empty");
  auto const expected_status =
      StatusRecordIs(expected_sql_state, expected_message);

  EXPECT_THAT(odbc_bq_client, expected_status);
}

#ifdef _WIN32
static std::string ReadFile(std::string const& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

TEST(ExportWindowsSystemCertsToPemTest, PemContainsCertificateMarkers) {
  std::string pem_path = ExportWindowsSystemCertsToPem();
  ASSERT_FALSE(pem_path.empty());
  ASSERT_TRUE(std::filesystem::exists(pem_path)) << "PEM file should exist";
  ASSERT_EQ(std::filesystem::path(pem_path).extension(), ".pem");

  std::string contents = ReadFile(pem_path);
  ASSERT_FALSE(contents.empty()) << "PEM file should not be empty";
  ASSERT_NE(contents.find("BEGIN CERTIFICATE"), std::string::npos)
      << "PEM should contain BEGIN CERTIFICATE";
  ASSERT_NE(contents.find("END CERTIFICATE"), std::string::npos)
      << "PEM should contain END CERTIFICATE";
}

#endif
}  // namespace google::cloud::odbc_bigquery_client_interface
