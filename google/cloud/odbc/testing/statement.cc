
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

#include "testing/statement.h"

namespace google {
namespace cloud {
namespace bigquery_odbc {

// Tests direct execution of statements using SQLExecDirect
SQLRETURN InsertDirectStatement(shared_ptr<ConnectionHandle> conn) {
  SQLRETURN status;
  const string table_name = kDatasetName + ".ODBC_INSERT_DIRECT_TEST";

  const string string_field = "Test String 1";
  char insert_stmt[kBufferLength];
  sprintf(insert_stmt, "INSERT INTO %s VALUES ('%s')", table_name.c_str(), string_field.c_str());

  //Create Table
  CreateTable(conn, table_name, "(string_field STRING)");

  //Execute insertion
  ExecuteStatement(conn, insert_stmt);

  //Drop Table
  DropTable(conn, table_name);

  return status;
}

// Tests insertion with params using SQLPrepare, SQLBindParameter and SQLExecute
SQLRETURN InsertStatement(shared_ptr<ConnectionHandle> conn) {
  SQLRETURN status;
  const string table_name = kDatasetName + ".ODBC_INSERT_PARAMS_TEST";
  char insert_stmt[kBufferLength];
  StrToChar(insert_stmt, "INSERT INTO " + table_name + " VALUES (?, ?)");

  //Create Table
  CreateTable(conn, table_name, "(StringField STRING, IntegerField INTEGER)");

  //Prepare statement with insert query string
  status = SQLPrepare(conn->hstmt, (SQLCHAR *)insert_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  //Testing SQLNumParams API
  SQLSMALLINT num_params;
  status = SQLNumParams(conn->hstmt, &num_params);
  CheckError(status, "SQLNumParams", conn);
  EXPECT_EQ(num_params, 2);

  //Add param 1(string) to insert query string
  constexpr char * str_field = "Test String 1";
  SQLLEN len_string_field = strlen(str_field);
  status = SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                            SQL_CHAR, len_string_field, 0, (SQLCHAR * )str_field,
                            len_string_field, NULL);
  CheckError(status, "SQLBindParameter", conn);

  //Add param 2 to insert query string
  int int_field = 42;
  status = SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_SSHORT,
                            SQL_INTEGER, 0, 0, &int_field,
                            0, NULL);
  CheckError(status, "SQLBindParameter", conn);

  //Execute insertion
  status = SQLExecute (conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  //Drop Table
  DropTable(conn, table_name);

  return status;
}

void CheckColumnData(shared_ptr<ConnectionHandle> conn, string table_name, Schema schema) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, "SELECT * FROM " + table_name);

  status = SQLPrepare(conn->hstmt, (SQLCHAR * )read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  //Check if the number of columns returned is correct
  SQLSMALLINT num_cols;
  status = SQLNumResultCols (conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, schema.size());

  //Loop through columns and verify descriptions
  vector<shared_ptr<Column>> cols(num_cols);
  for (int i = 0; i < num_cols; i++) {
    shared_ptr<Column> col_ptr(new Column());
    cols[i] = col_ptr;

    DescribeCol(conn, col_ptr, i + 1);

    //Verify returned column descriptions with the table schema
    EXPECT_STREQ((const char * )col_ptr->name, schema[i].name.c_str());
    EXPECT_EQ(col_ptr->name_len, schema[i].name.length());
    EXPECT_EQ(col_ptr->data_type, schema[i].type);
    EXPECT_EQ(col_ptr->nullable, SQL_NULLABLE);
  }
}

void CheckResults(shared_ptr<ConnectionHandle> conn, string table_name, Schema schema, StdRows data) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, "SELECT * FROM " + table_name);

  status = SQLPrepare(conn->hstmt, (SQLCHAR * )read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  //Check if the number of columns returned is correct
  SQLSMALLINT num_cols;
  status = SQLNumResultCols (conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_cols, schema.size());

  //Loop through columns and verify descriptions
  vector<shared_ptr<Column>> cols(num_cols);
  for (int i = 0; i < num_cols; i++) {
    shared_ptr<Column> col_ptr(new Column());
    cols[i] = col_ptr;
    
    DescribeCol(conn, col_ptr, i + 1);

    SqlToCdataTypes(col_ptr);
    
    //Allocating space for column data
    SQLCHAR col_data[col_ptr->data_size + 1];
    col_ptr->data = col_data;

    BindCol(conn, col_ptr, i + 1);
  }

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
