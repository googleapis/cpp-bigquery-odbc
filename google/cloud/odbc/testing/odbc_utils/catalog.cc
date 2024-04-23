
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
#include "google/cloud/internal/getenv.h"

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

std::map<int, std::string> Catalog::GetPrimaryKeys(
    std::shared_ptr<ODBCHandles> conn, std::string dataset, std::string table,
    bool use_ansi) {
  SQLRETURN status;
  int res_cols = 6;
  int col_idx = 0;
  Catalog catalog_result[res_cols];
  std::map<int, std::string> results;

  absl::optional<std::string> project_id_opt =
      ::google::cloud::internal::GetEnv(
          "CPP_BIGQUERY_ODBC_TEST_GOOGLE_CLOUD_PROJECT");
  auto catalog_name =
      (!project_id_opt.has_value()) ? kCatalogName : project_id_opt.value();

  // Make sure we treat the catalog arguments as OA (ordinary arguments).
  // See here for more info on catalog function arguments:
  // https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/arguments-in-catalog-functions?view=sql-server-ver16
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID,
                          (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  // Col1: catalog name , Col2: schema name, Col3: table name,
  // Col4: column name, Col5: key sequence , Col6: primary key name
  SQLSMALLINT val;
  while (col_idx < res_cols) {
    if (col_idx == 4) {
      // data type is SMALLINT.
      catalog_result[col_idx].target_type = SQL_C_SSHORT;
      catalog_result[col_idx].buffer_length = sizeof(SQLINTEGER);
      catalog_result[col_idx].target_value = (SQLPOINTER)&val;
    } else {
      // data type is Char.
      catalog_result[col_idx].target_type = SQL_C_CHAR;
      catalog_result[col_idx].buffer_length = kBufferLength;
      catalog_result[col_idx].target_value =
          malloc(sizeof(unsigned char) * catalog_result[col_idx].buffer_length);
    }
    status = SQLBindCol(conn->hstmt, (SQLUSMALLINT)col_idx + 1,
                        catalog_result[col_idx].target_type,
                        catalog_result[col_idx].target_value,
                        catalog_result[col_idx].buffer_length,
                        &(catalog_result[col_idx].str_len));
    CheckError(status, "SQLBindCol", conn);
    col_idx++;
  }

  if (dataset.length()) {
    if (use_ansi) {
      status = SQLPrimaryKeysA(
          conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)table.c_str(),
          (SQLSMALLINT)table.length());
    } else {
      status = SQLPrimaryKeys(
          conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)table.c_str(),
          (SQLSMALLINT)table.length());
    }
  } else {
    if (use_ansi) {
      status =
          SQLPrimaryKeysA(conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
                          (SQLSMALLINT)catalog_name.length(), NULL, 0,
                          (SQLCHAR*)table.c_str(), (SQLSMALLINT)table.length());
    } else {
      status =
          SQLPrimaryKeys(conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
                         (SQLSMALLINT)catalog_name.length(), NULL, 0,
                         (SQLCHAR*)table.c_str(), (SQLSMALLINT)table.length());
    }
  }
  CheckError(status, "SQLPrimaryKeys", conn, use_ansi);

// Remove the flag once SQLPrimaryKeys and SQLFetch are
// implemented for Google Driver.
#ifndef BQ_DRIVER_INTEGRATION_TESTS
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    // Col1: catalog, Col2: schema, Col3: table name,
    // Col4: column name, Col5: key sequence , Col6: primary key,
    std::string table_cat = (char*)catalog_result[1].target_value;
    std::string table_schema = (char*)catalog_result[2].target_value;
    std::string table_name = (char*)catalog_result[3].target_value;
    std::string col_name = (char*)catalog_result[4].target_value;
    SQLSMALLINT* key_seq =
        reinterpret_cast<SQLSMALLINT*>(catalog_result[5].target_value);
    std::string pk_name = (char*)catalog_result[6].target_value;

    if (!table_cat.empty()) results.insert({1, table_cat});
    if (!table_schema.empty()) results.insert({2, table_schema});
    if (!table_name.empty()) results.insert({3, table_name});
    if (!col_name.empty()) results.insert({4, col_name});
    if (key_seq && *key_seq >= 0) results.insert({5, std::to_string(*key_seq)});
    if (!pk_name.empty()) results.insert({6, pk_name});
  }
#endif  // BQ_DRIVER_INTEGRATION_TESTS
  return results;
}

}  // namespace google::cloud::odbc_tests
