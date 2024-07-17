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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_ODBC_INCLUDES_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_ODBC_INCLUDES_H

#define ODBCVER 0x0380

#ifdef _WIN32

#define _WINSOCKAPI_
#include <Windows.h>
#include <cstdint>
#undef GetJob
#undef SQLDriverConnect
#undef SQLDescribeCol
#undef SQLGetDiagField
#undef SQLGetDiagRec
#undef SQLColumns
#undef SQLTables
#undef SQLPrimaryKeys
#undef SQLProcedureColumns
#undef SQLProcedures
#undef SQLSpecialColumns
#undef SQLStatistics
#undef SQLTablePrivileges
#undef SQLForeignKeys
#undef SQLColumnPrivileges
#undef SQLBrowseConnect
#undef SQLConnect
#undef SQLGetInfo
#undef SQLGetTypeInfo
#undef SQLSetConnectAttr
#undef SQLGetConnectAttr
#undef SQLSetStmtAttr
#undef SQLGetStmtAttr
#undef SQLGetDescField
#undef SQLGetDescRec
#undef SQLSetDescField
#undef SQLPrepare
#undef SQLGetCursorName
#undef SQLSetCursorName
#undef SQLExecDirect
#undef SQLColAttribute
#undef SQLColAttributes
#undef SQLNativeSql
#endif  //_WIN32

#include <odbcinst.h>
#include <sql.h>
#include <sqlext.h>

#define SQL_ODBC3_API_START SQL_API_SQLALLOCHANDLE
#define SQL_ODBC3_API_LAST SQL_API_SQLFETCHSCROLL

#define SQL_ODBC2_API_START SQL_API_SQLALLOCCONNECT
#define SQL_ODBC2_API_LAST SQL_API_SQLBINDPARAMETER

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_ODBC_INCLUDES_H
