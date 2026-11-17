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
#include "google/cloud/odbc/bq_client_interface/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <grpcpp/security/credentials.h>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace google::cloud::odbc_bigquery_client_interface {

using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(ODBCBQClient, CreateBQClientFailsWithInvalidCredentials) {
  auto odbc_bq_client =
      ODBCBQClient::CreateBQClient({OauthMechanism::kServiceAccount, ""});

  auto const expected_sql_state = odbc_internal::SQLStates::k_HY000();
  auto const expected_message =
      HasSubstr("The path to the file can't be empty");
  auto const expected_status =
      StatusRecordIs(expected_sql_state, expected_message);

  EXPECT_THAT(odbc_bq_client, expected_status);
}

TEST(ReadFileTest, EmptyPathReturnsError) {
  auto result = ReadFile("");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.GetStatusRecord().message,
            "The path to the file can't be empty");
}

TEST(ReadFileTest, NonExistentFileReturnsError) {
  auto result = ReadFile("non_existent_file_path_12345.pem");
  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.GetStatusRecord().message,
              HasSubstr("Could not open file:"));
}

#ifdef _WIN32
TEST(ExportWindowsSystemCertsToPemTest, PemContainsCertificateMarkers) {
  // Call the function
  auto result = ExportWindowsSystemCertsToPem();
  ASSERT_STATUS_RECORD_OK(result);

  // Extract the path from the StatusRecordOr
  std::string pem_path = *result;

  ASSERT_FALSE(pem_path.empty());
  ASSERT_TRUE(std::filesystem::exists(pem_path))
      << "PEM file should exist at " << pem_path;
  ASSERT_EQ(std::filesystem::path(pem_path).extension(), ".pem");

  auto contents_or = ReadFile(pem_path);
  ASSERT_STATUS_RECORD_OK(contents_or);
  std::string contents = *contents_or;
  ASSERT_FALSE(contents.empty()) << "PEM file should not be empty";
  ASSERT_NE(contents.find("BEGIN CERTIFICATE"), std::string::npos)
      << "PEM should contain BEGIN CERTIFICATE";
  ASSERT_NE(contents.find("END CERTIFICATE"), std::string::npos)
      << "PEM should contain END CERTIFICATE";

  // Passing a filesystem path string directly to pem_root_certs fails
  // gRPC credentials creation with nullptr (the bug in v1.3.4).
  grpc::SslCredentialsOptions bad_ssl_opts;
  bad_ssl_opts.pem_root_certs = pem_path;
  auto bad_creds = grpc::SslCredentials(bad_ssl_opts);
  EXPECT_EQ(bad_creds, nullptr);

  // Passing the PEM certificate contents succeeds and creates valid
  // credentials.
  grpc::SslCredentialsOptions good_ssl_opts;
  good_ssl_opts.pem_root_certs = contents;
  auto good_creds = grpc::SslCredentials(good_ssl_opts);
  EXPECT_NE(good_creds, nullptr);

  // Cleanup: Since the filename is now fixed/cached, remove it after the test
  // so the next run validates the generation logic again rather than just the
  // cache check.
  std::filesystem::remove(pem_path);
}

#endif
}  // namespace google::cloud::odbc_bigquery_client_interface
