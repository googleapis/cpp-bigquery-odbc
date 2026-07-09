// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "commons.h"
#include <chrono>
#include <iostream>
#include <string>

using namespace google::cloud::odbc_client;

int main(int argc, char* argv[]) {
  std::string conn_str;
  std::string cmd;
  std::string sql_query;

  std::string catalog_id;
  std::string schema_id;
  std::string table_id;
  std::string table_type;

  bool is_interactive = true;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--conn_str" && i + 1 < argc) {
      conn_str = argv[++i];
    } else if (arg == "--cmd" && i + 1 < argc) {
      cmd = argv[++i];
      is_interactive = false;
    } else if (arg == "--query" && i + 1 < argc) {
      sql_query = argv[++i];
    } else if (arg == "--catalog" && i + 1 < argc) {
      catalog_id = argv[++i];
    } else if (arg == "--schema" && i + 1 < argc) {
      schema_id = argv[++i];
    } else if (arg == "--table" && i + 1 < argc) {
      table_id = argv[++i];
    } else if (arg == "--type" && i + 1 < argc) {
      table_type = argv[++i];
    }
  }

  if (conn_str.empty()) {
    std::cout << "Enter Connection String (e.g. DSN=BigQueryDSN): ";
    std::getline(std::cin, conn_str);
  }

  ODBCHandles handles;

  try {
    auto start_time = std::chrono::steady_clock::now();
    Connect(conn_str, handles);

    if (cmd.empty()) {
      std::cout << "Choose Action (query, datasets, tables, projects, perf): ";
      std::getline(std::cin, cmd);
    }

    if (cmd == "query") {
      if (sql_query.empty()) {
        if (is_interactive) {
          std::cout << "Enter SQL Query: ";
          std::getline(std::cin, sql_query);
        } else {
          throw std::runtime_error(
              "SQL query is required in non-interactive mode. Pass using "
              "--query");
        }
      }
      std::cout << "Executing query: " << sql_query << std::endl;
      SQLRETURN rc =
          SQLExecDirect(handles.hstmt, (SQLCHAR*)sql_query.c_str(), SQL_NTS);
      CheckError(rc, "SQLExecDirect", handles);
      PrintResultSet(handles.hstmt, handles, {}, start_time);

    } else if (cmd == "projects") {
      std::cout << "Listing projects/catalogs..." << std::endl;
      SQLRETURN rc =
          SQLTables(handles.hstmt, (SQLCHAR*)"%", SQL_NTS, (SQLCHAR*)"", 0,
                    (SQLCHAR*)"", 0, (SQLCHAR*)"", 0);
      CheckError(rc, "SQLTables", handles);
      PrintResultSet(handles.hstmt, handles, {"TABLE_CAT"}, start_time);

    } else if (cmd == "datasets") {
      std::cout << "Listing datasets..." << std::endl;
      SQLRETURN rc = SQLTables(handles.hstmt, (SQLCHAR*)"", 0, (SQLCHAR*)"%",
                               SQL_NTS, (SQLCHAR*)"", 0, (SQLCHAR*)"", 0);
      CheckError(rc, "SQLTables", handles);
      PrintResultSet(handles.hstmt, handles, {"TABLE_SCHEM"}, start_time);

    } else if (cmd == "tables") {
      std::string catalog = catalog_id;
      if (catalog.empty() && is_interactive) {
        std::cout
            << "Enter Catalog/Project ID (optional, press Enter for default): ";
        std::getline(std::cin, catalog);
      }
      std::string schema = schema_id;
      if (schema.empty() && is_interactive) {
        std::cout
            << "Enter Schema/Dataset ID (optional, press Enter for all): ";
        std::getline(std::cin, schema);
      }
      std::string table = table_id;
      if (table.empty() && is_interactive) {
        std::cout << "Enter Table Name pattern (optional, press Enter for all, "
                     "e.g. %): ";
        std::getline(std::cin, table);
      }
      std::string type = table_type;
      if (type.empty() && is_interactive) {
        std::cout << "Enter Table Type (optional, press Enter for all, e.g. "
                     "TABLE, VIEW): ";
        std::getline(std::cin, type);
      }

      std::cout << "Listing tables..." << std::endl;
      SQLRETURN rc = SQLTables(
          handles.hstmt, catalog.empty() ? NULL : (SQLCHAR*)catalog.c_str(),
          catalog.empty() ? 0 : SQL_NTS,
          schema.empty() ? NULL : (SQLCHAR*)schema.c_str(),
          schema.empty() ? 0 : SQL_NTS,
          table.empty() ? NULL : (SQLCHAR*)table.c_str(),
          table.empty() ? 0 : SQL_NTS,
          type.empty() ? NULL : (SQLCHAR*)type.c_str(),
          type.empty() ? 0 : SQL_NTS);
      CheckError(rc, "SQLTables", handles);
      PrintResultSet(handles.hstmt, handles, {"TABLE_NAME", "TABLE_TYPE"},
                     start_time);

    } else if (cmd == "perf") {
      if (sql_query.empty()) {
        if (is_interactive) {
          std::cout << "Enter SQL Query: ";
          std::getline(std::cin, sql_query);
        } else {
          throw std::runtime_error(
              "SQL query is required in non-interactive mode. Pass using "
              "--query");
        }
      }
      std::cout << "Executing query (perf mode): " << sql_query << std::endl;
      SQLRETURN rc =
          SQLExecDirect(handles.hstmt, (SQLCHAR*)sql_query.c_str(), SQL_NTS);
      CheckError(rc, "SQLExecDirect", handles);
      MeasurePerformance(handles.hstmt, handles, start_time);

    } else {
      std::cerr << "Unknown command: " << cmd << std::endl;
    }
  } catch (std::exception const& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
