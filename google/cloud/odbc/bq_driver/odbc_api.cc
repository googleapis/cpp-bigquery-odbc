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
#include "google/cloud/odbc/bq_driver/odbc_driver_metadata.h"
#include "google/cloud/odbc/bq_driver/odbc_environment.h"
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
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLDriverConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLFetch;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLFreeHandle;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetEnvAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetFunctions;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLGetTypeInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionEntry_SQLSetEnvAttr;

using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLAllocHandle;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLBindCol;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLDriverConnect;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLFetch;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLFreeHandle;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetEnvAttr;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetFunctions;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLGetTypeInfo;
using ::google::cloud::odbc_bq_driver::TraceFunctionExit_SQLSetEnvAttr;
using ::google::cloud::odbc_bq_driver::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;

// Internal Helper Functions
namespace {
void RecordTraceStatus(std::string const& name, Status const& s) {
  if (!s.ok()) {
    std::cout << "Tracing is misconfigured: " << s.message() << std::endl;
    std::cout << "ODBC API: " << name << " will not be traced." << std::endl;
  }
}

bool IsTracingEnabled(std::string const& name) {
  if (!kTraceOptsConsole.ok()) {
    RecordTraceStatus(name, kTraceOptsConsole.status());
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
                                          *(*kTraceOptsConsole));

      rc = google::cloud::odbc_bq_driver::SQLAllocEnvHandle(outputHandle);

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionExit_SQLAllocHandle(rc, *(*kTraceOptsConsole));
      // Call to Release mutex for environment handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_DBC: {
      // Call to Acquire mutex for connection handle in odbc_lock.h.
      // Call to Trace function entry in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionEntry_SQLAllocHandle(handleType, inputHandle, outputHandle,
                                          *(*kTraceOptsConsole));

      rc = google::cloud::odbc_bq_driver::SQLAllocConnHandle(inputHandle,
                                                             outputHandle);

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionExit_SQLAllocHandle(rc, *(*kTraceOptsConsole));
      // Call to Release mutex for connection handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_STMT: {
      // Call to Acquire mutex for connection handle in odbc_lock.h.
      // Call to Trace function entry in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionEntry_SQLAllocHandle(handleType, inputHandle, outputHandle,
                                          *(*kTraceOptsConsole));

      rc = google::cloud::odbc_bq_driver::SQLAllocStmtHandle(inputHandle,
                                                             outputHandle);

      // Call to Trace function exit in odbc_trace.h if tracing is enabled.
      if (is_tracing_enabled)
        TraceFunctionExit_SQLAllocHandle(rc, *(*kTraceOptsConsole));
      // Call to Release mutex for connection handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_DESC: {
      // Call to Acquire mutex for descriptor handle in odbc_lock.h.
      // Call to Trace function entry in odbc_trace.h if tracing is enabled.

      // Call to Allocate descriptor handle in odbc_descriptor.h.

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
SQLRETURN SQL_API SQLDriverConnect(
    SQLHDBC connectionHandle, SQLHWND windowHandle, SQLCHAR* inConnectionString,
    SQLSMALLINT inConnectionStringLen, SQLCHAR* outConnectionString,
    SQLSMALLINT outConnectionStringBufferLen,
    SQLSMALLINT* outConnectionStringLen, SQLUSMALLINT driverCompletion) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLDriverConnect");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLDriverConnect(
        connectionHandle, windowHandle, inConnectionString,
        inConnectionStringLen, outConnectionString,
        outConnectionStringBufferLen, outConnectionStringLen, driverCompletion,
        *(*kTraceOptsConsole));

  // Call to internal common function for SQLDriverConnect and SQLDriverConnectW
  // in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLDriverConnectInternal(
      connectionHandle, windowHandle, inConnectionString, inConnectionStringLen,
      outConnectionString, outConnectionStringBufferLen, outConnectionStringLen,
      driverCompletion);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLDriverConnect(rc, *(*kTraceOptsConsole));

  // Call to Release mutex for connection handle in odbc_lock.h.

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

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLDriverConnect and SQLDriverConnectW
  // in odbc_connection.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Establishes connection to a driver and data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbrowseconnect-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLBrowseConnect(SQLHDBC connectionHandle,
                                   SQLCHAR* inConnectionString,
                                   SQLSMALLINT inConnectionStringLen,
                                   SQLCHAR* outConnectionString,
                                   SQLSMALLINT outConnectionStringBufferLen,
                                   SQLSMALLINT* outConnectionStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal common function for SQLBrowseConnect and SQLBrowseConnectW
  // in odbc_connection.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

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

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLBrowseConnect and SQLBrowseConnectW
  // in odbc_connection.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Establishes connection to a driver and data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlconnect-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLConnect(SQLHDBC connectionHandle, SQLCHAR* serverName,
                             SQLSMALLINT serverNameLen, SQLCHAR* userName,
                             SQLSMALLINT userNameLen, SQLCHAR* authString,
                             SQLSMALLINT authStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLConnect");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLConnect(connectionHandle, serverName, serverNameLen,
                                  userName, userNameLen, authString,
                                  authStringLen, *(*kTraceOptsConsole));

  // Call to internal common function for SQLConnect and SQLConnectW
  // in odbc_connection.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLConnect(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for connection handle in odbc_lock.h.

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

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLConnect and SQLConnectW
  // in odbc_connection.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns general information about the driver and data source
// associated with a connection
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetinfo-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetInfo(SQLHDBC connectionHandle, SQLUSMALLINT infoType,
                             SQLPOINTER infoValue,
                             SQLSMALLINT infoValueBufferLen,
                             SQLSMALLINT* infoValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetInfo");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetInfo(connectionHandle, infoType, infoValue,
                                  infoValueBufferLen, infoValueStringLen,
                                  *(*kTraceOptsConsole));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetInfoInternal(
      connectionHandle, infoType, infoValue, infoValueBufferLen,
      infoValueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetInfo(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for connection handle in odbc_lock.h.

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

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

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
  bool is_tracing_enabled = IsTracingEnabled("SQLGetFunctions");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetFunctions(
        connectionHandle, functionId, supportedFunction, *(*kTraceOptsConsole));

  // Call to internal function for SQLGetFunctions in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetFunctionsInternal(
      connectionHandle, functionId, supportedFunction);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetFunctions(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for connection handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns information about data types supported by the data source.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgettypeinfo-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetTypeInfo(SQLHSTMT statementHandle,
                                 SQLSMALLINT dataType) {
  SQLRETURN rc = SQL_SUCCESS;
  bool is_tracing_enabled = IsTracingEnabled("SQLGetTypeInfo");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetTypeInfo(statementHandle, dataType,
                                      *(*kTraceOptsConsole));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetTypeInfoInternal(statementHandle,
                                                               dataType);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetTypeInfo(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for statement handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets connection attributes.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetconnectattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetConnectAttr(SQLHDBC connectionHandle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal common function for SQLSetConnectAttr and
  // SQLSetConnectAttrW in odbc_connection.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetConnectAttr.
////////////////////////////////////////
SQLRETURN SQL_API SQLSetConnectAttrW(SQLHDBC connectionHandle,
                                     SQLINTEGER attribute, SQLPOINTER value,
                                     SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLSetConnectAttr and
  // SQLSetConnectAttrW in odbc_connection.h. Handle Unicode conversion of
  // output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current setting of a connection attribute.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetconnectattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetConnectAttr(SQLHDBC connectionHandle,
                                    SQLINTEGER attribute, SQLPOINTER value,
                                    SQLINTEGER valueBufferLen,
                                    SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal common function for SQLGetConnectAttr and
  // SQLGetConnectAttrW in odbc_connection.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

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

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetConnectAttr and
  // SQLGetConnectAttrW in odbc_connection.h. Handle Unicode conversion of
  // output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Sets attributes related to a statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlsetstmtattr-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLSetStmtAttr(SQLHSTMT statementHandle, SQLINTEGER attribute,
                                 SQLPOINTER value, SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal common function for SQLSetStmtAttr and SQLSetStmtAttrW
  // in odbc_statement.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
SQLRETURN SQL_API SQLGetStmtAttr(SQLHSTMT statementHandle, SQLINTEGER attribute,
                                 SQLPOINTER value, SQLINTEGER valueBufferLen,
                                 SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal common function for SQLGetStmtAttr and SQLGetStmtAttrW
  // in odbc_statement.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
  bool is_tracing_enabled = IsTracingEnabled("SQLSetEnvAttr");

  // Call to Acquire mutex for environmentHandle handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLSetEnvAttr(environmentHandle, attribute, value,
                                     valueStringLen, *(*kTraceOptsConsole));

  // Call to internal function for SQLSetEnvAttr in odbc_environment.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetEnvAttrInternal(
      environmentHandle, attribute, value, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLSetEnvAttr(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for environmentHandle handle in odbc_lock.h.

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
  bool is_tracing_enabled = IsTracingEnabled("SQLGetEnvAttr");

  // Call to Acquire mutex for environmentHandle handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLGetEnvAttr(environmentHandle, attribute, value,
                                     valueBufferLen, valueStringLen,
                                     *(*kTraceOptsConsole));

  // Call to internal function for SQLGetEnvAttr in odbc_environment.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetEnvAttrInternal(
      environmentHandle, attribute, value, valueBufferLen, valueStringLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLGetEnvAttr(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for environmentHandle handle in odbc_lock.h.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current setting or value of a single field of a descriptor
// record.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdescfield-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetDescField(SQLHDESC descriptorHandle,
                                  SQLSMALLINT recNumber, SQLSMALLINT fieldId,
                                  SQLPOINTER outDescValue,
                                  SQLINTEGER outDescValueBufferLen,
                                  SQLINTEGER* outDescValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLGetDescField and SQLGetDescFieldW
  // in odbc_descriptor.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
SQLRETURN SQL_API SQLGetDescRec(
    SQLHDESC descriptorHandle, SQLSMALLINT recNumber, SQLCHAR* name,
    SQLSMALLINT nameBufferLen, SQLSMALLINT* nameStringLen,
    SQLSMALLINT* descType, SQLSMALLINT* descSubType, SQLLEN* descOctetLen,
    SQLSMALLINT* descPrecision, SQLSMALLINT* descScale, SQLSMALLINT* nullable) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLGetDescRec and SQLGetDescRecW
  // in odbc_descriptor.h.

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
SQLRETURN SQL_API SQLSetDescField(SQLHDESC descriptorHandle,
                                  SQLSMALLINT recNumber,
                                  SQLSMALLINT fieldIdentifier,
                                  SQLPOINTER descValue,
                                  SQLINTEGER descValueBufferLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLSetDescField and SQLSetDescFieldW
  // in odbc_descriptor.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLSetDescRec in odbc_descriptor.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLCopyDesc in odbc_descriptor.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Prepares an SQL string for execution.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlprepare-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLPrepare(SQLHSTMT statementHandle, SQLCHAR* statementText,
                             SQLINTEGER statementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLPrepare and SQLPrepareW
  // in odbc_sql_requests.h.

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
SQLRETURN SQL_API SQLBindParameter(
    SQLHSTMT statementHandle, SQLUSMALLINT paramNumber, SQLSMALLINT paramType,
    SQLSMALLINT paramCType, SQLSMALLINT paramSqlType, SQLULEN paramColSize,
    SQLSMALLINT paramScale, SQLPOINTER paramDataValue,
    SQLLEN paramDataValueBufferLen, SQLLEN* paramDataValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLBindParameter in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the cursor name associated with a specified statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetcursorname-function
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetCursorName(SQLHSTMT statementHandle,
                                   SQLCHAR* cursorName,
                                   SQLSMALLINT cursorNameBufferLen,
                                   SQLSMALLINT* cursorNameStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLGetCursorName and SQLGetCursorNameW
  // in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
SQLRETURN SQL_API SQLSetCursorName(SQLHSTMT statementHandle,
                                   SQLCHAR* cursorName,
                                   SQLSMALLINT cursorNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLSetCursorName and SQLSetCursorNameW
  // in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLExecute in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
SQLRETURN SQL_API SQLNativeSql(SQLHDBC connectionHandle,
                               SQLCHAR* inStatementText,
                               SQLINTEGER inStatementTextLen,
                               SQLCHAR* outStatementText,
                               SQLINTEGER outStatementTextBufferLen,
                               SQLINTEGER* outStatementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLNativeSql and SQLNativeSqlW
  // in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

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

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLNativeSql and SQLNativeSqlW
  // in odbc_sql_requests.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

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

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLNumParams in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLDescribeParam in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLNumResultCols in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
  bool is_tracing_enabled = IsTracingEnabled("SQLFetch");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLFetch(statementHandle, *(*kTraceOptsConsole));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLFetchInternal(statementHandle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled) TraceFunctionExit_SQLFetch(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for statement handle in odbc_lock.h.

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
SQLRETURN SQL_API SQLColAttribute(SQLHSTMT statementHandle,
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
SQLRETURN SQL_API SQLDescribeCol(
    SQLHSTMT statementHandle, SQLUSMALLINT columnNumber, SQLCHAR* columnName,
    SQLSMALLINT columnNameBufferLen, SQLSMALLINT* columnNameLe,
    SQLSMALLINT* columnSQLdataType, SQLULEN* columnSize,
    SQLSMALLINT* decimalDigits, SQLSMALLINT* columnNullable) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLDescribeCol and SQLDescribeColW
  // in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
  bool is_tracing_enabled = IsTracingEnabled("SQLBindCol");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLBindCol(statementHandle, columnNumber, targetCType,
                                  targetValuePtr, targetValueBufferLen,
                                  targetValueStrLen, *(*kTraceOptsConsole));

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLBindColInternal(
      statementHandle, columnNumber, targetCType, targetValuePtr,
      targetValueBufferLen, targetValueStrLen);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLBindCol(rc, *(*kTraceOptsConsole));
  // Call to Release mutex for statement handle in odbc_lock.h.

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
SQLRETURN SQL_API SQLGetDiagField(SQLSMALLINT handleType, SQLHANDLE handle,
                                  SQLSMALLINT recNumber,
                                  SQLSMALLINT diagIdentifier,
                                  SQLPOINTER diagInfo,
                                  SQLSMALLINT diagInfoBufferLen,
                                  SQLSMALLINT* diagInfoStringLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLGetDiagField and SQLGetDiagFieldW
  // in odbc_diagnostics.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h as applicable for the handle type.

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

  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDiagField and SQLGetDiagFieldW
  // in odbc_diagnostics.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h as applicable for the handle type.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the current values of multiple fields of a diagnostic record that
// contains error, warning, and status information.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdiagrec-function.
////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLGetDiagRec(SQLSMALLINT handleType, SQLHANDLE handle,
                                SQLSMALLINT recNumber, SQLCHAR* sqlState,
                                SQLINTEGER* nativeError, SQLCHAR* messageText,
                                SQLSMALLINT messageTextBufferLen,
                                SQLSMALLINT* messageTextLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLGetDiagRec and SQLGetDiagRecW
  // in odbc_diagnostics.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h as applicable for the handle type.

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

  // Call to Acquire mutex in odbc_lock.h as applicable for the handle type.
  // Call to Trace Unicode function entry in odbc_trace.h if tracing is enabled.

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDiagRec and SQLGetDiagRecW
  // in odbc_diagnostics.h.
  // Handle Unicode conversion of output parameters.

  // Call to Trace Unicode function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h as applicable for the handle type.

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////
// Returns the list of column names in specified tables.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function.
////////////////////////////////////////////////////////////////////////////////////////
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
SQLRETURN SQL_API SQLTables(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                            SQLSMALLINT catalogNameLen, SQLCHAR* schemaName,
                            SQLSMALLINT schemaNameLen, SQLCHAR* tableName,
                            SQLSMALLINT tableNameLen, SQLCHAR* tableType,
                            SQLSMALLINT tableTypeLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLTables and SQLTablesW
  // in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
SQLRETURN SQL_API SQLPrimaryKeys(SQLHSTMT statementHandle, SQLCHAR* catalogName,
                                 SQLSMALLINT catalogNameLen,
                                 SQLCHAR* schemaName, SQLSMALLINT schemaNameLen,
                                 SQLCHAR* tableName, SQLSMALLINT tableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLPrimaryKeys and SQLPrimaryKeysW
  // in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
SQLForeignKeys(SQLHSTMT statementHandle, SQLCHAR* pkCatalogName,
               SQLSMALLINT pkCatalogNameLen, SQLCHAR* pkSchemaName,
               SQLSMALLINT pkSchemaNameLen, SQLCHAR* pkTableName,
               SQLSMALLINT pkTableNameLen, SQLCHAR* fkCatalogName,
               SQLSMALLINT fkCatalogNameLen, SQLCHAR* fkSchemaName,
               SQLSMALLINT fkSchemaNameLen, SQLCHAR* fkTableName,
               SQLSMALLINT fkTableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLForeignKeys and SQLForeignKeysW
  // in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLFreeStmt in odbc_statement.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for statement handle in odbc_lock.h.

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

  // Call to Acquire mutex in odbc_lock.h, as applicable for the handle type
  // passed in. Call to Trace function entry in odbc_trace.h if tracing is
  // enabled.

  // Call to internal function for SQLEndTran in odbc_statement.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h, as applicable for the handle type
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

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLCloseCursor in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLCancel in odbc_connection.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex for connection handle in odbc_lock.h.

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
  bool is_tracing_enabled = IsTracingEnabled("SQLFreeHandle");

  // Call to Acquire mutex in odbc_lock.h, as applicable for the handle type
  // passed in. Call to Trace function entry in odbc_trace.h if tracing is
  // enabled.
  if (is_tracing_enabled)
    TraceFunctionEntry_SQLFreeHandle(handleType, handle, *(*kTraceOptsConsole));

  // Call to internal function for SQLFreeHandle in odbc_commons.h
  rc = google::cloud::odbc_bq_driver::SQLFreeHandleInternal(handleType, handle);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  if (is_tracing_enabled)
    TraceFunctionExit_SQLFreeHandle(rc, *(*kTraceOptsConsole));
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

  // Call to Acquire mutex in odbc_lock.h, as applicable for the handle type
  // passed in. Call to Trace function entry in odbc_trace.h if tracing is
  // enabled.

  // Call to internal function for SQLCancelHandle in odbc_environment.h

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.
  // Call to Release mutex in odbc_lock.h, as applicable for the handle type
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
