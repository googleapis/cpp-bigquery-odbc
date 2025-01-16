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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_QUERY_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_QUERY_H

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

enum class ExecutionState {
  kStarted,
  kRunning,
  kFinished,
};

enum class QueryType {
  kTypeInfo,
};

class Query {
 public:
  virtual ~Query() = 0;

  virtual SQLRETURN GetColumn(int column_id, DataBuffer& buffer) = 0;

  virtual SQLRETURN FetchNextRow(int column_id, DataBuffer& buffer) = 0;

 private:
  QueryType query_type_;
  ExecutionState execution_state_;
  int cursor_;
};

// TODO(Kanchan): Add Unit testcase for GetColumnData in odbc_query_test.cc in
// the SQLGetData PR Part 2.
google::cloud::odbc_internal::StatusRecord GetColumnData(
    DSValue const& ds_val, BQDataType bq_data_type, SQLSMALLINT target_c_type,
    SQLPOINTER target_value, SQLLEN target_value_buffer_len,
    SQLLEN* target_value_string_len);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_QUERY_H
