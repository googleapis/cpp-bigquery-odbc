
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_CONNECTION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_CONNECTION_H

#include <testing/commons.h>

namespace google {
namespace cloud {
namespace bigquery_odbc {

const string kDefaultDataSource = "ODBCTestsDSN";
const string kDefaultConnectionString("DSN="+kDefaultDataSource);

// Connect using a <conn_str> and populate the ConnectionHandle
SQLRETURN Connect(string conn_str, shared_ptr<ConnectionHandle> conn);

// Connect using a datasource name directly and populate the ConnectionHandle
SQLRETURN ConnectDsn(string dsn, shared_ptr<ConnectionHandle> conn);

SQLRETURN Disconnect(shared_ptr<ConnectionHandle> conn);

SQLRETURN GetDriverInfo(shared_ptr<ConnectionHandle> conn);

SQLRETURN GetEnvInfo(shared_ptr<ConnectionHandle> conn);

SQLRETURN GetDescRec(shared_ptr<ConnectionHandle> conn);

SQLRETURN PrintDriverVerName(shared_ptr<ConnectionHandle> conn);

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_CONNECTION_H
