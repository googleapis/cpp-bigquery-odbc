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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

// This is the result populated by FetchPrimaryKeysFromDataSource() method.
// For each call, onely one of PostQueryResults or GetQueryResults will be
// populated with the following semantics:
//
// PostQueryResults:
//   - Query finished in specified or default timeout.
//   - All query results rows will be present in PostQueryResults.
// GetQueryResults
//   - Query did not finish in specified or default timeout and odbc bq client's
//     GetAllQueryResults was called by FetchPrimaryKeysFromDataSource()
//   - All query results rows will be present in GetQueryResults.
struct DSPrimaryKeysResults {
  absl::variant<absl::monostate,
                ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults,
                ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
      primary_key_results;
};

// Executes a BQ query and fetches the primary key results and
// populates the DSPrimaryKeysResults in accordance with the semantics
// mentioned above.
// 1) First makes a call to ODBCBQClient::Query()
// 2) If Query() finishes within the timeout and returns all results then no
//    further action is needed and the function returns. In this case,
//    the PostQueryResults will be populated in DSPrimaryKeysResults structure.
// 3) If Query() does not finish in specified timeout then a subsequent call is
// made to
//    ODBCBQClient::GetAllQueryResults() to fetch all the results. In this case,
//    the GetQueryResults will be populated in DSPrimaryKeysResults structure.
//
odbc_internal::StatusRecordOr<DSPrimaryKeysResults>
FetchPrimaryKeysFromDataSource(std::string const& catalog_name,
                               int catalogNameLen,
                               std::string const& schemaName, int schemaNameLen,
                               std::string const& tableName, int tableNameLen);

// Constructs and Populates the ODBC ResultSet structure from
// PostQueryResults in DSPrimaryKeysResults structure.
odbc_internal::StatusRecordOr<ResultSet> ProcessPKPostQueryResults(
    ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults const&
        primaryKeysQueryResults);

// Constructs and Populates the ODBC ResultSet structure from
// GetQueryResults in DSPrimaryKeysResults structure.
odbc_internal::StatusRecordOr<ResultSet> ProcessPKGetQueryResults(
    ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults const&
        primaryKeysQueryResults);

// Helper functions for SQLPrimaryKeys. We may move this to
// odbc_internal_commons if they are needed by other ODBC APIs.
odbc_internal::StatusRecordOr<BQDataType> ConvertDSType(
    std::string const& type);
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_PRIMARY_KEYS_H
