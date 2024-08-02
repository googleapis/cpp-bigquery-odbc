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

//////////////////////////////////////////////////////////////////
// This file is the entry point for the implementation of the
// ODBC APIs defined in <sql.h>, <sqlext.h> and <sqlucode.h>
//////////////////////////////////////////////////////////////////

#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_connection.h"
#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/odbc_diagnostics.h"
#include "google/cloud/odbc/bq_driver/odbc_driver_metadata.h"
#include "google/cloud/odbc/bq_driver/odbc_environment.h"
#include "google/cloud/odbc/bq_driver/odbc_lock.h"
#include "google/cloud/odbc/bq_driver/odbc_sql_requests.h"
#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/bq_driver/odbc_statement.h"
#include "google/cloud/odbc/bq_driver/odbc_trace.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/status_or.h"
////////////////////////////////////////////////////////////////////////////////////////
//
// ODBC APIs supported in initial driver release.
//
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// Suppressing clang-tidy errors as we don't have function
// implementation right now. Remove the lint blocks once
// functions are implemented.
////////////////////////////////////////////////////////////////////////////////////////

// NOLINTBEGIN

using ::google::cloud::Status;
using ::google::cloud::StatusOr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLAllocHandle;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLBindCol;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLBindParameter;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLCloseCursor;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLColAttribute;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLCopyDesc;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLDescribeCol;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLDescribeParam;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLDisconnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLDriverConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLEndTran;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLExecute;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLFetch;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLForeignKeys;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLFreeHandle;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetConnectAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetCursorName;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetDescField;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetDescRec;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetDiagField;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetDiagRec;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetEnvAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetFunctions;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetStmtAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetTypeInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLNumParams;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLNumResultCols;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLPrepare;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLPrimaryKeys;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLSetConnectAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLSetCursorName;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLSetDescField;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLSetDescRec;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLSetEnvAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLSetStmtAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLTables;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLBindParameter;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLNumResultCols;

using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLAllocHandle;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLBindCol;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLCloseCursor;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLColAttribute;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLCopyDesc;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLDescribeCol;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLDescribeParam;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLDisconnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLDriverConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLEndTran;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLExecute;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLFetch;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLForeignKeys;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLFreeHandle;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetConnectAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetCursorName;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetDescField;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetDescRec;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetDiagField;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetDiagRec;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetEnvAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetFunctions;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetStmtAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetTypeInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLNumParams;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLPrimaryKeys;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLSetConnectAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLSetCursorName;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLSetDescField;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLSetDescRec;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLSetEnvAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLSetStmtAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLTables;

using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLPrepare;
using ::google::cloud::odbc_bq_driver::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_internal::StatusRecord;

using ::google::cloud::odbc_bq_driver::AcquireHandleMutex;
using ::google::cloud::odbc_bq_driver::ReleaseHandleMutex;

// Internal Helper Functions
namespace {
void RecordTraceStatus(std::string const& name, StatusRecord const& s) {
  if (!s.ok()) {
    std::cout << "Tracing is misconfigured: " << s.message << std::endl;
    std::cout << "ODBC API: " << name << " will not be traced." << std::endl;
  }
}

bool IsTracingEnabled(std::string const& name) {
  if (!kTraceOption) {
    RecordTraceStatus(name, kTraceOption.GetStatusRecord());
    return false;
  }
  return true;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////////////
// SQLAllocHandle allocates an environment, connection, statement,
// or descriptor handle based on the handle type passed in.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlallochandle-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT handleType, SQLHANDLE inputHandle,
                                 SQLHANDLE* outputHandle) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLAllocHandle");

  switch (handleType) {
    case SQL_HANDLE_ENV: {
      // Call to Acquire mutex for environment handle in odbc_lock.h.
      // Call to Trace function entry in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionEntry_SQLAllocHandle(handleType, inputHandle, outputHandle,
                                          *(*kTraceOption));

      rc = google::cloud::odbc_bq_driver::SQLAllocEnvHandle(outputHandle);

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionExit_SQLAllocHandle(rc, *(*kTraceOption));
      // Call to Release mutex for environment handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_DBC: {
      // Call to Acquire mutex for connection handle in odbc_lock.h.
      // Call to Trace function entry in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionEntry_SQLAllocHandle(handleType, inputHandle, outputHandle,
                                          *(*kTraceOption));

      rc = google::cloud::odbc_bq_driver::SQLAllocConnHandle(inputHandle,
                                                             outputHandle);

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionExit_SQLAllocHandle(rc, *(*kTraceOption));
      // Call to Release mutex for connection handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_STMT: {
      // Call to Acquire mutex for connection handle in odbc_lock.h.
      // Call to Trace function entry in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionEntry_SQLAllocHandle(handleType, inputHandle, outputHandle,
                                          *(*kTraceOption));

      rc = google::cloud::odbc_bq_driver::SQLAllocStmtHandle(inputHandle,
                                                             outputHandle);

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionExit_SQLAllocHandle(rc, *(*kTraceOption));
      // Call to Release mutex for connection handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_DESC: {
      // Call to Acquire mutex for descriptor handle in odbc_lock.h.
      // Call to Trace function entry in odbc_trace.h if tracing is enabled.

      if (is_tracing_enabled)
        TraceFunctionEntry_SQLAllocHandle(handleType, inputHandle, outputHandle,
                                          *(*kTraceOption));

      rc = google::cloud::odbc_bq_driver::SQLAllocDescHandle(inputHandle,
                                                             outputHandle);

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionExit_SQLAllocHandle(rc, *(*kTraceOption));

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      // Call to Release mutex for descriptor handle in odbc_lock.h.
      break;
    }
    default: {
      return SQL_INVALID_HANDLE;
    }
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Establishes connection to a driver and data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldriverconnect-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLDriverConnectA(
    SQLHDBC connectionHandle, SQLHWND windowHandle, SQLCHAR* inConnectionString,
    SQLSMALLINT inConnectionStringLen, SQLCHAR* outConnectionString,
    SQLSMALLINT outConnectionStringBufferLen,
    SQLSMALLINT* outConnectionStringLen, SQLUSMALLINT driverCompletion) {
  return SQLDriverConnect(connectionHandle, windowHandle, inConnectionString,
                          inConnectionStringLen, outConnectionString,
                          outConnectionStringBufferLen, outConnectionStringLen,
                          driverCompletion);
}

SQLRETURN SQL_API SQLDriverConnect(
    SQLHDBC connectionHandle, SQLHWND windowHandle, SQLCHAR* inConnectionString,
    SQLSMALLINT inConnectionStringLen, SQLCHAR* outConnectionString,
    SQLSMALLINT outConnectionStringBufferLen,
    SQLSMALLINT* outConnectionStringLen, SQLUSMALLINT driverCompletion) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLDriverConnect");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLDriverConnect(
        connectionHandle, windowHandle, inConnectionString,
        inConnectionStringLen, outConnectionString,
        outConnectionStringBufferLen, outConnectionStringLen, driverCompletion,
        *(*kTraceOption));

  // Call to internal common function for SQLDriverConnect and SQLDriverConnectW
  // in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLDriverConnectInternal(
      connectionHandle, windowHandle, inConnectionString, inConnectionStringLen,
      outConnectionString, outConnectionStringBufferLen, outConnectionStringLen,
      driverCompletion);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLDriverConnect(rc, *(*kTraceOption));

  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
//////////////////////////////////////
// Unicode version of SQLDriverConnect.
//////////////////////////////////////
SQLRETURN SQL_API SQLDriverConnectW(
    SQLHDBC connectionHandle, SQLHWND windowHandle,
    SQLWCHAR* inConnectionString, SQLSMALLINT inConnectionStringLen,
    SQLWCHAR* outConnectionString, SQLSMALLINT outConnectionStringBufferLen,
    SQLSMALLINT* outConnectionStringLen, SQLUSMALLINT driverCompletion) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLDriverConnect and SQLDriverConnectW
  // in odbc_connection.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Establishes connection to a driver and data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbrowseconnect-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLBrowseConnectA(SQLHDBC connectionHandle,
                                    SQLCHAR* inConnectionString,
                                    SQLSMALLINT inConnectionStringLen,
                                    SQLCHAR* outConnectionString,
                                    SQLSMALLINT outConnectionStringBufferLen,
                                    SQLSMALLINT* outConnectionStringLen) {
  return SQLBrowseConnect(connectionHandle, inConnectionString,
                          inConnectionStringLen, outConnectionString,
                          outConnectionStringBufferLen, outConnectionStringLen);
}

SQLRETURN SQL_API SQLBrowseConnect(SQLHDBC connectionHandle,
                                   SQLCHAR* inConnectionString,
                                   SQLSMALLINT inConnectionStringLen,
                                   SQLCHAR* outConnectionString,
                                   SQLSMALLINT outConnectionStringBufferLen,
                                   SQLSMALLINT* outConnectionStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal common function for SQLBrowseConnect and SQLBrowseConnectW
  // in odbc_connection.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
//////////////////////////////////////
// Unicode version of SQLBrowseConnect.
//////////////////////////////////////
SQLRETURN SQL_API SQLBrowseConnectW(SQLHDBC connectionHandle,
                                    SQLWCHAR* inConnectionString,
                                    SQLSMALLINT inConnectionStringLen,
                                    SQLWCHAR* outConnectionString,
                                    SQLSMALLINT outConnectionStringBufferLen,
                                    SQLSMALLINT* outConnectionStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLBrowseConnect and SQLBrowseConnectW
  // in odbc_connection.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Establishes connection to a driver and data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlconnect-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLConnectA(SQLHDBC connectionHandle, SQLCHAR* serverName,
                              SQLSMALLINT serverNameLen, SQLCHAR* userName,
                              SQLSMALLINT userNameLen, SQLCHAR* authString,
                              SQLSMALLINT authStringLen) {
  return SQLConnect(connectionHandle, serverName, serverNameLen, userName,
                    userNameLen, authString, authStringLen);
}

SQLRETURN SQL_API SQLConnect(SQLHDBC connectionHandle, SQLCHAR* serverName,
                             SQLSMALLINT serverNameLen, SQLCHAR* userName,
                             SQLSMALLINT userNameLen, SQLCHAR* authString,
                             SQLSMALLINT authStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLConnect");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLConnect(connectionHandle, serverName, serverNameLen,
                                  userName, userNameLen, authString,
                                  authStringLen, *(*kTraceOption));

  // Call to internal common function for SQLConnect and SQLConnectW
  // in odbc_connection.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLConnect(rc, *(*kTraceOption));
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
//////////////////////////////////////
// Unicode version of SQLConnect.
//////////////////////////////////////
SQLRETURN SQL_API SQLConnectW(SQLHDBC connectionHandle, SQLWCHAR* serverName,
                              SQLSMALLINT serverNameLen, SQLWCHAR* userName,
                              SQLSMALLINT userNameLen, SQLWCHAR* authString,
                              SQLSMALLINT authStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLConnect and SQLConnectW
  // in odbc_connection.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns general information about the driver and data source
// associated with a connection
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetinfo-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetInfoA(SQLHDBC connectionHandle, SQLUSMALLINT infoType,
                              SQLPOINTER infoValue,
                              SQLSMALLINT infoValueBufferLen,
                              SQLSMALLINT* infoValueStringLen) {
  return SQLGetInfo(connectionHandle, infoType, infoValue, infoValueBufferLen,
                    infoValueStringLen);
}

SQLRETURN SQL_API SQLGetInfo(SQLHDBC connectionHandle, SQLUSMALLINT infoType,
                             SQLPOINTER infoValue,
                             SQLSMALLINT infoValueBufferLen,
                             SQLSMALLINT* infoValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetInfo");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetInfo(connectionHandle, infoType, infoValue,
                                  infoValueBufferLen, infoValueStringLen,
                                  *(*kTraceOption));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetInfoInternal(
      connectionHandle, infoType, infoValue, infoValueBufferLen,
      infoValueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLGetInfo(rc, *(*kTraceOption));
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  return rc;
}
//////////////////////////////////////
// Unicode version of SQLGetInfo.
//////////////////////////////////////
SQLRETURN SQL_API SQLGetInfoW(SQLHDBC connectionHandle, SQLUSMALLINT infoType,
                              SQLPOINTER infoValue,
                              SQLSMALLINT infoValueBufferLen,
                              SQLSMALLINT* infoValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns information about whether a driver supports a specific ODBC function.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetfunctions-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetFunctions(SQLHDBC connectionHandle,
                                  SQLUSMALLINT functionId,
                                  SQLUSMALLINT* supportedFunction) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetFunctions");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetFunctions(connectionHandle, functionId,
                                       supportedFunction, *(*kTraceOption));

  // Call to internal function for SQLGetFunctions in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetFunctionsInternal(
      connectionHandle, functionId, supportedFunction);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetFunctions(rc, *(*kTraceOption));
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns information about data types supported by the data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgettypeinfo-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetTypeInfoA(SQLHSTMT statementHandle,
                                  SQLSMALLINT dataType) {
  return SQLGetTypeInfo(statementHandle, dataType);
}

SQLRETURN SQL_API SQLGetTypeInfo(SQLHSTMT statementHandle,
                                 SQLSMALLINT dataType) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetTypeInfo");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  status = AcquireHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetTypeInfo(statementHandle, dataType,
                                      *(*kTraceOption));

  // Call to internal common function for SQLGetTypeInfo and SQLGetTypeInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetTypeInfoInternal(statementHandle,
                                                               dataType);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetTypeInfo(rc, *(*kTraceOption));
  // Call to Release mutex for statement handle in odbc_lock.h.
  status = ReleaseHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////
// Unicode version of SQLGetTypeInfo.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetTypeInfoW(SQLHSTMT statementHandle,
                                  SQLSMALLINT dataType) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Call to internal common function for SQLGetTypeInfo and SQLGetTypeInfoW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of  output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets connection attributes.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetconnectattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetConnectAttrA(SQLHDBC connectionHandle,
                                     SQLINTEGER attribute, SQLPOINTER value,
                                     SQLINTEGER valueStringLen) {
  return SQLSetConnectAttr(connectionHandle, attribute, value, valueStringLen);
}

SQLRETURN SQL_API SQLSetConnectAttr(SQLHDBC connectionHandle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetConnectAttr");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLSetConnectAttr(connectionHandle, attribute, value,
                                         valueStringLen, *(*kTraceOption));

  // Call to internal common function for SQLSetConnectAttr and
  // SQLSetConnectAttrW in odbc_connection.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetConnectAttrInternal(
      connectionHandle, attribute, value, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLSetConnectAttr(rc, *(*kTraceOption));
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetConnectAttr.
////////////////////////////////////////
SQLRETURN SQL_API SQLSetConnectAttrW(SQLHDBC connectionHandle,
                                     SQLINTEGER attribute, SQLPOINTER value,
                                     SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLSetConnectAttr and
  // SQLSetConnectAttrW in odbc_connection.h. Handle Unicode conversion of
  // output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current setting of a connection attribute.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetconnectattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetConnectAttrA(SQLHDBC connectionHandle,
                                     SQLINTEGER attribute, SQLPOINTER value,
                                     SQLINTEGER valueBufferLen,
                                     SQLINTEGER* valueStringLen) {
  return SQLGetConnectAttr(connectionHandle, attribute, value, valueBufferLen,
                           valueStringLen);
}

SQLRETURN SQL_API SQLGetConnectAttr(SQLHDBC connectionHandle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER valueBufferLen,
                                    SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetConnectAttr");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetConnectAttr(connectionHandle, attribute, value,
                                         valueBufferLen, valueStringLen,
                                         *(*kTraceOption));

  // Call to internal common function for SQLGetConnectAttr and
  // SQLGetConnectAttrW in odbc_connection.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetConnectAttrInternal(
      connectionHandle, attribute, value, valueBufferLen, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetConnectAttr(rc, *(*kTraceOption));
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetConnectAttr.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetConnectAttrW(SQLHDBC connectionHandle,
                                     SQLINTEGER attribute, SQLPOINTER value,
                                     SQLINTEGER valueBufferLen,
                                     SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetConnectAttr and
  // SQLGetConnectAttrW in odbc_connection.h. Handle Unicode conversion of
  // output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets attributes related to a statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetstmtattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetStmtAttrA(SQLHSTMT statementHandle,
                                  SQLINTEGER attribute, SQLPOINTER value,
                                  SQLINTEGER valueStringLen) {
  return SQLSetStmtAttr(statementHandle, attribute, value, valueStringLen);
}

SQLRETURN SQL_API SQLSetStmtAttr(SQLHSTMT statementHandle, SQLINTEGER attribute,
                                 SQLPOINTER value, SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLSetStmtAttr");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLSetStmtAttr(statementHandle, attribute, value,
                                      valueStringLen, *(*kTraceOption));

  // Call to internal common function for SQLSetStmtAttr and SQLSetStmtAttrW
  // in odbc_statement.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetStmtAttrInternal(
      statementHandle, attribute, value, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLSetStmtAttr(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetStmtAttr.
////////////////////////////////////////
SQLRETURN SQL_API SQLSetStmtAttrW(SQLHSTMT statementHandle,
                                  SQLINTEGER attribute, SQLPOINTER value,
                                  SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLSetStmtAttr and SQLSetStmtAttrW
  // in odbc_statement.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current setting of a statement attribute.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetstmtattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetStmtAttrA(SQLHSTMT statementHandle,
                                  SQLINTEGER attribute, SQLPOINTER value,
                                  SQLINTEGER valueBufferLen,
                                  SQLINTEGER* valueStringLen) {
  return SQLGetStmtAttr(statementHandle, attribute, value, valueBufferLen,
                        valueStringLen);
}

SQLRETURN SQL_API SQLGetStmtAttr(SQLHSTMT statementHandle, SQLINTEGER attribute,
                                 SQLPOINTER value, SQLINTEGER valueBufferLen,
                                 SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetStmtAttr");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetStmtAttr(statementHandle, attribute, value,
                                      valueBufferLen, valueStringLen,
                                      *(*kTraceOption));

  // Call to internal common function for SQLGetStmtAttr and SQLGetStmtAttrW
  // in odbc_statement.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetStmtAttrInternal(
      statementHandle, attribute, value, valueBufferLen, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetStmtAttr(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetStmtAttr.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetStmtAttrW(SQLHSTMT statementHandle,
                                  SQLINTEGER attribute, SQLPOINTER value,
                                  SQLINTEGER valueBufferLen,
                                  SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetStmtAttr and SQLGetStmtAttrW
  // in odbc_statement.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets attributes that govern aspects of environments.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetenvattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetEnvAttr(SQLHENV environmentHandle, SQLINTEGER attribute,
                                SQLPOINTER value, SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLSetEnvAttr");

  // Call to Acquire mutex for environmentHandle handle in odbc_lock.h.
  status = AcquireHandleMutex(environmentHandle, SQL_HANDLE_ENV);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLSetEnvAttr(environmentHandle, attribute, value,
                                     valueStringLen, *(*kTraceOption));

  // Call to internal function for SQLSetEnvAttr in odbc_environment.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetEnvAttrInternal(
      environmentHandle, attribute, value, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLSetEnvAttr(rc, *(*kTraceOption));
  // Call to Release mutex for environmentHandle handle in odbc_lock.h.
  status = ReleaseHandleMutex(environmentHandle, SQL_HANDLE_ENV);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current setting of an environment attribute.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetenvattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetEnvAttr(SQLHENV environmentHandle, SQLINTEGER attribute,
                                SQLPOINTER value, SQLINTEGER valueBufferLen,
                                SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetEnvAttr");

  // Call to Acquire mutex for environmentHandle handle in odbc_lock.h.
  status = AcquireHandleMutex(environmentHandle, SQL_HANDLE_ENV);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetEnvAttr(environmentHandle, attribute, value,
                                     valueBufferLen, valueStringLen,
                                     *(*kTraceOption));

  // Call to internal function for SQLGetEnvAttr in odbc_environment.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetEnvAttrInternal(
      environmentHandle, attribute, value, valueBufferLen, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLGetEnvAttr(rc, *(*kTraceOption));
  // Call to Release mutex for environmentHandle handle in odbc_lock.h.
  status = ReleaseHandleMutex(environmentHandle, SQL_HANDLE_ENV);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current setting or value of a single field of a descriptor
// record.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdescfield-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetDescFieldA(SQLHDESC descriptorHandle,
                                   SQLSMALLINT recNumber, SQLSMALLINT fieldId,
                                   SQLPOINTER outDescValue,
                                   SQLINTEGER outDescValueBufferLen,
                                   SQLINTEGER* outDescValueStringLen) {
  return SQLGetDescField(descriptorHandle, recNumber, fieldId, outDescValue,
                         outDescValueBufferLen, outDescValueStringLen);
}

SQLRETURN SQL_API SQLGetDescField(SQLHDESC descriptorHandle,
                                  SQLSMALLINT recNumber, SQLSMALLINT fieldId,
                                  SQLPOINTER outDescValue,
                                  SQLINTEGER outDescValueBufferLen,
                                  SQLINTEGER* outDescValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetDescField");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetDescField(descriptorHandle, recNumber, fieldId,
                                       outDescValue, outDescValueBufferLen,
                                       outDescValueStringLen, *(*kTraceOption));

  rc = google::cloud::odbc_bq_driver::SQLGetDescFieldInternal(
      descriptorHandle, recNumber, fieldId, outDescValue, outDescValueBufferLen,
      outDescValueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetDescField(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDescField.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetDescFieldW(SQLHDESC descriptorHandle,
                                   SQLSMALLINT recNumber, SQLSMALLINT fieldId,
                                   SQLPOINTER outDescValue,
                                   SQLINTEGER outDescValueBufferLen,
                                   SQLINTEGER* outDescValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDescField and SQLGetDescFieldW
  // in odbc_descriptor.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current settings or values of multiple fields of a descriptor
// record.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdescrec-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetDescRecA(
    SQLHDESC descriptorHandle, SQLSMALLINT recNumber, SQLCHAR* name,
    SQLSMALLINT nameBufferLen, SQLSMALLINT* nameStringLen,
    SQLSMALLINT* descType, SQLSMALLINT* descSubType, SQLLEN* descOctetLen,
    SQLSMALLINT* descPrecision, SQLSMALLINT* descScale, SQLSMALLINT* nullable) {
  return SQLGetDescRec(descriptorHandle, recNumber, name, nameBufferLen,
                       nameStringLen, descType, descSubType, descOctetLen,
                       descPrecision, descScale, nullable);
}

SQLRETURN SQL_API SQLGetDescRec(
    SQLHDESC descriptorHandle, SQLSMALLINT recNumber, SQLCHAR* name,
    SQLSMALLINT nameBufferLen, SQLSMALLINT* nameStringLen,
    SQLSMALLINT* descType, SQLSMALLINT* descSubType, SQLLEN* descOctetLen,
    SQLSMALLINT* descPrecision, SQLSMALLINT* descScale, SQLSMALLINT* nullable) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetDescRec");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetDescRec(descriptorHandle, recNumber, name,
                                     nameBufferLen, nameStringLen, descType,
                                     descSubType, descOctetLen, descPrecision,
                                     descScale, nullable, *(*kTraceOption));

  rc = google::cloud::odbc_bq_driver::SQLGetDescRecInternal(
      descriptorHandle, recNumber, name, nameBufferLen, nameStringLen, descType,
      descSubType, descOctetLen, descPrecision, descScale, nullable);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLGetDescRec(rc, *(*kTraceOption));

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDescRec.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetDescRecW(
    SQLHDESC descriptorHandle, SQLSMALLINT recNumber, SQLWCHAR* name,
    SQLSMALLINT nameBufferLen, SQLSMALLINT* nameStringLen,
    SQLSMALLINT* descType, SQLSMALLINT* descSubType, SQLLEN* descOctetLen,
    SQLSMALLINT* descPrecision, SQLSMALLINT* descScale, SQLSMALLINT* nullable) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDescRec and SQLGetDescRecW
  // in odbc_descriptor.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets the value of a single field of a descriptor record.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescfield-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetDescFieldA(SQLHDESC descriptorHandle,
                                   SQLSMALLINT recNumber,
                                   SQLSMALLINT fieldIdentifier,
                                   SQLPOINTER descValue,
                                   SQLINTEGER descValueBufferLen) {
  return SQLSetDescField(descriptorHandle, recNumber, fieldIdentifier,
                         descValue, descValueBufferLen);
}

SQLRETURN SQL_API SQLSetDescField(SQLHDESC descriptorHandle,
                                  SQLSMALLINT recNumber,
                                  SQLSMALLINT fieldIdentifier,
                                  SQLPOINTER descValue,
                                  SQLINTEGER descValueBufferLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLSetDescField");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLSetDescField(descriptorHandle, recNumber,
                                       fieldIdentifier, descValue,
                                       descValueBufferLen, *(*kTraceOption));

  rc = google::cloud::odbc_bq_driver::SQLSetDescFieldInternal(
      descriptorHandle, recNumber, fieldIdentifier, descValue,
      descValueBufferLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLSetDescField(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetDescField.
////////////////////////////////////////
SQLRETURN SQL_API SQLSetDescFieldW(SQLHDESC descriptorHandle,
                                   SQLSMALLINT recNumber,
                                   SQLSMALLINT fieldIdentifier,
                                   SQLPOINTER descValue,
                                   SQLINTEGER descValueBufferLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLSetDescField and SQLSetDescFieldW
  // in odbc_descriptor.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets multiple descriptor fields that affect the data type and buffer bound
// to a column or parameter data.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetdescrec-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetDescRec(SQLHDESC descriptorHandle,
                                SQLSMALLINT recNumber, SQLSMALLINT descType,
                                SQLSMALLINT descSubType, SQLLEN descOctetLen,
                                SQLSMALLINT descPrecision,
                                SQLSMALLINT descScale, SQLPOINTER descData,
                                SQLLEN* descOctetLenPtr,
                                SQLLEN* descIndicator) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLSetDescRec");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLSetDescRec(descriptorHandle, recNumber, descType,
                                     descSubType, descOctetLen, descPrecision,
                                     descScale, descData, descOctetLenPtr,
                                     descIndicator, *(*kTraceOption));

  rc = google::cloud::odbc_bq_driver::SQLSetDescRecInternal(
      descriptorHandle, recNumber, descType, descSubType, descOctetLen,
      descPrecision, descScale, descData, descOctetLenPtr, descIndicator);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLSetDescRec(rc, *(*kTraceOption));

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Copies descriptor information from one descriptor handle to another.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcopydesc-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLCopyDesc(SQLHDESC sourceDescHandle,
                              SQLHDESC targetDescHandle) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLCopyDesc");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  if (is_tracing_enabled)
    TraceFunctionEntry_SQLCopyDesc(sourceDescHandle, targetDescHandle,
                                   *(*kTraceOption));

  rc = google::cloud::odbc_bq_driver::SQLCopyDescInternal(sourceDescHandle,
                                                          targetDescHandle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLCopyDesc(rc, *(*kTraceOption));

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Prepares an SQL string for execution.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlprepare-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLPrepareA(SQLHSTMT statementHandle, SQLCHAR* statementText,
                              SQLINTEGER statementTextLen) {
  return SQLPrepare(statementHandle, statementText, statementTextLen);
}

SQLRETURN SQL_API SQLPrepare(SQLHSTMT statementHandle, SQLCHAR* statementText,
                             SQLINTEGER statementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLPrepare");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  if (IsTracingEnabled)
    TraceFunctionEntry_SQLPrepare(statementHandle, statementText,
                                  statementTextLen, *(*kTraceOption));

  // Call to common internal function for SQLPrepare and SQLPrepareW
  // in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLPrepareInternal(
      statementHandle, statementText, statementTextLen);

  if (IsTracingEnabled) TraceFunctionExit_SQLPrepare(rc, *(*kTraceOption));
  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLPrepare.
////////////////////////////////////////
SQLRETURN SQL_API SQLPrepareW(SQLHSTMT statementHandle, SQLWCHAR* statementText,
                              SQLINTEGER statementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLPrepare and SQLPrepareW
  // in odbc_sql_requests.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Binds a buffer to a parameter marker in an SQL statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindparameter-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API
SQLBindParameter(SQLHSTMT statementHandle, SQLUSMALLINT parameterNumber,
                 SQLSMALLINT inputOutputType, SQLSMALLINT valueType,
                 SQLSMALLINT parameterType, SQLULEN columnSize,
                 SQLSMALLINT decimalDigits, SQLPOINTER parameterValuePtr,
                 SQLLEN bufferLength, SQLLEN* strLen_or_IndPtr) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLBindParameter");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLBindParameter(
        statementHandle, parameterNumber, inputOutputType, valueType,
        parameterType, columnSize, decimalDigits, parameterValuePtr,
        bufferLength, strLen_or_IndPtr, *(*kTraceOption));

  // Call to internal function for SQLBindParameter in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLBindParameterInternal(
      statementHandle, parameterNumber, inputOutputType, valueType,
      parameterType, columnSize, decimalDigits, parameterValuePtr, bufferLength,
      strLen_or_IndPtr);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLBindParameter(rc, *(*kTraceOption));

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the cursor name associated with a specified statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetcursorname-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetCursorNameA(SQLHSTMT statementHandle,
                                    SQLCHAR* cursorName,
                                    SQLSMALLINT cursorNameBufferLen,
                                    SQLSMALLINT* cursorNameStringLen) {
  return SQLGetCursorName(statementHandle, cursorName, cursorNameBufferLen,
                          cursorNameStringLen);
}

SQLRETURN SQL_API SQLGetCursorName(SQLHSTMT statementHandle,
                                   SQLCHAR* cursorName,
                                   SQLSMALLINT cursorNameBufferLen,
                                   SQLSMALLINT* cursorNameStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetCursorName");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetCursorName(statementHandle, cursorName,
                                        cursorNameBufferLen,
                                        cursorNameStringLen, *(*kTraceOption));

  // Call to common internal function for SQLGetCursorName and SQLGetCursorNameW
  // in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetCursorNameInternal(
      statementHandle, cursorName, cursorNameBufferLen, cursorNameStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetCursorName(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetCursorName.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetCursorNameW(SQLHSTMT statementHandle,
                                    SQLWCHAR* cursorName,
                                    SQLSMALLINT cursorNameBufferLen,
                                    SQLSMALLINT* cursorNameStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetCursorName and SQLGetCursorNameW
  // in odbc_sql_requests.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Associates a cursor name with an active statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetcursorname-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetCursorNameA(SQLHSTMT statementHandle,
                                    SQLCHAR* cursorName,
                                    SQLSMALLINT cursorNameLen) {
  return SQLSetCursorName(statementHandle, cursorName, cursorNameLen);
}

SQLRETURN SQL_API SQLSetCursorName(SQLHSTMT statementHandle,
                                   SQLCHAR* cursorName,
                                   SQLSMALLINT cursorNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLSetCursorName");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLSetCursorName(statementHandle, cursorName,
                                        cursorNameLen, *(*kTraceOption));

  // Call to common internal function for SQLSetCursorName and SQLSetCursorNameW
  // in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetCursorNameInternal(
      statementHandle, cursorName, cursorNameLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLSetCursorName(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetCursorName.
////////////////////////////////////////
SQLRETURN SQL_API SQLSetCursorNameW(SQLHSTMT statementHandle,
                                    SQLWCHAR* cursorName,
                                    SQLSMALLINT cursorNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLSetCursorName and SQLSetCursorNameW
  // in odbc_sql_requests.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Executes a prepared statement, using the current values of the parameter
// marker variables if any parameter markers exist in the statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlexecute-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLExecute(SQLHSTMT statementHandle) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLExecute");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLExecute(statementHandle, *(*kTraceOption));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLExecuteInternal(statementHandle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLExecute(rc, *(*kTraceOption));
  // Call to Release mutex for statement handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Executes a prepared statement, using the current values of the parameter
// marker variables if any parameter markers exist in the statement.
//
// Fastest way to submit a SQL statement for one-time execution
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlexecdirect-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLExecDirectA(SQLHSTMT statementHandle,
                                 SQLCHAR* statementText,
                                 SQLINTEGER statementTextLen) {
  return SQLExecDirect(statementHandle, statementText, statementTextLen);
}

SQLRETURN SQL_API SQLExecDirect(SQLHSTMT statementHandle,
                                SQLCHAR* statementText,
                                SQLINTEGER statementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLExecDirect and SQLExecDirectW
  // in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLExecDirect.
////////////////////////////////////////
SQLRETURN SQL_API SQLExecDirectW(SQLHSTMT statementHandle,
                                 SQLWCHAR* statementText,
                                 SQLINTEGER statementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLExecDirect and SQLExecDirectW
  // in odbc_sql_requests.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the SQL string as modified by the driver. Does not execute the SQL
// statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlnativesql-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLNativeSqlA(SQLHDBC connectionHandle,
                                SQLCHAR* inStatementText,
                                SQLINTEGER inStatementTextLen,
                                SQLCHAR* outStatementText,
                                SQLINTEGER outStatementTextBufferLen,
                                SQLINTEGER* outStatementTextLen) {
  return SQLNativeSql(connectionHandle, inStatementText, inStatementTextLen,
                      outStatementText, outStatementTextBufferLen,
                      outStatementTextLen);
}

SQLRETURN SQL_API SQLNativeSql(SQLHDBC connectionHandle,
                               SQLCHAR* inStatementText,
                               SQLINTEGER inStatementTextLen,
                               SQLCHAR* outStatementText,
                               SQLINTEGER outStatementTextBufferLen,
                               SQLINTEGER* outStatementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLNativeSql and SQLNativeSqlW
  // in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLNativeSql.
////////////////////////////////////////
SQLRETURN SQL_API SQLNativeSqlW(SQLHDBC connectionHandle,
                                SQLWCHAR* inStatementText,
                                SQLINTEGER inStatementTextLen,
                                SQLWCHAR* outStatementText,
                                SQLINTEGER outStatementTextBufferLen,
                                SQLINTEGER* outStatementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLNativeSql and SQLNativeSqlW
  // in odbc_sql_requests.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the number of parameters in an SQL statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlnumparams-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLNumParams(SQLHSTMT statementHandle,
                               SQLSMALLINT* paramCount) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLSetDescField");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLNumParams(statementHandle, paramCount,
                                    *(*kTraceOption));

  // Call to internal function for SQLNumParams in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLNumParamsInternal(statementHandle,
                                                           paramCount);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLNumParams(rc, *(*kTraceOption));

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Used together with SQLPutData to supply parameter data at statement execution
// time, and with SQLGetData to retrieve streamed output parameter data.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlparamdata-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLParamData(SQLHSTMT statementHandle,
                               SQLPOINTER* paramOrTargetValue) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLParamData in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Allows an application to send data for a parameter or column to the driver at
// statement execution time.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlputdata-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLPutData(SQLHSTMT statementHandle, SQLPOINTER paramData,
                             SQLLEN paramDataLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLPutData in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the description of a parameter marker associated with a
// prepared SQL statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribeparam-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLDescribeParam(SQLHSTMT statementHandle,
                                   SQLUSMALLINT paramNumber,
                                   SQLSMALLINT* paramSqlType,
                                   SQLULEN* paramSize, SQLSMALLINT* paramScale,
                                   SQLSMALLINT* paramNullable) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLDescribeParam");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLDescribeParam(statementHandle, paramNumber,
                                        paramSqlType, paramSize, paramScale,
                                        paramNullable, *(*kTraceOption));

  // Call to internal function for SQLDescribeParam in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLDescribeParamInternal(
      statementHandle, paramNumber, paramSqlType, paramSize, paramScale,
      paramNullable);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLDescribeParam(rc, *(*kTraceOption));

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Retrieves data for a single column in the result set.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdata-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetData(SQLHSTMT statementHandle,
                             SQLUSMALLINT columnNumber, SQLSMALLINT targetCType,
                             SQLPOINTER targetValue,
                             SQLLEN targetValueBufferLen,
                             SQLLEN* targetValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLGetData in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the number of columns in a result set.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlnumresultcols-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLNumResultCols(SQLHSTMT statementHandle,
                                   SQLSMALLINT* columnCount) {
  SQLRETURN rc = SQL_SUCCESS;

  bool is_tracing_enabled = IsTracingEnabled("SQLNumResultCols");
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (IsTracingEnabled)
    TraceFunctionEntry_SQLNumResultCols(statementHandle, columnCount,
                                        *(*kTraceOption));

  // Call to internal function for SQLNumResultCols in odbc_sql_results.h.
  rc = google::cloud::odbc_bq_driver::SQLNumResultColsInternal(statementHandle,
                                                               columnCount);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (IsTracingEnabled)
    TraceFunctionExit_SQLNumResultCols(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////////////////////////////////////////////////////
// Fetches the next rowset of data from the result set and returns data for
// all bound columns.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfetch-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLFetch(SQLHSTMT statementHandle) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLFetch");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  status = AcquireHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLFetch(statementHandle, *(*kTraceOption));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLFetchInternal(statementHandle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLFetch(rc, *(*kTraceOption));
  // Call to Release mutex for statement handle in odbc_lock.h.
  status = ReleaseHandleMutex(statementHandle, SQL_HANDLE_STMT);

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Fetches the specified rowset of data from the result set and returns data for
// all bound columns. Rowsets can be specified at an absolute or relative
// position or by bookmark.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlextendedfetch-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLExtendedFetch(SQLHSTMT statementHandletmt,
                                   SQLUSMALLINT fetchOrientation,
                                   SQLLEN fetchOffset, SQLULEN* rowCount,
                                   SQLUSMALLINT* rowStatusArray) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLExtendedFetch in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns descriptor information for a column in a result set. Descriptor
// information is returned as a character string, a descriptor-dependent value,
// or an integer value.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolattribute-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLColAttributeA(SQLHSTMT statementHandle,
                                   SQLUSMALLINT columnNumber,
                                   SQLUSMALLINT fieldIdentifier,
                                   SQLPOINTER characterAttribute,
                                   SQLSMALLINT characterAttributeBufferLen,
                                   SQLSMALLINT* characterAttributeStringLen,
                                   SQLLEN* numericAttribute) {
  return SQLColAttribute(statementHandle, columnNumber, fieldIdentifier,
                         characterAttribute, characterAttributeBufferLen,
                         characterAttributeStringLen, numericAttribute);
}

SQLRETURN SQL_API SQLColAttribute(SQLHSTMT statementHandle,
                                  SQLUSMALLINT columnNumber,
                                  SQLUSMALLINT fieldIdentifier,
                                  SQLPOINTER characterAttribute,
                                  SQLSMALLINT characterAttributeBufferLen,
                                  SQLSMALLINT* characterAttributeStringLen,
                                  SQLLEN* numericAttribute) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLColAttribute");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLColAttribute(
        statementHandle, columnNumber, fieldIdentifier, characterAttribute,
        characterAttributeBufferLen, characterAttributeStringLen,
        numericAttribute, *(*kTraceOption));

  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLColAttributeInternal(
      statementHandle, columnNumber, fieldIdentifier, characterAttribute,
      characterAttributeBufferLen, characterAttributeStringLen,
      numericAttribute);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLDescribeParam(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColAttribute.
////////////////////////////////////////
SQLRETURN SQL_API SQLColAttributeW(SQLHSTMT statementHandle,
                                   SQLUSMALLINT columnNumber,
                                   SQLUSMALLINT fieldIdentifier,
                                   SQLPOINTER characterAttribute,
                                   SQLSMALLINT characterAttributeBufferLen,
                                   SQLSMALLINT* characterAttributeStringLen,
                                   SQLLEN* numericAttribute) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Deprecated and Replaced by SQLColAttribute in ODBC 3.0.
// Please see the definition for that function.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolattributes-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLColAttributesA(SQLHSTMT statementHandle,
                                    SQLUSMALLINT columnNumber,
                                    SQLUSMALLINT fieldIdentifier,
                                    SQLPOINTER characterAttribute,
                                    SQLSMALLINT characterAttributeBufferLen,
                                    SQLSMALLINT* characterAttributeStringLen,
                                    SQLLEN* numericAttribute) {
  return SQLColAttributes(statementHandle, columnNumber, fieldIdentifier,
                          characterAttribute, characterAttributeBufferLen,
                          characterAttributeStringLen, numericAttribute);
}

SQLRETURN SQL_API SQLColAttributes(SQLHSTMT statementHandle,
                                   SQLUSMALLINT columnNumber,
                                   SQLUSMALLINT fieldIdentifier,
                                   SQLPOINTER characterAttribute,
                                   SQLSMALLINT characterAttributeBufferLen,
                                   SQLSMALLINT* characterAttributeStringLen,
                                   SQLLEN* numericAttribute) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColAttributes.
////////////////////////////////////////
SQLRETURN SQL_API SQLColAttributesW(SQLHSTMT statementHandle,
                                    SQLUSMALLINT columnNumber,
                                    SQLUSMALLINT fieldIdentifier,
                                    SQLPOINTER characterAttribute,
                                    SQLSMALLINT characterAttributeBufferLen,
                                    SQLSMALLINT* characterAttributeStringLen,
                                    SQLLEN* numericAttribute) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the result descriptor information for one column in the result set.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribecol-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLDescribeColA(
    SQLHSTMT statementHandle, SQLUSMALLINT columnNumber, SQLCHAR* columnName,
    SQLSMALLINT columnNameBufferLen, SQLSMALLINT* columnNameLe,
    SQLSMALLINT* columnSQLdataType, SQLULEN* columnSize,
    SQLSMALLINT* decimalDigits, SQLSMALLINT* columnNullable) {
  return SQLDescribeCol(statementHandle, columnNumber, columnName,
                        columnNameBufferLen, columnNameLe, columnSQLdataType,
                        columnSize, decimalDigits, columnNullable);
}

SQLRETURN SQL_API SQLDescribeCol(
    SQLHSTMT statementHandle, SQLUSMALLINT columnNumber, SQLCHAR* columnName,
    SQLSMALLINT columnNameBufferLen, SQLSMALLINT* columnNameLe,
    SQLSMALLINT* columnSQLdataType, SQLULEN* columnSize,
    SQLSMALLINT* decimalDigits, SQLSMALLINT* columnNullable) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLDescribeCol");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLDescribeCol(
        statementHandle, columnNumber, columnName, columnNameBufferLen,
        columnNameLe, columnSQLdataType, columnSize, decimalDigits,
        columnNullable, *(*kTraceOption));

  // Call to common internal function for SQLDescribeCol and SQLDescribeColW
  // in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLDescribeColInternal(
      statementHandle, columnNumber, columnName, columnNameBufferLen,
      columnNameLe, columnSQLdataType, columnSize, decimalDigits,
      columnNullable);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLDescribeParam(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLDescribeCol.
////////////////////////////////////////
SQLRETURN SQL_API SQLDescribeColW(
    SQLHSTMT statementHandle, SQLUSMALLINT columnNumber, SQLWCHAR* columnName,
    SQLSMALLINT columnNameBufferLen, SQLSMALLINT* columnNameLen,
    SQLSMALLINT* columnSQLdataType, SQLULEN* columnSize,
    SQLSMALLINT* decimalDigits, SQLSMALLINT* columnNullable) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLDescribeCol and SQLDescribeColW
  // in odbc_sql_results.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Binds application data buffers to columns in the result set.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindcol-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLBindCol(SQLHSTMT statementHandle,
                             SQLUSMALLINT columnNumber, SQLSMALLINT targetCType,
                             SQLPOINTER targetValuePtr,
                             SQLLEN targetValueBufferLen,
                             SQLLEN* targetValueStrLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLBindCol");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  status = AcquireHandleMutex(statementHandle, SQL_HANDLE_STMT);
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLBindCol(statementHandle, columnNumber, targetCType,
                                  targetValuePtr, targetValueBufferLen,
                                  targetValueStrLen, *(*kTraceOption));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLBindColInternal(
      statementHandle, columnNumber, targetCType, targetValuePtr,
      targetValueBufferLen, targetValueStrLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLBindCol(rc, *(*kTraceOption));
  // Call to Release mutex for statement handle in odbc_lock.h.
  status = ReleaseHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the number of rows affected by an UPDATE, INSERT, or DELETE
// statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlrowcount-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLRowCount(SQLHSTMT statementHandle, SQLLEN* rowCount) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLRowCount in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Fetches the specified rowset of data from the result set and returns
// data for all bound columns.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfetchscroll-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLFetchScroll(SQLHSTMT statementHandle,
                                 SQLSMALLINT fetchOrientation,
                                 SQLLEN fetchOffset) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLFetchScroll in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Determines whether more results are available on a statement containing
// SELECT, UPDATE, INSERT, or DELETE statements and, if so, initializes
// processing for those results.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlmoreresults-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLMoreResults(SQLHSTMT statementHandle) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLMoreResults in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current value of a field of a record of the diagnostic data
// structure (associated with a specified handle) that contains error, warning,
// and status information.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagfield-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetDiagFieldA(SQLSMALLINT handleType, SQLHANDLE handle,
                                   SQLSMALLINT recNumber,
                                   SQLSMALLINT diagIdentifier,
                                   SQLPOINTER diagInfo,
                                   SQLSMALLINT diagInfoBufferLen,
                                   SQLSMALLINT* diagInfoStringLen) {
  return SQLGetDiagField(handleType, handle, recNumber, diagIdentifier,
                         diagInfo, diagInfoBufferLen, diagInfoStringLen);
}

SQLRETURN SQL_API SQLGetDiagField(SQLSMALLINT handleType, SQLHANDLE handle,
                                  SQLSMALLINT recNumber,
                                  SQLSMALLINT diagIdentifier,
                                  SQLPOINTER diagInfo,
                                  SQLSMALLINT diagInfoBufferLen,
                                  SQLSMALLINT* diagInfoStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetDiagField");

  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  status = AcquireHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetDiagField(
        handleType, handle, recNumber, diagIdentifier, diagInfo,
        diagInfoBufferLen, diagInfoStringLen, *(*kTraceOption));

  // Call to common internal function for SQLGetDiagField and SQLGetDiagFieldW
  // in odbc_diagnostics.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDiagFieldInternal(
      handleType, handle, recNumber, diagIdentifier, diagInfo,
      diagInfoBufferLen, diagInfoStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetDiagField(rc, *(*kTraceOption));

  // Call to Release mutex in odbc_lock.h as applicable for the handle type.
  status = ReleaseHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDiagField.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetDiagFieldW(SQLSMALLINT handleType, SQLHANDLE handle,
                                   SQLSMALLINT recNumber,
                                   SQLSMALLINT diagIdentifier,
                                   SQLPOINTER diagInfo,
                                   SQLSMALLINT diagInfoBufferLen,
                                   SQLSMALLINT* diagInfoStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  status = AcquireHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDiagField and SQLGetDiagFieldW
  // in odbc_diagnostics.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h as applicable for the handle type.
  status = ReleaseHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current values of multiple fields of a diagnostic record that
// contains error, warning, and status information.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagrec-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetDiagRecA(SQLSMALLINT handleType, SQLHANDLE handle,
                                 SQLSMALLINT recNumber, SQLCHAR* sqlState,
                                 SQLINTEGER* nativeError, SQLCHAR* messageText,
                                 SQLSMALLINT messageTextBufferLen,
                                 SQLSMALLINT* messageTextLen) {
  return SQLGetDiagRec(handleType, handle, recNumber, sqlState, nativeError,
                       messageText, messageTextBufferLen, messageTextLen);
}

SQLRETURN SQL_API SQLGetDiagRec(SQLSMALLINT handleType, SQLHANDLE handle,
                                SQLSMALLINT recNumber, SQLCHAR* sqlState,
                                SQLINTEGER* nativeError, SQLCHAR* messageText,
                                SQLSMALLINT messageTextBufferLen,
                                SQLSMALLINT* messageTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetDiagRec");

  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  status = AcquireHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetDiagRec(
        handleType, handle, recNumber, sqlState, nativeError, messageText,
        messageTextBufferLen, messageTextLen, *(*kTraceOption));

  // Call to common internal function for SQLGetDiagRec and SQLGetDiagRecW
  // in odbc_diagnostics.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDiagRecInternal(
      handleType, handle, recNumber, sqlState, nativeError, messageText,
      messageTextBufferLen, messageTextLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLGetDiagRec(rc, *(*kTraceOption));

  // Call to Release mutex in odbc_lock.h as applicable for the handle type.
  status = ReleaseHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDiagRec.
////////////////////////////////////////
SQLRETURN SQL_API SQLGetDiagRecW(SQLSMALLINT handleType, SQLHANDLE handle,
                                 SQLSMALLINT recNumber, SQLWCHAR* sqlState,
                                 SQLINTEGER* nativeError, SQLWCHAR* messageText,
                                 SQLSMALLINT messageTextBufferLen,
                                 SQLSMALLINT* messageTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  status = AcquireHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDiagRec and SQLGetDiagRecW
  // in odbc_diagnostics.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h as applicable for the handle type.
  status = ReleaseHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the list of column names in specified tables.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLColumnsA(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                              SQLSMALLINT catalogNameLen, SQLCHAR* schemaName,
                              SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
                              SQLSMALLINT tableNameLen, SQLCHAR* columnName,
                              SQLSMALLINT columnNameLen) {
  return SQLColumns(statementHandle, catalogName, catalogNameLen, schemaName,
                    schemaNameLen, tableName, tableNameLen, columnName,
                    columnNameLen);
}

SQLRETURN SQL_API SQLColumns(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                             SQLSMALLINT catalogNameLen, SQLCHAR* schemaName,
                             SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
                             SQLSMALLINT tableNameLen, SQLCHAR* columnName,
                             SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLColumns and SQLColumnsW
  // in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColumns.
////////////////////////////////////////
SQLRETURN SQL_API SQLColumnsW(SQLHSTMT statementHandle, SQLWCHAR* catalogName,
                              SQLSMALLINT catalogNameLen, SQLWCHAR* schemaName,
                              SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
                              SQLSMALLINT tableNameLen, SQLWCHAR* columnName,
                              SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLColumns and SQLColumnsW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the list of table, catalog, or schema names, and table types,
// stored in a specific data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqltables-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLTablesA(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                             SQLSMALLINT catalogNameLen, SQLCHAR* schemaName,
                             SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
                             SQLSMALLINT tableNameLen, SQLCHAR* tableType,
                             SQLSMALLINT tableTypeLen) {
  return SQLTables(statementHandle, catalogName, catalogNameLen, schemaName,
                   schemaNameLen, tableName, tableNameLen, tableType,
                   tableTypeLen);
}

SQLRETURN SQL_API SQLTables(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                            SQLSMALLINT catalogNameLen, SQLCHAR* schemaName,
                            SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
                            SQLSMALLINT tableNameLen, SQLCHAR* tableType,
                            SQLSMALLINT tableTypeLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  bool is_tracing_enabled = IsTracingEnabled("SQLTables");
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLTables(
        statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
        tableName, tableNameLen, tableType, tableTypeLen, *(*kTraceOption));

  // Call to common internal function for SQLTables and SQLTablesW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLTablesInternal(
      statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
      tableName, tableNameLen, tableType, tableTypeLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLTables(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLTables.
////////////////////////////////////////
SQLRETURN SQL_API SQLTablesW(SQLHSTMT statementHandle, SQLWCHAR* catalogName,
                             SQLSMALLINT catalogNameLen, SQLWCHAR* schemaName,
                             SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
                             SQLSMALLINT tableNameLen, SQLWCHAR* tableType,
                             SQLSMALLINT tableTypeLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLTables and SQLTablesW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the column names that make up the primary key for a table.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlprimarykeys-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLPrimaryKeysA(SQLHSTMT statementHandle,
                                  SQLCHAR* catalogName,
                                  SQLSMALLINT catalogNameLen,
                                  SQLCHAR* schemaName,
                                  SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
                                  SQLSMALLINT tableNameLen) {
  return SQLPrimaryKeys(statementHandle, catalogName, catalogNameLen,
                        schemaName, schemaNameLen, tableName, tableNameLen);
}

SQLRETURN SQL_API SQLPrimaryKeys(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                                 SQLSMALLINT catalogNameLen,
                                 SQLCHAR* schemaName, SQLSMALLINT schemaNameLen,
                                 SQLCHAR* tableName, SQLSMALLINT tableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLPrimaryKeys");
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLPrimaryKeys(
        statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
        tableName, tableNameLen, *(*kTraceOption));
  // Call to common internal function for SQLPrimaryKeys and SQLPrimaryKeysW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLPrimaryKeysInternal(
      statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
      tableName, tableNameLen);
  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLPrimaryKeys(rc, *(*kTraceOption));
  return rc;
}
////////////////////////////////////////
// Unicode version of SQLPrimaryKeys.
////////////////////////////////////////
SQLRETURN SQL_API SQLPrimaryKeysW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLPrimaryKeys and SQLPrimaryKeysW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Returns the list of input and output parameters, as well as the columns that
// make up the result set for the specified procedures.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlprocedurecolumns-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLProcedureColumnsA(
    SQLHSTMT statementHandle, SQLCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLCHAR* procName,
    SQLSMALLINT procNameLen, SQLCHAR* columnName, SQLSMALLINT columnNameLen) {
  return SQLProcedureColumns(statementHandle, catalogName, catalogNameLen,
                             schemaName, schemaNameLen, procName, procNameLen,
                             columnName, columnNameLen);
}

SQLRETURN SQL_API SQLProcedureColumns(
    SQLHSTMT statementHandle, SQLCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLCHAR* procName,
    SQLSMALLINT procNameLen, SQLCHAR* columnName, SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLProcedureColumns and
  // SQLProcedureColumnsW in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLProcedureColumns.
////////////////////////////////////////
SQLRETURN SQL_API SQLProcedureColumnsW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* procName,
    SQLSMALLINT procNameLen, SQLWCHAR* columnName, SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLProcedureColumns and
  // SQLProcedureColumnsW in odbc_driver_metadata.h. Handle Unicode conversion
  // of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Returns the list of procedure names stored in a specific data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlprocedures-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLProceduresA(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                                 SQLSMALLINT catalogNameLen,
                                 SQLCHAR* schemaName, SQLSMALLINT schemaNameLen,
                                 SQLCHAR* procName, SQLSMALLINT procNameLen) {
  return SQLProcedures(statementHandle, catalogName, catalogNameLen, schemaName,
                       schemaNameLen, procName, procNameLen);
}

SQLRETURN SQL_API SQLProcedures(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                                SQLSMALLINT catalogNameLen, SQLCHAR* schemaName,
                                SQLSMALLINT schemaNameLen, SQLCHAR* procName,
                                SQLSMALLINT procNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLProcedures and SQLProceduresW
  // in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLProcedures.
////////////////////////////////////////
SQLRETURN SQL_API SQLProceduresW(SQLHSTMT statementHandle,
                                 SQLWCHAR* catalogName,
                                 SQLSMALLINT catalogNameLen,
                                 SQLWCHAR* schemaName,
                                 SQLSMALLINT schemaNameLen, SQLWCHAR* procName,
                                 SQLSMALLINT procNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLProcedures and SQLProceduresW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Retrieves the following information about columns within a specified table:
//   - The optimal set of columns that uniquely identifies a row in the table.
//   - Columns that are automatically updated when any value in the row is
//     updated by a transaction.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlspecialcolumns-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSpecialColumnsA(
    SQLHSTMT statementHandle, SQLUSMALLINT identifierType, SQLCHAR* catalogName,
    SQLSMALLINT catalogNameLen, SQLCHAR* schemaName, SQLSMALLINT schemaNameLen,
    SQLCHAR* tableName, SQLSMALLINT tableNameLen, SQLUSMALLINT minRowIdScope,
    SQLUSMALLINT colNullable) {
  return SQLSpecialColumns(statementHandle, identifierType, catalogName,
                           catalogNameLen, schemaName, schemaNameLen, tableName,
                           tableNameLen, minRowIdScope, colNullable);
}

SQLRETURN SQL_API SQLSpecialColumns(
    SQLHSTMT statementHandle, SQLUSMALLINT identifierType, SQLCHAR* catalogName,
    SQLSMALLINT catalogNameLen, SQLCHAR* schemaName, SQLSMALLINT schemaNameLen,
    SQLCHAR* tableName, SQLSMALLINT tableNameLen, SQLUSMALLINT minRowIdScope,
    SQLUSMALLINT colNullable) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLSpecialColumns and
  // SQLSpecialColumnsW in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSpecialColumns.
////////////////////////////////////////
SQLRETURN SQL_API SQLSpecialColumnsW(
    SQLHSTMT statementHandle, SQLUSMALLINT identifierType,
    SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen, SQLWCHAR* schemaName,
    SQLSMALLINT schemaNameLen, SQLWCHAR* tableName, SQLSMALLINT tableNameLen,
    SQLUSMALLINT minRowIdScope, SQLUSMALLINT colNullable) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLSpecialColumns and
  // SQLSpecialColumnsW in odbc_driver_metadata.h. Handle Unicode conversion of
  // output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Retrieves a list of statistics about a single table and the indexes
// associated with the table. The driver returns the information as a result
// set.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlstatistics-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLStatisticsA(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                                 SQLSMALLINT catalogNameLen,
                                 SQLCHAR* schemaName, SQLSMALLINT schemaNameLen,
                                 SQLCHAR* tableName, SQLSMALLINT tableNameLen,
                                 SQLUSMALLINT indexType,
                                 SQLUSMALLINT reserved) {
  return SQLStatistics(statementHandle, catalogName, catalogNameLen, schemaName,
                       schemaNameLen, tableName, tableNameLen, indexType,
                       reserved);
}

SQLRETURN SQL_API SQLStatistics(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                                SQLSMALLINT catalogNameLen, SQLCHAR* schemaName,
                                SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
                                SQLSMALLINT tableNameLen,
                                SQLUSMALLINT indexType, SQLUSMALLINT reserved) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLStatistics and SQLStatisticsW
  // in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLStatistics.
////////////////////////////////////////
SQLRETURN SQL_API SQLStatisticsW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen, SQLUSMALLINT indexType, SQLUSMALLINT reserved) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLStatistics and SQLStatisticsW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Returns a list of tables and the privileges associated with each table.
// The driver returns the information as a result set on the specified
// statement..
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqltableprivileges-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLTablePrivilegesA(
    SQLHSTMT statementHandle, SQLCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
    SQLSMALLINT tableNameLen) {
  return SQLTablePrivileges(statementHandle, catalogName, catalogNameLen,
                            schemaName, schemaNameLen, tableName, tableNameLen);
}

SQLRETURN SQL_API SQLTablePrivileges(
    SQLHSTMT statementHandle, SQLCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
    SQLSMALLINT tableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLTablePrivileges and
  // SQLTablePrivilegesW in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLTablePrivileges.
////////////////////////////////////////
SQLRETURN SQL_API SQLTablePrivilegesW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLTablePrivileges and
  // SQLTablePrivilegesW in odbc_driver_metadata.h. Handle Unicode conversion of
  // output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Returns
//   -- A list of foreign keys in the specified table (columns in the specified
//   table that
//      refer to primary keys in other tables).
//   -- A list of foreign keys in other tables that refer to the primary key in
//   the
//      specified table.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlforeignkeys-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API
SQLForeignKeysA(SQLHSTMT statementHandle, SQLCHAR* pkCatalogName,
                SQLSMALLINT pkCatalogNameLen, SQLCHAR* pkSchemaName,
                SQLSMALLINT pkSchemaNameLen, SQLCHAR* pkTableName,
                SQLSMALLINT pkTableNameLen, SQLCHAR* fkCatalogName,
                SQLSMALLINT fkCatalogNameLen, SQLCHAR* fkSchemaName,
                SQLSMALLINT fkSchemaNameLen, SQLCHAR* fkTableName,
                SQLSMALLINT fkTableNameLen) {
  return SQLForeignKeys(statementHandle, pkCatalogName, pkCatalogNameLen,
                        pkSchemaName, pkSchemaNameLen, pkTableName,
                        pkTableNameLen, fkCatalogName, fkCatalogNameLen,
                        fkSchemaName, fkSchemaNameLen, fkTableName,
                        fkTableNameLen);
}

SQLRETURN SQL_API
SQLForeignKeys(SQLHSTMT statementHandle, SQLCHAR* pkCatalogName,
               SQLSMALLINT pkCatalogNameLen, SQLCHAR* pkSchemaName,
               SQLSMALLINT pkSchemaNameLen, SQLCHAR* pkTableName,
               SQLSMALLINT pkTableNameLen, SQLCHAR* fkCatalogName,
               SQLSMALLINT fkCatalogNameLen, SQLCHAR* fkSchemaName,
               SQLSMALLINT fkSchemaNameLen, SQLCHAR* fkTableName,
               SQLSMALLINT fkTableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLForeignKeys");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLForeignKeys(
        statementHandle, pkCatalogName, pkCatalogNameLen, pkSchemaName,
        pkSchemaNameLen, pkTableName, pkTableNameLen, fkCatalogName,
        fkCatalogNameLen, fkSchemaName, fkSchemaNameLen, fkTableName,
        fkTableNameLen, *(*kTraceOption));
  // Call to common internal function for SQLForeignKeys and SQLForeignKeysW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLForeignKeysInternal(
      statementHandle, pkCatalogName, pkCatalogNameLen, pkSchemaName,
      pkSchemaNameLen, pkTableName, pkTableNameLen, fkCatalogName,
      fkCatalogNameLen, fkSchemaName, fkSchemaNameLen, fkTableName,
      fkTableNameLen);
  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLForeignKeys(rc, *(*kTraceOption));

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLForeignKeys.
////////////////////////////////////////
SQLRETURN SQL_API
SQLForeignKeysW(SQLHSTMT statementHandle, SQLWCHAR* pkCatalogName,
                SQLSMALLINT pkCatalogNameLen, SQLWCHAR* pkSchemaName,
                SQLSMALLINT pkSchemaNameLen, SQLWCHAR* pkTableName,
                SQLSMALLINT pkTableNameLen, SQLWCHAR* fkCatalogName,
                SQLSMALLINT fkCatalogNameLen, SQLWCHAR* fkSchemaName,
                SQLSMALLINT fkSchemaNameLen, SQLWCHAR* fkTableName,
                SQLSMALLINT fkTableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLForeignKeys and SQLForeignKeysW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Returns a list of columns and associated privileges for the specified table.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumnprivileges-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLColumnPrivilegesA(
    SQLHSTMT statementHandle, SQLCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
    SQLSMALLINT tableNameLen, SQLCHAR* columnName, SQLSMALLINT columnNameLen) {
  return SQLColumnPrivileges(statementHandle, catalogName, catalogNameLen,
                             schemaName, schemaNameLen, tableName, tableNameLen,
                             columnName, columnNameLen);
}

SQLRETURN SQL_API SQLColumnPrivileges(
    SQLHSTMT statementHandle, SQLCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
    SQLSMALLINT tableNameLen, SQLCHAR* columnName, SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLColumnPrivileges and
  // SQLColumnPrivilegesW in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColumnPrivileges.
////////////////////////////////////////
SQLRETURN SQL_API SQLColumnPrivilegesW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen, SQLWCHAR* columnName, SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLColumnPrivileges and
  // SQLColumnPrivilegesW in odbc_driver_metadata.h. Handle Unicode conversion
  // of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Stops processing associated with a specific statement, closes any open
// cursors associated with the statement, discards pending results, or,
// optionally, frees all resources associated with the statement handle.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfreestmt-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLFreeStmt(SQLHSTMT statementHandle, SQLUSMALLINT option) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex for statement handle in odbc_lock.h.
  status = AcquireHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLFreeStmt in odbc_statement.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for statement handle in odbc_lock.h.
  status = ReleaseHandleMutex(statementHandle, SQL_HANDLE_STMT);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Requests a commit or rollback operation for all active operations on all
// statements associated with a connection.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlendtran-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLEndTran(SQLSMALLINT handleType, SQLHANDLE handle,
                             SQLSMALLINT completionType) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLEndTran");
  // Call to Acquire mutex in odbc_lock.h, as applicable for the handle type
  status = AcquireHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // passed in. Call to Trace function entry in odbc_trace.h if tracing is
  // enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLEndTran(handleType, handle, completionType,
                                  *(*kTraceOption));

  // Call to internal function for SQLEndTran in odbc_statement.h.
  rc = google::cloud::odbc_bq_driver::SQLEndTranInternal(handleType, handle,
                                                         completionType);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLEndTran(rc, *(*kTraceOption));
  // Call to Release mutex in odbc_lock.h, as applicable for the handle type
  status = ReleaseHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // passed in.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Cancels the processing on a statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcancel-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLCancel(SQLHSTMT statementHandle) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLCancel in odbc_statement.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Closes a cursor that has been opened on a statement and discards pending
// results.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlclosecursor-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLCloseCursor(SQLHSTMT statementHandle) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLCloseCursor");

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLCloseCursor(statementHandle, *(*kTraceOption));

  // Call to internal function for SQLCloseCursor in odbc_sql_results.h.
  rc = google::cloud::odbc_bq_driver::SQLCloseCursorInternal(statementHandle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLCloseCursor(rc, *(*kTraceOption));

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Closes the connection associated with a specific connection handle.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldisconnect-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLDisconnect(SQLHDBC connectionHandle) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLDisconnect");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  status = AcquireHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLDisconnect(connectionHandle, *(*kTraceOption));

  // Call to internal function for SQLCancel in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLDisconnectInternal(connectionHandle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLDisconnect(rc, *(*kTraceOption));
  // Call to Release mutex for connection handle in odbc_lock.h.
  status = ReleaseHandleMutex(connectionHandle, SQL_HANDLE_DBC);
  if (status != SQL_SUCCESS) {
    return status;
  }

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Frees resources associated with a specific environment, connection,
// statement, or descriptor handle.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfreehandle-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLFreeHandle(SQLSMALLINT handleType, SQLHANDLE handle) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  bool is_tracing_enabled = IsTracingEnabled("SQLFreeHandle");

  // Call to Acquire mutex in odbc_lock.h, as applicable for the handle type
  status = AcquireHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // passed in. Call to Trace function entry in odbc_trace.h if tracing is
  // enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLFreeHandle(handleType, handle, *(*kTraceOption));

  // Call to internal function for SQLFreeHandle in odbc_commons.h
  rc = google::cloud::odbc_bq_driver::SQLFreeHandleInternal(handleType, handle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLFreeHandle(rc, *(*kTraceOption));
  // Call to Release mutex in odbc_lock.h, as applicable for the handle type
  // passed in.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
// ODBC APIs supported in future driver releases.
//
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
// Cancels the processing on a connection or statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcancelhandle-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQLCancelHandle(SQLSMALLINT handleType, SQLHANDLE handle) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  // Call to Acquire mutex in odbc_lock.h, as applicable for the handle type
  status = AcquireHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // passed in. Call to Trace function entry in odbc_trace.h if tracing is
  // enabled.

  // Call to internal function for SQLCancelHandle in odbc_environment.h

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h, as applicable for the handle type
  status = ReleaseHandleMutex(handle, handleType);
  if (status != SQL_SUCCESS) {
    return status;
  }
  // passed in.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Sets the cursor position in a rowset and allows an application to refresh
// data in the rowset or to update or delete data in the result set.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetpos-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQLSetPos(SQLHSTMT statementHandle, SQLSETPOSIROW rowNumber,
                    SQLUSMALLINT operation, SQLUSMALLINT lockType) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLSetPos in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Performs bulk insertions and bulk bookmark operations, including update,
// delete, and fetch by bookmark.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbulkoperations-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLBulkOperations(SQLHSTMT statementHandle,
                                    SQLSMALLINT operation) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLBulkOperations in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
// NOLINTEND
