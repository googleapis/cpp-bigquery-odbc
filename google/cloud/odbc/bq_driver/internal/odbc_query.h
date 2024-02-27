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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_QUERY_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_QUERY_H

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

enum class ExecutionState {
  kNotStarted,
  kRunning,
  kFinished,
};

enum class QueryType {
  kUninitializedQuery,
  kTypeInfo,
};

class Query {
 public:
  Query() = default;
  ~Query() = default;

  virtual SQLRETURN GetColumn(int column_id, DataBuffer& buffer) = 0;

  virtual SQLRETURN FetchNextRow(int column_id, DataBuffer& buffer) = 0;

 protected:
  QueryType query_type_{QueryType::kUninitializedQuery};
  ExecutionState execution_state_{ExecutionState::kNotStarted};
  int cursor_{0};
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_QUERY_H
