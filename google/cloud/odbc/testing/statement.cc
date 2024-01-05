
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

namespace google::cloud::odbc_tests {

// Tests direct execution of statements using SQLExecDirect
SQLRETURN InsertDirectStatement(std::shared_ptr<ConnectionHandle> conn) {
  SQLRETURN status;
  auto const table_name = kDatasetName + ".ODBC_INSERT_DIRECT_TEST";
  Table table(table_name);

  const std::string string_field = "Test String 1";
  char insert_stmt[kBufferLength];
  sprintf(insert_stmt, "INSERT INTO %s VALUES ('%s')", table_name.c_str(), string_field.c_str());

  // Create Table
  table.Create(conn, "(string_field STRING)");

  // Execute insertion
  ExecuteStatement(conn, insert_stmt);

  // Drop Table
  table.Drop(conn);

  return status;
}

// Tests insertion with params using SQLPrepare, SQLBindParameter and SQLExecute
SQLRETURN InsertStatement(std::shared_ptr<ConnectionHandle> conn) {
  SQLRETURN status;
  auto const table_name = kDatasetName + ".ODBC_INSERT_PARAMS_TEST";
  char insert_stmt[kBufferLength];
  StrToChar(insert_stmt, "INSERT INTO " + table_name + " VALUES (?, ?)");

  Table table(table_name);

  // Create Table
  table.Create(conn, "(StringField STRING, IntegerField INTEGER)");

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

  // Drop Table
  table.Drop(conn);

  return status;
}

std::shared_ptr<Results> FetchResults(std::shared_ptr<ConnectionHandle> conn, std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  status = SQLPrepare(conn->hstmt, (SQLCHAR * )read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_cols;
  status = SQLNumResultCols (conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);

  std::vector<std::shared_ptr<Column>> cols(num_cols);
  Results results;
  for (int i = 0; i < num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i] = col_ptr;

    DescribeCol(conn, col_ptr, i + 1);

    std::string col_name = (char *)col_ptr->name;

    // Initializing results
    std::vector<std::string> cols_data;
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

      if(data_len == -1) {
        results[col_name].emplace_back(std::string());
        continue;
      }
      std::string val = (char *)data;
      results[col_name].push_back(val);
    }
  }

  return std::make_shared<Results>(results);
}

std::shared_ptr<Results> ScrollResults(std::shared_ptr<ConnectionHandle> conn, std::string query, int rs_size) {
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

  std::vector<std::shared_ptr<Column>> cols(num_cols);
  Results results;
  for (int i = 0; i < num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i] = col_ptr;

    DescribeCol(conn, col_ptr, 1);

    SQLCHAR result_set[rs_size * col_ptr->data_size];
    col_ptr->result_set = result_set;

    std::string col_name = (char *)col_ptr->name;

    SqlToCdataTypes(col_ptr);

    std::shared_ptr<SQLLEN[]> row_data_len(new SQLLEN[rs_size]);
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
        std::string col_name = (char *)cols[i_c]->name;
        auto data_len = cols[i_c]->data_len;
        if(cols[i_c]->row_data_len[i_r] < 0) {
          results[col_name].emplace_back(std::string());
          continue;
        }
        auto data_size = cols[i_c]->data_size;
        auto data = cols[i_c]->result_set + i_r*data_size;
        results[col_name].push_back((char *)data);
      }
    }
  }
  return std::make_shared<Results>(results);;
}

std::vector<std::shared_ptr<Column>> GetCols(std::shared_ptr<ConnectionHandle> conn, std::string query) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  status = SQLPrepare(conn->hstmt, (SQLCHAR * )read_stmt, strlen(read_stmt));
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_cols = 0;
  status = SQLNumResultCols (conn->hstmt, &num_cols);
  CheckError(status, "SQLNumResultCols", conn);

  std::vector<std::shared_ptr<Column>> cols(num_cols);
  for (int i = 0; i < num_cols; i++) {
    auto col_ptr = std::make_shared<Column>();
    cols[i] = col_ptr;

    DescribeCol(conn, col_ptr, i + 1);

    SqlToCdataTypes(col_ptr);
  }
  return cols;
}

std::shared_ptr<Results> FetchResultsWithSqlGetData(std::shared_ptr<ConnectionHandle> conn, std::string query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;

  auto cols = GetCols(conn, query);
  auto num_cols = cols.size();

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  Results results;
  // Read all the rows using SQLFetch
  while(1) {
    status = SQLFetch(conn->hstmt);
    if(status == SQL_NO_DATA) {
      break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }
    for (int i_c = 0; i_c < num_cols; i_c++) {
      SQLSMALLINT resp_status, resp_status_len;
      while(1) {
        status = SQLGetData(conn->hstmt, i_c + 1, SQL_CHAR, data, kBufferLength, &strlen_or_ind);
        CheckError(status, "SQLGetData", conn);
        if(SQL_SUCCEEDED(status)) {
          status = SQLGetDiagField(SQL_HANDLE_STMT, conn->hstmt, 1, i_c + 1, &resp_status, SQL_INTEGER, &resp_status_len);
          if(status = SQL_NO_DATA) {
            break;
          }
          CheckError(status, "SQLGetDiagField", conn);
        } else {
          break;
        }
      }
      std::string col_name = (char *)cols[i_c]->name;
      if(strlen_or_ind < 0) {
        results[col_name].emplace_back(std::string());
      } else {
        results[col_name].push_back((char *)data);
      }
    }
  }
  return std::make_shared<Results>(results);;
}

void InsertDataWithSqlPut(std::shared_ptr<ConnectionHandle> conn, std::string query, std::vector<std::string> data) {
  SQLRETURN status;
  SQLSMALLINT num_params;
  SQLSMALLINT data_type, decimal_digits, nullable;
  SQLULEN bytes_left;
  SQLLEN batch_size = 8;
  SQLCHAR * data_ptr;
  std::vector<SQLCHAR *> data_to_insert;
  for (int i = 0; i < data.size(); i++) {
    data_to_insert.push_back((SQLCHAR *)data[i].c_str());
  }

  char insert_stmt[kBufferLength];
  StrToChar(insert_stmt, query);

  // Prepare statement with insert query string
  status = SQLPrepare(conn->hstmt, (SQLCHAR *)insert_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLNumParams(conn->hstmt, &num_params);
  CheckError(status, "SQLNumParams", conn);


  for (int i = 0; i < num_params; i++) {
    status = SQLDescribeParam(conn->hstmt, i + 1, &data_type, &bytes_left, &decimal_digits, &nullable);
    CheckError(status, "SQLDescribeParam", conn);

    SQLULEN param_bytes = kBufferLength;
    SQLLEN chunk_size = SQL_LEN_DATA_AT_EXEC(param_bytes);
    data_ptr = data_to_insert[i];
    // TODO: This should ideally be done based on the parameter descriptions: data_type and bytes_left
    status = SQLBindParameter(
            conn->hstmt,
            i + 1,
            SQL_PARAM_INPUT,
            SQL_C_CHAR,
            SQL_LONGVARCHAR,
            param_bytes,
            0,
            (SQLPOINTER)data_ptr,
            0,
            &chunk_size);
    CheckError(status, "SQLBindParameter", conn);
  }

  SQLPOINTER bounded_data_ptr;
  status = SQLExecute(conn->hstmt);
  if (status != SQL_NEED_DATA) {
    CheckError(status, "SQLExecute", conn);
  }
  if (status == SQL_NEED_DATA) {
    status = SQLParamData(conn->hstmt, &bounded_data_ptr);
    if (status != SQL_NEED_DATA) {
      CheckError(status, "SQLParamData", conn);
    }
    data_ptr = (SQLCHAR *)bounded_data_ptr;
    bytes_left = strlen((char *)data_ptr);
  }
  while (status == SQL_NEED_DATA) {
    while (bytes_left > 0) {
      SQLLEN bytes_to_put = std::min((int)batch_size, (int)bytes_left);
      status = SQLPutData(conn->hstmt, data_ptr, bytes_to_put);
      CheckError(status, "SQLPutData", conn);
      data_ptr += bytes_to_put;
      bytes_left -= bytes_to_put;
    }
    status = SQLParamData(conn->hstmt, &bounded_data_ptr);
    if (status != SQL_NEED_DATA) {
      CheckError(status, "SQLParamData", conn);
    }
    data_ptr = (SQLCHAR *)bounded_data_ptr;
    if (status == SQL_NEED_DATA) {
      bytes_left = strlen((char *)data_ptr);
    }
  }
}

} // namespace google::cloud::odbc_tests
