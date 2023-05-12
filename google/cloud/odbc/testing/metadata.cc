
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

#include "testing/metadata.h"

namespace google {
namespace cloud {
namespace bigquery_odbc {

shared_ptr<Results> GetColumns(shared_ptr<ConnectionHandle> conn, string table_name) {
  SQLRETURN status;
  SQLCHAR col_name[kBufferLength];
  SQLLEN ret_col_name_len = 0;
  SQLLEN ret_table_name_len = 0;
  Results results;

  char table_name_cstr[table_name.length()];
  StrToChar(table_name_cstr, table_name);
  cout << "table_name:: " << table_name << endl;
  status = SQLColumns(conn->hstmt, NULL, 0, NULL, 0, (SQLCHAR*)table_name_cstr, SQL_NTS, NULL, 0);
  CheckError(status, "SQLColumns", conn);

  status = SQLBindCol(conn->hstmt, 3, SQL_C_CHAR, table_name_cstr, table_name.length(), &ret_table_name_len);
  CheckError(status, "SQLBindCol", conn);
  status = SQLBindCol(conn->hstmt, 4,  SQL_C_CHAR, col_name, kBufferLength, &ret_col_name_len);
  CheckError(status, "SQLBindCol", conn);

  while (SQL_SUCCEEDED(status)) {
    status = SQLFetch(conn->hstmt);
    if(status == SQL_NO_DATA) {
      //FAIL() << api << " returned SQL_NO_DATA";
      cout << "SQLFetch returned SQL_NO_DATA" << endl << endl;
      continue;
    }
    CheckError(status, "SQLFetch", conn);
    printf(" Column Name : %s, ", col_name);
    //printf (" Column Size : %i, ", ColumnSize);
    //printf (" Data Type   : %i\n", SQLDataType);
  }

  auto results_ptr = make_shared<Results>(results);
  return results_ptr;
}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
