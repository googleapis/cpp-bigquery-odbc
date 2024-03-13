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
  SQLHDESC ird_handle;  // Implementation row descriptor
  SQLHDESC ipd_handle;  // Implementation parameter descriptor
  int num_cols = schema.size();

  auto status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &ird_handle, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &ipd_handle, 0,
                          NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);

  status = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)("SELECT * FROM " + table_name).c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  Descriptor desc, desc_copy;

  for (int i = 0; i < num_cols; i++) {
    // Reads multiple descriptor fields for a column
    status = SQLGetDescRec(ird_handle, i + 1, desc.name, kBufferLength,
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
    status = SQLSetDescRec(ipd_handle, i + 1, desc.type, desc.sub_type,
                           desc.length, desc.precision, desc.scale, desc.name,
                           (SQLLEN*)&kBufferLength, NULL);
    CheckError(status, "SQLSetDescRec", conn);
    status = SQLGetDescRec(
        ird_handle, i + 1, desc_copy.name, kBufferLength, &desc_copy.string_len,
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
  SQLHDESC ird_handle;  // Implementation row descriptor
  SQLHDESC ipd_handle;  // Implementation parameter descriptor
  int num_cols = schema.size();

  auto status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &ird_handle, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC, &ipd_handle, 0,
                          NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);

  status = SQLExecDirect(
      conn->hstmt, (SQLCHAR*)("SELECT * FROM " + table_name).c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  Descriptor desc, desc_copy;

  for (int i = 0; i < num_cols; i++) {
    // Reads multiple descriptor fields for a column
    status = SQLGetDescRec(ird_handle, i + 1, desc.name, kBufferLength,
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

  status = SQLCopyDesc(ird_handle, ipd_handle);
  CheckError(status, "SQLCopyDesc", conn);

  // We use SQLGetDescField to read the descriptor fields one at a time,
  //  and check if they were copied correctly.
  for (int i = 0; i < num_cols; i++) {
    // Reads a single field from the column descriptor
    status = SQLGetDescField(ipd_handle, i + 1, SQL_DESC_NAME, &desc_copy.name,
                             kBufferLength, NULL);
    CheckError(status, "SQLGetDescField(SQL_DESC_NAME)", conn);
    std::string col_name = (char*)desc_copy.name;
    EXPECT_EQ(col_name, schema[i].name);

    status = SQLGetDescField(ipd_handle, i + 1, SQL_DESC_TYPE, &desc_copy.type,
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
  SQLHDESC ipd_handle;  // Implementation param descriptor
  auto status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_PARAM_DESC,
                               &ipd_handle, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_PARAM_DESC)", conn);
  status = SQLSetDescField(ipd_handle, 1, SQL_DESC_PARAMETER_TYPE,
                           (SQLPOINTER)SQL_PARAM_INPUT, SQL_IS_INTEGER);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);

  SQLSMALLINT type;
  status = SQLGetDescField(ipd_handle, 1, SQL_DESC_PARAMETER_TYPE, &type,
                           SQL_IS_SMALLINT, NULL);
  EXPECT_EQ(type, SQL_PARAM_INPUT);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests

#endif  // BQ_DRIVER_INTEGRATION_TESTS
