
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_CONNECTION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_CONNECTION_H

#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

// Returns the default DSN name after checking if ODBC_TESTS_DSN env is defined
inline std::string const GetDefaultDSN() {
  return google::cloud::internal::GetEnv("ODBC_TESTS_DSN")
      .value_or("ODBCTestsDSN");
}

std::string const kDefaultDataSource = GetDefaultDSN();

auto const kDefaultConnectionString = "DSN=" + GetDefaultDSN();

// Connect using a <conn_str> and populate the ODBCHandles
SQLRETURN Connect(std::string conn_str, std::shared_ptr<ODBCHandles> conn,
                  int timeout = 30, bool use_ansi = false);

// Connect using a datasource name directly and populate the ODBCHandles
SQLRETURN ConnectDsn(std::string dsn, std::shared_ptr<ODBCHandles> conn,
                     int timeout = 30, bool use_ansi = false);

SQLRETURN Disconnect(std::shared_ptr<ODBCHandles> conn);

SQLRETURN GetDriverInfo(std::shared_ptr<ODBCHandles> conn,
                        bool use_ansi = false);

SQLRETURN GetEnvInfo(std::shared_ptr<ODBCHandles> conn);

SQLRETURN PrintDriverVerName(std::shared_ptr<ODBCHandles> conn,
                             bool use_ansi = false);

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_CONNECTION_H
