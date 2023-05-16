
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

struct Catalog {
   SQLSMALLINT target_type;
   SQLPOINTER target_value;
   SQLINTEGER buffer_length;
   SQLLEN StrLen_or_Ind;
};

shared_ptr<Results> GetProcedures(shared_ptr<ConnectionHandle> conn) {
  cout << endl << endl;
  SQLRETURN status;
  SQLCHAR proc_cat[kBufferLength];
  SQLCHAR proc_schema[kBufferLength];
  SQLCHAR proc_name[kBufferLength];
  SQLSMALLINT proc_type;
  SQLLEN len_proc_name, len_proc_cat, len_proc_schema, len_proc_type;
  SQLLEN ret_col_name_len = 0;
  SQLLEN ret_table_name_len = 0;
  Results results;
  int num_cols = 8;

  char db_name[kBufferLength];
  StrToChar(db_name, conn->metadata.db_name);
  //cout << "DB_NAME::: " << (SQLCHAR *)conn->metadata.db_name.c_str() << endl;
  cout << "DB_NAME::: " << (SQLCHAR *)db_name << endl;
  status = SQLProcedures(conn->hstmt, NULL, 0, NULL, 0, NULL, 0);
  CheckError(status, "SQLTables", conn);

  SQLBindCol(conn->hstmt, 1, SQL_C_CHAR, proc_cat,
                   sizeof(proc_cat), &len_proc_cat);
  SQLBindCol(conn->hstmt, 2, SQL_C_CHAR, proc_schema,
                   sizeof(proc_schema), &len_proc_schema);
  SQLBindCol(conn->hstmt, 3, SQL_C_CHAR, proc_name,
                   sizeof(proc_name), &len_proc_name);
  SQLBindCol(conn->hstmt, 8, SQL_C_SHORT,&proc_type,
                   sizeof(proc_type), &len_proc_type);

  int i = 0, count = 0;
  while(1 && count++ < 700){
    status = SQLFetch(conn->hstmt);
    if(status == SQL_NO_DATA) {
      cout << "NO DATA::: " << endl; 
      break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      cout << "Some error:  " << endl;
      break;
    }
    cout << "Found Proc: " << proc_name << endl;
  }
  cout << endl << endl;

  auto results_ptr = make_shared<Results>(results);
  return results_ptr;
}


shared_ptr<Results> GetTables(shared_ptr<ConnectionHandle> conn) {
  cout << endl << endl;
  SQLRETURN status;
  SQLCHAR col_name[kBufferLength];
  SQLLEN ret_col_name_len = 0;
  SQLLEN ret_table_name_len = 0;
  Results results;
  int num_cols = 5;

  Catalog catalog_result[num_cols];

  for(int i = 0;i < num_cols; i++){
    catalog_result[i].target_type = SQL_C_CHAR;
    catalog_result[i].buffer_length = kBufferLength;
    catalog_result[i].target_value = malloc(sizeof(unsigned char)*catalog_result[i].buffer_length);
    status = SQLBindCol(conn->hstmt, (SQLUSMALLINT)i+1,
                             catalog_result[i].target_type,
                             catalog_result[i].target_value,
                             catalog_result[i].buffer_length,
                             &(catalog_result[i].StrLen_or_Ind));
    CheckError(status, "SQLBindCol", conn);
    //SQLTables(conn->hstmt, dbName, SQL_NTS, userName, SQL_NTS, "%",
    //                                    SQL_NTS, "TABLE", SQL_NTS );

  }
  //char db_name[kBufferLength];
  //char db_name[kBufferLength] = "ODBCTESTDATASET";
  //char db_name[kBufferLength] = "google.com:bq-devtools-test.ODBCTESTDATASET";
  char db_name[kBufferLength] = "bq-devtools-test";
  StrToChar(db_name, conn->metadata.db_name);
  //cout << "DB_NAME::: " << (SQLCHAR *)conn->metadata.db_name.c_str() << endl;
  cout << "DB_NAME::: " << (SQLCHAR *)db_name << endl;
  //status = SQLTables(conn->hstmt, (SQLCHAR *)conn->metadata.db_name.c_str(), SQL_NTS, NULL, SQL_NTS,
  //                          (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
  //status = SQLTables(conn->hstmt, (SQLCHAR *)db_name, SQL_NTS, NULL, SQL_NTS,
  //                          (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
  //status = SQLTables(conn->hstmt, (SQLCHAR *)db_name, SQL_NTS, NULL, SQL_NTS,
  //                          (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
  //status = SQLTables(conn->hstmt, (SQLCHAR*)SQL_ALL_CATALOGS, SQL_NTS, NULL, SQL_NTS,
  //                          (SQLCHAR *)"", SQL_NTS, (SQLCHAR *)"", SQL_NTS);
  // Get a list of databases on the current connection's server.  
  //status = SQLTables(conn->hstmt, (SQLCHAR*) "%", SQL_NTS, (SQLCHAR*)"", 0, (SQLCHAR*)"",  
  //  0, NULL, 0); // Prints catalogs
  
  // Get a list of all tables in the current database.
  //status = SQLTables(conn->hstmt, NULL, 0, NULL, 0, NULL, 0, NULL,0);
  //CheckError(status, "SQLTables", conn);
  status = SQLTables(conn->hstmt, (SQLCHAR *)db_name, SQL_NTS, NULL, 0, (SQLCHAR*)"%", SQL_NTS, (SQLCHAR*)"TABLE", SQL_NTS);
  CheckError(status, "SQLTables", conn);

  // Get a list of all tables in all databases.  
  //status = SQLTables(conn->hstmt, (SQLCHAR*) "%", SQL_NTS, NULL, 0, NULL, 0, NULL,0);  
  //CheckError(status, "SQLTables", conn);

  int i = 0, count = 0;
  while(1 && count++ < 700){
    status = SQLFetch(conn->hstmt);
    if(status == SQL_NO_DATA) {
      cout << "NO DATA::: " << endl; 
      break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      cout << "Some error:  " << endl;
      break;
    }
    char * table_name = (char *)catalog_result[2].target_value;
    char * catalog_name = (char *)catalog_result[0].target_value;

    //if (catalog_result[0].StrLen_or_Ind != SQL_NULL_DATA) // Prints catalogs
    //      cout << "Catalog = " << catalog_name << endl;// Prints catalogs
    //if(strcmp(catalog_name, "google.com:") <= 0) {
    if(catalog_name[0] == 'g' && catalog_name[1] == 'o' && catalog_name[2] == 'o' &&
      catalog_name[3] == 'g' && catalog_name[4] == 'l' && catalog_name[5] == 'e' &&
      catalog_name[6] == '.' && catalog_name[7] == 'c' && catalog_name[8] == 'o' &&
      catalog_name[9] == 'm' && catalog_name[10] == ':' && catalog_name[11] == 'b' &&
      catalog_name[12] == 'q' && catalog_name[13] == '-' && catalog_name[14] == 'd'
      ){
      printf("Catalog = %s\n", catalog_name);
      printf("Table = %s\n", table_name);
      printf("Table = %s\n", (char *)catalog_result[1].target_value);
      //break;
    }
    //cout << "Found Table: " << table_name << endl;
  }
  cout << endl << endl;

  auto results_ptr = make_shared<Results>(results);
  return results_ptr;
}

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
