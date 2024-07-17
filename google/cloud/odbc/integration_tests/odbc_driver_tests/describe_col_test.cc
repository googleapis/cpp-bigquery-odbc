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

void PrintColAttributes(std::shared_ptr<ODBCHandles> conn, int col_num) {
  SQLRETURN status;
  // Checking string attributes
  SQLCHAR col_attr[kBufferLength];
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_BASE_COLUMN_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_BASE_COLUMN_NAME),
             conn);
  std::cout << "SQL_DESC_BASE_COLUMN_NAME: " << col_attr << std::endl;
  // std::string col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ("Str1", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_BASE_TABLE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_BASE_TABLE_NAME),
             conn);
  std::cout << "SQL_DESC_BASE_TABLE_NAME: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ(table_name, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_CATALOG_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_CATALOG_NAME),
             conn);
  std::cout << "SQL_DESC_CATALOG_NAME: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ(kCatalogName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_LABEL,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_LABEL), conn);
  std::cout << "SQL_DESC_LABEL: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ("Str1", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_LITERAL_PREFIX,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LITERAL_PREFIX),
             conn);
  std::cout << "SQL_DESC_LITERAL_PREFIX: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ("'", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_LITERAL_SUFFIX,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LITERAL_SUFFIX),
             conn);
  std::cout << "SQL_DESC_LITERAL_SUFFIX: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ("'", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_LOCAL_TYPE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LOCAL_TYPE_NAME),
             conn);
  std::cout << "SQL_DESC_LOCAL_TYPE_NAME: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ("STRING", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_NAME), conn);
  std::cout << "SQL_DESC_NAME: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ("Str1", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_SCHEMA_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SCHEMA_NAME),
             conn);
  std::cout << "SQL_DESC_SCHEMA_NAME: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ(kDatasetName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_TABLE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TABLE_NAME),
             conn);
  std::cout << "SQL_DESC_TABLE_NAME: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ(table_name, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_TYPE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TYPE_NAME),
             conn);
  std::cout << "SQL_DESC_TYPE_NAME: " << col_attr << std::endl;
  // col = reinterpret_cast<char*>(col_attr);
  // EXPECT_EQ("STRING", col);

  // Checking int attributes
  SQLLEN col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_AUTO_UNIQUE_VALUE,
                           NULL, 0, NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_AUTO_UNIQUE_VALUE),
             conn);
  std::cout << "SQL_DESC_AUTO_UNIQUE_VALUE: " << col_attr_int << std::endl;
  // EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_CASE_SENSITIVE, NULL,
                           0, NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_CASE_SENSITIVE),
             conn);
  std::cout << "SQL_DESC_CASE_SENSITIVE: " << col_attr_int << std::endl;
  // EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_CONCISE_TYPE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_CONCISE_TYPE),
             conn);
  std::cout << "SQL_DESC_CONCISE_TYPE: " << col_attr_int << std::endl;
  // EXPECT_EQ(12, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_COUNT, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_COUNT), conn);
  std::cout << "SQL_DESC_COUNT: " << col_attr_int << std::endl;
  // EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_DISPLAY_SIZE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_DISPLAY_SIZE),
             conn);
  std::cout << "SQL_DESC_DISPLAY_SIZE: " << col_attr_int << std::endl;
  // EXPECT_EQ(16384, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_FIXED_PREC_SCALE,
                           NULL, 0, NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_FIXED_PREC_SCALE),
             conn);
  std::cout << "SQL_DESC_FIXED_PREC_SCALE: " << col_attr_int << std::endl;
  // EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_LENGTH, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_LENGTH),
             conn);
  std::cout << "SQL_DESC_LENGTH: " << col_attr_int << std::endl;
  // EXPECT_EQ(16384, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_NULLABLE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_NULLABLE),
             conn);
  std::cout << "SQL_DESC_NULLABLE: " << col_attr_int << std::endl;
  // EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_NUM_PREC_RADIX, NULL,
                           0, NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_NUM_PREC_RADIX),
             conn);
  std::cout << "SQL_DESC_NUM_PREC_RADIX: " << col_attr_int << std::endl;
  // EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_OCTET_LENGTH, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_OCTET_LENGTH),
             conn);
  std::cout << "SQL_DESC_OCTET_LENGTH: " << col_attr_int << std::endl;
  // EXPECT_EQ(65536, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_PRECISION, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_PRECISION),
             conn);
  std::cout << "SQL_DESC_PRECISION: " << col_attr_int << std::endl;
  // EXPECT_EQ(16384, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_SCALE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SCALE), conn);
  std::cout << "SQL_DESC_SCALE: " << col_attr_int << std::endl;
  // EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_SEARCHABLE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SEARCHABLE),
             conn);
  std::cout << "SQL_DESC_SEARCHABLE: " << col_attr_int << std::endl;
  // EXPECT_EQ(3, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_TYPE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TYPE), conn);
  std::cout << "SQL_DESC_TYPE: " << col_attr_int << std::endl;
  // EXPECT_EQ(12, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_UNNAMED, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UNNAMED),
             conn);
  std::cout << "SQL_DESC_UNNAMED: " << col_attr_int << std::endl;
  // EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_UNSIGNED, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UNSIGNED),
             conn);
  std::cout << "SQL_DESC_UNSIGNED: " << col_attr_int << std::endl;
  // EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, col_num, SQL_DESC_UPDATABLE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UPDATABLE),
             conn);
  std::cout << "SQL_DESC_UPDATABLE: " << col_attr_int << std::endl;
  // EXPECT_EQ(0, col_attr_int);

  std::cout << std::endl;
  std::cout << std::endl;
}

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

    std::string ret_col_name = (char*)column_name;
    EXPECT_EQ(ret_col_name, kFullSchema[i - 1].name);
    std::string table_col_type = kFullSchema[i - 1].type;
    std::string col_type_sanitized = SanitizeBQColType(table_col_type);
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

    std::cout << "i: " << i;
    std::cout << " BQ data type: " << table_col_type;
    std::cout << " column_name: " << column_name;
    std::cout << " column_name_len: " << column_name_len;
    std::cout << " data_type: " << data_type;
    std::cout << " column_size: " << column_size;
    std::cout << " decimal_digits: " << decimal_digits;
    std::cout << " nullable: " << nullable << std::endl;
    PrintColAttributes(conn, i);
  }

  // table.Drop(conn);
  // EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
