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
#include <regex>

namespace google::cloud::odbc_tests {

TEST(SQLDescribeColumn, DescribeAllColumns) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto table_name = kDatasetWithTablePrefix + "ODBC_DESCRIBE_COLUMNS_TEST";
  Table table(table_name);
  table.CreateWithPrepare(conn, getSchemaStr(kFullSchema));

  auto select_stmt = "SELECT * FROM " + table_name;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)select_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_columns;
  status = SQLNumResultCols(conn->hstmt, &num_columns);
  CheckError(status, "SQLNumResultCols", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  EXPECT_TRUE(num_columns > 0);

  for (int i = 1; i <= num_columns; i++) {
    SQLSMALLINT data_type = 0;
    SQLULEN column_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    SQLCHAR column_name[kBufferLength];
    SQLSMALLINT column_name_len = 0;

    status = SQLDescribeCol(conn->hstmt, i, column_name, kBufferLength,
                            &column_name_len, &data_type, &column_size,
                            &decimal_digits, &nullable);
    CheckError(status, "SQLDescribeCol[" + std::to_string(i) + "]", conn);

    std::string table_col_name = kFullSchema[i - 1].name;
    std::string table_col_type = kFullSchema[i - 1].type;
    std::string col_type_sanitized = SanitizeBQColType(table_col_type);
    std::string ret_col_name = (char*)column_name;

    EXPECT_EQ(ret_col_name, table_col_name);
    EXPECT_EQ(column_name_len, table_col_name.size());
    EXPECT_TRUE(AreSqlAndBqTypesSame(data_type, col_type_sanitized));
    // Most of the values returned are supposed to be the same as
    // SQLGetTypeInfo. The exceptions are handled conditionally below
    TypeInfoRow type_info =
        kSqlToBqDataTypes.at(data_type).at(col_type_sanitized);
    if (table_col_type == "FLOAT64") {
      // Existing driver returns different value for FLOAT64 columns
      EXPECT_EQ(column_size, 15);
    } else {
      EXPECT_EQ(column_size, type_info.col_size);
    }
    // Existing driver returns different values for TIME and DATETIME columns
    if (table_col_type == "TIME" || table_col_type == "DATETIME") {
      EXPECT_EQ(decimal_digits, 6);
    } else {
      EXPECT_EQ(decimal_digits, type_info.maximum_scale);
    }
    EXPECT_EQ(nullable, type_info.nullable);
  }

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
