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

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

namespace google::cloud::odbc_tests {

// Defines the idiomatic ODBC descriptors
// These fields can populated by a call to SQLGetDescRec
struct Descriptor {
  SQLSMALLINT string_len;
  SQLSMALLINT type;
  SQLSMALLINT sub_type;
  SQLLEN length;
  SQLSMALLINT precision;
  SQLSMALLINT scale;
  SQLSMALLINT nullable;
  SQLCHAR name[kBufferLength];
};

Schema kStdSchema = {
    {"Str2", SQL_VARCHAR},
    {"Int2", SQL_INTEGER},
    {"Float2", SQL_FLOAT},
};

void SetGetDescRec(std::shared_ptr<ODBCHandles> conn, std::string table_name,
                   Schema schema) {
  SQLSMALLINT desc_type;
  int num_cols = schema.size();

  auto status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);

  status = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)("SELECT * FROM " + table_name).c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  Descriptor desc, desc_copy;

  for (int i = 0; i < num_cols; i++) {
    // Reads multiple descriptor fields for a column
    status = SQLGetDescRec(conn->ird, i + 1, desc.name, kBufferLength,
                           &desc.string_len, &desc.type, &desc.sub_type,
                           &desc.length, &desc.precision, &desc.scale,
                           &desc.nullable);
    CheckError(status, "SQLGetDescRec", conn);
    std::string col_name = (char*)desc.name;
    EXPECT_EQ(col_name, schema[i].name);
    // We are checking if the bigquery data type corresponding to the returned
    //  sql data type correct.
    EXPECT_EQ(ToBqFieldType(desc.type), ToBqFieldType(schema[i].type));

    // Set the same values for another descriptor handle
    status = SQLSetDescRec(conn->ipd, i + 1, desc.type, desc.sub_type,
                           desc.length, desc.precision, desc.scale, desc.name,
                           (SQLLEN*)&kBufferLength, NULL);
    CheckError(status, "SQLSetDescRec", conn);
    status = SQLGetDescRec(
        conn->ird, i + 1, desc_copy.name, kBufferLength, &desc_copy.string_len,
        &desc_copy.type, &desc_copy.sub_type, &desc_copy.length,
        &desc_copy.precision, &desc_copy.scale, &desc_copy.nullable);
    CheckError(status, "SQLGetDescRec", conn);
    // Check if the values were set correctly by SQLSetDescRec
    EXPECT_EQ(desc_copy.string_len, desc.string_len);
    EXPECT_EQ(desc_copy.type, desc.type);
    EXPECT_EQ(desc_copy.sub_type, desc.sub_type);
    EXPECT_EQ(desc_copy.length, desc.length);
    EXPECT_EQ(desc_copy.precision, desc.precision);
    EXPECT_EQ(desc_copy.scale, desc.scale);
    EXPECT_EQ(desc_copy.nullable, desc.nullable);
  }
}

void CopyDescRec(std::shared_ptr<ODBCHandles> conn, std::string table_name,
                 Schema schema) {
  SQLSMALLINT desc_type;
  int num_cols = schema.size();

  auto status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);

  status = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)("SELECT * FROM " + table_name).c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  Descriptor desc, desc_copy;

  for (int i = 0; i < num_cols; i++) {
    // Reads multiple descriptor fields for a column
    status = SQLGetDescRec(conn->ird, i + 1, desc.name, kBufferLength,
                           &desc.string_len, &desc.type, &desc.sub_type,
                           &desc.length, &desc.precision, &desc.scale,
                           &desc.nullable);
    CheckError(status, "SQLGetDescRec", conn);
    std::string col_name = (char*)desc.name;
    EXPECT_EQ(col_name, schema[i].name);
    // We are checking if the bigquery data type corresponding to the returned
    //  sql data type correct.
    EXPECT_EQ(ToBqFieldType(desc.type), ToBqFieldType(schema[i].type));
  }

  status = SQLCopyDesc(conn->ird, conn->ipd);
  CheckError(status, "SQLCopyDesc", conn);

  // We use SQLGetDescField to read the descriptor fields one at a time,
  //  and check if they were copied correctly.
  for (int i = 0; i < num_cols; i++) {
    // Reads a single field from the column descriptor
    status = SQLGetDescField(conn->ipd, i + 1, SQL_DESC_NAME, &desc_copy.name,
                             kBufferLength, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_NAME)", conn);
    std::string col_name = (char*)desc_copy.name;
    EXPECT_EQ(col_name, schema[i].name);

    status = SQLGetDescField(conn->ipd, i + 1, SQL_DESC_TYPE, &desc_copy.type,
                             SQL_IS_SMALLINT, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_TYPE)", conn);
    EXPECT_EQ(ToBqFieldType(desc_copy.type), ToBqFieldType(schema[i].type));
  }
}

TEST(DescriptorFieldsTest, SQLSetDescRec) {
  auto const table_name = kDatasetName + ".ODBC_SET_DESCRIPTOR_REC_TEST";
  auto conn = std::make_shared<ODBCHandles>();
  Table table(table_name);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);

  table.Create(conn, getSchemaStr(kStdSchema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  SetGetDescRec(conn, table_name, kStdSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DescriptorFieldsTest, SQLCopyDesc) {
  auto const table_name = kDatasetName + ".ODBC_COPY_DESCRIPTOR_TEST";
  auto conn = std::make_shared<ODBCHandles>();
  Table table(table_name);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);

  table.Create(conn, getSchemaStr(kStdSchema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CopyDescRec(conn, table_name, kStdSchema);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DescriptorFieldsTest, SQLSetDescField) {
  auto const table_name = kDatasetName + ".ODBC_SET_DESCRIPTOR_FIELD_TEST";
  auto conn = std::make_shared<ODBCHandles>();
  Table table(table_name);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);

  table.Create(conn, getSchemaStr(kStdSchema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);
  status = SQLSetDescField(conn->ipd, 1, SQL_DESC_PARAMETER_TYPE,
                           (SQLPOINTER)SQL_PARAM_INPUT, SQL_IS_INTEGER);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);

  SQLSMALLINT type;
  status = SQLGetDescField(conn->ipd, 1, SQL_DESC_PARAMETER_TYPE, &type,
                           SQL_IS_SMALLINT, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_PARAMETER_TYPE)", conn);
  EXPECT_EQ(type, SQL_PARAM_INPUT);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

std::string kStr = "string";

void SetAllFields(SQLHDESC desc, std::shared_ptr<ODBCHandles> conn) {
  // Header fields
  auto status = SQLSetDescField(desc, 1, SQL_DESC_ALLOC_TYPE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ALLOC_TYPE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_ARRAY_SIZE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ARRAY_SIZE\n";
  }
  SQLUSMALLINT* array_status_ptr;
  status = SQLSetDescField(desc, 1, SQL_DESC_ARRAY_STATUS_PTR, (SQLPOINTER)array_status_ptr, SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ARRAY_STATUS_PTR\n";
  }
  SQLLEN* bind_offset;
  status = SQLSetDescField(desc, 1, SQL_DESC_BIND_OFFSET_PTR, (SQLPOINTER)bind_offset, SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BIND_OFFSET_PTR\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_BIND_TYPE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BIND_TYPE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_COUNT, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_COUNT\n";
  }
  SQLULEN* rows_processed_ptr;
  status = SQLSetDescField(desc, 1, SQL_DESC_ROWS_PROCESSED_PTR, (SQLPOINTER)rows_processed_ptr, SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ROWS_PROCESSED_PTR\n";
  }

  // Descriptor fields
//  std::string str = "string";
  status = SQLSetDescField(desc, 1, SQL_DESC_AUTO_UNIQUE_VALUE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_AUTO_UNIQUE_VALUE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_BASE_COLUMN_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BASE_COLUMN_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_BASE_TABLE_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BASE_TABLE_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_CASE_SENSITIVE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_CASE_SENSITIVE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_CATALOG_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_CATALOG_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_CONCISE_TYPE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_CONCISE_TYPE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_DATA_PTR, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DATA_PTR\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_CODE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DATETIME_INTERVAL_CODE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_PRECISION, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DATETIME_INTERVAL_PRECISION\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_DISPLAY_SIZE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DISPLAY_SIZE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_FIXED_PREC_SCALE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_FIXED_PREC_SCALE\n";
  }
  SQLLEN* indicator_ptr;
  status = SQLSetDescField(desc, 1, SQL_DESC_INDICATOR_PTR, (SQLPOINTER)indicator_ptr, SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_INDICATOR_PTR\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_LABEL, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LABEL\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_LENGTH, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LENGTH\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_LITERAL_PREFIX, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LITERAL_PREFIX\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_LITERAL_SUFFIX, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LITERAL_SUFFIX\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_LOCAL_TYPE_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LOCAL_TYPE_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_NULLABLE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_NULLABLE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_NUM_PREC_RADIX, (SQLPOINTER)0, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_NUM_PREC_RADIX\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_OCTET_LENGTH, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_OCTET_LENGTH\n";
  }
  SQLLEN* octet_length_ptr;
  status = SQLSetDescField(desc, 1, SQL_DESC_OCTET_LENGTH_PTR, (SQLPOINTER)octet_length_ptr, SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_OCTET_LENGTH_PTR\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_PARAMETER_TYPE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_PARAMETER_TYPE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_PRECISION, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_PRECISION\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_ROWVER, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ROWVER\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_SCALE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_SCALE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_SCHEMA_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_SCHEMA_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_SEARCHABLE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_SEARCHABLE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_TABLE_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_TABLE_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_TYPE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_TYPE\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_TYPE_NAME, (SQLPOINTER)kStr.c_str(), SQL_IS_POINTER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_TYPE_NAME\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_UNNAMED, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_UNNAMED\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_UNSIGNED, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_UNSIGNED\n";
  }
  status = SQLSetDescField(desc, 1, SQL_DESC_UPDATABLE, (SQLPOINTER)1, SQL_IS_INTEGER);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_UPDATABLE\n";
  }
}

void GetAllFields(SQLHDESC desc, std::shared_ptr<ODBCHandles> conn) {
  SQLINTEGER int_answer;
  // Header fields
  auto status = SQLGetDescField(desc, 1, SQL_DESC_ALLOC_TYPE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ALLOC_TYPE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_ARRAY_SIZE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ARRAY_SIZE\n";
  }
  SQLUSMALLINT* array_status_ptr;
  status = SQLGetDescField(desc, 1, SQL_DESC_ARRAY_STATUS_PTR, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ARRAY_STATUS_PTR\n";
  }
  SQLLEN* bind_offset;
  status = SQLGetDescField(desc, 1, SQL_DESC_BIND_OFFSET_PTR, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BIND_OFFSET_PTR\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_BIND_TYPE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BIND_TYPE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_COUNT, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_COUNT\n";
  }
  SQLULEN* rows_processed_ptr;
  status = SQLGetDescField(desc, 1, SQL_DESC_ROWS_PROCESSED_PTR, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ROWS_PROCESSED_PTR\n";
  }

  // Descriptor fields
  char* str[15];
  status = SQLGetDescField(desc, 1, SQL_DESC_AUTO_UNIQUE_VALUE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_AUTO_UNIQUE_VALUE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_BASE_COLUMN_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BASE_COLUMN_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_BASE_TABLE_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_BASE_TABLE_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_CASE_SENSITIVE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_CASE_SENSITIVE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_CATALOG_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_CATALOG_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_CONCISE_TYPE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_CONCISE_TYPE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_DATA_PTR, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DATA_PTR\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_CODE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DATETIME_INTERVAL_CODE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_PRECISION, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DATETIME_INTERVAL_PRECISION\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_DISPLAY_SIZE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_DISPLAY_SIZE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_FIXED_PREC_SCALE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_FIXED_PREC_SCALE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_INDICATOR_PTR, &int_answer, SQL_IS_POINTER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_INDICATOR_PTR\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_LABEL, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LABEL\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_LENGTH, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LENGTH\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_LITERAL_PREFIX, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LITERAL_PREFIX\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_LITERAL_SUFFIX, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LITERAL_SUFFIX\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_LOCAL_TYPE_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_LOCAL_TYPE_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_NULLABLE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_NULLABLE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_NUM_PREC_RADIX, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_NUM_PREC_RADIX\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_OCTET_LENGTH, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_OCTET_LENGTH\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_OCTET_LENGTH_PTR, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_OCTET_LENGTH_PTR\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_PARAMETER_TYPE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_PARAMETER_TYPE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_PRECISION, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_PRECISION\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_ROWVER, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_ROWVER\n";
    CheckError(status, "SQLGetStmtAttr(SQL_DESC_BASE_TABLE_NAME)", conn);
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_SCALE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_SCALE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_SCHEMA_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_SCHEMA_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_SEARCHABLE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_SEARCHABLE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_TABLE_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_TABLE_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_TYPE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_TYPE\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_TYPE_NAME, str, 15, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_TYPE_NAME\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_UNNAMED, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_UNNAMED\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_UNSIGNED, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_UNSIGNED\n";
  }
  status = SQLGetDescField(desc, 1, SQL_DESC_UPDATABLE, &int_answer, SQL_IS_INTEGER, NULL);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "Unsupported: SQL_DESC_UPDATABLE\n";
  }
}

TEST(DescriptorFieldsTest, SQLSetDescField_Simba) {
  auto const table_name = kDatasetName + ".ODBC_SET_DESCRIPTOR_FIELD_SIMBA_TEST";
  auto conn = std::make_shared<ODBCHandles>();
  Table table(table_name);
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);

  table.Create(conn, getSchemaStr(kStdSchema));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  // Get all possible descriptors
  auto status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &conn->ard, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn);
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_PARAM_DESC, &conn->apd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_PARAM_DESC)", conn);
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &conn->ipd, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);

  // Some descriptor fields (header fields) require statement to be prepared
  std::string query = "SELECT Str2 FROM " + table_name + " WHERE Str2 = ?";
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)query.c_str(), query.size());
  CheckError(status, "SQLPrepare", conn);

//  // Binding column data
//  auto col_ptr = std::make_shared<Column>();
//  DescribeCol(conn, col_ptr, 1);
//  SqlToCdataTypes(col_ptr);// Allocating space for column data
//  SQLCHAR col_data[col_ptr->data_size + 1];
//  col_ptr->data = col_data;
//  BindCol(conn, col_ptr, 1);
//
//  // Binding parameter data
//  constexpr char* str_field = "Test String 1";
//  SQLLEN len_string_field = strlen(str_field);
//  status = SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
//                            SQL_CHAR, len_string_field, 0, (SQLCHAR*)str_field,
//                            len_string_field, NULL);
//  CheckError(status, "SQLBindParameter", conn);
//
//  status = SQLExecute(conn->hstmt);
//  CheckError(status, "SQLExecute", conn);

  std::cout << "Setting fields for ARD:\n";
  SetAllFields(conn->ard, conn);
  std::cout << "Setting fields for APD:\n";
  SetAllFields(conn->apd, conn);
  std::cout << "Setting fields for IRD:\n";
  SetAllFields(conn->ird, conn);
  std::cout << "Setting fields for IPD:\n";
  SetAllFields(conn->ipd, conn);

  std::cout << "-------------------\n";



  std::cout << "Getting fields for ARD:\n";
  GetAllFields(conn->ard, conn);
  std::cout << "Getting fields for APD:\n";
  GetAllFields(conn->apd, conn);
  std::cout << "Getting fields for IRD:\n";
  GetAllFields(conn->ird, conn);
  std::cout << "Getting fields for IPD:\n";
  GetAllFields(conn->ipd, conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests

#endif  // BQ_DRIVER_INTEGRATION_TESTS
