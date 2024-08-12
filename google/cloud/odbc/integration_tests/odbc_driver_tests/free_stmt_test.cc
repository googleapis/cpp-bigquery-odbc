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

#include "google/cloud/odbc/testing/odbc_utils/connection.h"

namespace google::cloud::odbc_tests {

#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST(SQLFreeStmt, CloseCursor) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "Select 1";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute_1", conn);

  status = SQLFreeStmt(conn->hstmt, SQL_CLOSE);
  CheckError(status, "SQLFreeStmt_1", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute_2", conn);

  status = SQLFreeStmt(conn->hstmt, SQL_CLOSE);
  CheckError(status, "SQLFreeStmt_2", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLFreeStmt, UnbindColumns) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLLEN buf;
  auto status = SQLBindCol(conn->hstmt, 1, SQL_C_NUMERIC, &buf, 0, nullptr);
  CheckError(status, "SQLBindCol", conn);

  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &conn->ard, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);

  // Check that 1 column is bounded
  SQLSMALLINT count = 0;
  status = SQLGetDescField(conn->ard, 0, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(1, count);

  status = SQLFreeStmt(conn->hstmt, SQL_UNBIND);
  CheckError(status, "SQLFreeStmt", conn);

  // Check that 0 columns are bounded
  count = 0;
  status = SQLGetDescField(conn->ard, 0, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(0, count);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLFreeStmt, UnbindParameters) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLCHAR buf[20];
  auto status = SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                                 SQL_CHAR, 0, 0, &buf, 20, nullptr);
  CheckError(status, "SQLBindParameter", conn);

  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);

  // Check that 1 parameter is bounded
  SQLSMALLINT count = 0;
  status = SQLGetDescField(conn->apd, 0, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(1, count);

  status = SQLFreeStmt(conn->hstmt, SQL_RESET_PARAMS);
  CheckError(status, "SQLFreeStmt", conn);

  // Check that 0 parameters are bounded
  count = 0;
  status = SQLGetDescField(conn->apd, 0, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(0, count);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
