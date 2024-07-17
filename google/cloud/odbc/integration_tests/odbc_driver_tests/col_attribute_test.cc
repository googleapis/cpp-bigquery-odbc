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

namespace google::cloud::odbc_tests {

#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST(SQLColAttribute, CheckAllAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  std::string table_name =
      kTableNamePrefix + "ODBC_SQLColAttribute_CheckAllAttributes";
  std::string qualified_table_name = kDatasetName + "." + table_name;
  Table table(qualified_table_name);
  table.Create(conn, "(Str1 STRING)");

  std::string select_stmt = "SELECT * FROM " + qualified_table_name;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)select_stmt.c_str(),
                           select_stmt.size());
  CheckError(status, "SQLPrepare", conn);

  // Checking string attributes
  SQLCHAR col_attr[kBufferLength];
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_BASE_COLUMN_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_BASE_COLUMN_NAME),
             conn);
  std::string col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ("Str1", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_BASE_TABLE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_BASE_TABLE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(table_name, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_CATALOG_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_CATALOG_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kCatalogName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_LABEL, (SQLPOINTER)col_attr,
                           kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_LABEL), conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ("Str1", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_LITERAL_PREFIX,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LITERAL_PREFIX),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ("'", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_LITERAL_SUFFIX,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LITERAL_SUFFIX),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ("'", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_LOCAL_TYPE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LOCAL_TYPE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ("STRING", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_NAME, (SQLPOINTER)col_attr,
                           kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_NAME), conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ("Str1", col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_SCHEMA_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SCHEMA_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kDatasetName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_TABLE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TABLE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(table_name, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_TYPE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TYPE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ("STRING", col);

  // Checking int attributes
  SQLLEN col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_AUTO_UNIQUE_VALUE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_AUTO_UNIQUE_VALUE),
             conn);
  EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_CASE_SENSITIVE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_CASE_SENSITIVE),
             conn);
  EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_CONCISE_TYPE),
             conn);
  EXPECT_EQ(12, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_COUNT, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_COUNT), conn);
  EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_DISPLAY_SIZE),
             conn);
  EXPECT_EQ(16384, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_FIXED_PREC_SCALE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_FIXED_PREC_SCALE),
             conn);
  EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_LENGTH, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_LENGTH),
             conn);
  EXPECT_EQ(16384, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_NULLABLE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_NULLABLE),
             conn);
  EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_NUM_PREC_RADIX, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_NUM_PREC_RADIX),
             conn);
  EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_OCTET_LENGTH),
             conn);
  EXPECT_EQ(65536, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_PRECISION, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_PRECISION),
             conn);
  EXPECT_EQ(16384, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_SCALE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SCALE), conn);
  EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_SEARCHABLE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SEARCHABLE),
             conn);
  EXPECT_EQ(3, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_TYPE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TYPE), conn);
  EXPECT_EQ(12, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_UNNAMED, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UNNAMED),
             conn);
  EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_UNSIGNED, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UNSIGNED),
             conn);
  EXPECT_EQ(1, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, 1, SQL_DESC_UPDATABLE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UPDATABLE),
             conn);
  EXPECT_EQ(0, col_attr_int);

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
