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

namespace google {
namespace cloud {
namespace bigquery_odbc {

void SqlToCdataTypes(shared_ptr<Column> col_ptr) {
  switch (col_ptr->data_type) {
    case SQL_BIGINT:
    case SQL_INTEGER:
      col_ptr->data_type = SQL_C_LONG;
      break;
    case SQL_DOUBLE:
      col_ptr->data_type = SQL_C_DOUBLE;
    case SQL_FLOAT:
      col_ptr->data_type = SQL_C_FLOAT;
      break;
    case SQL_VARCHAR:
    case SQL_C_CHAR:
      col_ptr->data_type = SQL_C_CHAR;
      break;
    default:
      FAIL() << " Invalid column data type " << col_ptr->data_type;
    }
}

string getSchemaStr(Schema schema) {
  string schema_str = "(";
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

void GetErrorDetails(const string api, shared_ptr<ConnectionHandle> conn) {
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[15];
  char error_str[kBufferLength];
  SQLINTEGER native_error = 0;
  SQLRETURN status;
  int rec_num;

  //Get statement errors
  rec_num = 0;
  while (conn->hstmt && rec_num < 5) {
    status = SQLGetDiagRec(SQL_HANDLE_STMT, conn->hstmt, ++rec_num, sqlstate, &native_error,
                        buf, kBufferLength, NULL);
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num, api.c_str(), buf,
            (long)native_error, sqlstate);
    FAIL() << error_str;
  }

  //Get connection errors
  rec_num = 0;
  while (conn->hdbc && rec_num < 5) {
    status = SQLGetDiagRec(SQL_HANDLE_DBC, conn->hdbc, ++rec_num, sqlstate, &native_error, buf,
                        kBufferLength, NULL);
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num, api.c_str(), buf,
            (long)native_error, sqlstate);
    FAIL() << error_str;
  }

  //Get environment errors
  rec_num = 0;
  while (conn->henv && rec_num < 5) {
    status = SQLGetDiagRec(SQL_HANDLE_ENV, conn->henv, ++rec_num, sqlstate, &native_error, buf,
                        kBufferLength, NULL);
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num, api.c_str(), buf,
            (long)native_error, sqlstate);
    FAIL() << error_str;
  }
}

inline void CheckError(SQLRETURN status, const string api, shared_ptr<ConnectionHandle> conn) {
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails(api, conn);
    throw std::runtime_error(api + " failed with status: " + to_string(status));
  }
}

void CreateTable(shared_ptr<ConnectionHandle> conn, string table_name, string schema_str) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt, "CREATE OR REPLACE TABLE " + table_name + " " + schema_str);
  SQLRETURN status = SQLExecDirect(conn->hstmt, (SQLCHAR *)create_table_stmt, SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void DropTable(shared_ptr<ConnectionHandle> conn, string table_name) {
  char drop_table_stmt[kBufferLength];
  StrToChar(drop_table_stmt, "DROP TABLE " + table_name);
  SQLRETURN status = SQLExecDirect(conn->hstmt, (SQLCHAR *)drop_table_stmt, SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void ExecuteStatement(shared_ptr<ConnectionHandle> conn, char stmt[]) {
  SQLRETURN status = SQLExecDirect(conn->hstmt, (SQLCHAR *)stmt, SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

//TODO(#11): Generic implementation of InsertIntoTable function from testing/commons.*
void InsertIntoTable(shared_ptr<ConnectionHandle> conn, string table_name, StdRows rows) {
  string insert_stmt =  "INSERT INTO " + table_name + " VALUES ";
  int num_rows = rows.size();
  if(!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    StdRow row = rows[i];
    string row_str = "( ";

    string str_field = row.str_field;
    if(!str_field.empty()) {
      row_str.append("'" + str_field + "', ");
    } else {
      row_str.append("NULL, ");
    }
    
    int int_field = row.int_field;
    if(int_field != NULL) {
      row_str.append(to_string(int_field) + ", ");
    } else {
      row_str.append("NULL, ");
    }

    float float_field = row.float_field;
    if(float_field != NULL) {
      row_str.append(to_string(float_field));
    } else {
      row_str.append("NULL");
    }

    row_str.append(")");
    if(i != (num_rows -1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status = SQLExecDirect(conn->hstmt, (SQLCHAR *)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void DescribeCol(shared_ptr<ConnectionHandle> conn, shared_ptr<Column> col_ptr, SQLUSMALLINT col_index) {
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

void BindCol(shared_ptr<ConnectionHandle> conn, shared_ptr<Column> col_ptr, SQLUSMALLINT col_index) {
  SQLRETURN status = SQLBindCol (
                conn->hstmt,
                col_index,
                col_ptr->data_type,
                col_ptr->data,
                col_ptr->data_size,
                &col_ptr->data_len);
  CheckError(status, "SQLBindCol", conn);
}

void VerifyColumnWiseResults(StdRows input_data, Results col_wise_data, vector<string> col_names) {
  if(!col_names.size()) {
    vector<string> all_col_names;
    for (auto it = col_wise_data.begin(); it != col_wise_data.end(); it++) {
      all_col_names.push_back(it->first);
    }
    col_names = all_col_names;
  }
  for (string col_name: col_names) {
    vector<string> ret_col_values = col_wise_data[col_name];
    //We have to sort inserted and returned values because we haven't specified the ordering
    sort(ret_col_values.begin(), ret_col_values.end(), str_comparison);

    vector<string> input_col_values;
    for (auto data: input_data) {
      input_col_values.push_back(data.str_field);
    }
    sort(input_col_values.begin(), input_col_values.end(), str_comparison);

    //Check if the sorted inserted and returned vectors have same values
    EXPECT_EQ(ret_col_values.size(), input_col_values.size());
    for (int i = 0; i < ret_col_values.size(); i++) {
      EXPECT_EQ(ret_col_values[i], input_col_values[i]);
    }
  }
}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
