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
#include "testing/functions.h"
#include "testing/statement.h"
#include "testing/metadata.h"

namespace google {
namespace cloud {
namespace bigquery_odbc {

const StdRows kSampleData{
  { "Test String 1", 1, 1.1 },
  { .int_field = 237, .float_field = 2.22 },
  { "Test String 3", NULL, 3.333 },
  { "Test String 4", 49 },
  { "Test String 5", 53, 5 },
  { "Test String 6", 698, 0.31 },
  { "Test String 7", 12, 71.6 },
  { "Test String 8", 83, 8.8 },
};

/*
TEST(ConnectionTest, SQLDriverConnect) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}


TEST(DriverInfoTest, SQLGetInfo) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DriverAttributesTest, SQLGetEnvAttr) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetEnvInfo(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DescriptorFieldsTest, SQLGetDescRec) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetDescRec(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(FunctionsTest, SQLGetFunctions) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetAllFunctions(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecDirect) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(InsertDirectStatement(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLExecute) {
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(InsertStatement(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLDescribeCol) {
  const string table_name = kDatasetName + ".ODBC_COLUMN_DESCRIPTION_TEST";

  Schema schema {
    { "StringField", SQL_VARCHAR},
    { "IntegerField", SQL_BIGINT},
    { "FloatField", SQL_DOUBLE}
  };

  //Create Table
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTable(conn, table_name, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  //Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  InsertIntoTable(conn, table_name, kSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckColumnData(conn, table_name, schema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  DropTable(conn, table_name);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLFetch) {
  const string table_name = kDatasetName + ".ODBC_CHECK_RESULTS_TEST";

  //TODO(#14): Add integer and floating point fields too
  //Schema returned by the query
  Schema schema {
    { "StringField", SQL_VARCHAR}
  };

  //Create Table
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTable(conn, table_name, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  //Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  InsertIntoTable(conn, table_name, kSampleData);
  SQLLEN rows_count = 0;
  SQLRETURN status = SQLRowCount(conn->hstmt, &rows_count);
  CheckError(status, "SQLRowCount", conn);
  EXPECT_EQ(rows_count, kSampleData.size());
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);


  //Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  //TODO(#14): Add integer and floating point fields too
  string query = "SELECT StringField FROM " + table_name;
  Results results = *FetchResults(conn, query);

  VerifyColumnWiseResults(kSampleData, results, vector<string>());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  //Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  DropTable(conn, table_name);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(StatementTest, SQLFetchScroll) {
  const string table_name = kDatasetName + ".ODBC_SCROLL_RESULTS_TEST";

  //Schema returned by the query
  Schema schema {
    { "StringField", SQL_VARCHAR}
  };

  //Create Table
  shared_ptr<ConnectionHandle> conn(new ConnectionHandle());
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CreateTable(conn, table_name, "(StringField STRING, IntegerField INTEGER, FloatField FLOAT64)");
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  //Insert data to read
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  InsertIntoTable(conn, table_name, kSampleData);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  //Execute a read query and check whether the results returned are as expected
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  string query = "SELECT StringField FROM " + table_name;
  Results results = *ScrollResults(conn, query, 3);
  VerifyColumnWiseResults(kSampleData, results, vector<string>());

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  //Delete table
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  DropTable(conn, table_name);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
*/

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
