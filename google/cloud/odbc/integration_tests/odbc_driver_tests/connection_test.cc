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

#include "google/cloud/odbc/testing/odbc_utils/connection.h"

namespace google::cloud::odbc_tests {

std::vector<int> GetMajorMinorVer(std::string version_str) {
  std::vector<int> versions;
  int start, end = -1;

  do {
    start = end + 1;
    end = version_str.find(".", start);
    versions.emplace_back(stoi(version_str.substr(start, end - start)));
  } while (end != -1);

  return versions;
}

void VerifyDriverInfo(std::shared_ptr<ConnectionHandle> conn) {
  EXPECT_EQ(conn->metadata.dsn_name, GetDefaultDSN());
  std::vector<int> db_odbc_versions =
      GetMajorMinorVer(conn->metadata.db_odbc_ver);
  EXPECT_EQ(db_odbc_versions[0], 3);
  std::vector<int> driver_odbc_versions =
      GetMajorMinorVer(conn->metadata.driver_odbc_ver);
  EXPECT_EQ(driver_odbc_versions[0], 3);
  EXPECT_EQ(conn->metadata.driver_name,
            "Simba ODBC Driver for Google BigQuery");
}
/*
TEST(ConnectionTest, SQLDriverConnect) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
*/
// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS
/*
TEST(ConnectionTest, SQLConnect) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DriverInfoTest, SQLGetInfo) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);
  VerifyDriverInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
*/
TEST(DriverInfoTest, SQLGetInfo2) {
  auto conn = std::make_shared<ConnectionHandle>();
  // EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  // EXPECT_EQ(GetDriverInfo2(conn), SQL_SUCCESS);
  GetDriverInfo2(conn);
  // EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
/*
// This test is temporarily disabled till this issue is fixed for the driver
TEST(ConnectionTest, DISABLED_SQLGetConnectAttr) {
  srand(time(NULL));
  int timeout = (rand() % 30) + 1;
  SQLUINTEGER timeout_ret;
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, timeout), SQL_SUCCESS);

  auto status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                                  (SQLPOINTER)&timeout_ret,
                                  (SQLINTEGER)sizeof(timeout_ret), NULL);
  CheckError(status, "SQLGetConnectAttr", conn);
  EXPECT_EQ(timeout, timeout_ret);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
*/
#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
