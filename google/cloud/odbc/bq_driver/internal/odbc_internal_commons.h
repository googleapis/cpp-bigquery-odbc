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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_INTERNAL_COMMONS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_INTERNAL_COMMONS_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "absl/types/variant.h"
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

// Data Types as supported by the BQ DataSource.
enum BQDataType {
  kArray,
  kBigNumeric,
  kNumeric,
  kBytes,
  kInt64,
  kDate,
  kFloat64,
  kInterval,
  kGeography,
  kDatetime,
  kTime,
  kBool,
  kString,
  kRange,
  kStruct,
  kJson,
  kTimeStamp,
  kNull
};

// Contains the column data types, as represented by the data source,
// for each column in a ResultSet row.
struct ColumnSchema {
  int col_index;
  BQDataType col_type;
};

// Data Source Value.
using DSValue = std::vector<char>;

// Data Source Row.
using DSRow = std::vector<DSValue>;

// ResultSet rows containing data source rows.
using ResultSetRows = std::vector<DSRow>;

// Row schema representing the column schema of the
// data source row columns.
using RowSchema = std::vector<ColumnSchema>;

// ResultSet structure representing the data from the BQ DataSource.
struct ResultSet {
  RowSchema row_schema;
  ResultSetRows rows;
  mutable int cursor{0};  // points to the next row that can be fetched
};

inline void StringToDSValue(std::string& str, DSValue& value) {
  value.resize(str.size());
  std::copy(str.begin(), str.end(), value.begin());
}

inline void StringToDSValue(const SQLCHAR* c_str, DSValue& value) {
  std::string str = reinterpret_cast<char const*>(c_str);
  StringToDSValue(str, value);
}

inline void DSValueToString(DSValue const& value, std::string& str) {
  str.assign(value.begin(), value.end());
}

inline void IntToDSValue(int64_t int_val, DSValue& ds_value) {
  ds_value.resize(sizeof(int_val));
  std::memcpy(ds_value.data(), &int_val, sizeof(int64_t));
}

template <typename SrcType>
inline void ArithmeticToDSValue(SrcType arithmetic_val, DSValue& ds_value) {
  ds_value.resize(sizeof(SrcType));
  std::memcpy(ds_value.data(), &arithmetic_val, sizeof(SrcType));
}

template <typename SrcType>
inline SrcType DSValueToArithmetic(DSValue& ds_value) {
  SrcType val;
  std::memcpy(&val, ds_value.data(), sizeof(val));
  return val;
}

inline int64_t DSValueToInt(DSValue& ds_value) {
  int64_t int_val;
  std::memcpy(&int_val, ds_value.data(), sizeof(int_val));
  return int_val;
}

// This is the result populated by performing a bq query API.
// For each call, onely one of PostQueryResults or GetQueryResults will be
// populated with the following semantics:
//
// PostQueryResults:
//   - Query finished in specified or default timeout.
//   - All query results rows will be present in PostQueryResults.
// GetQueryResults
//   - Query did not finish in specified or default timeout and odbc bq client's
//     GetAllQueryResults was called.
//   - All query results rows will be present in GetQueryResults.
struct DSResults {
  absl::variant<absl::monostate,
                google::cloud::bigquery_v2_minimal_internal::PostQueryResults,
                google::cloud::bigquery_v2_minimal_internal::GetQueryResults>
      data_source_results;
};

//////////////////////////////////////////////////////////////////////
// Common Helper functions for data retrieval from BQ data source
/////////////////////////////////////////////////////////////////////
odbc_internal::StatusRecordOr<DSResults> FetchBQData(
    ConnectionHandle& conn_handle,
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
        post_query_request);

////////////////////////////////////////////////////////////////////////
// Common Helper functions for processing data results from BQ data source and
// converting that to ODBC result sets.
////////////////////////////////////////////////////////////////////////

odbc_internal::StatusRecordOr<ResultSet> ProcessResultSetRows(
    google::cloud::bigquery_v2_minimal_internal::TableSchema const& schema,
    std::vector<google::cloud::bigquery_v2_minimal_internal::RowData> const&
        rows);

odbc_internal::StatusRecordOr<ResultSet> ProcessPostQueryResults(
    google::cloud::bigquery_v2_minimal_internal::PostQueryResults const&
        post_query_results);

odbc_internal::StatusRecordOr<ResultSet> ProcessGetQueryResults(
    google::cloud::bigquery_v2_minimal_internal::GetQueryResults const&
        get_query_results);

odbc_internal::StatusRecordOr<ResultSet> ProcessQueryResults(
    DSResults const& query_results);

///////////////////////////////////////////////////////
// Common helper functions.
////////////////////////////////////////////////////////
odbc_internal::StatusRecordOr<BQDataType> ConvertDSType(
    std::string const& type);

odbc_internal::StatusRecordOr<
    std::vector<google::cloud::bigquery_v2_minimal_internal::QueryParameter>>
ConstructStringQueryParameters(
    std::map<std::string, std::string> const& params);

odbc_internal::StatusRecordOr<
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest>
ConstructNamedParametersPostQueryRequest(
    std::string const& catalog, std::string const& dataset,
    std::string const& named_query,
    std::map<std::string, std::string> const& named_query_params);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_INTERNAL_COMMONS_H
