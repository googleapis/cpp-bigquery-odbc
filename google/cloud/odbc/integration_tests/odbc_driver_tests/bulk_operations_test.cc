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

#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include <gtest/gtest.h>

#ifndef BQ_DRIVER_INTEGRATION_TESTS

namespace google::cloud::odbc_tests {

// Helper to extract the primary SQLSTATE from the statement handle
static std::string GetStatementSqlState(SQLHSTMT hstmt) {
  SQLCHAR sql_state[6] = {0};
  SQLINTEGER native_error = 0;
  SQLCHAR message[1024] = {0};
  SQLSMALLINT message_len = 0;

  SQLRETURN diag_rc =
      SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sql_state, &native_error,
                    message, sizeof(message), &message_len);
  if (diag_rc == SQL_SUCCESS || diag_rc == SQL_SUCCESS_WITH_INFO) {
    return std::string(reinterpret_cast<char*>(sql_state));
  }
  return "";
}

// ---------------------------------------------------------------------------
// 1. SQLGetFunctions parity: Check SQL_API_SQLBULKOPERATIONS reporting
// ---------------------------------------------------------------------------

TEST(BulkOperationsTest, SQLGetFunctions_ODBC3_BulkOperations) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLUSMALLINT supported = SQL_FALSE;
  EXPECT_EQ(
      SQL_SUCCESS,
      SQLGetFunctions(conn->hdbc, SQL_API_SQLBULKOPERATIONS, &supported));

  // Existing driver parity: Existing driver does not support SQLBulkOperations and reports SQL_FALSE
  EXPECT_EQ(SQL_FALSE, supported);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BulkOperationsTest, SQLGetFunctions_AllFunctions_BulkOperations) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
  EXPECT_EQ(
      SQL_SUCCESS,
      SQLGetFunctions(conn->hdbc, SQL_API_ODBC3_ALL_FUNCTIONS, odbc3_fns));

  // Existing driver parity: bit for SQL_API_SQLBULKOPERATIONS is false
  EXPECT_EQ(SQL_FALSE,
            SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBULKOPERATIONS));

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// ---------------------------------------------------------------------------
// 2. SQLGetInfo parity: Check cursor & bulk operation capabilities
// ---------------------------------------------------------------------------

TEST(BulkOperationsTest, SQLGetInfo_CursorAndBulkAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLUINTEGER pos_ops = 0;
  SQLSMALLINT out_len = 0;
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetInfo(conn->hdbc, SQL_POS_OPERATIONS, &pos_ops,
                       sizeof(pos_ops), &out_len));

  SQLUINTEGER fwd_attr = 0;
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetInfo(conn->hdbc, SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1,
                       &fwd_attr, sizeof(fwd_attr), &out_len));

  // Existing driver baseline: SQL_POS_OPERATIONS and cursor attributes report 0
  EXPECT_EQ(static_cast<SQLUINTEGER>(0), pos_ops);
  EXPECT_EQ(static_cast<SQLUINTEGER>(0), fwd_attr);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// ---------------------------------------------------------------------------
// 3. SQLBulkOperations execution: SQL_ADD
// ---------------------------------------------------------------------------

TEST(BulkOperationsTest, SQLBulkOperations_Add) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string table_name =
      kDatasetWithTablePrefix + "BULK_OPERATIONS_ADD_TEST";
  Table table(table_name);
  table.Create(conn, "(id INT64, name STRING)");

  // 1. Prepare/Execute query to open a cursor and populate IRD
  std::string select_query = "SELECT id, name FROM " + table_name + " WHERE 1=0";
  SQLRETURN rc = SQLExecDirect(
      conn->hstmt,
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(select_query.c_str())),
      SQL_NTS);
  EXPECT_EQ(rc, SQL_SUCCESS);

  // 2. Set row array size for bulk operations
  const SQLULEN kRowArraySize = 2;
  rc = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                      reinterpret_cast<SQLPOINTER>(kRowArraySize), 0);
  EXPECT_EQ(rc, SQL_SUCCESS);

  // 3. Bind column buffers
  SQLBIGINT id_data[kRowArraySize] = {101, 102};
  SQLLEN id_ind[kRowArraySize] = {0, 0};
  char name_data[kRowArraySize][64] = {"Alpha", "Beta"};
  SQLLEN name_ind[kRowArraySize] = {5, 4};

  rc = SQLBindCol(conn->hstmt, 1, SQL_C_SBIGINT, id_data, sizeof(SQLBIGINT),
                  id_ind);
  EXPECT_EQ(rc, SQL_SUCCESS);

  rc = SQLBindCol(conn->hstmt, 2, SQL_C_CHAR, name_data, 64, name_ind);
  EXPECT_EQ(rc, SQL_SUCCESS);

  // Set row status array and rows processed pointer
  SQLUSMALLINT row_status[kRowArraySize] = {0};
  rc = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROW_STATUS_PTR, row_status, 0);
  EXPECT_EQ(rc, SQL_SUCCESS);

  SQLULEN rows_processed = 0;
  rc = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROWS_FETCHED_PTR, &rows_processed,
                      0);
  EXPECT_EQ(rc, SQL_SUCCESS);

  // 4. Call SQLBulkOperations(SQL_ADD)
  rc = SQLBulkOperations(conn->hstmt, SQL_ADD);

  // Existing driver parity: Existing driver does NOT support SQLBulkOperations.
  // Driver Manager returns SQL_ERROR with SQLSTATE "IM001" (Driver does not support this function)
  EXPECT_EQ(rc, SQL_ERROR);
  std::string sql_state = GetStatementSqlState(conn->hstmt);
  EXPECT_EQ(sql_state, "IM001");

  // Clean up cursor and test table
  SQLCloseCursor(conn->hstmt);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// ---------------------------------------------------------------------------
// 4. SQLBulkOperations bookmark operations: Expect unsupported error
// ---------------------------------------------------------------------------

TEST(BulkOperationsTest, SQLBulkOperations_BookmarkOperations_NotSupported) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "SELECT 1 AS col";
  SQLRETURN rc = SQLExecDirect(
      conn->hstmt,
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())), SQL_NTS);
  EXPECT_EQ(rc, SQL_SUCCESS);

  // Test SQL_UPDATE_BY_BOOKMARK
  rc = SQLBulkOperations(conn->hstmt, SQL_UPDATE_BY_BOOKMARK);
  EXPECT_EQ(rc, SQL_ERROR);
  std::string sql_state = GetStatementSqlState(conn->hstmt);

  // Existing driver: Driver does not support this function
  EXPECT_EQ(sql_state, "IM001");

  // Test SQL_DELETE_BY_BOOKMARK
  rc = SQLBulkOperations(conn->hstmt, SQL_DELETE_BY_BOOKMARK);
  EXPECT_EQ(rc, SQL_ERROR);
  sql_state = GetStatementSqlState(conn->hstmt);
  EXPECT_EQ(sql_state, "IM001");

  // Test SQL_FETCH_BY_BOOKMARK
  rc = SQLBulkOperations(conn->hstmt, SQL_FETCH_BY_BOOKMARK);
  EXPECT_EQ(rc, SQL_ERROR);
  sql_state = GetStatementSqlState(conn->hstmt);
  EXPECT_EQ(sql_state, "IM001");

  SQLCloseCursor(conn->hstmt);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// ---------------------------------------------------------------------------
// 5. SQLBulkOperations invalid operation code: Expect error
// ---------------------------------------------------------------------------

TEST(BulkOperationsTest, SQLBulkOperations_InvalidOperation) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query = "SELECT 1 AS col";
  SQLRETURN rc = SQLExecDirect(
      conn->hstmt,
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())), SQL_NTS);
  EXPECT_EQ(rc, SQL_SUCCESS);

  const SQLUSMALLINT kInvalidOperation = 9999;
  rc = SQLBulkOperations(conn->hstmt, kInvalidOperation);
  EXPECT_EQ(rc, SQL_ERROR);

  std::string sql_state = GetStatementSqlState(conn->hstmt);
  // Both existing driver and Google driver: ODBC Driver Manager validates the operation
  // code and returns HY092 (Invalid attribute/option identifier).
  EXPECT_EQ(sql_state, "HY092");

  SQLCloseCursor(conn->hstmt);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests

#endif  // BQ_DRIVER_INTEGRATION_TESTS
