
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

#ifndef GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_STATEMENT_H
#define GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_STATEMENT_H

#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

SQLRETURN InsertStatement(std::shared_ptr<ODBCHandles> conn);

SQLRETURN InsertStatementWithBindParameter(std::shared_ptr<ODBCHandles> conn);
SQLRETURN InsertStatementWithoutBindParameter(
    std::shared_ptr<ODBCHandles> conn);

SQLRETURN InsertDirectStatement(std::shared_ptr<ODBCHandles> conn);

// Fetches results of a read query row-by-row and returns them as a map with the
// column as keys
std::shared_ptr<Results> FetchResults(std::shared_ptr<ODBCHandles> conn,
                                      std::string query, bool use_bind_col);

// Fetches results of a read query as a result set of size <rs_size>
//  and returns them as a map with the column as keys
std::shared_ptr<Results> ScrollResults(std::shared_ptr<ODBCHandles> conn,
                                       std::string query, int rs_size);

// Fetches results of a read query using SQLFetch and SQLGetData
// Returns the results as a map with the column as keys
std::shared_ptr<Results> FetchResultsWithSqlGetData(
    std::shared_ptr<ODBCHandles> conn, std::string query);

void InsertDataWithSqlPut(std::shared_ptr<ODBCHandles> conn, std::string query,
                          std::vector<std::string> data);

}  // namespace google::cloud::odbc_tests

#endif  // GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_STATEMENT_H
