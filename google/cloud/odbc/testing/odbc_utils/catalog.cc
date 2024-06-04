
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

RowWiseResults Catalog::GetPrimaryKeys(std::shared_ptr<ODBCHandles> conn,
                                       std::string dataset, std::string table,
                                       bool use_ansi) {
  SQLRETURN status;
  int res_cols = 6;
  int col_idx = 0;
  Catalog catalog_result[res_cols];
  RowWiseResults results;

  if (dataset.empty()) {
    return results;
  }

  if (table.empty()) {
    return results;
  }

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

  if (use_ansi) {
    status = SQLPrimaryKeysA(
        conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
        (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
        (SQLSMALLINT)dataset.length(), (SQLCHAR*)table.c_str(),
        (SQLSMALLINT)table.length());
  } else {
    status =
        SQLPrimaryKeys(conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
                       (SQLSMALLINT)catalog_name.length(),
                       (SQLCHAR*)dataset.c_str(), (SQLSMALLINT)dataset.length(),
                       (SQLCHAR*)table.c_str(), (SQLSMALLINT)table.length());
  }
  CheckError(status, "SQLPrimaryKeys", conn, use_ansi);

  int i = 0;
  while (1) {
    std::map<int, std::string> catalog_results;
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }
    // Col1: catalog, Col2: schema, Col3: table name,
    // Col4: column name, Col5: key sequence , Col6: primary key
    // Note: ODBC coumns typically start from 1, but catalog_result
    // will be populated starting from index 0
    std::string table_cat = (char*)catalog_result[0].target_value;
    std::string table_schema = (char*)catalog_result[1].target_value;
    std::string table_name = (char*)catalog_result[2].target_value;
    std::string col_name = (char*)catalog_result[3].target_value;
    SQLSMALLINT* key_seq =
        reinterpret_cast<SQLSMALLINT*>(catalog_result[4].target_value);
    std::string pk_name = (char*)catalog_result[5].target_value;

    if (!table_cat.empty()) catalog_results.insert({1, table_cat});
    if (!table_schema.empty()) catalog_results.insert({2, table_schema});
    if (!table_name.empty()) catalog_results.insert({3, table_name});
    if (!col_name.empty()) catalog_results.insert({4, col_name});
    if (key_seq && *key_seq >= 0)
      catalog_results.insert({5, std::to_string(*key_seq)});
    if (!pk_name.empty()) catalog_results.insert({6, pk_name});
    results.emplace_back(catalog_results);
  }
  return results;
}

RowWiseResults Catalog::GetForeignKeys(std::shared_ptr<ODBCHandles> conn,
                                       std::string dataset,
                                       std::string pk_table,
                                       std::string fk_table, bool use_ansi) {
  SQLRETURN status;
  int res_cols = 15;
  int col_idx = 0;
  Catalog catalog_result[res_cols];
  RowWiseResults results;
  if (dataset.empty()) {
    return results;
  }

  if (pk_table.empty() && fk_table.empty()) {
    return results;
  }

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

  // Col1: pk catalog name , Col2: pk schema name, Col3: pk table name,
  // Col4: pk column name, Col5: fk catalog name, Col6: fk schema name,
  // Col7: fk table name, Col8: fk column name,  Col9: key sequence,
  // Col10: update rule, Col 11: delete rule, Col12: fk name,
  // Col13: pk name, Col14: Deferrability
  SQLSMALLINT val;
  while (col_idx < res_cols) {
    if (col_idx == 9 || col_idx == 10) {
      // UPDATE_RULE and DELETE_RULE not supported by BQ so skip.
      col_idx++;
      continue;
    } else if (col_idx == 13 || col_idx == 8) {
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

  if (!pk_table.empty() && !fk_table.empty()) {
    if (use_ansi) {
      status = SQLForeignKeysA(
          conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)pk_table.c_str(),
          (SQLSMALLINT)pk_table.length(), (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)fk_table.c_str(),
          (SQLSMALLINT)fk_table.length());
    } else {
      status = SQLForeignKeys(
          conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)pk_table.c_str(),
          (SQLSMALLINT)pk_table.length(), (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)fk_table.c_str(),
          (SQLSMALLINT)fk_table.length());
    }
  } else if (!pk_table.empty()) {
    if (use_ansi) {
      status = SQLForeignKeysA(
          conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)pk_table.c_str(),
          (SQLSMALLINT)pk_table.length(), nullptr, 0, nullptr, 0, nullptr, 0);
    } else {
      status = SQLForeignKeys(
          conn->hstmt, (SQLCHAR*)catalog_name.c_str(),
          (SQLSMALLINT)catalog_name.length(), (SQLCHAR*)dataset.c_str(),
          (SQLSMALLINT)dataset.length(), (SQLCHAR*)pk_table.c_str(),
          (SQLSMALLINT)pk_table.length(), nullptr, 0, nullptr, 0, nullptr, 0);
    }
  } else {
    if (use_ansi) {
      status = SQLForeignKeysA(
          conn->hstmt, nullptr, 0, nullptr, 0, nullptr, 0,
          (SQLCHAR*)catalog_name.c_str(), (SQLSMALLINT)catalog_name.length(),
          (SQLCHAR*)dataset.c_str(), (SQLSMALLINT)dataset.length(),
          (SQLCHAR*)fk_table.c_str(), (SQLSMALLINT)fk_table.length());
    } else {
      status = SQLForeignKeys(
          conn->hstmt, nullptr, 0, nullptr, 0, nullptr, 0,
          (SQLCHAR*)catalog_name.c_str(), (SQLSMALLINT)catalog_name.length(),
          (SQLCHAR*)dataset.c_str(), (SQLSMALLINT)dataset.length(),
          (SQLCHAR*)fk_table.c_str(), (SQLSMALLINT)fk_table.length());
    }
  }
  CheckError(status, "SQLForeignKeys", conn, use_ansi);

// TODO(sachinpro): Remove the flag once SQLFetch is implemented for Google BQ
// Driver.
#ifndef BQ_DRIVER_INTEGRATION_TESTS
  while (1) {
    std::map<int, std::string> catalog_results;
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    // Col1: pk catalog name , Col2: pk schema name, Col3: pk table name,
    // Col4: pk column name, Col5: fk catalog name, Col6: fk schema name,
    // Col7: fk table name, Col8: fk column name,  Col9: key sequence,
    // Col10: update rule, Col 11: delete rule, Col12: fk name,
    // Col13: pk name, Col14: Deferrability
    std::string pk_table_cat = (char*)catalog_result[1].target_value;
    std::string pk_table_schema = (char*)catalog_result[2].target_value;
    std::string pk_table_name = (char*)catalog_result[3].target_value;
    std::string pk_col_name = (char*)catalog_result[4].target_value;
    std::string fk_table_cat = (char*)catalog_result[5].target_value;
    std::string fk_table_schema = (char*)catalog_result[6].target_value;
    std::string fk_table_name = (char*)catalog_result[7].target_value;
    std::string fk_col_name = (char*)catalog_result[8].target_value;
    SQLSMALLINT* key_seq =
        reinterpret_cast<SQLSMALLINT*>(catalog_result[9].target_value);
    std::string fk_name = (char*)catalog_result[12].target_value;
    std::string pk_name = (char*)catalog_result[13].target_value;
    SQLSMALLINT* deferrability =
        reinterpret_cast<SQLSMALLINT*>(catalog_result[14].target_value);

    if (!pk_table_cat.empty()) catalog_results.insert({1, pk_table_cat});
    if (!pk_table_schema.empty()) catalog_results.insert({2, pk_table_schema});
    if (!pk_table_name.empty()) catalog_results.insert({3, pk_table_name});
    if (!pk_col_name.empty()) catalog_results.insert({4, pk_col_name});
    if (!fk_table_cat.empty()) catalog_results.insert({5, fk_table_cat});
    if (!fk_table_schema.empty()) catalog_results.insert({6, fk_table_schema});
    if (!fk_table_name.empty()) catalog_results.insert({7, fk_table_name});
    if (!fk_col_name.empty()) catalog_results.insert({8, fk_col_name});
    if (key_seq && *key_seq >= 0)
      catalog_results.insert({9, std::to_string(*key_seq)});
    catalog_results.insert({10, "NULL"});  // UPDATE_RULE
    catalog_results.insert({11, "NULL"});  // DELETE_RULE
    if (!fk_name.empty()) catalog_results.insert({12, fk_name});
    if (!pk_name.empty()) catalog_results.insert({13, pk_name});
    if (deferrability && *deferrability >= 0)
      catalog_results.insert({14, std::to_string(*deferrability)});
    results.emplace_back(catalog_results);
  }

#endif  // BQ_DRIVER_INTEGRATION_TESTS

  return results;
}

}  // namespace google::cloud::odbc_tests
