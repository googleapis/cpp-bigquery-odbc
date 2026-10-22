// Copyright 2026 Google LLC
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

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace google::cloud::odbc_tests {
#ifdef BQ_DRIVER_INTEGRATION_TESTS

namespace {

auto constexpr kValidCert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIBfakeleafcertificate\n"
    "-----END CERTIFICATE-----\n";
auto constexpr kValidKey =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEfakeprivatekey\n"
    "-----END PRIVATE KEY-----\n";

class MtlsIntegrationTest : public ::testing::Test {};

void ExpectConnectError(std::string const& conn_str) {
  auto conn = std::make_shared<ODBCHandles>();
  SetAttributes(conn, 30, false);
  SQLCHAR out_conn_str[kBufferLength] = {0};
  SQLSMALLINT out_conn_str_len = 0;
  SQLRETURN status = SQLDriverConnect(
      conn->hdbc, nullptr,
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(conn_str.c_str())), SQL_NTS,
      out_conn_str, sizeof(out_conn_str), &out_conn_str_len,
      SQL_DRIVER_COMPLETE);

  EXPECT_EQ(status, SQL_ERROR);

  SQLCHAR sqlstate[6] = {0};
  SQLINTEGER native_error = 0;
  SQLCHAR buf[kBufferLength] = {0};
  SQLSMALLINT len = 0;
  SQLRETURN diag_status =
      SQLGetDiagRec(SQL_HANDLE_DBC, conn->hdbc, 1, sqlstate, &native_error, buf,
                    kBufferLength, &len);
  if (diag_status != SQL_SUCCESS && conn->henv != nullptr) {
    diag_status = SQLGetDiagRec(SQL_HANDLE_ENV, conn->henv, 1, sqlstate,
                                &native_error, buf, kBufferLength, &len);
  }
  EXPECT_EQ(diag_status, SQL_SUCCESS);
  EXPECT_STREQ(reinterpret_cast<char*>(sqlstate), "HY000");

  CleanupODBCHandles(*conn);
}

void ExpectQuerySuccess(std::string const& conn_str) {
  auto conn = std::make_shared<ODBCHandles>();
  ASSERT_EQ(Connect(conn_str, conn), SQL_SUCCESS);

  SQLRETURN status = SQLExecDirect(
      conn->hstmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>("SELECT 1")),
      SQL_NTS);
  EXPECT_EQ(status, SQL_SUCCESS);

  status = SQLFetch(conn->hstmt);
  EXPECT_EQ(status, SQL_SUCCESS);

  SQLINTEGER val = 0;
  SQLLEN indicator = 0;
  status =
      SQLGetData(conn->hstmt, 1, SQL_C_SLONG, &val, sizeof(val), &indicator);
  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(val, 1);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace

// Verifies that a non-existent client certificate path returns an eager error.
TEST_F(MtlsIntegrationTest, ConnectFail_NonExistentCertPath) {
  std::string conn_str =
      kDefaultConnectionString +
      ";UseClientCertificate=1;"
      "ClientCertificatePath=/path/does/not/exist/no_such_cert.pem;";
  ExpectConnectError(conn_str);
}

// Verifies that invalid endpoint modes are rejected with an error.
TEST_F(MtlsIntegrationTest, ConnectFail_InvalidUseMtlsEndpointValue) {
  std::string conn_str =
      kDefaultConnectionString + ";UseMtlsEndpoint=invalid_endpoint_mode;";
  ExpectConnectError(conn_str);
}

// Verifies that setting UseClientCertificate=0 bypasses certificate loading.
TEST_F(MtlsIntegrationTest, ConnectSuccess_UseClientCertificateDisabled) {
  std::string conn_str = kDefaultConnectionString +
                         ";UseClientCertificate=0;"
                         "ClientCertificatePath=/path/does/not/exist/cert.pem;";
  ExpectQuerySuccess(conn_str);
}

// Verifies that setting UseMtlsEndpoint=never connects over regular TLS.
TEST_F(MtlsIntegrationTest, ConnectSuccess_UseMtlsEndpointNever) {
  std::string conn_str = kDefaultConnectionString + ";UseMtlsEndpoint=never;";
  ExpectQuerySuccess(conn_str);
}

// Verifies that untrusted client certificates are rejected by Google mTLS
// frontend.
TEST_F(MtlsIntegrationTest, UntrustedCert_ServerRejection) {
  auto temp_dir = std::filesystem::temp_directory_path() / "odbc_mtls_it";
  std::filesystem::create_directories(temp_dir);
  auto cert_file = (temp_dir / "client_cert.pem").string();
  auto key_file = (temp_dir / "client_key.pem").string();
  {
    std::ofstream(cert_file, std::ios::binary) << kValidCert;
    std::ofstream(key_file, std::ios::binary) << kValidKey;
  }

  std::string conn_str = kDefaultConnectionString +
                         ";UseClientCertificate=1;"
                         "ClientCertificatePath=" +
                         cert_file +
                         ";"
                         "ClientPrivateKeyPath=" +
                         key_file +
                         ";"
                         "UseMtlsEndpoint=always;";

  auto conn = std::make_shared<ODBCHandles>();
  SetAttributes(conn, 30, false);
  SQLCHAR out_conn_str[kBufferLength] = {0};
  SQLSMALLINT out_conn_str_len = 0;
  SQLRETURN status = SQLDriverConnect(
      conn->hdbc, nullptr,
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(conn_str.c_str())), SQL_NTS,
      out_conn_str, sizeof(out_conn_str), &out_conn_str_len,
      SQL_DRIVER_COMPLETE);

  if (status == SQL_ERROR) {
    SQLCHAR sqlstate[6] = {0};
    SQLINTEGER native_error = 0;
    SQLCHAR buf[kBufferLength] = {0};
    SQLSMALLINT len = 0;
    EXPECT_EQ(SQLGetDiagRec(SQL_HANDLE_DBC, conn->hdbc, 1, sqlstate,
                            &native_error, buf, sizeof(buf), &len),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(sqlstate), "HY000");
    CleanupODBCHandles(*conn);
  } else {
    ASSERT_EQ(status, SQL_SUCCESS);
    status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
    ASSERT_EQ(status, SQL_SUCCESS);

    SQLRETURN exec_status = SQLExecDirect(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>("SELECT 1")),
        SQL_NTS);
    EXPECT_EQ(exec_status, SQL_ERROR);

    SQLCHAR sqlstate[6] = {0};
    SQLINTEGER native_error = 0;
    SQLCHAR buf[kBufferLength] = {0};
    SQLSMALLINT len = 0;
    EXPECT_EQ(SQLGetDiagRec(SQL_HANDLE_STMT, conn->hstmt, 1, sqlstate,
                            &native_error, buf, sizeof(buf), &len),
              SQL_SUCCESS);
    EXPECT_STREQ(reinterpret_cast<char*>(sqlstate), "HY000");
    CleanupODBCHandles(*conn);
  }

  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);
}

// Live mTLS integration test executed when real client certificates are
// configured.
TEST_F(MtlsIntegrationTest, LiveMtls_QueryExecution) {
  auto cert = GetEnv("CPP_BIGQUERY_ODBC_TEST_CLIENT_CERT_PATH");
  auto key = GetEnv("CPP_BIGQUERY_ODBC_TEST_CLIENT_KEY_PATH");
  if (!cert.has_value() || cert->empty() || !key.has_value() || key->empty()) {
    GTEST_SKIP() << "Skipping live mTLS test: "
                    "CPP_BIGQUERY_ODBC_TEST_CLIENT_CERT_PATH and "
                    "CPP_BIGQUERY_ODBC_TEST_CLIENT_KEY_PATH not set";
  }

  std::string conn_str = kDefaultConnectionString +
                         ";UseClientCertificate=1;"
                         "ClientCertificatePath=" +
                         *cert +
                         ";"
                         "ClientPrivateKeyPath=" +
                         *key +
                         ";"
                         "UseMtlsEndpoint=auto;";
  ExpectQuerySuccess(conn_str);
}

// Device certificate test via Endpoint Verification metadata.
TEST_F(MtlsIntegrationTest, DeviceCertificate_EndpointVerification) {
  auto home = GetEnv("USERPROFILE").value_or(GetEnv("HOME").value_or(""));
  std::filesystem::path epv_path = std::filesystem::path(home) /
                                   ".secureConnect" /
                                   "context_aware_metadata.json";
  if (!std::filesystem::exists(epv_path)) {
    GTEST_SKIP() << "Skipping device certificate test: " << epv_path.string()
                 << " does not exist";
  }

  std::string conn_str = kDefaultConnectionString +
                         ";UseClientCertificate=1;"
                         "UseMtlsEndpoint=auto;";
  ExpectQuerySuccess(conn_str);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS
}  // namespace google::cloud::odbc_tests
