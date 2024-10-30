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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_CONNECTION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_CONNECTION_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include <string>


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

namespace google::cloud::odbc_bq_driver {
using google::cloud::odbc_bq_driver_internal::Section;

SQLRETURN SQLAllocConnHandle(SQLHDBC in_handle, SQLHANDLE* out_conn_handle);

SQLRETURN SQLDriverConnectInternal(SQLHDBC conn_handle, SQLHWND window_handle,
                                   SQLCHAR* in_conn_str,
                                   SQLSMALLINT in_conn_str_len,
                                   SQLCHAR* out_conn_str,
                                   SQLSMALLINT out_conn_str_buflen,
                                   SQLSMALLINT* out_conn_str_len,
                                   SQLUSMALLINT driver_completion);

SQLRETURN SQLGetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER buf_len, SQLINTEGER* str_len);

SQLRETURN SQLSetConnectAttrInternal(SQLHDBC connection_handle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER str_len);

SQLRETURN SQLDisconnectInternal(SQLHDBC connection_handle);

SQLRETURN SQLConnectInternal(SQLHDBC conn_handle, SQLCHAR* server_name,
                             SQLSMALLINT server_name_len, SQLCHAR* user_name,
                             SQLSMALLINT user_name_len, SQLCHAR* auth_string,
                             SQLSMALLINT auth_string_len);
#ifdef _WIN32
bool TestODBCConnection(const std::string& dsn);
bool TestODBCConnectionAd(const std::shared_ptr<Section>& section);
#endif
}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_CONNECTION_H
