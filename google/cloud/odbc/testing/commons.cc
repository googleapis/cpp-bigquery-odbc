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

#include <testing/commons.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace bigquery_odbc {

using namespace std::chrono_literals;

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
    if(i < schema.size() - 1) {
      schema_str.append(", ");
    }
  }
  schema_str.append(")");
  return schema_str;
}

void GetErrorDetails(const std::string api, std::shared_ptr<ConnectionHandle> conn) {
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[15];
  char error_str[kBufferLength];
  SQLINTEGER native_error = 0;
  SQLRETURN status;
  int rec_num;

  // Get statement errors
  rec_num = 0;
  while (conn->hstmt && rec_num < 5) {
    status = SQLGetDiagRec(SQL_HANDLE_STMT, conn->hstmt, ++rec_num, sqlstate, &native_error,
                        buf, kBufferLength, NULL);
    if (status == SQL_NO_DATA) {
      continue;
    }
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec(SQL_HANDLE_STMT) failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num, api.c_str(), buf,
            (long)native_error, sqlstate);
    FAIL() << error_str;
  }

  // Get connection errors
  rec_num = 0;
  while (conn->hdbc && rec_num < 5) {
    status = SQLGetDiagRec(SQL_HANDLE_DBC, conn->hdbc, ++rec_num, sqlstate, &native_error, buf,
                        kBufferLength, NULL);
    if (status == SQL_NO_DATA) {
      continue;
    }
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec(SQL_HANDLE_DBC) failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num, api.c_str(), buf,
            (long)native_error, sqlstate);
    FAIL() << error_str;
  }

  // Get environment errors
  rec_num = 0;
  while (conn->henv && rec_num < 5) {
    status = SQLGetDiagRec(SQL_HANDLE_ENV, conn->henv, ++rec_num, sqlstate, &native_error, buf,
                        kBufferLength, NULL);
    if (status == SQL_NO_DATA) {
      continue;
    }
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec(SQL_HANDLE_ENV) failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num, api.c_str(), buf,
            (long)native_error, sqlstate);
    FAIL() << error_str;
  }
}

inline void CheckError(SQLRETURN status, const std::string api, std::shared_ptr<ConnectionHandle> conn) {
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails(api, conn);
    throw std::runtime_error(api + " failed with status: " + std::to_string(status));
  }
}

void Table::Create(std::shared_ptr<ConnectionHandle> conn, std::string schema_str) {
  std::cout << "Create Table\n";
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt, "CREATE OR REPLACE TABLE " + table_name_ + " " + schema_str);
  SQLRETURN status = SQLExecDirect(conn->hstmt, (SQLCHAR *)create_table_stmt, SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
  std::cout << "Table Created\n";
  Wait();
}

void Table::Drop(std::shared_ptr<ConnectionHandle> conn) {
  std::cout << "Drop Table\n";
  char drop_table_stmt[kBufferLength];
  StrToChar(drop_table_stmt, "DROP TABLE IF EXISTS " + table_name_);
  auto status = SQLExecDirect(conn->hstmt, (SQLCHAR *)drop_table_stmt, SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
  std::cout << "Table Dropped\n";
  Wait();
}

void ExecuteStatement(std::shared_ptr<ConnectionHandle> conn, char stmt[]) {
  std::cout << "Execute Statement\n";
  auto status = SQLExecDirect(conn->hstmt, (SQLCHAR *)stmt, SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
  std::cout << "Statement executed\n";
  Wait();
}

// TODO(#11): Generic implementation of InsertIntoTable function from testing/commons.*
void Table::Insert(std::shared_ptr<ConnectionHandle> conn, StdRows rows) {
  std::cout << "Insert Data\n";
  auto insert_stmt =  "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    auto row = rows[i];
    std::string row_str = "( ";

    auto str_field = row.str_field;
    if(!str_field.empty()) {
      row_str.append("'" + str_field + "', ");
    } else {
      row_str.append("NULL, ");
    }
    
    auto int_field = row.int_field;
    if(int_field != NULL) {
      row_str.append(std::to_string(int_field) + ", ");
    } else {
      row_str.append("NULL, ");
    }

    auto float_field = row.float_field;
    if(float_field != NULL) {
      row_str.append(std::to_string(float_field));
    } else {
      row_str.append("NULL");
    }

    row_str.append(")");
    if(i != (num_rows -1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  auto status = SQLExecDirect(conn->hstmt, (SQLCHAR *)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
  std::cout << "Data Inserted\n";
  Wait();
}

void DescribeCol(std::shared_ptr<ConnectionHandle> conn, std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index) {
  SQLRETURN status = SQLDescribeCol (
                conn->hstmt,
                col_index,
                col_ptr->name,
                kBufferLength,
                &col_ptr->name_len,
                &col_ptr->data_type,
                &col_ptr->data_size,
                &col_ptr->decimal_digits,
                &col_ptr->nullable);
  CheckError(status, "SQLDescribeCol", conn);
}

void BindCol(std::shared_ptr<ConnectionHandle> conn, std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index) {
  auto status = SQLBindCol (
                conn->hstmt,
                col_index,
                col_ptr->data_type,
                col_ptr->data,
                col_ptr->data_size,
                &col_ptr->data_len);
  CheckError(status, "SQLBindCol", conn);
}

// Waits until asynchronous BQ job is finished
inline void Wait() {
      const auto start = std::chrono::high_resolution_clock::now();
//  std::this_thread::sleep_for(1000ms);
      const auto end = std::chrono::high_resolution_clock::now();
      const std::chrono::duration<double, std::milli> elapsed = end - start;

      std::cout << "Waited " << elapsed.count() << '\n';
}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
