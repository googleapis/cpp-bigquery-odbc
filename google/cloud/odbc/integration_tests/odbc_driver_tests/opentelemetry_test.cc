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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include <sql.h>
#include <sqlext.h>

namespace google::cloud::odbc_tests {

// We cannot reliably intercept std::cout across the DLL boundary on Windows in
// this integration test. Instead, this test just ensures that telemetry can be
// initialized and doesn't crash the driver when executing typical queries.
// Spans will be printed to stdout visually.
class OpenTelemetryTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(OpenTelemetryTest, StandardRestPathSpanHierarchy) {
  testing::internal::CaptureStdout();

  auto conn = std::make_shared<ODBCHandles>();
  std::string dsn = "DSN=" + GetDefaultDSN() + ";EnableTelemetry=1;";
  ASSERT_EQ(Connect(dsn, conn), SQL_SUCCESS);

  auto stmt = std::make_shared<ODBCHandles>();
  stmt->henv = conn->henv;
  stmt->hdbc = conn->hdbc;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &stmt->hstmt),
            SQL_SUCCESS);

  std::string query = "SELECT 1 as num";
  ASSERT_EQ(SQLExecDirect(stmt->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  SQLFetch(stmt->hstmt);

  SQLFreeHandle(SQL_HANDLE_STMT, stmt->hstmt);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string output = testing::internal::GetCapturedStdout();
  // std::cout << output;

  EXPECT_THAT(output, testing::HasSubstr("odbc.connect"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.sql.exec_direct"));
  EXPECT_THAT(output, testing::HasSubstr("bigquery.query.submit"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.disconnect"));
}

TEST_F(OpenTelemetryTest, HtapiSpanChain) {
  testing::internal::CaptureStdout();

  auto conn = std::make_shared<ODBCHandles>();
  std::string dsn = "DSN=" + GetDefaultDSN() +
                    ";EnableTelemetry=1;AllowHtapiForLargeResults=1;"
                    "LargeResultCountThreshold=1;";
  ASSERT_EQ(Connect(dsn, conn), SQL_SUCCESS);

  auto stmt = std::make_shared<ODBCHandles>();
  stmt->henv = conn->henv;
  stmt->hdbc = conn->hdbc;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &stmt->hstmt),
            SQL_SUCCESS);

  std::string query = "SELECT * FROM UNNEST(GENERATE_ARRAY(1, 100)) as num";
  ASSERT_EQ(SQLExecDirect(stmt->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  while (SQLFetch(stmt->hstmt) == SQL_SUCCESS) {
  }

  SQLFreeHandle(SQL_HANDLE_STMT, stmt->hstmt);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_THAT(output, testing::HasSubstr("odbc.connect"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.sql.exec_direct"));
  EXPECT_THAT(output, testing::HasSubstr("bigquery.query.submit"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.sql.fetch"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.disconnect"));
}

TEST_F(OpenTelemetryTest, StatelessQuery) {
  testing::internal::CaptureStdout();

  auto conn = std::make_shared<ODBCHandles>();
  std::string dsn = "DSN=" + GetDefaultDSN() + ";EnableTelemetry=1;";
  ASSERT_EQ(Connect(dsn, conn), SQL_SUCCESS);

  auto stmt = std::make_shared<ODBCHandles>();
  stmt->henv = conn->henv;
  stmt->hdbc = conn->hdbc;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &stmt->hstmt),
            SQL_SUCCESS);

  std::string query = "SELECT 1 + 1";
  ASSERT_EQ(SQLExecDirect(stmt->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  SQLFreeHandle(SQL_HANDLE_STMT, stmt->hstmt);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_THAT(output, testing::HasSubstr("odbc.connect"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.sql.exec_direct"));
  EXPECT_THAT(output, testing::HasSubstr("bigquery.query.submit"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.disconnect"));
}

TEST_F(OpenTelemetryTest, CatalogMetadata) {
  testing::internal::CaptureStdout();

  auto conn = std::make_shared<ODBCHandles>();
  std::string dsn = "DSN=" + GetDefaultDSN() + ";EnableTelemetry=1;";
  ASSERT_EQ(Connect(dsn, conn), SQL_SUCCESS);

  auto stmt = std::make_shared<ODBCHandles>();
  stmt->henv = conn->henv;
  stmt->hdbc = conn->hdbc;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &stmt->hstmt),
            SQL_SUCCESS);

  ASSERT_EQ(SQLGetTypeInfo(stmt->hstmt, SQL_ALL_TYPES), SQL_SUCCESS);

  SQLFreeHandle(SQL_HANDLE_STMT, stmt->hstmt);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string output = testing::internal::GetCapturedStdout();

  EXPECT_THAT(output, testing::HasSubstr("odbc.connect"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.catalog"));
  EXPECT_THAT(output, testing::HasSubstr("SQLGetTypeInfo"));
  EXPECT_THAT(output, testing::HasSubstr("odbc.disconnect"));
}

TEST_F(OpenTelemetryTest, WithCustomEndpoint) {
  testing::internal::CaptureStdout();

  auto conn = std::make_shared<ODBCHandles>();
  std::string dsn = "DSN=" + GetDefaultDSN() +
                    ";EnableTelemetry=1;OpenTelemetryEndpoint=localhost:4317;";
  ASSERT_EQ(Connect(dsn, conn), SQL_SUCCESS);

  auto stmt = std::make_shared<ODBCHandles>();
  stmt->henv = conn->henv;
  stmt->hdbc = conn->hdbc;
  ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &stmt->hstmt),
            SQL_SUCCESS);

  std::string query = "SELECT 1 as num";
  ASSERT_EQ(SQLExecDirect(stmt->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS),
            SQL_SUCCESS);

  SQLFetch(stmt->hstmt);

  SQLFreeHandle(SQL_HANDLE_STMT, stmt->hstmt);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  std::string output = testing::internal::GetCapturedStdout();

  // Custom endpoint routes via OTLP gRPC instead of dumping spans to stdout.
  EXPECT_THAT(output,
              testing::Not(testing::HasSubstr("name          : odbc.connect")));
}

}  // namespace google::cloud::odbc_tests