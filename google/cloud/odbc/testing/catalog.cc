
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

#include "testing/catalog.h"

namespace google {
namespace cloud {
namespace bigquery_odbc {

struct Catalog {
   SQLSMALLINT target_type;
   SQLPOINTER target_value;
   SQLINTEGER buffer_length;
   SQLLEN str_len;
};

std::shared_ptr<Results> GetProcedures(std::shared_ptr<ConnectionHandle> conn) {
  std::cout << std::endl << std::endl;
  SQLRETURN status;
  SQLCHAR proc_cat[kBufferLength];
  SQLCHAR proc_schema[kBufferLength];
  SQLCHAR proc_name[kBufferLength];
  SQLSMALLINT proc_type;
  SQLLEN len_proc_name, len_proc_cat, len_proc_schema, len_proc_type;
  Results results;
  int res_cols = 8;

  char project_id[kBufferLength];
  StrToChar(project_id, conn->metadata.project_id);
  //cout << "PROJECT_ID::: " << (SQLCHAR *)conn->metadata.project_id.c_str() << std::endl;
  std::cout << "PROJECT_ID::: " << (SQLCHAR *)project_id << std::endl;
  //status = SQLProcedures(conn->hstmt, NULL, 0, NULL, 0, NULL, 0);
  status = SQLProcedures(conn->hstmt, (SQLCHAR*)"google.com:bq-devtools-test%", SQL_NTS, NULL, 0, NULL,0);
  CheckError(status, "SQLProcedures", conn);

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
      std::cout << "NO DATA::: " << count << std::endl; 
      break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      std::cout << "Some error:  " << std::endl;
      break;
    }
    std::cout << "Found Proc: " << proc_name << std::endl;
  }
  std::cout << std::endl << std::endl;

  auto results_ptr = std::make_shared<Results>(results);
  return results_ptr;
}


std::shared_ptr<Results> GetTables(std::shared_ptr<ConnectionHandle> conn, std::string dataset) {
  SQLRETURN status;
  int res_cols = 5;
  Catalog catalog_result[res_cols];
  Results results;

  for (int i = 0;i < res_cols; i++){
    catalog_result[i].target_type = SQL_C_CHAR;
    catalog_result[i].buffer_length = kBufferLength;
    catalog_result[i].target_value = malloc(sizeof(unsigned char)*catalog_result[i].buffer_length);
    status = SQLBindCol(conn->hstmt, (SQLUSMALLINT)i+1,
                             catalog_result[i].target_type,
                             catalog_result[i].target_value,
                             catalog_result[i].buffer_length,
                             &(catalog_result[i].str_len));
    CheckError(status, "SQLBindCol", conn);
  }
  // No results are returned if we don't append "%"
  std::string project_id = conn->metadata.project_id + "%";

  if (dataset.length()) {
    status = SQLTables(conn->hstmt, (SQLCHAR *)project_id.c_str(), SQL_NTS, (SQLCHAR *)dataset.c_str(), SQL_NTS, NULL, 0, NULL,0);
  } else {
    status = SQLTables(conn->hstmt, (SQLCHAR *)project_id.c_str(), SQL_NTS, NULL, 0, NULL, 0, NULL,0);
  }
  CheckError(status, "SQLTables", conn);

  int i = 0, count = 0;
  while(1){
    status = SQLFetch(conn->hstmt);
    if(status == SQL_NO_DATA) {
      break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    // Col 1: Catalog Name/Project Id, Col 2: Dataset name, Col 3: Table Name
    std::string dataset_name = (char *)catalog_result[1].target_value;
    std:: string table_name = (char *)catalog_result[2].target_value;
    results[dataset_name].emplace_back(table_name);
  }

  auto results_ptr = std::make_shared<Results>(results);
  return results_ptr;
}

std::shared_ptr<Results> GetColumns(std::shared_ptr<ConnectionHandle> conn, std::string dataset, std::string table_name) {
  SQLRETURN status;
  SQLCHAR col_name[kBufferLength];
  //SQLLEN ret_col_name_len = 0;
  //SQLLEN ret_table_name_len = 0;
  int res_cols = 5;
  Catalog catalog_result[res_cols];

  //std::string project_id = conn->metadata.project_id + "%";
  std::string project_id = conn->metadata.project_id;
  char project_id_cstr[kBufferLength];
  StrToChar(project_id_cstr, project_id);
  //status = SQLColumns(conn->hstmt, (SQLCHAR *)project_id.c_str(), SQL_NTS, (SQLCHAR *)(dataset + "%").c_str(), SQL_NTS, (SQLCHAR*)table_name_cstr, SQL_NTS, NULL, 0);
  //status = SQLColumns(conn->hstmt, (SQLCHAR *)project_id.c_str(), SQL_NTS, (SQLCHAR *)(dataset + "%").c_str(), SQL_NTS, (SQLCHAR*)table_name.c_str(), SQL_NTS, (SQLCHAR *)"%", SQL_NTS);
  //status = SQLColumns(conn->hstmt, NULL, 0, (SQLCHAR *)(dataset + "%").c_str(), SQL_NTS, (SQLCHAR *)(table_name + "%").c_str(), SQL_NTS, NULL, 0);
  std::cout << "Data: " << project_id_cstr << " " << dataset.c_str() << " " << table_name.c_str() << std::endl;
  //status = SQLColumns(conn->hstmt, (SQLCHAR *)project_id_cstr, SQL_NTS, (SQLCHAR *)dataset.c_str(), SQL_NTS, (SQLCHAR *)table_name.c_str(), SQL_NTS, NULL, 0);
  //status = SQLProcedures(conn->hstmt, (SQLCHAR *)project_id_cstr, SQL_NTS, NULL, 0, NULL, 0);
  status = SQLProcedures(conn->hstmt, (SQLCHAR*)"%", SQL_NTS, NULL, 0, NULL, 0);
  //status = SQLColumns(conn->hstmt, (SQLCHAR *)project_id.c_str(), SQL_NTS, (SQLCHAR *)(dataset + "%").c_str(), SQL_NTS, (SQLCHAR*)table_name.c_str(), SQL_NTS, NULL, 0);
  //status = SQLColumns(conn->hstmt, (SQLCHAR *)project_id.c_str(), SQL_NTS, (SQLCHAR *)dataset.c_str(), SQL_NTS, (SQLCHAR*)table_name.c_str(), SQL_NTS, NULL, 0);
  CheckError(status, "SQLColumns", conn);

  for (int i = 0;i < res_cols; i++){
    catalog_result[i].target_type = SQL_C_CHAR;
    catalog_result[i].buffer_length = kBufferLength;
    catalog_result[i].target_value = malloc(sizeof(unsigned char)*catalog_result[i].buffer_length);
    status = SQLBindCol(conn->hstmt, (SQLUSMALLINT)i+1,
                             catalog_result[i].target_type,
                             catalog_result[i].target_value,
                             catalog_result[i].buffer_length,
                             &(catalog_result[i].str_len));
    CheckError(status, "SQLBindCol", conn);
  }

  int count = 0;
  while (1 && ++count < 10) {
    status = SQLFetch(conn->hstmt);
    if(status == SQL_NO_DATA) {
      std::cout << "SQLFetch returned SQL_NO_DATA:: " << count << std::endl << std::endl;
      break;
    }
    if(!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    CheckError(status, "SQLFetch", conn);
    printf(" Column Name : %s, ", col_name);
    //printf (" Column Size : %i, ", ColumnSize);
    //printf (" Data Type   : %i\n", SQLDataType);
  }

  Results results;
  auto results_ptr = std::make_shared<Results>(results);
  return results_ptr;
}

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google
