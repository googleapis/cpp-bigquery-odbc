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
#include <stdexcept>
#include <vector>

namespace google::cloud::odbc_client {

ODBCHandles::~ODBCHandles() {
  if (hstmt != SQL_NULL_HSTMT) {
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
  }
  if (connected && hdbc != SQL_NULL_HDBC) {
    SQLDisconnect(hdbc);
  }
  if (hdbc != SQL_NULL_HDBC) {
    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
  }
  if (henv != SQL_NULL_HENV) {
    SQLFreeHandle(SQL_HANDLE_ENV, henv);
  }
}

void CheckError(SQLRETURN status, std::string const& api,
                ODBCHandles& handles) {
  if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
    return;
  }

  SQLSMALLINT handle_type = SQL_HANDLE_ENV;
  SQLHANDLE handle = handles.henv;

  if (handles.hstmt != SQL_NULL_HSTMT) {
    handle_type = SQL_HANDLE_STMT;
    handle = handles.hstmt;
  } else if (handles.hdbc != SQL_NULL_HDBC) {
    handle_type = SQL_HANDLE_DBC;
    handle = handles.hdbc;
  }

  std::cerr << "Error during " << api << " (RC=" << status << "):" << std::endl;
  SQLCHAR sqlstate[6];
  SQLINTEGER native_error;
  SQLCHAR message_text[SQL_MAX_MESSAGE_LENGTH];
  SQLSMALLINT text_length;
  SQLSMALLINT i = 1;
  while (SQLGetDiagRec(handle_type, handle, i++, sqlstate, &native_error,
                       message_text, sizeof(message_text),
                       &text_length) == SQL_SUCCESS) {
    std::cerr << "  SQLSTATE: " << sqlstate
              << " | Native Error: " << native_error
              << " | Message: " << message_text << std::endl;
  }

  throw std::runtime_error(api +
                           " failed with status: " + std::to_string(status));
}

void Connect(std::string const& conn_str, ODBCHandles& handles) {
  SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &handles.henv);
  if (rc != SQL_SUCCESS) {
    throw std::runtime_error("Failed to allocate environment handle");
  }

  rc = SQLSetEnvAttr(handles.henv, SQL_ATTR_ODBC_VERSION,
                     (SQLPOINTER)SQL_OV_ODBC3, 0);
  CheckError(rc, "SQLSetEnvAttr", handles);

  rc = SQLAllocHandle(SQL_HANDLE_DBC, handles.henv, &handles.hdbc);
  CheckError(rc, "SQLAllocHandle(DBC)", handles);

  std::cout << "Connecting to database..." << std::endl;
  SQLCHAR out_conn_str[1024];
  SQLSMALLINT out_conn_str_len;
  rc = SQLDriverConnect(handles.hdbc, NULL, (SQLCHAR*)conn_str.c_str(), SQL_NTS,
                        out_conn_str, sizeof(out_conn_str), &out_conn_str_len,
                        SQL_DRIVER_COMPLETE);
  CheckError(rc, "SQLDriverConnect", handles);
  handles.connected = true;
  std::cout << "Connected successfully! Out connection string: " << out_conn_str
            << std::endl;

  rc = SQLAllocHandle(SQL_HANDLE_STMT, handles.hdbc, &handles.hstmt);
  CheckError(rc, "SQLAllocHandle(STMT)", handles);

  rc = SQLSetStmtAttr(handles.hstmt, SQL_ATTR_METADATA_ID,
                      (SQLPOINTER)SQL_FALSE, 0);
  CheckError(rc, "SQLSetStmtAttr(SQL_ATTR_METADATA_ID)", handles);
}

void Disconnect(ODBCHandles& handles) {
  if (handles.hstmt != SQL_NULL_HSTMT) {
    SQLFreeHandle(SQL_HANDLE_STMT, handles.hstmt);
    handles.hstmt = SQL_NULL_HSTMT;
  }
  if (handles.connected && handles.hdbc != SQL_NULL_HDBC) {
    SQLDisconnect(handles.hdbc);
    handles.connected = false;
  }
  if (handles.hdbc != SQL_NULL_HDBC) {
    SQLFreeHandle(SQL_HANDLE_DBC, handles.hdbc);
    handles.hdbc = SQL_NULL_HDBC;
  }
  if (handles.henv != SQL_NULL_HENV) {
    SQLFreeHandle(SQL_HANDLE_ENV, handles.henv);
    handles.henv = SQL_NULL_HENV;
  }
}

void PrintResultSet(SQLHSTMT hstmt, ODBCHandles& handles,
                    std::vector<std::string> const& allowed_cols,
                    std::chrono::steady_clock::time_point start_time) {
  SQLSMALLINT num_cols = 0;
  SQLRETURN rc = SQLNumResultCols(hstmt, &num_cols);
  CheckError(rc, "SQLNumResultCols", handles);
  if (num_cols == 0) {
    std::cout << "No columns in result set." << std::endl;
    return;
  }

  // Get all column names and details, and track which ones we should display
  std::vector<std::string> col_names;
  std::vector<bool> should_display(num_cols, true);
  bool has_filters = !allowed_cols.empty();

  for (SQLSMALLINT i = 1; i <= num_cols; i++) {
    SQLCHAR col_name[256];
    SQLSMALLINT col_name_len;
    SQLSMALLINT col_type;
    SQLULEN col_size;
    SQLSMALLINT decimal_digits;
    SQLSMALLINT nullable;
    rc = SQLDescribeCol(hstmt, i, col_name, sizeof(col_name), &col_name_len,
                        &col_type, &col_size, &decimal_digits, &nullable);
    CheckError(rc, "SQLDescribeCol", handles);
    std::string name = reinterpret_cast<char*>(col_name);
    col_names.push_back(name);

    if (has_filters) {
      bool found = false;
      for (auto const& filter : allowed_cols) {
        if (filter == name) {
          found = true;
          break;
        }
      }
      should_display[i - 1] = found;
    }
  }

  // Print header
  bool first_col = true;
  for (SQLSMALLINT i = 1; i <= num_cols; i++) {
    if (!should_display[i - 1]) continue;
    if (!first_col) std::cout << " | ";
    std::cout << col_names[i - 1];
    first_col = false;
  }
  std::cout << std::endl;
  std::cout << std::string(80, '-') << std::endl;

  auto fetch_start_time = std::chrono::steady_clock::now();

  // Print rows
  while ((rc = SQLFetch(hstmt)) == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
    first_col = true;
    for (SQLSMALLINT i = 1; i <= num_cols; i++) {
      char buf[4096];
      SQLLEN indicator;
      rc = SQLGetData(hstmt, i, SQL_C_CHAR, buf, sizeof(buf), &indicator);
      CheckError(rc, "SQLGetData", handles);

      if (!should_display[i - 1]) continue;
      if (!first_col) std::cout << " | ";
      if (indicator == SQL_NULL_DATA) {
        std::cout << "NULL";
      } else {
        std::cout << buf;
      }
      first_col = false;
    }
    std::cout << std::endl;
  }
  if (rc != SQL_NO_DATA) {
    CheckError(rc, "SQLFetch", handles);
  }

  auto end_time = std::chrono::steady_clock::now();
  auto fetch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - fetch_start_time)
                            .count();

  std::cout << std::string(80, '-') << std::endl;
  std::cout << "Iteration time: " << fetch_duration << " ms";
  if (start_time.time_since_epoch().count() > 0) {
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              end_time - start_time)
                              .count();
    std::cout << ", Overall total time: " << total_duration << " ms";
  }
  std::cout << std::endl;
}

void MeasurePerformance(SQLHSTMT hstmt, ODBCHandles& handles,
                        std::chrono::steady_clock::time_point start_time) {
  SQLSMALLINT num_cols = 0;
  SQLRETURN rc = SQLNumResultCols(hstmt, &num_cols);
  CheckError(rc, "SQLNumResultCols", handles);
  if (num_cols == 0) {
    std::cout << "No columns in result set." << std::endl;
    return;
  }

  std::cout << "Fetching results in performance mode..." << std::endl;
  long long total_rows = 0;

  auto fetch_start_time = std::chrono::steady_clock::now();
  auto prev_time = fetch_start_time;

  while ((rc = SQLFetch(hstmt)) == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
    for (SQLSMALLINT i = 1; i <= num_cols; i++) {
      char buf[256];
      SQLLEN indicator;
      // Fetch and discard data to simulate extraction overhead
      SQLGetData(hstmt, i, SQL_C_CHAR, buf, sizeof(buf), &indicator);
    }
    total_rows++;
    if (total_rows % 1000 == 0) {
      auto current_time = std::chrono::steady_clock::now();
      auto total_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(current_time -
                                                                start_time)
              .count();
      auto fetch_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              current_time - fetch_start_time)
              .count();
      auto diff_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(current_time -
                                                                prev_time)
              .count();
      std::cout << total_rows
                << " rows fetched... (last 1000: " << diff_duration
                << " ms, iteration total: " << fetch_duration
                << " ms, overall total: " << total_duration << " ms)"
                << std::endl;
      prev_time = current_time;
    }
  }

  if (rc != SQL_NO_DATA) {
    CheckError(rc, "SQLFetch", handles);
  }

  auto end_time = std::chrono::steady_clock::now();
  auto fetch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - fetch_start_time)
                            .count();
  auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time)
                            .count();
  std::cout << "Performance test complete. Total rows fetched: " << total_rows
            << ", Iteration time: " << fetch_duration << " ms"
            << ", Overall total time: " << total_duration << " ms" << std::endl;
}

}  // namespace google::cloud::odbc_client
