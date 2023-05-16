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

#include "testing/connection.h"

namespace google {
namespace cloud {
namespace bigquery_odbc {

vector<int> GetMajorMinorVer(string version_str) {
  vector<int> versions;
  int start, end = -1;

  do {
    start = end + 1;
    end = version_str.find(".", start);
    versions.emplace_back(stoi(version_str.substr(start, end - start)));
  } while (end != -1);

  return versions;
}

void VerifyDriverInfo(shared_ptr<ConnectionHandle> conn) {
  EXPECT_EQ(conn->metadata.dsn_name, "ODBCTestsDSN");
  vector<int> db_odbc_versions = GetMajorMinorVer(conn->metadata.db_odbc_ver);
  EXPECT_EQ(db_odbc_versions[0], 3);
  vector<int> driver_odbc_versions = GetMajorMinorVer(conn->metadata.driver_odbc_ver);
  EXPECT_EQ(driver_odbc_versions[0], 3);
  EXPECT_EQ(conn->metadata.driver_name, "Simba ODBC Driver for Google BigQuery");
}

TEST(ConnectionTest, SQLDriverConnect) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLConnect) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DriverInfoTest, SQLGetInfo) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);
  VerifyDriverInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
