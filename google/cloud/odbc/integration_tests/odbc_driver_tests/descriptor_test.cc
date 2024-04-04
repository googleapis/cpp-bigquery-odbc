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
  SQLPOINTER data_ptr;
  SQLLEN* string_length_ptr;
  SQLLEN* indicator_ptr;
};

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

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
#endif  // BQ_DRIVER_INTEGRATION_TESTS

TEST(SQLGetDescField, Field_SQL_DESC_ALLOC_TYPE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Getting fields
  SQLSMALLINT alloc_type;
  status =
      SQLGetDescField(conn->ard, 0, SQL_DESC_ALLOC_TYPE, &alloc_type, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_ALLOC_TYPE)", conn);
  EXPECT_EQ(SQL_DESC_ALLOC_USER, alloc_type);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetDescField, Field_SQL_DESC_TYPE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Setting Field
  status = SQLSetDescField(conn->ard, 1, SQL_DESC_TYPE, (SQLPOINTER)SQL_INTEGER,
                           NULL);
  CheckError(status, "SQLSetDescField(SQL_DESC_CONCISE_TYPE)", conn);

  // Getting fields
  SQLSMALLINT concise_type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_CONCISE_TYPE, &concise_type,
                           0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_PARAMETER_TYPE)", conn);
  EXPECT_EQ(SQL_INTEGER, concise_type);
  SQLSMALLINT type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_TYPE, &type, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_TYPE)", conn);
  EXPECT_EQ(SQL_INTEGER, type);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetDescField, Field_SQL_DESC_CONCISE_TYPE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Setting Field
  status = SQLSetDescField(conn->ard, 1, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)SQL_INTERVAL_MONTH, NULL);
  CheckError(status, "SQLSetDescField(SQL_DESC_CONCISE_TYPE)", conn);

  // Getting fields
  SQLSMALLINT concise_type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_CONCISE_TYPE, &concise_type,
                           0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_PARAMETER_TYPE)", conn);
  EXPECT_EQ(SQL_INTERVAL_MONTH, concise_type);
  SQLSMALLINT type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_TYPE, &type, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_TYPE)", conn);
  EXPECT_EQ(SQL_INTERVAL, type);
  SQLSMALLINT datetime_code;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
                           &datetime_code, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATETIME_INTERVAL_CODE)", conn);
  EXPECT_EQ(SQL_CODE_MONTH, datetime_code);
  SQLINTEGER datetime_precision;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_DATETIME_INTERVAL_PRECISION,
                           &datetime_precision, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATETIME_INTERVAL_PRECISION)",
             conn);
  EXPECT_EQ(2, datetime_precision);
  SQLSMALLINT precision;
  status =
      SQLGetDescField(conn->ard, 1, SQL_DESC_PRECISION, &precision, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_PRECISION)", conn);
  EXPECT_EQ(0, precision);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetDescField, Field_SQL_DESC_ARRAY_STATUS_PTR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Setting Field
  SQLUSMALLINT array_status_ptr[3];
  status = SQLSetDescField(conn->ard, 0, SQL_DESC_ARRAY_STATUS_PTR,
                           array_status_ptr, NULL);
  CheckError(status, "SQLSetDescField(SQL_DESC_CONCISE_TYPE)", conn);

  // Getting fields
  SQLUSMALLINT* new_array_status_ptr = nullptr;
  status = SQLGetDescField(conn->ard, 0, SQL_DESC_ARRAY_STATUS_PTR,
                           &new_array_status_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_ARRAY_STATUS_PTR)", conn);
  EXPECT_EQ(array_status_ptr, new_array_status_ptr);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetDescField, DefaultField_SQL_DESC_LENGTH) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Setting Field
  status = SQLSetDescField(conn->ard, 3, SQL_DESC_LENGTH, (SQLPOINTER)3, NULL);
  CheckError(status, "SQLSetDescField(SQL_DESC_LENGTH)", conn);

  // Getting fields
  SQLSMALLINT count = 0;
  status = SQLGetDescField(conn->ard, 0, SQL_DESC_COUNT, &count, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn);
  EXPECT_EQ(3, count);
  SQLULEN length = 0;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_LENGTH, &length, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_LENGTH)", conn);
  EXPECT_EQ(0, length);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetDescRec, Success_SQL_DATETIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Getting fields
  Descriptor desc_to_set;
  desc_to_set.type = SQL_DATETIME;
  desc_to_set.sub_type = SQL_CODE_DATE;
  desc_to_set.precision = 0;
  desc_to_set.scale = 0;
  int data = 10;
  desc_to_set.data_ptr = &data;
  SQLLEN octet_length = 0;
  desc_to_set.length = 3;
  desc_to_set.string_length_ptr = &octet_length;
  SQLLEN indicator[3];
  desc_to_set.indicator_ptr = indicator;
  status = SQLSetDescRec(
      conn->ard, 1, desc_to_set.type, desc_to_set.sub_type, desc_to_set.length,
      desc_to_set.precision, desc_to_set.scale, desc_to_set.data_ptr,
      desc_to_set.string_length_ptr, desc_to_set.indicator_ptr);
  CheckError(status, "SQLSetDescRec", conn);

  Descriptor desc_to_get;
  status = SQLGetDescRec(
      conn->ard, 1, desc_to_get.name, kBufferLength, &desc_to_get.string_len,
      &desc_to_get.type, &desc_to_get.sub_type, &desc_to_get.length,
      &desc_to_get.precision, &desc_to_get.scale, &desc_to_get.nullable);
  CheckError(status, "SQLGetDescRec", conn);

  EXPECT_EQ(desc_to_set.type, desc_to_get.type);
  EXPECT_EQ(desc_to_set.sub_type, desc_to_get.sub_type);
  EXPECT_EQ(desc_to_set.length, desc_to_get.length);
  EXPECT_EQ(desc_to_set.precision, desc_to_get.precision);
  EXPECT_EQ(desc_to_set.scale, desc_to_get.scale);

  // Checking fields which were set by SQLSetDescRec, but were not retrieved by
  // SQLGetDescRec
  SQLSMALLINT concise_type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_CONCISE_TYPE, &concise_type,
                           0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_PARAMETER_TYPE)", conn);
  EXPECT_EQ(SQL_TYPE_DATE, concise_type);

  SQLPOINTER* data_ptr = nullptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_DATA_PTR, &data_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATA_PTR)", conn);
  EXPECT_EQ(desc_to_set.data_ptr, data_ptr);

  SQLLEN* octet_length_ptr = nullptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_OCTET_LENGTH_PTR,
                           &octet_length_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_OCTET_LENGTH_PTR)", conn);
  EXPECT_EQ(desc_to_set.string_length_ptr, octet_length_ptr);

  SQLLEN* indicator_ptr = nullptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_INDICATOR_PTR, &indicator_ptr,
                           0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_INDICATOR_PTR)", conn);
  EXPECT_EQ(desc_to_set.indicator_ptr, indicator_ptr);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetDescRec, Success_SQL_INTEGER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Getting fields
  Descriptor desc_to_set;
  desc_to_set.type = SQL_INTEGER;
  desc_to_set.sub_type = 0;
  desc_to_set.precision = 10;
  desc_to_set.scale = 5;
  int data = 10;
  desc_to_set.data_ptr = &data;
  SQLLEN octet_length = 0;
  desc_to_set.length = 3;
  desc_to_set.string_length_ptr = &octet_length;
  SQLLEN indicator[3];
  desc_to_set.indicator_ptr = indicator;
  status = SQLSetDescRec(
      conn->ard, 1, desc_to_set.type, desc_to_set.sub_type, desc_to_set.length,
      desc_to_set.precision, desc_to_set.scale, desc_to_set.data_ptr,
      desc_to_set.string_length_ptr, desc_to_set.indicator_ptr);
  CheckError(status, "SQLSetDescRec", conn);

  Descriptor desc_to_get;
  status = SQLGetDescRec(
      conn->ard, 1, desc_to_get.name, kBufferLength, &desc_to_get.string_len,
      &desc_to_get.type, &desc_to_get.sub_type, &desc_to_get.length,
      &desc_to_get.precision, &desc_to_get.scale, &desc_to_get.nullable);
  CheckError(status, "SQLGetDescRec", conn);

  EXPECT_EQ(desc_to_set.type, desc_to_get.type);
  EXPECT_EQ(desc_to_set.sub_type, desc_to_get.sub_type);
  EXPECT_EQ(desc_to_set.length, desc_to_get.length);
  EXPECT_EQ(desc_to_set.precision, desc_to_get.precision);
  EXPECT_EQ(desc_to_set.scale, desc_to_get.scale);

  // Checking fields which were set by SQLSetDescRec, but were not retrieved by
  // SQLGetDescRec
  SQLSMALLINT concise_type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_CONCISE_TYPE, &concise_type,
                           0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_PARAMETER_TYPE)", conn);
  EXPECT_EQ(SQL_INTEGER, concise_type);

  SQLPOINTER* data_ptr = nullptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_DATA_PTR, &data_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_DATA_PTR)", conn);
  EXPECT_EQ(desc_to_set.data_ptr, data_ptr);

  SQLLEN* octet_length_ptr = nullptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_OCTET_LENGTH_PTR,
                           &octet_length_ptr, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_OCTET_LENGTH_PTR)", conn);
  EXPECT_EQ(desc_to_set.string_length_ptr, octet_length_ptr);

  SQLLEN* indicator_ptr = nullptr;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_INDICATOR_PTR, &indicator_ptr,
                           0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_INDICATOR_PTR)", conn);
  EXPECT_EQ(desc_to_set.indicator_ptr, indicator_ptr);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLSetDescRec, DoNothing_InvalidType) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  // Set initial state of the field. It shouldn't change if SQLSetDescRec
  // returns SQL_ERROR
  SQLSMALLINT scale = 15;
  status =
      SQLSetDescField(conn->ard, 1, SQL_DESC_SCALE, (SQLPOINTER)scale, NULL);
  CheckError(status, "SQLSetDescField(SQL_DESC_SCALE)", conn);

  Descriptor desc_to_set;
  desc_to_set.type = 555;
  desc_to_set.sub_type = 555;
  desc_to_set.precision = 10;
  desc_to_set.scale = 10;
  int data = 10;
  desc_to_set.data_ptr = &data;
  SQLLEN octet_length = 0;
  desc_to_set.length = 3;
  desc_to_set.string_length_ptr = &octet_length;
  SQLLEN indicator[3];
  desc_to_set.indicator_ptr = indicator;

  status = SQLSetDescRec(
      conn->ard, 1, desc_to_set.type, desc_to_set.sub_type, desc_to_set.length,
      desc_to_set.precision, desc_to_set.scale, desc_to_set.data_ptr,
      desc_to_set.string_length_ptr, desc_to_set.indicator_ptr);
  EXPECT_EQ(SQL_ERROR, status);

  // Checking that initial state wasn't changed
  SQLSMALLINT type;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_TYPE, &type, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_TYPE)", conn);
  EXPECT_EQ(SQL_C_DEFAULT, type);

  SQLSMALLINT scale_get;
  status = SQLGetDescField(conn->ard, 1, SQL_DESC_SCALE, &scale_get, 0, NULL);
  CheckError(status, "SQLGetDescField(SQL_DESC_SCALE)", conn);
  EXPECT_EQ(scale, scale_get);

  status = SQLFreeHandle(SQL_HANDLE_DESC, conn->ard);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DESC)", conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
