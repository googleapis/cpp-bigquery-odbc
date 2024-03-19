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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_DIAGNOSTIC_RECORDS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_DIAGNOSTIC_RECORDS_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/status.h"
#include "absl/strings/match.h"
#include <string>

namespace google::cloud::odbc_internal {

// Contains general information about a function's execution
struct HeaderRecord {
  std::string function;
  SQLINTEGER function_code = 0;
  SQLLEN cursor_row_count = 0;
  SQLLEN row_count = 0;
};

// Contains information about specific errors or warnings, happened during
// function execution
struct StatusRecord {
  // Converts google-cloud-cpp::Status to odbc::StatusRecord
  static StatusRecord ConvertFrom(Status const& status) {
    std::string message = "[BigQuery] " + status.message();
    switch (status.code()) {
      case StatusCode::kInvalidArgument:
        return {SQLStates::k_42000(), message, 400};
      case StatusCode::kUnauthenticated:
        return {SQLStates::k_28000(), message, 401};
      case StatusCode::kPermissionDenied:
        return {SQLStates::k_42000(), message, 403};
      case StatusCode::kNotFound:
        return {SQLStates::k_HY000(), message, 404};
      case StatusCode::kAborted:
        return {SQLStates::k_HY000(), message, 409};
      case StatusCode::kInternal:
        return {SQLStates::k_HY000(), message, 501};
      default:
        return {SQLStates::k_HY000(), message, 500};
    }
  }

  [[nodiscard]] SQLRETURN CalculateReturnCode() const {
    if (ok()) {
      return SQL_SUCCESS;
    }
    if (absl::StartsWith(sql_state, "01")) {
      return SQL_SUCCESS_WITH_INFO;
    }
    return SQL_ERROR;
  }

  inline static StatusRecord Ok() { return {}; }

  [[nodiscard]] bool ok() const { return sql_state.empty() && message.empty(); }

  std::string sql_state;
  std::string message;
  SQLINTEGER native_error_code = 0;
  SQLINTEGER column_number = SQL_COLUMN_NUMBER_UNKNOWN;
  SQLLEN row_number = SQL_ROW_NUMBER_UNKNOWN;
  std::string connection_name;
  std::string server_name;
};

}  // namespace google::cloud::odbc_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_DIAGNOSTIC_RECORDS_H
