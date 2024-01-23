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
#include "google/cloud/odbc/testing/odbc_utils/properties.h"

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

namespace google::cloud::odbc_tests {

void CheckDataTypes(std::shared_ptr<ConnectionHandle> conn) {
  auto status = SQLGetTypeInfo(conn->hstmt, SQL_ALL_TYPES);
  CheckError(status, "SQLGetTypeInfo", conn);

  SQLCHAR type_name[kBufferLength];
  SQLSMALLINT sql_data_type;
  SQLINTEGER col_size;
  SQLLEN type_name_len = 0, data_type_len = 0, col_size_len = 0;

  status = SQLBindCol(conn->hstmt, 1, SQL_C_CHAR, (SQLPOINTER)type_name,
                      (SQLLEN)sizeof(type_name), &type_name_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 2, SQL_C_SHORT, (SQLPOINTER)&sql_data_type,
                      (SQLLEN)sizeof(sql_data_type), &data_type_len);
  CheckError(status, "SQLBindCol", conn);

  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    CheckError(status, "SQLFetch", conn);

    std::string bq_data_type = (char*)type_name;
    EXPECT_EQ(kBqToSqlDataTypes.at(bq_data_type), sql_data_type);
  }
}

TEST(DriverPropertiesTest, SQLGetFunctions) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetAllFunctions(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DriverPropertiesTest, SQLGetTypeInfo) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests

#endif  // BQ_DRIVER_INTEGRATION_TESTS
