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

#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_tests {

#ifndef BQ_DRIVER_INTEGRATION_TESTS

std::vector<int> const kStringAttributes{
    SQL_DESC_BASE_COLUMN_NAME, SQL_DESC_BASE_TABLE_NAME,
    SQL_DESC_CATALOG_NAME,     SQL_DESC_LABEL,
    SQL_DESC_LITERAL_PREFIX,   SQL_DESC_LITERAL_SUFFIX,
    SQL_DESC_LOCAL_TYPE_NAME,  SQL_DESC_NAME,
    SQL_DESC_SCHEMA_NAME,      SQL_DESC_TABLE_NAME,
    SQL_DESC_TYPE_NAME};
std::vector<int> const kIntAttributes{SQL_DESC_AUTO_UNIQUE_VALUE,
                                      SQL_DESC_CASE_SENSITIVE,
                                      SQL_DESC_CONCISE_TYPE,
                                      SQL_DESC_COUNT,
                                      SQL_DESC_DISPLAY_SIZE,
                                      SQL_DESC_FIXED_PREC_SCALE,
                                      SQL_DESC_LENGTH,
                                      SQL_DESC_NULLABLE,
                                      SQL_DESC_NUM_PREC_RADIX,
                                      SQL_DESC_OCTET_LENGTH,
                                      SQL_DESC_PRECISION,
                                      SQL_DESC_SCALE,
                                      SQL_DESC_SEARCHABLE,
                                      SQL_DESC_TYPE,
                                      SQL_DESC_UNNAMED,
                                      SQL_DESC_UNSIGNED,
                                      SQL_DESC_UPDATABLE};

void CheckStringAttributes(std::shared_ptr<ODBCHandles> conn) {
  SQLRETURN status;
  for (int field_identifier : kStringAttributes) {
    SQLCHAR buf_col[kBufferLength];
    status = SQLColAttribute(conn->hstmt, 1, field_identifier,
                             (SQLPOINTER)buf_col, kBufferLength, NULL, NULL);
    CheckError(status, "SQLColAttribute " + std::to_string(field_identifier),
               conn);

    SQLCHAR buf_desc[kBufferLength];
    status = SQLGetDescField(conn->ird, 1, field_identifier,
                             (SQLPOINTER)buf_desc, kBufferLength, NULL);
    CheckError(status, "SQLGetDescField " + std::to_string(field_identifier),
               conn);

    std::string col = reinterpret_cast<char*>(buf_col);
    std::string desc = reinterpret_cast<char*>(buf_desc);
    EXPECT_EQ(col, desc);
  }
}

void CheckIntAttributes(std::shared_ptr<ODBCHandles> conn) {
  SQLRETURN status;
  SQLLEN buf_col = 0;
  SQLLEN buf_desc = 0;
  for (int field_identifier : kIntAttributes) {
    buf_col = 0L;
    status = SQLColAttribute(conn->hstmt, 1, field_identifier, NULL, 0, NULL,
                             &buf_col);
    CheckError(status, "SQLColAttribute " + std::to_string(field_identifier),
               conn);

    buf_desc = 0L;
    status =
        SQLGetDescField(conn->ird, 1, field_identifier, &buf_desc, 0, NULL);
    CheckError(status, "SQLGetDescField " + std::to_string(field_identifier),
               conn);

    EXPECT_EQ(buf_col, (SQLSMALLINT)buf_desc);
  }
}

TEST(SQLColAttribute, CheckAllAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  std::string select_stmt = "SELECT * FROM INTEGRATION_TESTS.Test_Table";
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)select_stmt.c_str(),
                           select_stmt.size());
  CheckError(status, "SQLPrepare", conn);

  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);

  CheckStringAttributes(conn);
  CheckIntAttributes(conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
