
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

#include "catalog.h"

namespace google::cloud::odbc_tests {

Catalog::~Catalog() = default;

std::shared_ptr<Results> Catalog::GetTables(std::shared_ptr<ODBCHandles> conn,
                                            std::string dataset,
                                            bool use_ansi) {
  SQLRETURN status;
  int res_cols = 5;
  Catalog catalog_result[res_cols];
  Results results;

  for (int i = 0; i < res_cols; i++) {
    catalog_result[i].target_type = SQL_C_CHAR;
    catalog_result[i].buffer_length = kBufferLength;
    catalog_result[i].target_value =
        malloc(sizeof(unsigned char) * catalog_result[i].buffer_length);
    status = SQLBindCol(
        conn->hstmt, (SQLUSMALLINT)i + 1, catalog_result[i].target_type,
        catalog_result[i].target_value, catalog_result[i].buffer_length,
        &(catalog_result[i].str_len));
    CheckError(status, "SQLBindCol", conn);
  }
  // No results are returned if we don't append "%"
  auto project_id = conn->metadata.project_id + "%";

  if (dataset.length()) {
    if (use_ansi) {
      status = SQLTablesA(conn->hstmt, (SQLCHAR*)project_id.c_str(), SQL_NTS,
                          (SQLCHAR*)dataset.c_str(), SQL_NTS, NULL, 0, NULL, 0);
    } else {
      status = SQLTables(conn->hstmt, (SQLCHAR*)project_id.c_str(), SQL_NTS,
                         (SQLCHAR*)dataset.c_str(), SQL_NTS, NULL, 0, NULL, 0);
    }
  } else {
    if (use_ansi) {
      status = SQLTablesA(conn->hstmt, (SQLCHAR*)project_id.c_str(), SQL_NTS,
                          NULL, 0, NULL, 0, NULL, 0);

    } else {
      status = SQLTables(conn->hstmt, (SQLCHAR*)project_id.c_str(), SQL_NTS,
                         NULL, 0, NULL, 0, NULL, 0);
    }
  }
  CheckError(status, "SQLTables", conn, use_ansi);

  int i = 0, count = 0;
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    // Col1: Catalog Name/Project Id, Col2: Dataset name, Col3: Table Name
    std::string dataset_name = (char*)catalog_result[1].target_value;
    std::string table_name = (char*)catalog_result[2].target_value;
    results[dataset_name].emplace_back(table_name);
  }

  return std::make_shared<Results>(results);
}

}  // namespace google::cloud::odbc_tests
