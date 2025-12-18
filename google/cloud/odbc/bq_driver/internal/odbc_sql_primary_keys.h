// Copyright 2024 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_PRIMARY_KEYS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_PRIMARY_KEYS_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

// Executes a BQ query and fetches the primary key results and
// populates the DSResults, as mentioned below:
//
// 1) First makes a call to ODBCBQClient::Query()
// 2) If Query() finishes within the timeout and returns all results then no
//    further action is needed and the function returns. In this case,
//    the PostQueryResults will be populated in DSResults structure.
// 3) If Query() does not finish in specified timeout then a subsequent call is
// made to
//    ODBCBQClient::GetAllQueryResults() to fetch all the results. In this case,
//    the GetQueryResults will be populated in DSResults structure.
//
static std::vector<std::string> const kPrimaryKeysOrder = {
    "TABLE_CAT",   "TABLE_SCHEM", "TABLE_NAME",
    "COLUMN_NAME", "KEY_SEQ",     "PK_NAME",
};

odbc_internal::StatusRecordOr<DSResults> FetchPrimaryKeysFromDataSource(
    StatementHandle& stmt_handle, std::string const& catalog_name,
    int catalog_name_len, std::string const& schema_name, int schema_name_len,
    std::string const& table_name, int table_name_len);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_PRIMARY_KEYS_H
