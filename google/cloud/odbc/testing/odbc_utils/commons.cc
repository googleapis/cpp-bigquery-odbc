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

#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

using ::google::cloud::internal::ExponentialBackoffPolicy;

std::string GetRandomString(int len) {
  static constexpr char kChars[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  std::string str;
  str.reserve(len);
  for (int i = 0; i < len; i++) {
    str += kChars[rand() % (sizeof(kChars) - 1)];
  }
  return str;
}

std::string getSchemaStr(Schema schema) {
  std::string schema_str = "(";
  for (int i = 0; i < schema.size(); i++) {
    ColumnMinimal col = schema[i];
    schema_str.append(col.name + " " + ToBqFieldType(col.type));
    if (i < schema.size() - 1) {
      schema_str.append(", ");
    }
  }
  schema_str.append(")");
  return schema_str;
}

void GetErrorDetails(std::string const& api, SQLHANDLE handle,
                     SQLSMALLINT handle_type, bool use_ansi = false) {
  if (handle == nullptr) {
    return;
  }
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[15];
  char error_str[kBufferLength];
  SQLINTEGER native_error = 0;
  SQLRETURN status;
  int rec_num = 0;
  int num_recs = 0;

  status =
      SQLGetDiagField(handle_type, handle, 0, SQL_DIAG_NUMBER, &num_recs, 0, 0);
  if (!SQL_SUCCEEDED(status)) {
    FAIL() << "SQLGetDiagField(" << handle_type
           << ") failed with status: " << status;
    return;
  }
  while (handle && num_recs--) {
    if (use_ansi) {
      status = SQLGetDiagRecA(handle_type, handle, ++rec_num, sqlstate,
                              &native_error, buf, kBufferLength, NULL);
    } else {
      status = SQLGetDiagRec(handle_type, handle, ++rec_num, sqlstate,
                             &native_error, buf, kBufferLength, NULL);
    }
    if (status == SQL_NO_DATA) {
      continue;
    }
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec(" << handle_type
             << ") failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num,
            api.c_str(), buf, (long)native_error, sqlstate);
    FAIL() << error_str;
  }
}

void GetErrorDetails(std::string const& api, std::shared_ptr<ODBCHandles> conn,
                     bool use_ansi = false) {
  GetErrorDetails(api, conn->ard, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->ird, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->apd, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->ipd, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->hstmt, SQL_HANDLE_STMT, use_ansi);
  GetErrorDetails(api, conn->hdbc, SQL_HANDLE_DBC, use_ansi);
  GetErrorDetails(api, conn->henv, SQL_HANDLE_ENV, use_ansi);
}

inline void CheckError(SQLRETURN status, std::string const api,
                       std::shared_ptr<ODBCHandles> conn, bool use_ansi) {
  if (!SQL_SUCCEEDED(status)) {
    if (use_ansi) {
      std::string ansi_api = "ANSI-";
      ansi_api.append(api);
      GetErrorDetails(ansi_api, conn, use_ansi);
    } else {
      GetErrorDetails(api, conn, use_ansi);
    }
    throw std::runtime_error(api +
                             " failed with status: " + std::to_string(status));
  }
}

void Table::Create(std::shared_ptr<ODBCHandles> conn, std::string schema_str,
                   bool use_ansi) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt,
            "CREATE OR REPLACE TABLE " + table_name_ + " " + schema_str);
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void Table::Drop(std::shared_ptr<ODBCHandles> conn, bool use_ansi) {
  char drop_table_stmt[kBufferLength];
  StrToChar(drop_table_stmt, "DROP TABLE IF EXISTS " + table_name_);
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)drop_table_stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)drop_table_stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void CreateTableDirect(std::shared_ptr<ODBCHandles> conn,
                       std::string create_table_schema, bool use_ansi) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt, create_table_schema);

  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void ExecuteStatement(std::shared_ptr<ODBCHandles> conn, char stmt[],
                      bool use_ansi) {
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

// TODO(#11): Generic implementation of InsertIntoTable function from
// testing/commons.*
void Table::Insert(std::shared_ptr<ODBCHandles> conn, StdRows rows,
                   bool use_ansi) {
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    auto row = rows[i];
    std::string row_str = "( ";

    auto str_field = row.str_field;
    if (!str_field.empty()) {
      row_str.append("'" + str_field + "', ");
    } else {
      row_str.append("NULL, ");
    }

    auto int_field = row.int_field;
    if (int_field != NULL) {
      row_str.append(std::to_string(int_field) + ", ");
    } else {
      row_str.append("NULL, ");
    }

    auto float_field = row.float_field;
    if (float_field != NULL) {
      row_str.append(std::to_string(float_field));
    } else {
      row_str.append("NULL");
    }

    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status;
  if (use_ansi) {
    status =
        SQLExecDirectA(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void DescribeCol(std::shared_ptr<ODBCHandles> conn,
                 std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index,
                 bool use_ansi) {
  SQLRETURN status;
  if (use_ansi) {
    status = SQLDescribeColA(conn->hstmt, col_index, col_ptr->name,
                             kBufferLength, &col_ptr->name_len,
                             &col_ptr->data_type, &col_ptr->data_size,
                             &col_ptr->decimal_digits, &col_ptr->nullable);

  } else {
    status = SQLDescribeCol(conn->hstmt, col_index, col_ptr->name,
                            kBufferLength, &col_ptr->name_len,
                            &col_ptr->data_type, &col_ptr->data_size,
                            &col_ptr->decimal_digits, &col_ptr->nullable);
  }

  CheckError(status, "SQLDescribeCol", conn, use_ansi);
}

void BindCol(std::shared_ptr<ODBCHandles> conn, std::shared_ptr<Column> col_ptr,
             SQLUSMALLINT col_index) {
  if (col_ptr->data_len_ptr == nullptr) {
    col_ptr->data_len_ptr = &col_ptr->data_len;
  }
  auto status =
      SQLBindCol(conn->hstmt, col_index, col_ptr->data_type, col_ptr->data,
                 col_ptr->data_size, col_ptr->data_len_ptr);

  CheckError(status, "SQLBindCol", conn);
}

void BindColManually(std::shared_ptr<ODBCHandles> conn,
                     std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index,
                     bool use_ansi) {
  SQLHDESC ard_handle;  // Application row descriptor
  SQLRETURN status;
  if (use_ansi) {
    status = SQLGetStmtAttrA(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &ard_handle, 0,
                             NULL);

  } else {
    status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &ard_handle, 0,
                            NULL);
  }
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn, use_ansi);

  // Get the highest record
  SQLSMALLINT record_count;
  if (use_ansi) {
    status = SQLGetDescFieldA(ard_handle, 0, SQL_DESC_COUNT, &record_count,
                              SQL_IS_SMALLINT, NULL);

  } else {
    status = SQLGetDescField(ard_handle, 0, SQL_DESC_COUNT, &record_count,
                             SQL_IS_SMALLINT, NULL);
  }
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn, use_ansi);

  // Update the highest record
  if (col_index > record_count) {
    status = SQLSetDescField(ard_handle, 0, SQL_DESC_COUNT,
                             (SQLPOINTER)col_index, SQL_IS_INTEGER);
    CheckError(status, "SQLGetStmtAttr(SQL_DESC_COUNT)", conn);
  }

  // Assign column attributes

  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_TYPE,
                           (SQLPOINTER)col_ptr->data_type, SQL_IS_SMALLINT);
  CheckError(status, "SQLSetDescField(SQL_DESC_TYPE)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)col_ptr->data_type, SQL_IS_SMALLINT);
  CheckError(status, "SQLSetDescField(SQL_DESC_CONCISE_TYPE)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_LENGTH,
                           &col_ptr->data_size, SQL_IS_UINTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_LENGTH)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_OCTET_LENGTH,
                           &col_ptr->data_size, SQL_IS_UINTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_OCTET_LENGTH)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_DATA_PTR,
                           col_ptr->data, SQL_NTS);
  CheckError(status, "SQLSetDescField(SQL_DESC_OCTET_LENGTH)", conn);
  if (col_ptr->data_len_ptr == nullptr) {
    col_ptr->data_len_ptr = &col_ptr->data_len;
  }
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_INDICATOR_PTR,
                           col_ptr->data_len_ptr, SQL_IS_INTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_INDICATOR_PTR)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_OCTET_LENGTH_PTR,
                           col_ptr->data_len_ptr, SQL_IS_INTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_OCTET_LENGTH_PTR)", conn);
}

}  // namespace google::cloud::odbc_tests
