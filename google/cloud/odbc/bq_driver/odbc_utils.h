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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/status_or.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver {

inline constexpr int kPrecisionUnchanged = 111;
inline constexpr int kScaleUnchanged = 112;

std::string const kExistingDriver = "Simba ODBC Driver for Google BigQuery";

inline char* ToCharStr(SQLCHAR const* sql_str,
                       std::string const& default_val = "") {
  if (!sql_str) {
    return const_cast<char*>(default_val.c_str());
  }
  return reinterpret_cast<char*>(const_cast<SQLCHAR*>(sql_str));
}

inline SQLCHAR* ToSqlChar(char const* str) {
  if (!str) {
    return reinterpret_cast<SQLCHAR*>(const_cast<char*>(""));
  }
  return reinterpret_cast<SQLCHAR*>(const_cast<char*>(str));
}

inline SQLWCHAR* ToSqlWChar(wchar_t const* wstr) {
  if (!wstr) {
    return reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(L""));
  }
  return reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wstr));
}

// Very simple client side check for email. We want all validations to be
// done on the BQ server hence keeping the client side check simple. Otherwise
// client can reject emails that server accepts or vice-versa. It is not
// possible to keep email rules in sync between odbc client and BQ server.
inline bool IsValidEmail(std::string const& email) {
  return (!email.empty() && absl::StrContains(email, "@") &&
          absl::StrContains(email, "."));
}

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::ConnectionHandle*>
ValidateConnectionHandle(SQLHDBC connection_handle,
                         bool check_if_connected = true);

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::EnvironmentHandle*>
ValidateEnvironmentHandle(SQLHENV environment_handle);

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::StatementHandle*>
ValidateStatementHandle(SQLHSTMT stmt_handle);

odbc_internal::StatusRecordOr<
    google::cloud::odbc_bq_driver_internal::DescriptorHandle*>
ValidateDescriptorHandle(SQLHDESC desc_handle);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_UTILS_H
