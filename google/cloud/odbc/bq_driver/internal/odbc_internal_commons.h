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

#include "google/cloud/odbc/internal/odbc_includes.h"
#include <cstring>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

// Data Types as supported by the BQ DataSource.
enum BQDataType { kString, kInt64 };

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

// ResultSet rowa containing data source rows.
using ResultSetRows = std::vector<DSRow>;

// Row schema representing the column schema of the
// data source row columns.
using RowSchema = std::vector<ColumnSchema>;

// ResultSet structure representing the data from the BQ DataSource.
struct ResultSet {
  RowSchema row_schema;
  ResultSetRows rows;
};

inline void StringToDSValue(
    std::string& str,
    ::google::cloud::odbc_bq_driver_internal::DSValue& value) {
  value.resize(str.size());
  std::copy(str.begin(), str.end(), value.begin());
}

inline void DSValueToString(
    ::google::cloud::odbc_bq_driver_internal::DSValue& value,
    std::string& str) {
  str.assign(value.begin(), value.end());
}

inline void IntToDSValue(int64_t int_val, DSValue& ds_value) {
  ds_value.resize(sizeof(int_val));
  std::memcpy(ds_value.data(), &int_val, sizeof(int64_t));
}

inline int64_t DSValueToInt(DSValue& ds_value) {
  int64_t int_val;
  std::memcpy(&int_val, ds_value.data(), sizeof(int_val));
  return int_val;
}

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_INTERNAL_COMMONS_H
