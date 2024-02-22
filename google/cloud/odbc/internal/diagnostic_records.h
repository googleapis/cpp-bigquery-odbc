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

#ifndef GOOGLE_CLOUD_ODBC_INTERNAL_DIAGNOSTIC_RECORDS_H
#define GOOGLE_CLOUD_ODBC_INTERNAL_DIAGNOSTIC_RECORDS_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include <string>

namespace google::cloud::odbc_internal {

// Contains general information about a function's execution
struct HeaderRecord {
  std::string function;
  int function_code = 0;
  int cursor_row_count = 0;
  int row_count = 0;
};

// Contains information about specific errors or warnings, happened during
// function execution
struct StatusRecord {
  std::string sql_state;
  std::string message;
  int native_error_code = 0;
  int column_number = SQL_COLUMN_NUMBER_UNKNOWN;
  int row_number = SQL_ROW_NUMBER_UNKNOWN;
  std::string connection_name;
  std::string server_name;
};

}  // namespace google::cloud::odbc_internal

#endif  // GOOGLE_CLOUD_ODBC_INTERNAL_DIAGNOSTIC_RECORDS_H
