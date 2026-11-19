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
#include <gmock/gmock.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

namespace google::cloud::odbc_bigquery_client_interface {

using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;
using ::testing::Not;

namespace {

// DER-encoded self-signed public X.509 test certificate
// (CN=odbc-bq-unit-test-ca; private key discarded at generation time) used to
// verify PEM loading in gRPC without triggering spellcheckers on base64 text.
constexpr std::uint8_t kTestCaCertDer[] = {
    0x30, 0x82, 0x01, 0x92, 0x30, 0x82, 0x01, 0x39, 0xa0, 0x03, 0x02, 0x01,
    0x02, 0x02, 0x14, 0x10, 0x85, 0x23, 0xd9, 0xd5, 0x92, 0x00, 0x21, 0xb4,
    0x8c, 0x4d, 0x49, 0x65, 0xb3, 0x94, 0x1b, 0x8e, 0x15, 0xc4, 0x45, 0x30,
    0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x30,
    0x1f, 0x31, 0x1d, 0x30, 0x1b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x14,
    0x6f, 0x64, 0x62, 0x63, 0x2d, 0x62, 0x71, 0x2d, 0x75, 0x6e, 0x69, 0x74,
    0x2d, 0x74, 0x65, 0x73, 0x74, 0x2d, 0x63, 0x61, 0x30, 0x1e, 0x17, 0x0d,
    0x32, 0x36, 0x31, 0x31, 0x31, 0x30, 0x31, 0x31, 0x33, 0x34, 0x35, 0x39,
    0x5a, 0x17, 0x0d, 0x33, 0x36, 0x31, 0x31, 0x30, 0x37, 0x31, 0x31, 0x33,
    0x34, 0x35, 0x39, 0x5a, 0x30, 0x1f, 0x31, 0x1d, 0x30, 0x1b, 0x06, 0x03,
    0x55, 0x04, 0x03, 0x0c, 0x14, 0x6f, 0x64, 0x62, 0x63, 0x2d, 0x62, 0x71,
    0x2d, 0x75, 0x6e, 0x69, 0x74, 0x2d, 0x74, 0x65, 0x73, 0x74, 0x2d, 0x63,
    0x61, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    0x03, 0x42, 0x00, 0x04, 0x55, 0xac, 0x82, 0xf4, 0xcf, 0x39, 0x3f, 0xc2,
    0xcc, 0xd7, 0x05, 0x22, 0xbb, 0x4a, 0x7e, 0xfb, 0xb1, 0x25, 0x3a, 0x77,
    0xdd, 0x27, 0xad, 0x6c, 0x0f, 0x00, 0xf3, 0x17, 0x73, 0x88, 0x5b, 0x84,
    0x82, 0x81, 0x40, 0x07, 0x27, 0x0d, 0x0d, 0x88, 0x05, 0x9d, 0x56, 0x37,
    0xd0, 0x92, 0x92, 0x22, 0x39, 0x41, 0xbc, 0x7c, 0x95, 0x8e, 0x7c, 0x76,
    0xcf, 0x23, 0xa1, 0x0e, 0xd8, 0x2d, 0xe1, 0x6e, 0xa3, 0x53, 0x30, 0x51,
    0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x67,
    0xd8, 0xf3, 0x62, 0xc6, 0x86, 0xcb, 0x58, 0x36, 0x86, 0x8e, 0x72, 0x26,
    0xd3, 0x7d, 0xd2, 0xf8, 0x51, 0x76, 0x76, 0x30, 0x1f, 0x06, 0x03, 0x55,
    0x1d, 0x23, 0x04, 0x18, 0x30, 0x16, 0x80, 0x14, 0x67, 0xd8, 0xf3, 0x62,
    0xc6, 0x86, 0xcb, 0x58, 0x36, 0x86, 0x8e, 0x72, 0x26, 0xd3, 0x7d, 0xd2,
    0xf8, 0x51, 0x76, 0x76, 0x30, 0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
    0x01, 0xff, 0x04, 0x05, 0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0a, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x47, 0x00,
    0x30, 0x44, 0x02, 0x20, 0x75, 0x90, 0xea, 0x31, 0x61, 0xc7, 0xb4, 0x9e,
    0xfe, 0x2d, 0x15, 0xc9, 0x67, 0x75, 0xa3, 0x73, 0x60, 0xc1, 0xc0, 0x01,
    0x98, 0x27, 0x68, 0x0d, 0xdf, 0xae, 0xe1, 0x1b, 0x6c, 0x48, 0xfb, 0x52,
    0x02, 0x20, 0x5b, 0xe4, 0x58, 0xd5, 0x05, 0x3a, 0x60, 0x38, 0xf6, 0xdd,
    0x0f, 0xe4, 0x4b, 0xa0, 0x9e, 0x94, 0x0f, 0xbd, 0xd6, 0x8c, 0x02, 0x33,
    0x1e, 0x35, 0xbc, 0x05, 0x47, 0x9b, 0x26, 0x47, 0xcc, 0xe6,
};

char EncodeBase64Sextet(std::uint32_t value) {
  if (value < 26) return static_cast<char>('A' + value);
  if (value < 52) return static_cast<char>('a' + (value - 26));
  if (value < 62) return static_cast<char>('0' + (value - 52));
  return value == 62 ? '+' : '/';
}

std::string TestCaCertPem() {
  std::string b64;
  std::size_t const len = sizeof(kTestCaCertDer);
  b64.reserve(((len + 2) / 3) * 4);
  for (std::size_t i = 0; i < len; i += 3) {
    std::uint32_t const b0 = kTestCaCertDer[i];
    std::uint32_t const b1 = (i + 1 < len) ? kTestCaCertDer[i + 1] : 0;
    std::uint32_t const b2 = (i + 2 < len) ? kTestCaCertDer[i + 2] : 0;
    std::uint32_t const triple = (b0 << 16U) | (b1 << 8U) | b2;
    b64.push_back(EncodeBase64Sextet((triple >> 18U) & 0x3fU));
    b64.push_back(EncodeBase64Sextet((triple >> 12U) & 0x3fU));
    b64.push_back((i + 1 < len) ? EncodeBase64Sextet((triple >> 6U) & 0x3fU)
                                : '=');
    b64.push_back((i + 2 < len) ? EncodeBase64Sextet(triple & 0x3fU) : '=');
  }
  std::string pem = "-----BEGIN CERTIFICATE-----\n";
  for (std::size_t i = 0; i < b64.size(); i += 64) {
    pem.append(b64, i, 64);
    pem.push_back('\n');
  }
  pem.append("-----END CERTIFICATE-----\n");
  return pem;
}

class ScopedTempFile {
 public:
  ScopedTempFile(std::string const& filename, std::string const& contents)
      : path_(std::filesystem::temp_directory_path() / filename) {
    std::ofstream out(path_, std::ios::out | std::ios::binary);
    out << contents;
  }

  ~ScopedTempFile() {
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

  ScopedTempFile(ScopedTempFile const&) = delete;
  ScopedTempFile& operator=(ScopedTempFile const&) = delete;
  ScopedTempFile(ScopedTempFile&&) = delete;
  ScopedTempFile& operator=(ScopedTempFile&&) = delete;

  std::string PathString() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

ConnProps MakeValidTestConnProps(std::string pem_root_certs) {
  ConnProps conn_props{};
  conn_props.auth_mechanism = OauthMechanism::kExternalUser;
  conn_props.byoid_aud_url =
      "//iam.googleapis.com/locations/global/workforcePools/test-pool/"
      "providers/test-provider";
  conn_props.byoid_creds_src = R"({"file": "test-subject-token"})";
  conn_props.byoid_subj_token_type =
      "urn:ietf:params:oauth:token-type:id_token";
  conn_props.byoid_token_url = "https://sts.googleapis.com/v1/token";
  conn_props.ssl_credentials.pem_root_certs = std::move(pem_root_certs);
  return conn_props;
}

}  // namespace

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

TEST(ODBCBQClient, ReadPemFileContentsHandlesEmptyMissingAndEmptyFiles) {
  EXPECT_TRUE(ReadPemFileContents("").empty());

  auto const missing_path = (std::filesystem::temp_directory_path() /
                             "odbc_bq_nonexistent_ca_roots_9f4b2c.pem")
                                .string();
  std::error_code ec;
  std::filesystem::remove(missing_path, ec);
  EXPECT_TRUE(ReadPemFileContents(missing_path).empty());

  ScopedTempFile const empty_pem("odbc_bq_test_empty_roots.pem", "");
  EXPECT_TRUE(ReadPemFileContents(empty_pem.PathString()).empty());
}

TEST(ODBCBQClient, ReadPemFileContentsPreservesCertificateBundleAndCrlf) {
  std::string const cert_pem = TestCaCertPem();
  std::string bundle;
  bundle.reserve(cert_pem.size() * 2 + 32);
  bundle.append(cert_pem);
  for (char ch : cert_pem) {
    if (ch == '\n') {
      bundle.append("\r\n");
    } else {
      bundle.push_back(ch);
    }
  }

  ScopedTempFile const bundle_pem("odbc_bq_test_bundle_roots.pem", bundle);
  EXPECT_EQ(ReadPemFileContents(bundle_pem.PathString()), bundle);
}

TEST(ODBCBQClient, GrpcSslCredentialsAcceptsLoadedPemContents) {
  ScopedTempFile const valid_pem("odbc_bq_test_grpc_ssl_roots.pem",
                                 TestCaCertPem());

  grpc::SslCredentialsOptions ssl_opts;
  ssl_opts.pem_root_certs = ReadPemFileContents(valid_pem.PathString());
  ASSERT_FALSE(ssl_opts.pem_root_certs.empty());

  testing::internal::CaptureStderr();
  auto ssl_creds = grpc::SslCredentials(ssl_opts);
  auto channel = grpc::CreateCustomChannel("127.0.0.1:443", ssl_creds, {});
  std::string const stderr_output = testing::internal::GetCapturedStderr();

  EXPECT_NE(ssl_creds, nullptr);
  EXPECT_NE(channel, nullptr);
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Could not load any root certificate")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Cannot load server root certificates")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Handshaker factory creation failed")));
}

TEST(ODBCBQClient, CreateBQClientLoadsPemRootCertsWithoutSslErrors) {
  ScopedTempFile const valid_pem("odbc_bq_test_ca_roots.pem", TestCaCertPem());
  ConnProps const conn_props = MakeValidTestConnProps(valid_pem.PathString());

  testing::internal::CaptureStderr();
  auto client = ODBCBQClient::CreateBQClient(conn_props);
  std::string const stderr_output = testing::internal::GetCapturedStderr();

  ASSERT_STATUS_RECORD_OK(client);
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Could not load any root certificate")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Cannot load server root certificates")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Handshaker factory creation failed")));
}

TEST(ODBCBQClient, GrpcSslCredentialsRejectsRawFilePathInsteadOfPemContents) {
  ScopedTempFile const valid_pem("odbc_bq_test_raw_path_roots.pem",
                                 TestCaCertPem());

  // Passing a file path directly to pem_root_certs (the pre-fix bug) must fail
  // gRPC TSI root certificate parsing on channel creation, confirming that
  // GrpcSslCredentialsAcceptsLoadedPemContents truly exercises PEM parsing.
  grpc::SslCredentialsOptions bad_ssl_opts;
  bad_ssl_opts.pem_root_certs = valid_pem.PathString();

  testing::internal::CaptureStderr();
  auto ssl_creds = grpc::SslCredentials(bad_ssl_opts);
  auto channel = grpc::CreateCustomChannel("127.0.0.1:443", ssl_creds, {});
  std::string const stderr_output = testing::internal::GetCapturedStderr();

  EXPECT_NE(channel, nullptr);
  EXPECT_THAT(stderr_output, HasSubstr("Could not load any root certificate"));
  EXPECT_THAT(stderr_output, HasSubstr("Handshaker factory creation failed"));
}

TEST(ODBCBQClient, CreateBQClientSkipsMissingPemFileWithoutGrpcSslErrors) {
  auto const missing_path = (std::filesystem::temp_directory_path() /
                             "odbc_bq_nonexistent_client_roots_7a1c3e.pem")
                                .string();
  std::error_code ec;
  std::filesystem::remove(missing_path, ec);

  testing::internal::CaptureStderr();
  auto client =
      ODBCBQClient::CreateBQClient(MakeValidTestConnProps(missing_path));
  std::string const stderr_output = testing::internal::GetCapturedStderr();

  ASSERT_STATUS_RECORD_OK(client);
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Could not load any root certificate")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Cannot load server root certificates")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Handshaker factory creation failed")));
}

TEST(ODBCBQClient, CreateBQClientEmitsGrpcSslErrorOnMalformedPemContents) {
  ScopedTempFile const invalid_pem("odbc_bq_test_invalid_roots.pem",
                                   "not valid PEM data");

  testing::internal::CaptureStderr();
  auto client = ODBCBQClient::CreateBQClient(
      MakeValidTestConnProps(invalid_pem.PathString()));
  std::string const stderr_output = testing::internal::GetCapturedStderr();

  ASSERT_STATUS_RECORD_OK(client);
  EXPECT_THAT(stderr_output, HasSubstr("Could not load any root certificate"));
}

#ifdef _WIN32
static std::string ReadFile(std::string const& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

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

  std::string contents = ReadFile(pem_path);
  ASSERT_FALSE(contents.empty()) << "PEM file should not be empty";
  ASSERT_NE(contents.find("BEGIN CERTIFICATE"), std::string::npos)
      << "PEM should contain BEGIN CERTIFICATE";
  ASSERT_NE(contents.find("END CERTIFICATE"), std::string::npos)
      << "PEM should contain END CERTIFICATE";

  // Cleanup: Since the filename is now fixed/cached, remove it after the test
  // so the next run validates the generation logic again rather than just the
  // cache check.
  std::filesystem::remove(pem_path);
}

TEST(ExportWindowsSystemCertsToPemTest,
     CreateBQClientLoadsSystemTrustStoreWithoutSslErrors) {
  ConnProps conn_props = MakeValidTestConnProps("");
  conn_props.ssl_credentials.use_system_trust_store = true;

  testing::internal::CaptureStderr();
  auto client = ODBCBQClient::CreateBQClient(conn_props);
  std::string const stderr_output = testing::internal::GetCapturedStderr();

  ASSERT_STATUS_RECORD_OK(client);
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Could not load any root certificate")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Cannot load server root certificates")));
  EXPECT_THAT(stderr_output,
              Not(HasSubstr("Handshaker factory creation failed")));

  auto pem_path_or = ExportWindowsSystemCertsToPem();
  if (pem_path_or) {
    std::error_code ec;
    std::filesystem::remove(*pem_path_or, ec);
  }
}

#endif
}  // namespace google::cloud::odbc_bigquery_client_interface
