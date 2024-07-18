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

struct ColAttributeRow {
  std::string literal_prefix;
  std::string literal_suffix;
  SQLLEN case_sensitive;
  SQLLEN display_size;
  SQLLEN num_prec_radix;
  SQLLEN octet_length;
  SQLLEN precision;
  SQLLEN scale;
  SQLLEN unsigned_attribute;
};

ColAttributeRow const kBqBignumericColAttr{
    "",  // literal_prefix
    "",  // literal_suffix
    0,   // case_sensitive
    79,  // display_size
    10,  // num_prec_radix
    79,  // octet_length
    77,  // precision
    38,  // scale
    0    // unsigned_attribute
};

ColAttributeRow const kBqBoolColAttr{
    "",  // literal_prefix
    "",  // literal_suffix
    0,   // case_sensitive
    1,   // display_size
    0,   // num_prec_radix
    1,   // octet_length
    1,   // precision
    0,   // scale
    1    // unsigned_attribute
};

ColAttributeRow const kBqBytesColAttr{
    "0x",   // literal_prefix
    "",     // literal_suffix
    0,      // case_sensitive
    32768,  // display_size
    0,      // num_prec_radix
    16384,  // octet_length
    16384,  // precision
    0,      // scale
    1       // unsigned_attribute
};

ColAttributeRow const kBqDateColAttr{
    "'",  // literal_prefix
    "'",  // literal_suffix
    0,    // case_sensitive
    10,   // display_size
    0,    // num_prec_radix
    6,    // octet_length
    0,    // precision
    0,    // scale
    1     // unsigned_attribute
};

ColAttributeRow const kBqDatetimeColAttr{
    "'",  // literal_prefix
    "'",  // literal_suffix
    0,    // case_sensitive
    26,   // display_size
    0,    // num_prec_radix
    16,   // octet_length
    6,    // precision
    6,    // scale
    1     // unsigned_attribute
};

ColAttributeRow const kBqFloat64ColAttr{
    "",  // literal_prefix
    "",  // literal_suffix
    0,   // case_sensitive
    24,  // display_size
    2,   // num_prec_radix
    8,   // octet_length
    53,  // precision
    0,   // scale
    0    // unsigned_attribute
};

ColAttributeRow const kBqGeographyColAttr{
    "'",    // literal_prefix
    "'",    // literal_suffix
    1,      // case_sensitive
    16384,  // display_size
    0,      // num_prec_radix
    65536,  // octet_length
    16384,  // precision
    0,      // scale
    1       // unsigned_attribute
};

ColAttributeRow const kBqInt64ColAttr{
    "",  // literal_prefix
    "",  // literal_suffix
    0,   // case_sensitive
    20,  // display_size
    10,  // num_prec_radix
    20,  // octet_length
    19,  // precision
    0,   // scale
    0    // unsigned_attribute
};

ColAttributeRow const kBqIntervalColAttr{
    "'",    // literal_prefix
    "'",    // literal_suffix
    1,      // case_sensitive
    16384,  // display_size
    0,      // num_prec_radix
    65536,  // octet_length
    16384,  // precision
    0,      // scale
    1       // unsigned_attribute
};

ColAttributeRow const kBqJsonColAttr{
    "'",    // literal_prefix
    "'",    // literal_suffix
    1,      // case_sensitive
    16384,  // display_size
    0,      // num_prec_radix
    65536,  // octet_length
    16384,  // precision
    0,      // scale
    1       // unsigned_attribute
};

ColAttributeRow const kBqNumericColAttr{
    "",  // literal_prefix
    "",  // literal_suffix
    0,   // case_sensitive
    40,  // display_size
    10,  // num_prec_radix
    40,  // octet_length
    38,  // precision
    9,   // scale
    0    // unsigned_attribute
};

ColAttributeRow const kBqStringColAttr{
    "'",    // literal_prefix
    "'",    // literal_suffix
    1,      // case_sensitive
    16384,  // display_size
    0,      // num_prec_radix
    65536,  // octet_length
    16384,  // precision
    0,      // scale
    1       // unsigned_attribute
};

ColAttributeRow const kBqTimeColAttr{
    "'",  // literal_prefix
    "'",  // literal_suffix
    0,    // case_sensitive
    15,   // display_size
    0,    // num_prec_radix
    6,    // octet_length
    6,    // precision
    6,    // scale
    1     // unsigned_attribute
};

ColAttributeRow const kBqTimestampColAttr{
    "'",  // literal_prefix
    "'",  // literal_suffix
    0,    // case_sensitive
    26,   // display_size
    0,    // num_prec_radix
    16,   // octet_length
    6,    // precision
    6,    // scale
    1     // unsigned_attribute
};

ColAttributeRow const kBqStructColAttr{
    "'",    // literal_prefix
    "'",    // literal_suffix
    1,      // case_sensitive
    16384,  // display_size
    0,      // num_prec_radix
    65536,  // octet_length
    16384,  // precision
    0,      // scale
    1       // unsigned_attribute
};

ColAttributeRow const kBqArrayColAttr{
    "'",    // literal_prefix
    "'",    // literal_suffix
    1,      // case_sensitive
    16384,  // display_size
    0,      // num_prec_radix
    65536,  // octet_length
    16384,  // precision
    0,      // scale
    1       // unsigned_attribute
};

struct BqType {
  std::string bq_type;
  std::string col_name;
  TypeInfoRow info_row;
  ColAttributeRow col_attribute_row;
};

static std::vector<BqType> const kDataTypesColumns = {
    {"BIGNUMERIC", "col_BIGNUMERIC", kBqBignumericTypeInfoRow,
     kBqBignumericColAttr},
    {"BOOL", "col_BOOL", kBqBoolTypeInfoRow, kBqBoolColAttr},
    {"BYTES", "col_BYTES", kBqBytesTypeInfoRow, kBqBytesColAttr},
    {"DATE", "col_DATE", kBqDateTypeInfoRow, kBqDateColAttr},
    {"DATETIME", "col_DATETIME", kBqDatetimeTypeInfoRow, kBqDatetimeColAttr},
    {"FLOAT64", "col_FLOAT64", kBqFloat64TypeInfoRow, kBqFloat64ColAttr},
    {"GEOGRAPHY", "col_GEOGRAPHY", kBqGeographyTypeInfoRow,
     kBqGeographyColAttr},
    {"INT64", "col_INT64", kBqInt64TypeInfoRow, kBqInt64ColAttr},
    {"INTERVAL", "col_INTERVAL", kBqIntervalTypeInfoRow, kBqIntervalColAttr},
    {"JSON", "col_JSON", kBqJsonTypeInfoRow, kBqJsonColAttr},
    {"NUMERIC", "col_NUMERIC", kBqNumericTypeInfoRow, kBqNumericColAttr},
    {"STRING", "col_STRING", kBqStringTypeInfoRow, kBqStringColAttr},
    {"TIME", "col_TIME", kBqTimeTypeInfoRow, kBqTimeColAttr},
    {"TIMESTAMP", "col_TIMESTAMP", kBqTimestampTypeInfoRow,
     kBqTimestampColAttr},
    {"STRUCT<x INT64>", "col_STRUCT", kBqStructTypeInfoRow, kBqStructColAttr},
    {"ARRAY<INT64>", "col_ARRAY", kBqArrayTypeInfoRow, kBqArrayColAttr},
};

std::string const kTableName =
    kTableNamePrefix + "ODBC_SQLColAttribute_CheckAllAttributes";

void CheckAttributes(int i, std::shared_ptr<ODBCHandles> const& conn) {
  SQLRETURN status;
  std::string col;
  std::string col_expected;
  TypeInfoRow info_row = kDataTypesColumns[i - 1].info_row;
  ColAttributeRow col_attr_row = kDataTypesColumns[i - 1].col_attribute_row;

  // Checking string attributes
  SQLCHAR col_attr[kBufferLength];
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_BASE_COLUMN_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_BASE_COLUMN_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kDataTypesColumns[i - 1].col_name, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_BASE_TABLE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_BASE_TABLE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kTableName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_CATALOG_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_CATALOG_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kCatalogName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_LABEL, (SQLPOINTER)col_attr,
                           kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_LABEL), conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kDataTypesColumns[i - 1].col_name, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_LITERAL_PREFIX,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LITERAL_PREFIX),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(col_attr_row.literal_prefix, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_LITERAL_SUFFIX,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LITERAL_SUFFIX),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(col_attr_row.literal_suffix, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_LOCAL_TYPE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_LOCAL_TYPE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(reinterpret_cast<char*>(info_row.local_type_name), col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_NAME, (SQLPOINTER)col_attr,
                           kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_NAME), conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kDataTypesColumns[i - 1].col_name, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_SCHEMA_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SCHEMA_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kDatasetName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_TABLE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TABLE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(kTableName, col);

  memset(col_attr, 0, kBufferLength);
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_TYPE_NAME,
                           (SQLPOINTER)col_attr, kBufferLength, NULL, NULL);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TYPE_NAME),
             conn);
  col = reinterpret_cast<char*>(col_attr);
  EXPECT_EQ(reinterpret_cast<char*>(info_row.type_name), col);

  // Checking int attributes
  SQLLEN col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_AUTO_UNIQUE_VALUE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_AUTO_UNIQUE_VALUE),
             conn);
  EXPECT_EQ(info_row.auto_unique_value, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_CASE_SENSITIVE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_CASE_SENSITIVE),
             conn);
  EXPECT_EQ(col_attr_row.case_sensitive, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_CONCISE_TYPE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_CONCISE_TYPE),
             conn);
  EXPECT_EQ(info_row.data_type, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_COUNT, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_COUNT), conn);
  EXPECT_EQ(kDataTypesColumns.size(), col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_DISPLAY_SIZE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_DISPLAY_SIZE),
             conn);

  EXPECT_EQ(col_attr_row.display_size, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_FIXED_PREC_SCALE, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_FIXED_PREC_SCALE),
             conn);
  EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_LENGTH, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_LENGTH),
             conn);
  EXPECT_EQ(info_row.col_size, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_NULLABLE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_NULLABLE),
             conn);
  EXPECT_EQ(info_row.nullable, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_NUM_PREC_RADIX, NULL, 0,
                           NULL, &col_attr_int);
  CheckError(status,
             "SQLColAttribute " + std::to_string(SQL_DESC_NUM_PREC_RADIX),
             conn);
  EXPECT_EQ(col_attr_row.num_prec_radix, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_OCTET_LENGTH, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_OCTET_LENGTH),
             conn);
  EXPECT_EQ(col_attr_row.octet_length, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_PRECISION, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_PRECISION),
             conn);
  EXPECT_EQ(col_attr_row.precision, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_SCALE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SCALE), conn);
  EXPECT_EQ(col_attr_row.scale, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_SEARCHABLE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_SEARCHABLE),
             conn);
  EXPECT_EQ(3, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_TYPE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_TYPE), conn);
  EXPECT_EQ(info_row.sql_data_type, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_UNNAMED, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UNNAMED),
             conn);
  EXPECT_EQ(0, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_UNSIGNED, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UNSIGNED),
             conn);
  EXPECT_EQ(col_attr_row.unsigned_attribute, col_attr_int);

  col_attr_int = 0;
  status = SQLColAttribute(conn->hstmt, i, SQL_DESC_UPDATABLE, NULL, 0, NULL,
                           &col_attr_int);
  CheckError(status, "SQLColAttribute " + std::to_string(SQL_DESC_UPDATABLE),
             conn);
  EXPECT_EQ(0, col_attr_int);
}

TEST(SQLColAttribute, CheckAllAttributes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  std::string qualified_table_name = kDatasetName + "." + kTableName;
  Table table(qualified_table_name);
  std::string table_schema =
      "(" + kDataTypesColumns[0].col_name + " " + kDataTypesColumns[0].bq_type;
  for (int i = 1; i < kDataTypesColumns.size(); i++) {
    table_schema.append(", " + kDataTypesColumns[i].col_name + " " +
                        kDataTypesColumns[i].bq_type);
  }
  table_schema.append(")");
  table.Create(conn, table_schema);

  std::string select_stmt = "SELECT * FROM " + qualified_table_name;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)select_stmt.c_str(),
                           select_stmt.size());
  CheckError(status, "SQLPrepare", conn);

  for (int i = 1; i <= kDataTypesColumns.size(); i++) {
    CheckAttributes(i, conn);
  }

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
