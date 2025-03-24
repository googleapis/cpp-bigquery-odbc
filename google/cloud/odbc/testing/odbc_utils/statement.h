
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_STATEMENT_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_STATEMENT_H

#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

SQLRETURN GetStmtAttr(SQLHSTMT stmt_handle, SQLINTEGER attribute,
                      SQLPOINTER value, SQLINTEGER value_buffer_len,
                      SQLINTEGER* value_string_len, bool use_ansi);

void VerifyRowWiseResults(RowWiseResults const& actual_results,
                          RowWiseResults const& expected_results);

void VerifyRowWiseResults(RowWiseResults const& actual_results,
                          StdRows const& expected_results);

SQLRETURN InsertStatement(std::shared_ptr<ODBCHandles> conn,
                          bool use_ansi = false);

SQLRETURN InsertStatementWithBindParameter(std::shared_ptr<ODBCHandles> conn,
                                           bool use_ansi = false);
SQLRETURN InsertStatementWithoutBindParameter(std::shared_ptr<ODBCHandles> conn,
                                              bool use_ansi = false);

SQLRETURN InsertDirectStatement(std::shared_ptr<ODBCHandles> conn,
                                bool use_ansi = false);

// Fetches results of a read query row-by-row and returns them as a map with the
// column as keys
std::shared_ptr<Results> FetchResults(std::shared_ptr<ODBCHandles> conn,
                                      std::string query, bool use_bind_col,
                                      bool use_ansi = false);

// Fetches results of a read query with row-wise binding and returns them as a
// map with the column as keys
std::shared_ptr<Results> FetchRowWise(std::shared_ptr<ODBCHandles> conn,
                                      std::string query, int num_cols);

// Uses SQLExecDirect to execute a read query and fetch results
std::shared_ptr<Results> FetchDirect(std::shared_ptr<ODBCHandles> conn,
                                     std::string query, int num_cols,
                                     bool is_async = false,
                                     bool use_ansi = false);

// Fetches results of a read query as a result set of size <rs_size>
//  and returns them as a map with the column as keys
std::shared_ptr<Results> ScrollResults(std::shared_ptr<ODBCHandles> conn,
                                       std::string query, int rs_size,
                                       bool use_ansi = false);
std::shared_ptr<Results> FetchScrollResultsAllColumns(
    std::shared_ptr<ODBCHandles> conn, std::string query,
    SQLSMALLINT fetch_orientation, bool use_bind_col, bool use_ansi = false);

// Fetches results of a read query using SQLFetch and SQLGetData
// Returns the results as a map with the column as keys
std::shared_ptr<Results> FetchResultsWithSqlGetData(
    std::shared_ptr<ODBCHandles> conn, std::string query);  // No ANSI version.

void InsertDataWithSqlPut(std::shared_ptr<ODBCHandles> conn, std::string query,
                          std::vector<std::string> data, bool use_ansi = false);

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_STATEMENT_H
