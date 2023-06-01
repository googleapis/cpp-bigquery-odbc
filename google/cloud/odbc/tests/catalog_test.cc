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
#include "testing/catalog.h"

namespace google {
namespace cloud {
namespace bigquery_odbc {

using namespace std;

std::map<std::string, Schema> kTables = {
  { "ODBC_SQLTables_TEST_1",
    { { "Str1", SQL_VARCHAR } }
  },
  { "ODBC_SQLTables_TEST_2",
    { { "Str2", SQL_VARCHAR },
      { "Int2", SQL_INTEGER },
      { "Float2", SQL_FLOAT },
    }
  },
  { "ODBC_SQLTables_TEST_3",
    { { "Str3", SQL_VARCHAR },
      { "Int3", SQL_INTEGER },
      { "Float3", SQL_FLOAT },
      { "Date3", SQL_DATETIME}
    }
  }
};

// Drops all tables in a dataset
void ClearDataset(string kDatasetName, shared_ptr<vector<string>> table_names_ptr = nullptr) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);

  vector<string> table_names;
  if(!table_names_ptr) {
    table_names = (*GetTables(conn, kDatasetName))[kDatasetName];
  } else {
    table_names = *table_names_ptr;
  }
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
  for(auto table_name: table_names) {
    string table_name_full = kDatasetName + "." + table_name;
    cout << "dropping:: " << table_name_full << endl;
    DropTable(conn, table_name_full);
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

/*
TEST(CatalogTest, SQLColumns) {
  std::shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  for (auto it: kTables) {
    std::string table_name = it.first;
    std::string table_name_full = kDatasetName + "." + table_name;
    // Create Table
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    CreateTable(conn, table_name_full, getSchemaStr(it.second));
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

    // Verify table schemas
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);

    SQLUINTEGER metadata_id;
    auto status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_METADATA_ID, (SQLPOINTER)&metadata_id, (SQLINTEGER)sizeof(metadata_id), NULL);
    CheckError(status, "SQLGetConnectAttr", conn);

    cout << "metadata_id:: " << metadata_id << endl;


    Results results = *GetColumns(conn, kDatasetName, table_name);
    //Results results = *GetTables(conn);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  for (auto it: kTables) {
    std::string table_name = it.first;
    std::string table_name_full = kDatasetName + "." + table_name;
    // Drop Tables
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    //DropTable(conn, table_name_full);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }
}
*/

TEST(CatalogTest, SQLTables) {
  ClearDataset(kDatasetName);
  std::shared_ptr<ConnectionHandle> conn(new ConnectionHandle());

  // Create tables
  for (auto it: kTables) {
    string table_name = it.first;
    string table_name_full = kDatasetName + "." + table_name;
    // Create Table
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    CreateTable(conn, table_name_full, getSchemaStr(it.second));
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }

  // Verify if the tables returned by SQLTables are the same as the ones created
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);

  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);
  auto table_names = (*GetTables(conn, kDatasetName))[kDatasetName];
  for(auto it: kTables) {
    EXPECT_NE(std::find(table_names.begin(), table_names.end(), it.first), table_names.end());
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  ClearDataset(kDatasetName, make_shared<vector<string>>(table_names));
}

/*
TEST(CatalogTest, SQLColumns) {
  map<string, Schema> tables = {
    { "ODBC_SQLColumns_TEST_1",
      { { "Str1", SQL_VARCHAR } }
    },
    { "ODBC_SQLColumns_TEST_2",
      { { "Str2", SQL_VARCHAR },
        { "Int2", SQL_INTEGER },
        { "Float2", SQL_FLOAT },
      }
    },
    { "ODBC_SQLColumns_TEST_3",
      { { "Str3", SQL_VARCHAR },
        { "Int3", SQL_INTEGER },
        { "Float3", SQL_FLOAT },
        { "Date3", SQL_DATETIME}
      }
    }
  };
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  for (auto it: tables) {
    string table_name = it.first;
    string table_name_full = kDatasetName + "." + table_name;
    //Create Table
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    CreateTable(conn, table_name_full, getSchemaStr(it.second));
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

    //Verify columns
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    Results results = *GetColumns(conn, table_name);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

    //Drop Tables
    EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
    DropTable(conn, table_name_full);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }
}
*/

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
