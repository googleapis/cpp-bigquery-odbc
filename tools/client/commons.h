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

#ifndef CPP_BIGQUERY_ODBC_TOOLS_CLIENT_COMMONS_H
#define CPP_BIGQUERY_ODBC_TOOLS_CLIENT_COMMONS_H

#include <chrono>
#include <sql.h>
#include <sqlext.h>
#include <string>
#include <vector>

namespace google::cloud::odbc_client {

struct ODBCHandles {
  SQLHENV henv = SQL_NULL_HENV;
  SQLHDBC hdbc = SQL_NULL_HDBC;
  SQLHSTMT hstmt = SQL_NULL_HSTMT;
  bool connected = false;

  ~ODBCHandles();
};

void CheckError(SQLRETURN status, std::string const& api, ODBCHandles& handles);
void Connect(std::string const& conn_str, ODBCHandles& handles);
void Disconnect(ODBCHandles& handles);
void PrintResultSet(SQLHSTMT hstmt, ODBCHandles& handles,
                    std::vector<std::string> const& allowed_cols = {},
                    std::chrono::steady_clock::time_point start_time = {});
void MeasurePerformance(SQLHSTMT hstmt, ODBCHandles& handles,
                        std::chrono::steady_clock::time_point start_time);

}  // namespace google::cloud::odbc_client

#endif  // CPP_BIGQUERY_ODBC_TOOLS_CLIENT_COMMONS_H
