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

#include "testing/metadata.h"

namespace google {
namespace cloud {
namespace bigquery_odbc {

TEST(StatementTest, SQLColumns) {
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
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    CreateTable(conn, table_name_full, getSchemaStr(it.second));
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

    //Verify columns
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    Results results = *GetColumns(conn, table_name);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

    //Drop Tables
    EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
    DropTable(conn, table_name_full);
    EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
  }
}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
