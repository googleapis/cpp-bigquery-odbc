
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

std::vector<SQLTableResult> Catalog::GetTables(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& project_id,
    char const* dataset, char const* table, char const* table_type,
    bool use_ansi, int rows_expected) {
  SQLRETURN status;
  int const res_cols = 5;

  TestingDataBuffer columns[res_cols];
  std::vector<SQLTableResult> results;

  for (int i = 0; i < res_cols; i++) {
    status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(i + 1),
                        SQL_C_CHAR, columns[i].target_value,
                        columns[i].buffer_length, &(columns[i].str_len));
    CheckError(status, "SQLBindCol", conn);
  }

  SQLSMALLINT dataset_length = dataset ? SQL_NTS : 0;
  SQLSMALLINT table_length = table ? SQL_NTS : 0;
  SQLSMALLINT table_type_length = table_type ? SQL_NTS : 0;

  if (use_ansi) {
    status = SQLTablesA(
        conn->hstmt,
        const_cast<SQLCHAR*>(
            reinterpret_cast<const SQLCHAR*>(project_id.c_str())),
        SQL_NTS,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(dataset)),
        dataset_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table)),
        table_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table_type)),
        table_type_length);
  } else {
    status = SQLTables(
        conn->hstmt,
        const_cast<SQLCHAR*>(
            reinterpret_cast<const SQLCHAR*>(project_id.c_str())),
        SQL_NTS,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(dataset)),
        dataset_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table)),
        table_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table_type)),
        table_type_length);
  }
  CheckError(status, "SQLTables", conn, use_ansi);

  while (true) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    std::optional<std::string> project_name =
        (columns[0].str_len != SQL_NULL_DATA)
            ? std::optional<std::string>{reinterpret_cast<char*>(
                  columns[0].target_value)}
            : std::nullopt;
    std::optional<std::string> dataset_name =
        (columns[1].str_len != SQL_NULL_DATA)
            ? std::optional<std::string>{reinterpret_cast<char*>(
                  columns[1].target_value)}
            : std::nullopt;
    std::optional<std::string> table_name =
        (columns[2].str_len != SQL_NULL_DATA)
            ? std::optional<std::string>{reinterpret_cast<char*>(
                  columns[2].target_value)}
            : std::nullopt;
    std::optional<std::string> table_type_name =
        (columns[3].str_len != SQL_NULL_DATA)
            ? std::optional<std::string>{reinterpret_cast<char*>(
                  columns[3].target_value)}
            : std::nullopt;
    std::optional<std::string> description =
        (columns[4].str_len != SQL_NULL_DATA)
            ? std::optional<std::string>{reinterpret_cast<char*>(
                  columns[4].target_value)}
            : std::nullopt;

    results.push_back(
        {project_name, dataset_name, table_name, table_type_name, description});
  }
  if (rows_expected >= 0) {
    EXPECT_EQ(results.size(), rows_expected);
  }
  return results;
}

std::vector<SQLColumnsResult> Catalog::GetColumns(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& project_id,
    char const* dataset, char const* table, char const* column, bool use_ansi) {
  SQLRETURN status;
  // For details on the columns returned, please see the spec
  // for SQLColumns API.
  // https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function
  int const res_cols = 18;
  TestingDataBuffer columns[res_cols];

  std::vector<SQLColumnsResult> results;

  for (int i = 0; i < res_cols; i++) {
    switch (i) {
      // VARCHAR
      case 0:
      case 1:
      case 2:
      case 3:
      case 5:
      case 11:
      case 12:
      case 17: {
        status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(i + 1),
                            SQL_C_CHAR, columns[i].target_value,
                            columns[i].buffer_length, &(columns[i].str_len));
        break;
      }
      // SMALLINT
      case 4:
      case 8:
      case 9:
      case 10:
      case 13:
      case 14: {
        status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(i + 1),
                            SQL_C_SSHORT, columns[i].target_value,
                            sizeof(SQLSMALLINT), &(columns[i].str_len));
        break;
      }
      default:
        // INTEGER
        {
          status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(i + 1),
                              SQL_C_SLONG, columns[i].target_value,
                              sizeof(SQLINTEGER), &(columns[i].str_len));
          break;
        }
    }
    CheckError(status, "SQLBindCol", conn);
  }

  SQLSMALLINT dataset_length = dataset ? strlen(dataset) : 0;
  SQLSMALLINT table_length = table ? strlen(table) : 0;
  SQLSMALLINT column_length = column ? strlen(column) : 0;
  if (use_ansi) {
    status = SQLColumnsA(
        conn->hstmt,
        (project_id.empty()
             ? nullptr
             : const_cast<SQLCHAR*>(
                   reinterpret_cast<const SQLCHAR*>(project_id.c_str()))),
        project_id.length(),
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(dataset)),
        dataset_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table)),
        table_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(column)),
        column_length);
  } else {
    status = SQLColumns(
        conn->hstmt,
        (project_id.empty()
             ? nullptr
             : const_cast<SQLCHAR*>(
                   reinterpret_cast<const SQLCHAR*>(project_id.c_str()))),
        project_id.length(),
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(dataset)),
        dataset_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table)),
        table_length,
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(column)),
        column_length);
  }
  CheckError(status, "SQLColumns", conn, use_ansi);

  while (true) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
      break;
    }
    std::string project_name =
        (columns[0].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[0].target_value)
            : "";
    std::string dataset_name =
        (columns[1].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[1].target_value)
            : "";
    std::string table_name =
        (columns[2].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[2].target_value)
            : "";
    std::string column_name =
        (columns[3].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[3].target_value)
            : "";
    SQLSMALLINT data_type =
        (columns[4].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLSMALLINT*>(columns[4].target_value))
            : -1;

    std::string col_type_name =
        (columns[5].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[5].target_value)
            : "";
    SQLINTEGER col_size =
        (columns[6].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLINTEGER*>(columns[6].target_value))
            : SQL_NULL_DATA;
    SQLINTEGER buf_len =
        (columns[7].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLINTEGER*>(columns[7].target_value))
            : SQL_NULL_DATA;
    SQLSMALLINT dec_digits =
        (columns[8].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLSMALLINT*>(columns[8].target_value))
            : SQL_NULL_DATA;
    SQLSMALLINT radix =
        (columns[9].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLSMALLINT*>(columns[9].target_value))
            : SQL_NULL_DATA;
    SQLSMALLINT nullable =
        (columns[10].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLSMALLINT*>(columns[10].target_value))
            : SQL_NULL_DATA;
    std::string description =
        (columns[11].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[11].target_value)
            : "";
    std::string column_default =
        (columns[12].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[12].target_value)
            : "";
    SQLSMALLINT sql_data_type =
        (columns[13].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLSMALLINT*>(columns[13].target_value))
            : SQL_NULL_DATA;
    SQLSMALLINT sql_dt_sub =
        (columns[14].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLSMALLINT*>(columns[14].target_value))
            : SQL_NULL_DATA;
    SQLINTEGER octet_len =
        (columns[15].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLINTEGER*>(columns[15].target_value))
            : SQL_NULL_DATA;
    SQLINTEGER ord_pos =
        (columns[16].str_len != SQL_NULL_DATA)
            ? *(reinterpret_cast<SQLINTEGER*>(columns[16].target_value))
            : SQL_NULL_DATA;
    std::string is_nullable =
        (columns[17].str_len != SQL_NULL_DATA)
            ? reinterpret_cast<char*>(columns[17].target_value)
            : "";

    results.push_back({project_name, dataset_name, table_name, column_name,
                       description, col_type_name, column_default, is_nullable,
                       data_type, sql_data_type, sql_dt_sub, dec_digits, radix,
                       nullable, col_size, buf_len, octet_len, ord_pos});
  }

  return results;
}

RowWiseResults Catalog::GetPrimaryKeys(std::shared_ptr<ODBCHandles> const& conn,
                                       std::string const& dataset,
                                       std::string const& table,
                                       bool use_ansi) {
  SQLRETURN status;
  int const res_cols = 6;
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
                          ToSqlPointer(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  TestingDataBuffer columns[res_cols];

  // Col1: catalog name , Col2: schema name, Col3: table name,
  // Col4: column name, Col5: key sequence , Col6: primary key name
  for (int i = 0; i < res_cols; i++) {
    if (i == 4) {
      // data type is SMALLINT.
      columns[i].target_type = SQL_C_SSHORT;
    } else {
      // data type is Char.
      columns[i].target_type = SQL_C_CHAR;
    }
    status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(i + 1),
                        columns[i].target_type, columns[i].target_value,
                        columns[i].buffer_length, &(columns[i].str_len));
    CheckError(status, "SQLBindCol", conn);
  }

  if (use_ansi) {
    status = SQLPrimaryKeysA(
        conn->hstmt,
        const_cast<SQLCHAR*>(
            reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
        static_cast<SQLSMALLINT>(catalog_name.length()),
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
        static_cast<SQLSMALLINT>(dataset.length()),
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table.c_str())),
        static_cast<SQLSMALLINT>(table.length()));
  } else {
    status = SQLPrimaryKeys(
        conn->hstmt,
        const_cast<SQLCHAR*>(
            reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
        static_cast<SQLSMALLINT>(catalog_name.length()),
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
        static_cast<SQLSMALLINT>(dataset.length()),
        const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(table.c_str())),
        static_cast<SQLSMALLINT>(table.length()));
  }
  CheckError(status, "SQLPrimaryKeys", conn, use_ansi);

  int i = 0;
  while (true) {
    Row catalog_results;
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      CheckError(status, "SQLFetch", conn);
    }
    // Col1: catalog, Col2: schema, Col3: table name,
    // Col4: column name, Col5: key sequence , Col6: primary key
    // Note: ODBC coumns typically start from 1, but columns
    // will be populated starting from index 0
    std::string table_cat =
        (columns[0].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[0].target_value)
            : "";
    std::string table_schema =
        (columns[1].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[1].target_value)
            : "";
    std::string table_name =
        (columns[2].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[2].target_value)
            : "";
    std::string col_name =
        (columns[3].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[3].target_value)
            : "";
    auto* key_seq =
        reinterpret_cast<SQLSMALLINT*>(columns[4].target_value);
    std::string pk_name =
        (columns[5].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[5].target_value)
            : "";

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

RowWiseResults Catalog::GetForeignKeys(std::shared_ptr<ODBCHandles> const& conn,
                                       std::string const& dataset,
                                       std::string const& pk_table,
                                       std::string const& fk_table,
                                       bool use_ansi) {
  SQLRETURN status;
  int const res_cols = 11;
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
                          ToSqlPointer(SQL_FALSE), 0);
  CheckError(status, "SQLSetStmtAttr", conn);

  TestingDataBuffer columns[res_cols];
  // Col1: pk catalog name , Col2: pk schema name, Col3: pk table name,
  // Col4: pk column name, Col5: fk catalog name, Col6: fk schema name,
  // Col7: fk table name, Col8: fk column name,  Col9: key sequence,
  // Col10: fk name, Col11: pk name.
  for (int i = 0; i < res_cols; i++) {
    if (i == 8) {
      // data type is SMALLINT.
      columns[i].target_type = SQL_C_SSHORT;
    } else {
      // data type is Char.
      columns[i].target_type = SQL_C_CHAR;
    }
    status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(i + 1),
                        columns[i].target_type, columns[i].target_value,
                        columns[i].buffer_length, &(columns[i].str_len));
    CheckError(status, "SQLBindCol", conn);
  }

  if (!pk_table.empty() && !fk_table.empty()) {
    if (use_ansi) {
      status = SQLForeignKeysA(
          conn->hstmt,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(pk_table.c_str())),
          static_cast<SQLSMALLINT>(pk_table.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(fk_table.c_str())),
          static_cast<SQLSMALLINT>(fk_table.length()));
    } else {
      status = SQLForeignKeys(
          conn->hstmt,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(pk_table.c_str())),
          static_cast<SQLSMALLINT>(pk_table.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(fk_table.c_str())),
          static_cast<SQLSMALLINT>(fk_table.length()));
    }
  } else if (!pk_table.empty()) {
    if (use_ansi) {
      status = SQLForeignKeysA(
          conn->hstmt,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(pk_table.c_str())),
          static_cast<SQLSMALLINT>(pk_table.length()), nullptr, 0, nullptr, 0,
          nullptr, 0);
    } else {
      status = SQLForeignKeys(
          conn->hstmt,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(pk_table.c_str())),
          static_cast<SQLSMALLINT>(pk_table.length()), nullptr, 0, nullptr, 0,
          nullptr, 0);
    }
  } else {
    if (use_ansi) {
      status = SQLForeignKeysA(
          conn->hstmt, nullptr, 0, nullptr, 0, nullptr, 0,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(fk_table.c_str())),
          static_cast<SQLSMALLINT>(fk_table.length()));
    } else {
      status = SQLForeignKeys(
          conn->hstmt, nullptr, 0, nullptr, 0, nullptr, 0,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(catalog_name.c_str())),
          static_cast<SQLSMALLINT>(catalog_name.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(dataset.c_str())),
          static_cast<SQLSMALLINT>(dataset.length()),
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(fk_table.c_str())),
          static_cast<SQLSMALLINT>(fk_table.length()));
    }
  }
  CheckError(status, "SQLForeignKeys", conn, use_ansi);

  while (true) {
    Row catalog_results;
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
    // Col10: fk name, Col11: pk name.
    std::string pk_table_cat =
        (columns[0].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[0].target_value)
            : "";
    std::string pk_table_schema =
        (columns[1].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[1].target_value)
            : "";
    std::string pk_table_name =
        (columns[2].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[2].target_value)
            : "";
    std::string pk_col_name =
        (columns[3].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[3].target_value)
            : "";
    std::string fk_table_cat =
        (columns[4].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[4].target_value)
            : "";
    std::string fk_table_schema =
        (columns[5].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[5].target_value)
            : "";
    std::string fk_table_name =
        (columns[6].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[6].target_value)
            : "";
    std::string fk_col_name =
        (columns[7].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[7].target_value)
            : "";
    auto* key_seq =
        reinterpret_cast<SQLSMALLINT*>(columns[8].target_value);
    std::string fk_name =
        (columns[9].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[9].target_value)
            : "";
    std::string pk_name =
        (columns[10].str_len != SQL_NULL_DATA)
            ? static_cast<char*>(columns[10].target_value)
            : "";

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
    catalog_results.insert({10, "NULL"});  // UPDATE_RULE (Not supported by BQ)
    catalog_results.insert({11, "NULL"});  // DELETE_RULE (Not supported by BQ)
    if (!fk_name.empty()) catalog_results.insert({12, fk_name});
    if (!pk_name.empty()) catalog_results.insert({13, pk_name});
    catalog_results.insert(
        {14, std::to_string(
                 SQL_NOT_DEFERRABLE)});  // DEFERRABILITY (Not supported by BQ)
    results.emplace_back(catalog_results);
  }
  return results;
}

bool operator==(SQLColumnsResult const& lhs, SQLColumnsResult const& rhs) {
  return (
      lhs.project_name == rhs.project_name &&
      lhs.dataset_name == rhs.dataset_name &&
      lhs.table_name == rhs.table_name && lhs.column_name == rhs.column_name &&
      lhs.description == rhs.description &&
      lhs.col_type_name == rhs.col_type_name &&
      lhs.col_default == rhs.col_default &&
      lhs.is_nullable == rhs.is_nullable &&
      lhs.sql_data_type == rhs.sql_data_type &&
      lhs.data_type == rhs.data_type &&
      lhs.sql_date_time_sub == rhs.sql_date_time_sub &&
      lhs.decimal_digits == rhs.decimal_digits && lhs.radix == rhs.radix &&
      lhs.nullable == rhs.nullable && lhs.col_size == rhs.col_size &&
      lhs.buffer_len == rhs.buffer_len &&
      lhs.char_octet_len == rhs.char_octet_len && lhs.ord_pos == rhs.ord_pos);
}

bool operator>(SQLColumnsResult const& lhs, SQLColumnsResult const& rhs) {
  return (
      lhs.project_name > rhs.project_name &&
      lhs.dataset_name > rhs.dataset_name && lhs.table_name > rhs.table_name &&
      lhs.column_name > rhs.column_name && lhs.description > rhs.description &&
      lhs.col_type_name > rhs.col_type_name &&
      lhs.col_default > rhs.col_default && lhs.is_nullable > rhs.is_nullable &&
      lhs.sql_data_type > rhs.sql_data_type && lhs.data_type > rhs.data_type &&
      lhs.sql_date_time_sub > rhs.sql_date_time_sub &&
      lhs.decimal_digits > rhs.decimal_digits && lhs.radix > rhs.radix &&
      lhs.nullable > rhs.nullable && lhs.col_size > rhs.col_size &&
      lhs.buffer_len > rhs.buffer_len &&
      lhs.char_octet_len > rhs.char_octet_len && lhs.ord_pos > rhs.ord_pos);
}

bool operator<(SQLColumnsResult const& lhs, SQLColumnsResult const& rhs) {
  return (
      lhs.project_name < rhs.project_name &&
      lhs.dataset_name < rhs.dataset_name && lhs.table_name < rhs.table_name &&
      lhs.column_name < rhs.column_name && lhs.description < rhs.description &&
      lhs.col_type_name < rhs.col_type_name &&
      lhs.col_default < rhs.col_default && lhs.is_nullable < rhs.is_nullable &&
      lhs.sql_data_type < rhs.sql_data_type && lhs.data_type < rhs.data_type &&
      lhs.sql_date_time_sub < rhs.sql_date_time_sub &&
      lhs.decimal_digits < rhs.decimal_digits && lhs.radix < rhs.radix &&
      lhs.nullable < rhs.nullable && lhs.col_size < rhs.col_size &&
      lhs.buffer_len < rhs.buffer_len &&
      lhs.char_octet_len < rhs.char_octet_len && lhs.ord_pos < rhs.ord_pos);
}
}  // namespace google::cloud::odbc_tests
