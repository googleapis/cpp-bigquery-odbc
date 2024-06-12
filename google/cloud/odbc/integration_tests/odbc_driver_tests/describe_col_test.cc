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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include <gtest/gtest.h>
#include <regex>

namespace google::cloud::odbc_tests {
using google::cloud::odbc_bq_driver_internal::ColumnSchema;


TEST(StatementTest, SQLDescribeColumn) {
   auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  std::string query =
      "SELECT * FROM INTEGRATION_TESTS.Test_Table";
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

   status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);

  SQLSMALLINT num_cols = 0;
  status = SQLGetDescField(conn->ird, 1, SQL_DESC_COUNT, &num_cols, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);

  // Loop through columns and verify descriptions
  std::vector<std::shared_ptr<Column>> cols(num_cols);
  for (int i = 0; i < num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i] = col_ptr;

    DescribeCol(conn, col_ptr, i + 1);
// Validate Nullable
  SQLSMALLINT out_nullable;
  SQLRETURN status = SQLGetDescField(conn->ird, i+1, SQL_DESC_NULLABLE,
                           &out_nullable, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_NULLABLE)", conn);
  EXPECT_EQ(col_ptr->nullable, out_nullable);

// Validate column name and length
  SQLCHAR out_column_Name[20];
  SQLINTEGER str_len = 0;
  status = SQLGetDescField(conn->ird, i+1, SQL_DESC_NAME,
                           &out_column_Name, kBufferLength, &str_len);
  CheckError(status, "SQLGetDescField(SQL_DESC_NAME)", conn);
  //EXPECT_STREQ((char const*)col_ptr->name, (char const*)out_column_Name);
  //EXPECT_EQ(col_ptr->name_len, str_len);

// Validate concise type
    SQLSMALLINT out_concise_c_type;
 status =
      SQLGetDescField(conn->ird, i+1, SQL_DESC_CONCISE_TYPE,
                      &out_concise_c_type, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_CONCISE_TYPE)", conn);
    EXPECT_EQ(col_ptr->data_type, out_concise_c_type);

// Validate precision (decimal digit)
    SQLSMALLINT out_desc_precision;
    switch (out_concise_c_type) {
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
    case SQL_CODE_SECOND:
    case SQL_CODE_DAY_TO_SECOND:
    case SQL_CODE_HOUR_TO_SECOND:
    case SQL_CODE_MINUTE_TO_SECOND:
      
   status =
      SQLGetDescField(conn->ird, i+1, SQL_DESC_PRECISION,
                      &out_desc_precision, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_PRECISION)", conn);
  EXPECT_EQ(col_ptr->decimal_digits, out_desc_precision);
      break;
    default:
   status =
      SQLGetDescField(conn->ird, i+1, SQL_DESC_SCALE,
                      &out_desc_precision, 0, nullptr);
  CheckError(status, "SQLGetDescField(SQL_DESC_SCALE)", conn);
  EXPECT_EQ(col_ptr->decimal_digits, out_desc_precision);
  }
  }

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

}

}  // namespace google::cloud::odbc_tests
