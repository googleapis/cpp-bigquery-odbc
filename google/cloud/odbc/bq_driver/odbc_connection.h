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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_CONNECTION_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_CONNECTION_H

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_includes.h"

////////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// ODBC connection:
//
// SQLAllocConnectInternal
// SQLDriverConnectInternal
// SQLBrowseConnectInternal
// SQLConnectInternal
// SQLSetConnectAttrInternal
// SQLGetConnectAttrInternal
// SQLDisconnectInternal
/////////////////////////////////////////////////////////////

// NOLINTBEGIN(modernize-concat-nested-namespaces)
namespace google {
namespace cloud {
namespace odbc_bq_driver {

SQLRETURN SQL_API SQLDriverConnectInternal(
    SQLHDBC connectionHandle, SQLHWND windowHandle, SQLCHAR *inConnectionString,
    SQLSMALLINT inConnectionStringLen, SQLCHAR *outConnectionString,
    SQLSMALLINT outConnectionStringBufferLen, SQLSMALLINT *outConnectionStringLen,
    SQLUSMALLINT driverCompletion);

}  // namespace odbc_bq_driver
}  // namespace cloud
}  // namespace google
// NOLINTEND(modernize-concat-nested-namespaces)

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_CONNECTION_H

