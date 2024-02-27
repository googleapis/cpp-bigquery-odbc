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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include <iostream>

namespace google::cloud::odbc_bq_driver_internal {

SQLRETURN TypeInfoQuery::Execute(SQLSMALLINT data_type) {
  query_type_ = QueryType::kTypeInfo;
  execution_state_ = ExecutionState::kRunning;

  if (data_type == SQL_ALL_TYPES) {
    for (auto [sql_data_type, bq_data_type_info] : kSqlToBqDataTypes) {
      for (auto [bq_data_type, type_info] : bq_data_type_info) {
        rows_.push_back(type_info);
      }
    }
  } else {
    for (auto [bq_data_type, type_info] : kSqlToBqDataTypes.at(data_type)) {
      rows_.push_back(type_info);
    }
  }
  execution_state_ = ExecutionState::kFinished;

  cursor_ = 0;
  return SQL_SUCCESS;
}

SQLRETURN TypeInfoQuery::GetColumn(int column_id, DataBuffer& buffer) {
  return SQL_SUCCESS;
}

SQLRETURN TypeInfoQuery::FetchNextRow(int column_id, DataBuffer& buffer) {
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver_internal
