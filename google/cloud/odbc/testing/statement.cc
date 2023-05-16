
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

  // Create Table
  CreateTable(conn, table_name, "(string_field STRING)");

  // Execute insertion
  ExecuteStatement(conn, insert_stmt);

  // Drop Table
  DropTable(conn, table_name);

  return status;
}

// Tests insertion with params using SQLPrepare, SQLBindParameter and SQLExecute
SQLRETURN InsertStatement(shared_ptr<ConnectionHandle> conn) {
  SQLRETURN status;
  const string table_name = kDatasetName + ".ODBC_INSERT_PARAMS_TEST";
  char insert_stmt[kBufferLength];
  StrToChar(insert_stmt, "INSERT INTO " + table_name + " VALUES (?, ?)");

  // Create Table
  CreateTable(conn, table_name, "(StringField STRING, IntegerField INTEGER)");

  // Prepare statement with insert query string
  status = SQLPrepare(conn->hstmt, (SQLCHAR *)insert_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  // Add param 1(string) to insert query string
  constexpr char * str_field = "Test String 1";
  SQLLEN len_string_field = strlen(str_field);
  status = SQLBindParameter(conn->hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                            SQL_CHAR, len_string_field, 0, (SQLCHAR * )str_field,
                            len_string_field, NULL);
  CheckError(status, "SQLBindParameter", conn);

  // Add param 2 to insert query string
  int int_field = 42;
  status = SQLBindParameter(conn->hstmt, 2, SQL_PARAM_INPUT, SQL_C_SSHORT,
                            SQL_INTEGER, 0, 0, &int_field,
                            0, NULL);
  CheckError(status, "SQLBindParameter", conn);

  // Execute insertion
  status = SQLExecute (conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  //Drop Table
  DropTable(conn, table_name);

  return status;
}

shared_ptr<Results> FetchResults(shared_ptr<ConnectionHandle> conn, string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  status = SQLPrepare(conn->hstmt, (SQLCHAR * )read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_cols;
  status = SQLNumResultCols (conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);

  vector<shared_ptr<Column>> cols(num_cols);
  Results results;
  for (int i = 0; i < num_cols; i++) {
    shared_ptr<Column> col_ptr(new Column());
    cols[i] = col_ptr;
    
    DescribeCol(conn, col_ptr, i + 1);

    string col_name = (char *)col_ptr->name;

    //Initializing results
    vector<string> cols_data;
    results[col_name] = cols_data;

    SqlToCdataTypes(col_ptr);
    
    // Allocating space for column data
    SQLCHAR col_data[col_ptr->data_size + 1];
    col_ptr->data = col_data;

    BindCol(conn, col_ptr, i + 1);
  }

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  // Read all the rows using SQLFetch
  while(1) {
    status = SQLFetch(conn->hstmt);
    if(status == SQL_NO_DATA) {
      break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }

    for (int i_c = 0; i_c < num_cols; i_c++) {
      auto col_name = (char * )cols[i_c]->name;
      auto data = cols[i_c]->data;
      auto data_len = cols[i_c]->data_len;

      if(data_len == SQL_NULL_DATA) {
        results[col_name].emplace_back(string());
        continue;
      }
      string val = (char *)data;
      results[col_name].push_back(val);
    }
  }

  auto results_ptr = make_shared<Results>(results);
  return results_ptr;
}

shared_ptr<Results> ScrollResults(shared_ptr<ConnectionHandle> conn, string query, int rs_size) {
  SQLRETURN status;
  int num_rows_fetched = 0;

  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROW_BIND_TYPE, SQL_BIND_BY_COLUMN, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)rs_size, 0);
  CheckError(status, "SQLSetStmtAttr", conn);
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROWS_FETCHED_PTR, (SQLPOINTER)&num_rows_fetched, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  status = SQLPrepare(conn->hstmt, (SQLCHAR * )read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_cols = 0;
  status = SQLNumResultCols (conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);

  vector<shared_ptr<Column>> cols(num_cols);
  Results results;
  for (int i = 0; i < num_cols; i++) {
    shared_ptr<Column> col_ptr(new Column());
    cols[i] = col_ptr;

    DescribeCol(conn, col_ptr, 1);

    SQLCHAR result_set[rs_size * col_ptr->data_size];
    col_ptr->result_set = result_set;

    string col_name = (char *)col_ptr->name;

    SqlToCdataTypes(col_ptr);

    shared_ptr<SQLLEN[]> row_data_len(new SQLLEN[rs_size]);
    col_ptr->row_data_len = row_data_len;
    status = SQLBindCol (
              conn->hstmt,
              1,
              col_ptr->data_type,
              col_ptr->result_set,
              col_ptr->data_size,
              col_ptr->row_data_len.get());
    CheckError(status, "SQLBindCol", conn);
  }

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
  while(1) {
    status = SQLFetchScroll(conn->hstmt, SQL_FETCH_NEXT, 0);
    if (status == SQL_NO_DATA_FOUND) {
        break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetchScroll", conn);
      break;
    }

    for (int i_r = 0; i_r < num_rows_fetched; i_r++) {
      for (int i_c = 0; i_c < num_cols; i_c++) {
        string col_name = (char *)cols[i_c]->name;
        auto data_len = cols[i_c]->data_len;
        if(cols[i_c]->row_data_len[i_r] < 0) {
          results[col_name].emplace_back(string());
          continue;
        }
        auto data_size = cols[i_c]->data_size;
        auto data = cols[i_c]->result_set + i_r*data_size;
        results[col_name].push_back((char *)data);
      }
    }
  }
  auto results_ptr = make_shared<Results>(results);
  return results_ptr;
}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
