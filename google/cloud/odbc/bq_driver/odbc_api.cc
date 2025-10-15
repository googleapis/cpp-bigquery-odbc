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

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_attr.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
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
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/status_or.h"
#ifdef _WIN32
#include "google/cloud/odbc/bq_driver/odbc_windows.h"
#endif  //_WIN32
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
using google::cloud::odbc_bq_driver_internal::ConnectionAttr;
using google::cloud::odbc_bq_driver_internal::ConnectionValueType;
using google::cloud::odbc_bq_driver_internal::ConvertSQLWCHARToString;
using google::cloud::odbc_bq_driver_internal::IsDiagIdentifierString;
using google::cloud::odbc_bq_driver_internal::IsFieldIdentifierString;
using google::cloud::odbc_bq_driver_internal::IsInfoTypeString;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;
using google::cloud::odbc_bq_driver_internal::Utf8ToUtf16;
using google::cloud::odbc_bq_driver_internal::WStrToOutputBufferResponse;
using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

using ::google::cloud::odbc_bq_driver::HandleLock;

using google::cloud::odbc_bq_driver::ToCharStr;
using google::cloud::odbc_bq_driver::ToSqlChar;
using google::cloud::odbc_bq_driver::ToSqlWChar;

constexpr int kBufferLength = 4096;

// Internal Helper Functions
namespace {
void RecordTraceStatus(std::string const& name, StatusRecord const& s) {
  if (!s.ok()) {
    std::cout << "Tracing is misconfigured: " << s.message << std::endl;
    std::cout << "ODBC API: " << name << " will not be traced." << std::endl;
  }
}

void InitializeTracing(std::string const& name) {
  bool const kInitLogging = TraceOptions::InitializeLogging();
}

// Converts input pointer value which is SQLWCHAR* to SQLCHAR*
// and returns the converted value.
StatusRecordOr<std::string> ConvertSQLPointerToSQLChar(SQLPOINTER in_val,
                                                       SQLINTEGER in_val_len) {
  SQLWCHAR* in_wchar_val = reinterpret_cast<SQLWCHAR*>(in_val);
  StatusRecordOr<std::string> utf8_in_val =
      ConvertSQLWCHARToString(in_wchar_val, in_val_len);
  if (!utf8_in_val) {
    return utf8_in_val.GetStatusRecord();
  }
  return utf8_in_val;
}

// Converts input pointer value which is a SQLCHAR* to SQLWCHAR*
// and returns the converted value.
StatusRecordOr<std::wstring> ConvertSQLPointerToSQLWChar(
    SQLPOINTER in_val, SQLINTEGER in_val_len) {
  SQLCHAR* in_sqlchar_val = reinterpret_cast<SQLCHAR*>(in_val);
  std::string in_str_val(ToCharStr(in_sqlchar_val));
  StatusRecordOr<std::wstring> utf16_in_val = Utf8ToUtf16(in_str_val);
  if (!utf16_in_val) {
    return utf16_in_val.GetStatusRecord();
  }
  return utf16_in_val;
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
  InitializeTracing("SQLAllocHandle");

  switch (handleType) {
    case SQL_HANDLE_ENV: {
      // Call to Acquire mutex for environment handle in odbc_lock.h.
      HandleLock lock(nullptr, SQL_HANDLE_ENV, true);  // Use global lock
      if (!lock.isLocked()) {
        return SQL_INVALID_HANDLE;
      }
      rc = google::cloud::odbc_bq_driver::SQLAllocEnvHandle(outputHandle);

      // Call to Release mutex for environment handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_DBC: {
      // Call to Acquire mutex for connection handle in odbc_lock.h.
      HandleLock lock(inputHandle, SQL_HANDLE_ENV);
      if (!lock.isLocked()) {
        return SQL_INVALID_HANDLE;
      }
      rc = google::cloud::odbc_bq_driver::SQLAllocConnHandle(inputHandle,
                                                             outputHandle);
      // Call to Release mutex for connection handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_STMT: {
      // Call to Acquire mutex for connection handle in odbc_lock.h.
      HandleLock lock(inputHandle, SQL_HANDLE_DBC);
      if (!lock.isLocked()) {
        return SQL_INVALID_HANDLE;
      }
      rc = google::cloud::odbc_bq_driver::SQLAllocStmtHandle(inputHandle,
                                                             outputHandle);

      // Call to Release mutex for connection handle in odbc_lock.h.
      break;
    }
    case SQL_HANDLE_DESC: {
      // Call to Acquire mutex for descriptor handle in odbc_lock.h.
      HandleLock lock(inputHandle, SQL_HANDLE_DBC);
      if (!lock.isLocked()) {
        return SQL_INVALID_HANDLE;
      }
      rc = google::cloud::odbc_bq_driver::SQLAllocDescHandle(inputHandle,
                                                             outputHandle);

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
  InitializeTracing("SQLDriverConnect");

  // Call to Acquire mutex for connection handle in odbc_lock.h.
  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLDriverConnect and SQLDriverConnectW
  // in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLDriverConnectInternal(
      connectionHandle, windowHandle, inConnectionString, inConnectionStringLen,
      outConnectionString, outConnectionStringBufferLen, outConnectionStringLen,
      driverCompletion);

  return rc;
}
//////////////////////////////////////
// Unicode version of SQLDriverConnect.
//////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLDriverConnectW(
    SQLHDBC connectionHandle, SQLHWND windowHandle,
    SQLWCHAR* inConnectionString, SQLSMALLINT inConnectionStringLen,
    SQLWCHAR* outConnectionString, SQLSMALLINT outConnectionStringBufferLen,
    SQLSMALLINT* outConnectionStringLen, SQLUSMALLINT driverCompletion) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLDriverConnectW");
  // Call to Acquire mutex for connection handle in odbc_lock.h.
  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_in_connection_str;
  SQLCHAR* sqlchar_in_connection_str = nullptr;
  if (inConnectionString) {
    utf8_in_connection_str =
        ConvertSQLWCHARToString(inConnectionString, inConnectionStringLen);
    if (!utf8_in_connection_str) {
      return utf8_in_connection_str.GetCalculatedReturnCode();
    }
    sqlchar_in_connection_str = ToSqlChar(utf8_in_connection_str->data());
    if (inConnectionStringLen && inConnectionStringLen != SQL_NTS)
      inConnectionStringLen = utf8_in_connection_str->length();
  }
  // outConnectionString is an output value that is not populated by the user.
  // This should not be unicode converted if it is empty. Instead we send a
  // SQLCHAR empty value directly to the internal function.
  SQLCHAR* out_conn_str = reinterpret_cast<SQLCHAR*>(outConnectionString);
  SQLSMALLINT out_conn_str_len = 0;
  // Call to internal common function for SQLDriverConnect and
  // SQLDriverConnectW in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLDriverConnectInternal(
      connectionHandle, windowHandle, sqlchar_in_connection_str,
      inConnectionStringLen, out_conn_str, outConnectionStringBufferLen,
      &out_conn_str_len, driverCompletion);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && outConnectionString) {
    StatusRecordOr<std::wstring> utf16_out_conn_str;
    if (out_conn_str_len > 0) {
      utf16_out_conn_str = Utf8ToUtf16((char*)out_conn_str);
    } else {
      std::string val(ToCharStr(out_conn_str));
      utf16_out_conn_str = Utf8ToUtf16(val);
    }
    if (!utf16_out_conn_str) {
      return utf16_out_conn_str.GetCalculatedReturnCode();
    }
    outConnectionString = ToSqlWChar(utf16_out_conn_str->data());
  }
  if (outConnectionStringLen) *outConnectionStringLen = out_conn_str_len;

  // Call to Release mutex for connection handle in odbc_lock.h.

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
  InitializeTracing("SQLBrowseConnect");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLBrowseConnect and SQLBrowseConnectW
  // in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLBrowseConnectInternal(
      connectionHandle, inConnectionString, inConnectionStringLen,
      outConnectionString, outConnectionStringBufferLen,
      outConnectionStringLen);

  return rc;
}
//////////////////////////////////////
// Unicode version of SQLBrowseConnect.
//////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLBrowseConnectW(SQLHDBC connectionHandle,
                                    SQLWCHAR* inConnectionString,
                                    SQLSMALLINT inConnectionStringLen,
                                    SQLWCHAR* outConnectionString,
                                    SQLSMALLINT outConnectionStringBufferLen,
                                    SQLSMALLINT* outConnectionStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLBrowseConnectW");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  SQLCHAR* sqlchar_in_connection_str = nullptr;
  StatusRecordOr<std::string> utf8_in_connection_str;
  if (inConnectionString) {
    utf8_in_connection_str =
        ConvertSQLWCHARToString(inConnectionString, inConnectionStringLen);
    if (!utf8_in_connection_str) {
      return utf8_in_connection_str.GetCalculatedReturnCode();
    }
    sqlchar_in_connection_str = ToSqlChar(utf8_in_connection_str->data());
    if (inConnectionStringLen && inConnectionStringLen != SQL_NTS)
      inConnectionStringLen = utf8_in_connection_str->length();
  }
  // Call to internal common function for SQLBrowseConnect and SQLBrowseConnectW
  // in odbc_connection.h.
  // TODO: Internal call should be made with out_connection_string and
  // out_connection_string_len as the output parameters
  SQLCHAR* out_connection_string =
      reinterpret_cast<SQLCHAR*>(outConnectionString);
  rc = google::cloud::odbc_bq_driver::SQLBrowseConnectInternal(
      connectionHandle, sqlchar_in_connection_str, inConnectionStringLen,
      out_connection_string, outConnectionStringBufferLen,
      outConnectionStringLen);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) || rc == SQL_NEED_DATA) {
    StatusRecordOr<std::wstring> utf16_out_conn_str =
        Utf8ToUtf16((char*)out_connection_string);
    if (!utf16_out_conn_str) {
      return utf16_out_conn_str.GetCalculatedReturnCode();
    }
    std::memset(outConnectionString, '\0',
                outConnectionStringBufferLen * sizeof(SQLWCHAR));
    std::memcpy((SQLWCHAR*)outConnectionString,
                ToSqlWChar(utf16_out_conn_str->data()),
                utf16_out_conn_str->size() * sizeof(SQLWCHAR));
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
  InitializeTracing("SQLConnect");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLConnect and SQLConnectW
  // in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLConnectInternal(
      connectionHandle, serverName, serverNameLen, userName, userNameLen,
      authString, authStringLen);

  return rc;
}
//////////////////////////////////////
// Unicode version of SQLConnect.
//////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLConnectW(SQLHDBC connectionHandle, SQLWCHAR* serverName,
                              SQLSMALLINT serverNameLen, SQLWCHAR* userName,
                              SQLSMALLINT userNameLen, SQLWCHAR* authString,
                              SQLSMALLINT authStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLConnectW");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.

  // For this API, DriverManager requires serverName or DSN string
  // to be non-empty hence we need to validate it before proceeding further.
  size_t w_server_name_len = 0;
  if (serverName) {
    std::wstring w_server_str(reinterpret_cast<wchar_t const*>(serverName));
    w_server_name_len = w_server_str.length();
    if (w_server_name_len == 0) {
      auto status =
          StatusRecord{SQLStates::k_HY000(),
                       "serverName or datasource name cannot be null/empty"};
      return status.CalculateReturnCode();
    }
  } else {
    auto status =
        StatusRecord{SQLStates::k_HY000(),
                     "serverName or datasource name cannot be null/empty"};
    return status.CalculateReturnCode();
  }

  StatusRecordOr<std::string> utf8_server_name =
      ConvertSQLWCHARToString(serverName, serverNameLen);
  if (!utf8_server_name) {
    return utf8_server_name.GetCalculatedReturnCode();
  }
  if (serverNameLen && serverNameLen != SQL_NTS)
    serverNameLen = utf8_server_name->length();

  // User name and Auth strings are optional. Make sure they are non-empty
  // before converting to unicode.
  size_t w_user_name_len = 0;
  StatusRecordOr<std::string> utf8_user_name;
  if (userName) {
    std::wstring w_user_name_str(reinterpret_cast<wchar_t const*>(userName));
    w_user_name_len = w_user_name_str.length();
  }
  if (w_user_name_len > 0) {
    utf8_user_name = ConvertSQLWCHARToString(userName, w_user_name_len);
    if (!utf8_user_name) {
      return utf8_user_name.GetCalculatedReturnCode();
    }
    if (userNameLen && userNameLen != SQL_NTS)
      userNameLen = utf8_user_name->length();
  }

  size_t w_auth_str_len = 0;
  StatusRecordOr<std::string> utf8_auth_str;
  if (authString) {
    std::wstring w_auth_str(reinterpret_cast<wchar_t const*>(authString));
    w_auth_str_len = w_auth_str.length();
  }
  if (w_auth_str_len > 0) {
    utf8_auth_str = ConvertSQLWCHARToString(authString, w_auth_str_len);
    if (!utf8_auth_str) {
      return utf8_auth_str.GetCalculatedReturnCode();
    }
    if (authStringLen && authStringLen != SQL_NTS)
      authStringLen = utf8_auth_str->length();
  } else if (w_user_name_len > 0) {
    // It is an error to supply a username without a auth string.
    auto status = StatusRecord{
        SQLStates::k_HY000(),
        "authString cannot be empty or null of non-empty userName"};
    return status.CalculateReturnCode();
  }

  // Call to internal common function for SQLConnect and SQLConnectW
  // in odbc_connection.h.
  if (w_user_name_len > 0) {
    rc = google::cloud::odbc_bq_driver::SQLConnectInternal(
        connectionHandle, ToSqlChar(utf8_server_name->data()), serverNameLen,
        ToSqlChar(utf8_user_name->data()), userNameLen,
        ToSqlChar(utf8_auth_str->data()), authStringLen);
  } else {
    rc = google::cloud::odbc_bq_driver::SQLConnectInternal(
        connectionHandle, ToSqlChar(utf8_server_name->data()), serverNameLen,
        ToSqlChar(""), w_user_name_len, ToSqlChar(""), w_auth_str_len);
  }

  // Handle Unicode conversion of output parameters.
  StatusRecordOr<std::wstring> utf16_server_name =
      Utf8ToUtf16(*utf8_server_name);
  if (!utf16_server_name) {
    return utf16_server_name.GetCalculatedReturnCode();
  }
  serverNameLen = utf16_server_name->length();
  std::memcpy(serverName, ToSqlWChar(utf16_server_name->data()), serverNameLen);

  if (w_user_name_len > 0) {
    StatusRecordOr<std::wstring> utf16_user_name = Utf8ToUtf16(*utf8_user_name);
    if (!utf16_user_name) {
      return utf16_user_name.GetCalculatedReturnCode();
    }
    userNameLen = utf16_user_name->length();
    std::memcpy(userName, ToSqlWChar(utf16_user_name->data()), userNameLen);
  }

  if (w_auth_str_len > 0) {
    StatusRecordOr<std::wstring> utf16_auth_str = Utf8ToUtf16(*utf8_auth_str);
    if (!utf16_auth_str) {
      return utf16_auth_str.GetCalculatedReturnCode();
    }
    authStringLen = utf16_auth_str->length();
    std::memcpy(authString, ToSqlWChar(utf16_auth_str->data()), authStringLen);
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
  InitializeTracing("SQLGetInfo");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetInfoInternal(
      connectionHandle, infoType, infoValue, infoValueBufferLen,
      infoValueStringLen);

  return rc;
}
//////////////////////////////////////
// Unicode version of SQLGetInfo.
//////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetInfoW(SQLHDBC connectionHandle, SQLUSMALLINT infoType,
                              SQLPOINTER infoValue,
                              SQLSMALLINT infoValueBufferLen,
                              SQLSMALLINT* infoValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLGetInfoW");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  SQLCHAR info_val_buffer[kBufferLength] = {0};
  SQLSMALLINT info_val_buffer_len = 0;

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetInfoInternal(
      connectionHandle, infoType, info_val_buffer, infoValueBufferLen,
      &info_val_buffer_len);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc)) {
    if (IsInfoTypeString(infoType)) {
      std::memset(infoValue, '\0', infoValueBufferLen);

      if (info_val_buffer_len > 0) {
        StatusRecordOr<std::wstring> utf16_info_val =
            Utf8ToUtf16((char*)info_val_buffer);
        if (!utf16_info_val) {
          return utf16_info_val.GetCalculatedReturnCode();
        }

        std::vector<SQLWCHAR> sql_w_str(utf16_info_val->begin(),
                                        utf16_info_val->end());
        sql_w_str.emplace_back(L'\0');
        std::size_t bytes_available =
            static_cast<std::size_t>(infoValueBufferLen);
        std::size_t bytes_to_copy =
            std::min(sql_w_str.size() * sizeof(SQLWCHAR), bytes_available);

        std::memcpy(infoValue, sql_w_str.data(), bytes_to_copy);
      }
    } else {
      if (info_val_buffer_len > 0) {
        std::memcpy(infoValue, info_val_buffer, info_val_buffer_len);
      } else {
        std::memset(infoValue, '\0', infoValueBufferLen);
      }
    }
  }
  if (infoValueStringLen)
    *infoValueStringLen = info_val_buffer_len * sizeof(SQLWCHAR);

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
  InitializeTracing("SQLGetFunctions");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLGetFunctions in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetFunctionsInternal(
      connectionHandle, functionId, supportedFunction);

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
  InitializeTracing("SQLGetTypeInfo");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLGetTypeInfo and SQLGetTypeInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetTypeInfoInternal(statementHandle,
                                                               dataType);

  return rc;
}

////////////////////////////////////////
// Unicode version of SQLGetTypeInfo.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetTypeInfoW(SQLHSTMT statementHandle,
                                  SQLSMALLINT dataType) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLGetTypeInfoW");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLGetTypeInfo and SQLGetTypeInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetTypeInfoInternal(statementHandle,
                                                               dataType);
  // Handle Unicode conversion of  output parameters.

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
  InitializeTracing("SQLSetConnectAttr");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLSetConnectAttr and
  // SQLSetConnectAttrW in odbc_connection.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetConnectAttrInternal(
      connectionHandle, attribute, value, valueStringLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetConnectAttr.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLSetConnectAttrW(SQLHDBC connectionHandle,
                                     SQLINTEGER attribute, SQLPOINTER value,
                                     SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLSetConnectAttrW");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // If the Attribute value is a character string then we need to do the unicode
  // conversion on the input parameters.
  SQLPOINTER updated_attrib_val;
  SQLINTEGER updated_value_string_len;
  StatusRecordOr<std::string> updated_attrib_status;
  ConnectionAttr conn_attr;
  if (conn_attr.GetAttributeValueType(attribute) ==
      ConnectionValueType::kSqlChr) {
    if (valueStringLen && valueStringLen > 0) {
      updated_attrib_status =
          ConvertSQLPointerToSQLChar(value, valueStringLen / sizeof(SQLWCHAR));
    } else {
      updated_attrib_status = ConvertSQLPointerToSQLChar(value, valueStringLen);
    }
    if (!updated_attrib_status) {
      return updated_attrib_status.GetCalculatedReturnCode();
    }
    updated_attrib_val = (SQLPOINTER)ToSqlChar(updated_attrib_status->data());
    updated_value_string_len = strlen(updated_attrib_status->c_str());
  } else {
    // If we are not dealing with strings no conversions needed.
    updated_attrib_val = value;
    updated_value_string_len = valueStringLen;
  }

  // Handle Unicode conversion of input parameters.

  // Call to internal common function for SQLSetConnectAttr and
  // SQLSetConnectAttrW in odbc_connection.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetConnectAttrInternal(
      connectionHandle, attribute, updated_attrib_val,
      updated_value_string_len);

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
  InitializeTracing("SQLGetConnectAttr");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLGetConnectAttr and
  // SQLGetConnectAttrW in odbc_connection.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetConnectAttrInternal(
      connectionHandle, attribute, value, valueBufferLen, valueStringLen);

  return rc;
}

////////////////////////////////////////
// Unicode version of SQLGetConnectAttr.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetConnectAttrW(SQLHDBC connectionHandle,
                                     SQLINTEGER attribute, SQLPOINTER value,
                                     SQLINTEGER valueBufferLen,
                                     SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLGetConnectAttrW");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // For character strings SQLPOINTER may point to a WCHAR output value.
  // They need to be handled separately.
  ConnectionAttr conn_attr;
  SQLPOINTER updated_attrib_val;
  SQLCHAR attrib_val[kBufferLength] = "Not Set";
  StatusRecordOr<std::wstring> updated_out_attr_status;
  if (conn_attr.GetAttributeValueType(attribute) ==
      ConnectionValueType::kSqlChr) {
    updated_attrib_val = (SQLPOINTER)attrib_val;
  } else {
    updated_attrib_val = value;
  }

  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetConnectAttr and
  // SQLGetConnectAttrW in odbc_connection.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetConnectAttrInternal(
      connectionHandle, attribute, updated_attrib_val, valueBufferLen,
      valueStringLen);
  // Handle unicode conversion for attribute string values for output
  // parameters.
  if (SQL_SUCCEEDED(rc) && conn_attr.GetAttributeValueType(attribute) ==
                               ConnectionValueType::kSqlChr) {
    updated_out_attr_status =
        ConvertSQLPointerToSQLWChar(updated_attrib_val, valueBufferLen);
    if (!updated_out_attr_status) {
      return updated_out_attr_status.GetCalculatedReturnCode();
    }
    *valueStringLen =
        wcslen(updated_out_attr_status->data()) * sizeof(SQLWCHAR);
    std::vector<SQLWCHAR> sql_w_str(
        updated_out_attr_status->c_str(),
        updated_out_attr_status->c_str() + *valueStringLen);
    sql_w_str.emplace_back(L'\0');
    std::memset(value, '\0', valueBufferLen);
    std::memcpy(value, sql_w_str.data(), sql_w_str.size());
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
  InitializeTracing("SQLSetStmtAttr");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to internal common function for SQLSetStmtAttr and SQLSetStmtAttrW
  // in odbc_statement.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetStmtAttrInternal(
      statementHandle, attribute, value, valueStringLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetStmtAttr.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLSetStmtAttrW(SQLHSTMT statementHandle,
                                  SQLINTEGER attribute, SQLPOINTER value,
                                  SQLINTEGER valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLSetStmtAttrW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Handle Unicode conversion of input parameters.

  // Call to internal common function for SQLSetStmtAttr and SQLSetStmtAttrW
  // in odbc_statement.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetStmtAttrInternal(
      statementHandle, attribute, value, valueStringLen);

  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLGetStmtAttr");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to internal common function for SQLGetStmtAttr and SQLGetStmtAttrW
  // in odbc_statement.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetStmtAttrInternal(
      statementHandle, attribute, value, valueBufferLen, valueStringLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetStmtAttr.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetStmtAttrW(SQLHSTMT statementHandle,
                                  SQLINTEGER attribute, SQLPOINTER value,
                                  SQLINTEGER valueBufferLen,
                                  SQLINTEGER* valueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Handle Unicode conversion of input parameters.
  // Call to internal common function for SQLGetStmtAttr and SQLGetStmtAttrW
  // in odbc_statement.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetStmtAttrInternal(
      statementHandle, attribute, value, valueBufferLen, valueStringLen);

  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLSetEnvAttr");

  HandleLock lock(environmentHandle, SQL_HANDLE_ENV);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLSetEnvAttr in odbc_environment.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetEnvAttrInternal(
      environmentHandle, attribute, value, valueStringLen);

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
  InitializeTracing("SQLGetEnvAttr");
  HandleLock lock(environmentHandle, SQL_HANDLE_ENV);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLGetEnvAttr in odbc_environment.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetEnvAttrInternal(
      environmentHandle, attribute, value, valueBufferLen, valueStringLen);

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
  InitializeTracing("SQLGetDescField");
  HandleLock lock(descriptorHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  rc = google::cloud::odbc_bq_driver::SQLGetDescFieldInternal(
      descriptorHandle, recNumber, fieldId, outDescValue, outDescValueBufferLen,
      outDescValueStringLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDescField.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetDescFieldW(SQLHDESC descriptorHandle,
                                   SQLSMALLINT recNumber, SQLSMALLINT fieldId,
                                   SQLPOINTER outDescValue,
                                   SQLINTEGER outDescValueBufferLen,
                                   SQLINTEGER* outDescValueStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLGetDescFieldW");
  HandleLock lock(descriptorHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  SQLCHAR out_desc_val[kBufferLength] = {0};
  SQLINTEGER out_desc_val_string_len = 0;

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDescField and SQLGetDescFieldW
  // in odbc_descriptor.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDescFieldInternal(
      descriptorHandle, recNumber, fieldId, (SQLPOINTER)out_desc_val,
      outDescValueBufferLen, &out_desc_val_string_len);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && out_desc_val_string_len > 0) {
    if (IsFieldIdentifierString(fieldId)) {
      StatusRecordOr<std::wstring> utf16_out_desc_val =
          Utf8ToUtf16((char*)out_desc_val);
      if (!utf16_out_desc_val) {
        return utf16_out_desc_val.GetCalculatedReturnCode();
      }
      out_desc_val_string_len =
          wcslen(utf16_out_desc_val->data()) * sizeof(SQLWCHAR);
      std::vector<SQLWCHAR> sql_w_str(utf16_out_desc_val->begin(),
                                      utf16_out_desc_val->end());
      sql_w_str.emplace_back(L'\0');
      std::memset(outDescValue, '\0', outDescValueBufferLen);
      std::memcpy(outDescValue, sql_w_str.data(), out_desc_val_string_len);
    } else {
      std::memcpy(outDescValue, (SQLPOINTER)out_desc_val,
                  out_desc_val_string_len);
    }
  }
  if (outDescValueStringLen) *outDescValueStringLen = out_desc_val_string_len;

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
  InitializeTracing("SQLGetDescRec");
  HandleLock lock(descriptorHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  rc = google::cloud::odbc_bq_driver::SQLGetDescRecInternal(
      descriptorHandle, recNumber, name, nameBufferLen, nameStringLen, descType,
      descSubType, descOctetLen, descPrecision, descScale, nullable);

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDescRec.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetDescRecW(
    SQLHDESC descriptorHandle, SQLSMALLINT recNumber, SQLWCHAR* name,
    SQLSMALLINT nameBufferLen, SQLSMALLINT* nameStringLen,
    SQLSMALLINT* descType, SQLSMALLINT* descSubType, SQLLEN* descOctetLen,
    SQLSMALLINT* descPrecision, SQLSMALLINT* descScale, SQLSMALLINT* nullable) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLGetDescRecW");
  HandleLock lock(descriptorHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  SQLCHAR name_buffer[kBufferLength];
  SQLSMALLINT name_string_len = 0;

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDescRec and SQLGetDescRecW
  // in odbc_descriptor.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDescRecInternal(
      descriptorHandle, recNumber, name_buffer, nameBufferLen, &name_string_len,
      descType, descSubType, descOctetLen, descPrecision, descScale, nullable);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && name_string_len > 0) {
    StatusRecordOr<std::wstring> utf16_name = Utf8ToUtf16((char*)name_buffer);
    if (!utf16_name) {
      return utf16_name.GetCalculatedReturnCode();
    }
    std::memset(name, '\0', nameBufferLen * sizeof(SQLWCHAR));
    std::memcpy(name, ToSqlWChar(utf16_name->data()),
                name_string_len * sizeof(SQLWCHAR));
  }
  if (nameStringLen) *nameStringLen = name_string_len;

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
  InitializeTracing("SQLSetDescField");
  HandleLock lock(descriptorHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  rc = google::cloud::odbc_bq_driver::SQLSetDescFieldInternal(
      descriptorHandle, recNumber, fieldIdentifier, descValue,
      descValueBufferLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetDescField.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLSetDescFieldW(SQLHDESC descriptorHandle,
                                   SQLSMALLINT recNumber,
                                   SQLSMALLINT fieldIdentifier,
                                   SQLPOINTER descValue,
                                   SQLINTEGER descValueBufferLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLSetDescFieldW");
  HandleLock lock(descriptorHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  SQLPOINTER updated_desc_val = descValue;
  StatusRecordOr<std::string> updated_desc_status;
  if (IsFieldIdentifierString(fieldIdentifier)) {
    updated_desc_status = ConvertSQLPointerToSQLChar(descValue, SQL_NTS);
    if (!updated_desc_status) {
      return updated_desc_status.GetCalculatedReturnCode();
    }
    updated_desc_val = (SQLPOINTER)ToSqlChar(updated_desc_status->data());
  }

  // Handle Unicode conversion of input parameters.

  // Call to common internal function for SQLSetDescField and SQLSetDescFieldW
  // in odbc_descriptor.h.
  rc = google::cloud::odbc_bq_driver::SQLSetDescFieldInternal(
      descriptorHandle, recNumber, fieldIdentifier, updated_desc_val,
      descValueBufferLen);

  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLSetDescRec");
  HandleLock lock(descriptorHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  rc = google::cloud::odbc_bq_driver::SQLSetDescRecInternal(
      descriptorHandle, recNumber, descType, descSubType, descOctetLen,
      descPrecision, descScale, descData, descOctetLenPtr, descIndicator);

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
  InitializeTracing("SQLCopyDesc");
  HandleLock lock(sourceDescHandle, SQL_HANDLE_DESC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  rc = google::cloud::odbc_bq_driver::SQLCopyDescInternal(sourceDescHandle,
                                                          targetDescHandle);

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
  InitializeTracing("SQLPrepare");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLPrepare and SQLPrepareW
  // in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLPrepareInternal(
      statementHandle, statementText, statementTextLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLPrepare.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLPrepareW(SQLHSTMT statementHandle, SQLWCHAR* statementText,
                              SQLINTEGER statementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLPrepareW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_stmt_txt;
  SQLCHAR* sqlchar_stmt_txt = nullptr;
  if (statementText) {
    utf8_stmt_txt = ConvertSQLWCHARToString(statementText, statementTextLen);
    if (!utf8_stmt_txt) {
      return utf8_stmt_txt.GetCalculatedReturnCode();
    }
    sqlchar_stmt_txt = ToSqlChar(utf8_stmt_txt->data());
    if (statementTextLen && statementTextLen != SQL_NTS)
      statementTextLen = utf8_stmt_txt->length();
  }
  // Call to common internal function for SQLPrepare and SQLPrepareW
  // in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLPrepareInternal(
      statementHandle, sqlchar_stmt_txt, statementTextLen);

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
  InitializeTracing("SQLBindParameter");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLBindParameter in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLBindParameterInternal(
      statementHandle, parameterNumber, inputOutputType, valueType,
      parameterType, columnSize, decimalDigits, parameterValuePtr, bufferLength,
      strLen_or_IndPtr);

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
  InitializeTracing("SQLGetCursorName");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLGetCursorName and SQLGetCursorNameW
  // in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetCursorNameInternal(
      statementHandle, cursorName, cursorNameBufferLen, cursorNameStringLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetCursorName.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetCursorNameW(SQLHSTMT statementHandle,
                                    SQLWCHAR* cursorName,
                                    SQLSMALLINT cursorNameBufferLen,
                                    SQLSMALLINT* cursorNameStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  SQLCHAR cursor_name[kBufferLength] = {0};
  SQLSMALLINT cursor_name_len = 0;

  // Call to common internal function for SQLGetCursorName and SQLGetCursorNameW
  // in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetCursorNameInternal(
      statementHandle, cursor_name, cursorNameBufferLen, &cursor_name_len);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && cursor_name_len > 0) {
    StatusRecordOr<std::wstring> utf16_cur_name =
        Utf8ToUtf16((char*)cursor_name);
    if (!utf16_cur_name) {
      return utf16_cur_name.GetCalculatedReturnCode();
    }
    std::vector<SQLWCHAR> sql_w_str(utf16_cur_name->begin(),
                                    utf16_cur_name->end());
    sql_w_str.emplace_back(L'\0');
    std::memcpy(cursorName, sql_w_str.data(),
                (sql_w_str.size() + 1) * sizeof(SQLWCHAR));
  }
  if (cursorNameStringLen) *cursorNameStringLen = cursor_name_len;

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
  InitializeTracing("SQLSetCursorName");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLSetCursorName and SQLSetCursorNameW
  // in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetCursorNameInternal(
      statementHandle, cursorName, cursorNameLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSetCursorName.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLSetCursorNameW(SQLHSTMT statementHandle,
                                    SQLWCHAR* cursorName,
                                    SQLSMALLINT cursorNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLSetCursorNameW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  if (cursorNameLen <= 0 && cursorNameLen != SQL_NTS) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "Invalid string length"};
    return status_record.CalculateReturnCode();
  }
  SQLCHAR* sqlchar_cur_name = nullptr;
  StatusRecordOr<std::string> utf8_cur_name;
  if (cursorName) {
    utf8_cur_name = ConvertSQLWCHARToString(cursorName, cursorNameLen);
    if (!utf8_cur_name) {
      return utf8_cur_name.GetCalculatedReturnCode();
    }
    sqlchar_cur_name = ToSqlChar(utf8_cur_name->data());
    if (cursorNameLen && cursorNameLen != SQL_NTS)
      cursorNameLen = utf8_cur_name->length();
  }

  // Call to common internal function for SQLSetCursorName and SQLSetCursorNameW
  // in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLSetCursorNameInternal(
      statementHandle, sqlchar_cur_name, cursorNameLen);

  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLExecute");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to Acquire mutex for statement handle in odbc_lock.h.

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLExecuteInternal(statementHandle);

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
  InitializeTracing("SQLExecDirect");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLExecDirect and SQLExecDirectW
  // in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLExecDirectInternal(
      statementHandle, statementText, statementTextLen);

  return rc;
}

////////////////////////////////////////
// Unicode version of SQLExecDirect.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLExecDirectW(SQLHSTMT statementHandle,
                                 SQLWCHAR* statementText,
                                 SQLINTEGER statementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLExecDirectW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_stmt_txt;
  SQLCHAR* sqlchar_stmt_txt = nullptr;
  if (statementText) {
    utf8_stmt_txt = ConvertSQLWCHARToString(statementText, statementTextLen);
    if (!utf8_stmt_txt) {
      return utf8_stmt_txt.GetCalculatedReturnCode();
    }
    sqlchar_stmt_txt = ToSqlChar(utf8_stmt_txt->data());
    if (statementTextLen && statementTextLen != SQL_NTS)
      statementTextLen = utf8_stmt_txt->length();
  }

  // Call to common internal function for SQLExecDirect and SQLExecDirectW
  // in odbc_sql_requests.h.
  // Handle Unicode conversion of output parameters.
  rc = google::cloud::odbc_bq_driver::SQLExecDirectInternal(
      statementHandle, sqlchar_stmt_txt, statementTextLen);

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
  InitializeTracing("SQLNativeSql");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLNativeSql and SQLNativeSqlW
  // in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLNativeSqlInternal(
      connectionHandle, inStatementText, inStatementTextLen, outStatementText,
      outStatementTextBufferLen, outStatementTextLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLNativeSql.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLNativeSqlW(SQLHDBC connectionHandle,
                                SQLWCHAR* inStatementText,
                                SQLINTEGER inStatementTextLen,
                                SQLWCHAR* outStatementText,
                                SQLINTEGER outStatementTextBufferLen,
                                SQLINTEGER* outStatementTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  SQLCHAR out_statement_text[kBufferLength] = {0};
  InitializeTracing("SQLNativeSqlW");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_in_stmt_txt;
  SQLCHAR* sqlchar_in_stmt_txt = nullptr;
  if (inStatementText) {
    utf8_in_stmt_txt =
        ConvertSQLWCHARToString(inStatementText, inStatementTextLen);
    if (!utf8_in_stmt_txt) {
      return utf8_in_stmt_txt.GetCalculatedReturnCode();
    }
    sqlchar_in_stmt_txt = ToSqlChar(utf8_in_stmt_txt->data());
    if (inStatementTextLen && inStatementTextLen != SQL_NTS)
      inStatementTextLen = utf8_in_stmt_txt->length();
  }

  // Call to common internal function for SQLNativeSql and SQLNativeSqlW
  // in odbc_sql_requests.h.
  // TODO: Internal call should be made with out_statement_text as the output
  // parameter.
  // Handle Unicode conversion of output parameters.
  rc = ::google::cloud::odbc_bq_driver::SQLNativeSqlInternal(
      connectionHandle, sqlchar_in_stmt_txt, inStatementTextLen,
      out_statement_text, outStatementTextBufferLen, outStatementTextLen);

  std::string outStatementTextStr = (char*)out_statement_text;
  if (!outStatementTextStr.empty()) {
    StatusRecordOr<std::wstring> utf16_out_stmt_txt =
        Utf8ToUtf16(outStatementTextStr);
    if (!utf16_out_stmt_txt) {
      return utf16_out_stmt_txt.GetCalculatedReturnCode();
    }
    SQLLEN out_len = 0;
    if (outStatementText) {
      WStrToOutputBufferResponse(
          *utf16_out_stmt_txt, outStatementText, outStatementTextBufferLen,
          utf16_out_stmt_txt->size(), utf16_out_stmt_txt->size(), &out_len);
    }
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
  InitializeTracing("SQLSetDescField");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLNumParams in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLNumParamsInternal(statementHandle,
                                                           paramCount);

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
  InitializeTracing("SQLParamData");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLParamData in odbc_sql_requests.h.
  rc = google::cloud::odbc_bq_driver::SQLParamDataInternal(statementHandle,
                                                           paramOrTargetValue);

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
  InitializeTracing("SQLPutData");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLPutData in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLPutDataInternal(
      statementHandle, paramData, paramDataLen);

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
  InitializeTracing("SQLDescribeParam");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLDescribeParam in odbc_sql_requests.h.
  rc = ::google::cloud::odbc_bq_driver::SQLDescribeParamInternal(
      statementHandle, paramNumber, paramSqlType, paramSize, paramScale,
      paramNullable);

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
  InitializeTracing("SQLGetData");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLGetData in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLGetDataInternal(
      statementHandle, columnNumber, targetCType, targetValue,
      targetValueBufferLen, targetValueStringLen);

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
  InitializeTracing("SQLNumResultCols");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLNumResultCols in odbc_sql_results.h.
  rc = google::cloud::odbc_bq_driver::SQLNumResultColsInternal(statementHandle,
                                                               columnCount);

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
  InitializeTracing("SQLFetch");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLFetchInternal(statementHandle);

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
  HandleLock lock(statementHandletmt, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
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
// TODO(b/369324094): Unicode SQLColAttribute API not linking with ODBC library
// on Windows x86
#if !defined(_WIN32) || defined(_WIN64)
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
#endif /* WIN32 || WIN64 */

SQLRETURN SQL_API SQLColAttribute(SQLHSTMT statementHandle,
                                  SQLUSMALLINT columnNumber,
                                  SQLUSMALLINT fieldIdentifier,
                                  SQLPOINTER characterAttribute,
                                  SQLSMALLINT characterAttributeBufferLen,
                                  SQLSMALLINT* characterAttributeStringLen,
                                  SQLLEN* numericAttribute) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLColAttribute");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLColAttributeInternal(
      statementHandle, columnNumber, fieldIdentifier, characterAttribute,
      characterAttributeBufferLen, characterAttributeStringLen,
      numericAttribute);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColAttribute.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLColAttributeW(SQLHSTMT statementHandle,
                                   SQLUSMALLINT columnNumber,
                                   SQLUSMALLINT fieldIdentifier,
                                   SQLPOINTER characterAttribute,
                                   SQLSMALLINT characterAttributeBufferLen,
                                   SQLSMALLINT* characterAttributeStringLen,
                                   SQLLEN* numericAttribute) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLSMALLINT character_attribute_string_len = 0;
  InitializeTracing("SQLColAttributeW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  SQLPOINTER updated_character_attrib_val;
  SQLCHAR character_attrib_val[kBufferLength] = "Not Set";
  StatusRecordOr<std::wstring> updated_out_character_attr_status;
  updated_character_attrib_val = (SQLPOINTER)character_attrib_val;
  // reset characterAttribute buffer to make sure it doesn't contain garbage or
  // cache value.
  std::memset(characterAttribute, '\0', characterAttributeBufferLen);

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLColAttributeInternal(
      statementHandle, columnNumber, fieldIdentifier,
      updated_character_attrib_val, characterAttributeBufferLen,
      &character_attribute_string_len, numericAttribute);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && character_attribute_string_len >= 0) {
    if (IsFieldIdentifierString(fieldIdentifier)) {
      updated_out_character_attr_status = ConvertSQLPointerToSQLWChar(
          updated_character_attrib_val, characterAttributeBufferLen);
      if (!updated_out_character_attr_status) {
        return updated_out_character_attr_status.GetCalculatedReturnCode();
      }
      std::wstring const& wstr = *updated_out_character_attr_status;
      size_t const bytes_to_copy =
          std::min<size_t>(static_cast<size_t>(characterAttributeBufferLen),
                           wstr.size() * sizeof(SQLWCHAR));

      std::memcpy(characterAttribute, wstr.data(), bytes_to_copy);
      if (characterAttributeBufferLen >= sizeof(SQLWCHAR)) {
        SQLWCHAR* wchar_buf = static_cast<SQLWCHAR*>(characterAttribute);
        wchar_buf[bytes_to_copy / sizeof(SQLWCHAR)] = 0;
      }
      character_attribute_string_len = static_cast<SQLSMALLINT>(wstr.size());

    } else {
      std::memcpy(characterAttribute, (SQLPOINTER)updated_character_attrib_val,
                  characterAttributeBufferLen);
    }
  }
  if (characterAttributeStringLen) {
    *characterAttributeStringLen = character_attribute_string_len;
#ifdef WIN32
    *characterAttributeStringLen =
        character_attribute_string_len * sizeof(SQLWCHAR);
#endif  // WIN32
  }

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
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColAttributes.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLColAttributesW(SQLHSTMT statementHandle,
                                    SQLUSMALLINT columnNumber,
                                    SQLUSMALLINT fieldIdentifier,
                                    SQLPOINTER characterAttribute,
                                    SQLSMALLINT characterAttributeBufferLen,
                                    SQLSMALLINT* characterAttributeStringLen,
                                    SQLLEN* numericAttribute) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLCHAR character_attribute_buffer[kBufferLength] = {0};
  SQLSMALLINT character_attribute_buffer_len = 0;
  InitializeTracing("SQLColAttributesW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLColAttribute and SQLColAttributeW
  // in odbc_sql_results.h.
  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && character_attribute_buffer_len > 0) {
    StatusRecordOr<std::wstring> utf16_character_attribute =
        Utf8ToUtf16((char*)character_attribute_buffer);
    if (!utf16_character_attribute) {
      return utf16_character_attribute.GetCalculatedReturnCode();
    }
    std::memcpy(characterAttribute,
                (SQLPOINTER)ToSqlWChar(utf16_character_attribute->data()),
                character_attribute_buffer_len);
  }
  if (characterAttributeStringLen)
    *characterAttributeStringLen = character_attribute_buffer_len;

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
  InitializeTracing("SQLDescribeCol");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to common internal function for SQLDescribeCol and SQLDescribeColW
  // in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLDescribeColInternal(
      statementHandle, columnNumber, columnName, columnNameBufferLen,
      columnNameLe, columnSQLdataType, columnSize, decimalDigits,
      columnNullable);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLDescribeCol.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLDescribeColW(
    SQLHSTMT statementHandle, SQLUSMALLINT columnNumber, SQLWCHAR* columnName,
    SQLSMALLINT columnNameBufferLen, SQLSMALLINT* columnNameLen,
    SQLSMALLINT* columnSQLdataType, SQLULEN* columnSize,
    SQLSMALLINT* decimalDigits, SQLSMALLINT* columnNullable) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLDescribeColW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  SQLCHAR column_name_buffer[kBufferLength] = {0};
  SQLSMALLINT column_name_string_len = 0;

  // Handle Unicode conversion of input parameters.

  // Call to common internal function for SQLDescribeCol and SQLDescribeColW
  // in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLDescribeColInternal(
      statementHandle, columnNumber, column_name_buffer, columnNameBufferLen,
      &column_name_string_len, columnSQLdataType, columnSize, decimalDigits,
      columnNullable);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && column_name_string_len > 0) {
    StatusRecordOr<std::wstring> utf16_col_name =
        Utf8ToUtf16((char*)column_name_buffer);
    if (!utf16_col_name) {
      return utf16_col_name.GetCalculatedReturnCode();
    }
    std::vector<SQLWCHAR> sql_w_str(utf16_col_name->begin(),
                                    utf16_col_name->end());
    sql_w_str.emplace_back(L'\0');
    std::memset(columnName, '\0', columnNameBufferLen);
    std::memcpy(columnName, sql_w_str.data(),
                column_name_string_len * sizeof(SQLWCHAR));
  }

  if (columnNameLen) {
    *columnNameLen = column_name_string_len;
  }

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
  InitializeTracing("SQLBindCol");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLBindColInternal(
      statementHandle, columnNumber, targetCType, targetValuePtr,
      targetValueBufferLen, targetValueStrLen);

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
  SQLRETURN status;
  InitializeTracing("SQLRowCount");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLRowCount in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLRowCountInternal(statementHandle,
                                                            rowCount);

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
  SQLRETURN status;
  InitializeTracing("SQLFetchScroll");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLFetchScroll in odbc_sql_results.h.
  rc = ::google::cloud::odbc_bq_driver::SQLFetchScrollInternal(
      statementHandle, fetchOrientation, fetchOffset);

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
  InitializeTracing("SQLMoreResults");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal common function for SQLGetInfo and SQLGetInfoW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLMoreResultsInternal(statementHandle);

  // Call to Release mutex for statement handle in odbc_lock.h.

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
  InitializeTracing("SQLGetDiagField");

  HandleLock lock(handle, handleType);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLGetDiagField and SQLGetDiagFieldW
  // in odbc_diagnostics.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDiagFieldInternal(
      handleType, handle, recNumber, diagIdentifier, diagInfo,
      diagInfoBufferLen, diagInfoStringLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDiagField.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetDiagFieldW(SQLSMALLINT handleType, SQLHANDLE handle,
                                   SQLSMALLINT recNumber,
                                   SQLSMALLINT diagIdentifier,
                                   SQLPOINTER diagInfo,
                                   SQLSMALLINT diagInfoBufferLen,
                                   SQLSMALLINT* diagInfoStringLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  InitializeTracing("SQLGetDiagFieldW");

  HandleLock lock(handle, handleType);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  SQLPOINTER updated_diag_info;
  SQLCHAR diag_info[kBufferLength] = "Not Set";
  SQLSMALLINT diag_info_str_len = 0;
  StatusRecordOr<std::wstring> updated_out_diag_info_status;
  if (IsDiagIdentifierString(diagIdentifier)) {
    updated_diag_info = (SQLPOINTER)diag_info;
  } else {
    updated_diag_info = diagInfo;
  }

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDiagField and SQLGetDiagFieldW
  // in odbc_diagnostics.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDiagFieldInternal(
      handleType, handle, recNumber, diagIdentifier, updated_diag_info,
      diagInfoBufferLen, &diag_info_str_len);

  // Handle Unicode conversion of output parameters.
  if (SQL_SUCCEEDED(rc) && diag_info_str_len > 0) {
    std::memset(diagInfo, '\0', diagInfoBufferLen);
    if (IsDiagIdentifierString(diagIdentifier)) {
      updated_out_diag_info_status =
          ConvertSQLPointerToSQLWChar(updated_diag_info, diagInfoBufferLen);
      if (!updated_out_diag_info_status) {
        return updated_out_diag_info_status.GetCalculatedReturnCode();
      }
      diag_info_str_len =
          wcslen(updated_out_diag_info_status->data()) * sizeof(SQLWCHAR);
      std::vector<SQLWCHAR> sql_w_str(
          updated_out_diag_info_status->c_str(),
          updated_out_diag_info_status->c_str() + diag_info_str_len);
      sql_w_str.emplace_back(L'\0');
      std::memcpy(diagInfo, sql_w_str.data(), sql_w_str.size());

    } else {
      std::memcpy(diagInfo, updated_diag_info, diagInfoBufferLen);
    }
  }
  if (diagInfoStringLen) *diagInfoStringLen = diag_info_str_len;

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
  InitializeTracing("SQLGetDiagRec");

  HandleLock lock(handle, handleType);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLGetDiagRec and SQLGetDiagRecW
  // in odbc_diagnostics.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDiagRecInternal(
      handleType, handle, recNumber, sqlState, nativeError, messageText,
      messageTextBufferLen, messageTextLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLGetDiagRec.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLGetDiagRecW(SQLSMALLINT handleType, SQLHANDLE handle,
                                 SQLSMALLINT recNumber, SQLWCHAR* sqlState,
                                 SQLINTEGER* nativeError, SQLWCHAR* messageText,
                                 SQLSMALLINT messageTextBufferLen,
                                 SQLSMALLINT* messageTextLen) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;
  SQLCHAR sql_state_buffer[kBufferLength] = {0};
  SQLCHAR* message_text_buffer = reinterpret_cast<SQLCHAR*>(messageText);
  SQLSMALLINT message_text_buffer_len = 0;
  InitializeTracing("SQLGetDiagRecW");

  HandleLock lock(handle, handleType);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  // Call to common internal function for SQLGetDiagRec and SQLGetDiagRecW
  // in odbc_diagnostics.h.
  rc = google::cloud::odbc_bq_driver::SQLGetDiagRecInternal(
      handleType, handle, recNumber, sql_state_buffer, nativeError,
      message_text_buffer, messageTextBufferLen, &message_text_buffer_len);

  // Handle Unicode conversion of output parameters.

  if (sqlState) {
    StatusRecordOr<std::wstring> utf16_sql_state =
        Utf8ToUtf16((char*)sql_state_buffer);
    if (!utf16_sql_state) {
      return utf16_sql_state.GetCalculatedReturnCode();
    }
    std::memcpy(sqlState, ToSqlWChar(utf16_sql_state->data()),
                utf16_sql_state->size() * sizeof(SQLWCHAR));
  }

  if (messageText && message_text_buffer_len > 0) {
    StatusRecordOr<std::wstring> utf16_msg_txt =
        Utf8ToUtf16((char*)message_text_buffer);
    if (!utf16_msg_txt) {
      return utf16_msg_txt.GetCalculatedReturnCode();
    }
    std::memset(messageText, '\0', messageTextBufferLen);
    std::memcpy(messageText, ToSqlWChar(utf16_msg_txt->data()),
                utf16_msg_txt->size() * sizeof(SQLWCHAR));
  }
  if (messageTextLen) *messageTextLen = message_text_buffer_len;

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
  SQLRETURN status;
  InitializeTracing("SQLColumns");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to common internal function for SQLColumns and SQLColumnsW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLColumnsInternal(
      statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
      tableName, tableNameLen, columnName, columnNameLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColumns.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLColumnsW(SQLHSTMT statementHandle, SQLWCHAR* catalogName,
                              SQLSMALLINT catalogNameLen, SQLWCHAR* schemaName,
                              SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
                              SQLSMALLINT tableNameLen, SQLWCHAR* columnName,
                              SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLColumnsW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_catalog_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_catalog_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  SQLCHAR* sqlchar_table_name = nullptr;
  if (tableName) {
    utf8_table_name = ConvertSQLWCHARToString(tableName, tableNameLen);
    if (!utf8_table_name) {
      return utf8_table_name.GetCalculatedReturnCode();
    }
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
    if (tableNameLen && tableNameLen != SQL_NTS)
      tableNameLen = utf8_table_name->length();
  }

  StatusRecordOr<std::string> utf8_col_name;
  SQLCHAR* sqlchar_column_name = nullptr;
  if (columnName) {
    utf8_col_name = ConvertSQLWCHARToString(columnName, columnNameLen);
    if (!utf8_col_name) {
      return utf8_col_name.GetCalculatedReturnCode();
    }
    sqlchar_column_name = ToSqlChar(utf8_col_name->data());
    if (columnNameLen && columnNameLen != SQL_NTS)
      columnNameLen = utf8_col_name->length();
  }

  // Call to common internal function for SQLColumns and SQLColumnsW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.
  rc = google::cloud::odbc_bq_driver::SQLColumnsInternal(
      statementHandle, sqlchar_catalog_name, catalogNameLen,
      sqlchar_schema_name, schemaNameLen, sqlchar_table_name, tableNameLen,
      sqlchar_column_name, columnNameLen);

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
  InitializeTracing("SQLTables");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to common internal function for SQLTables and SQLTablesW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLTablesInternal(
      statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
      tableName, tableNameLen, tableType, tableTypeLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLTables.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLTablesW(SQLHSTMT statementHandle, SQLWCHAR* catalogName,
                             SQLSMALLINT catalogNameLen, SQLWCHAR* schemaName,
                             SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
                             SQLSMALLINT tableNameLen, SQLWCHAR* tableType,
                             SQLSMALLINT tableTypeLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLTablesW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_category_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_category_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  SQLCHAR* sqlchar_table_name = nullptr;
  if (tableName) {
    utf8_table_name = ConvertSQLWCHARToString(tableName, tableNameLen);
    if (!utf8_table_name) {
      return utf8_table_name.GetCalculatedReturnCode();
    }
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
    if (tableNameLen && tableNameLen != SQL_NTS)
      tableNameLen = utf8_table_name->length();
  }

  StatusRecordOr<std::string> utf8_table_type;
  SQLCHAR* sqlchar_table_type = nullptr;
  if (tableType) {
    utf8_table_type = ConvertSQLWCHARToString(tableType, tableTypeLen);
    if (!utf8_table_type) {
      return utf8_table_type.GetCalculatedReturnCode();
    }
    sqlchar_table_type = ToSqlChar(utf8_table_type->data());

    if (tableTypeLen && tableTypeLen != SQL_NTS)
      tableTypeLen = utf8_table_type->length();
  }

  // Call to common internal function for SQLTables and SQLTablesW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLTablesInternal(
      statementHandle, sqlchar_category_name, catalogNameLen,
      sqlchar_schema_name, schemaNameLen, sqlchar_table_name, tableNameLen,
      sqlchar_table_type, tableTypeLen);
  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLPrimaryKeys");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to common internal function for SQLPrimaryKeys and SQLPrimaryKeysW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLPrimaryKeysInternal(
      statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
      tableName, tableNameLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLPrimaryKeys.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLPrimaryKeysW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_category_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_category_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  SQLCHAR* sqlchar_table_name = nullptr;
  if (tableName) {
    utf8_table_name = ConvertSQLWCHARToString(tableName, tableNameLen);
    if (!utf8_table_name) {
      return utf8_table_name.GetCalculatedReturnCode();
    }
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
    if (tableNameLen && tableNameLen != SQL_NTS)
      tableNameLen = utf8_table_name->length();
  }

  // Call to common internal function for SQLPrimaryKeys and SQLPrimaryKeysW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLPrimaryKeysInternal(
      statementHandle, sqlchar_category_name, catalogNameLen,
      sqlchar_schema_name, schemaNameLen, sqlchar_table_name, tableNameLen);

  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLProcedureColumns");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLProcedureColumns and
  // SQLProcedureColumnsW in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLProcedureColumnsInternal(
      statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
      procName, procNameLen, columnName, columnNameLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLProcedureColumns.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLProcedureColumnsW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* procName,
    SQLSMALLINT procNameLen, SQLWCHAR* columnName, SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLProcedureColumnsW");

  // Call to Acquire mutex for statement handle in odbc_lock.h.
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_catalog_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_catalog_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_proc_name;
  SQLCHAR* sqlchar_proc_name = nullptr;
  if (procName) {
    utf8_proc_name = ConvertSQLWCHARToString(procName, procNameLen);
    if (!utf8_proc_name) {
      return utf8_proc_name.GetCalculatedReturnCode();
    }
    sqlchar_proc_name = ToSqlChar(utf8_proc_name->data());
    if (procNameLen && procNameLen != SQL_NTS)
      procNameLen = utf8_proc_name->length();
  }

  StatusRecordOr<std::string> utf8_col_name;
  SQLCHAR* sqlchar_column_name = nullptr;
  if (columnName) {
    utf8_col_name = ConvertSQLWCHARToString(columnName, columnNameLen);
    if (!utf8_col_name) {
      return utf8_col_name.GetCalculatedReturnCode();
    }
    sqlchar_column_name = ToSqlChar(utf8_col_name->data());
    if (columnNameLen && columnNameLen != SQL_NTS)
      columnNameLen = utf8_col_name->length();
  }

  // Call to common internal function for SQLProcedureColumns and
  // SQLProcedureColumnsW in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.
  rc = google::cloud::odbc_bq_driver::SQLProcedureColumnsInternal(
      statementHandle, sqlchar_catalog_name, catalogNameLen,
      sqlchar_schema_name, schemaNameLen, sqlchar_proc_name, procNameLen,
      sqlchar_column_name, columnNameLen);

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
  InitializeTracing("SQLProcedures");

  // Acquire mutex lock
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLProcedures and SQLProceduresW
  // in odbc_driver_metadata.h.
  rc = ::google::cloud::odbc_bq_driver::SQLProcedureInternal(
      statementHandle, catalogName, catalogNameLen, schemaName, schemaNameLen,
      procName, procNameLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLProcedures.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLProceduresW(SQLHSTMT statementHandle,
                                 SQLWCHAR* catalogName,
                                 SQLSMALLINT catalogNameLen,
                                 SQLWCHAR* schemaName,
                                 SQLSMALLINT schemaNameLen, SQLWCHAR* procName,
                                 SQLSMALLINT procNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLProceduresW");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.

  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_catalog_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_catalog_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_proc_name;
  SQLCHAR* sqlchar_proc_name = nullptr;
  if (procName) {
    utf8_proc_name = ConvertSQLWCHARToString(procName, procNameLen);
    if (!utf8_proc_name) {
      return utf8_proc_name.GetCalculatedReturnCode();
    }
    sqlchar_proc_name = ToSqlChar(utf8_proc_name->data());
    if (procNameLen && procNameLen != SQL_NTS)
      procNameLen = utf8_proc_name->length();
  }

  // Call to common internal function for SQLProcedures and SQLProceduresW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.
  rc = google::cloud::odbc_bq_driver::SQLProcedureInternal(
      statementHandle, sqlchar_catalog_name, catalogNameLen,
      sqlchar_schema_name, schemaNameLen, sqlchar_proc_name, procNameLen);

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
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLSpecialColumns and
  // SQLSpecialColumnsW in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLSpecialColumns.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLSpecialColumnsW(
    SQLHSTMT statementHandle, SQLUSMALLINT identifierType,
    SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen, SQLWCHAR* schemaName,
    SQLSMALLINT schemaNameLen, SQLWCHAR* tableName, SQLSMALLINT tableNameLen,
    SQLUSMALLINT minRowIdScope, SQLUSMALLINT colNullable) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLSpecialColumnsW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_category_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_category_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  SQLCHAR* sqlchar_table_name = nullptr;
  if (tableName) {
    utf8_table_name = ConvertSQLWCHARToString(tableName, tableNameLen);
    if (!utf8_table_name) {
      return utf8_table_name.GetCalculatedReturnCode();
    }
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
    if (tableNameLen && tableNameLen != SQL_NTS)
      tableNameLen = utf8_table_name->length();
  }
  // Call to common internal function for SQLSpecialColumns and
  // SQLSpecialColumnsW in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

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
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLStatistics and SQLStatisticsW
  // in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLStatistics.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLStatisticsW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen, SQLUSMALLINT indexType, SQLUSMALLINT reserved) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLStatisticsW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_category_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_category_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  SQLCHAR* sqlchar_table_name = nullptr;
  if (tableName) {
    utf8_table_name = ConvertSQLWCHARToString(tableName, tableNameLen);
    if (!utf8_table_name) {
      return utf8_table_name.GetCalculatedReturnCode();
    }
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
    if (tableNameLen && tableNameLen != SQL_NTS)
      tableNameLen = utf8_table_name->length();
  }

  // Call to common internal function for SQLStatistics and SQLStatisticsW
  // in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

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
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLTablePrivileges and
  // SQLTablePrivilegesW in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLTablePrivileges.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLTablePrivilegesW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLTablePrivilegesW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_category_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_category_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  SQLCHAR* sqlchar_table_name = nullptr;
  if (tableName) {
    utf8_table_name = ConvertSQLWCHARToString(tableName, tableNameLen);
    if (!utf8_table_name) {
      return utf8_table_name.GetCalculatedReturnCode();
    }
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
    if (tableNameLen && tableNameLen != SQL_NTS)
      tableNameLen = utf8_table_name->length();
  }

  // Call to common internal function for SQLTablePrivileges and
  // SQLTablePrivilegesW in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLForeignKeys");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to common internal function for SQLForeignKeys and SQLForeignKeysW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLForeignKeysInternal(
      statementHandle, pkCatalogName, pkCatalogNameLen, pkSchemaName,
      pkSchemaNameLen, pkTableName, pkTableNameLen, fkCatalogName,
      fkCatalogNameLen, fkSchemaName, fkSchemaNameLen, fkTableName,
      fkTableNameLen);

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLForeignKeys.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API
SQLForeignKeysW(SQLHSTMT statementHandle, SQLWCHAR* pkCatalogName,
                SQLSMALLINT pkCatalogNameLen, SQLWCHAR* pkSchemaName,
                SQLSMALLINT pkSchemaNameLen, SQLWCHAR* pkTableName,
                SQLSMALLINT pkTableNameLen, SQLWCHAR* fkCatalogName,
                SQLSMALLINT fkCatalogNameLen, SQLWCHAR* fkSchemaName,
                SQLSMALLINT fkSchemaNameLen, SQLWCHAR* fkTableName,
                SQLSMALLINT fkTableNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLForeignKeysW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.
  StatusRecordOr<std::string> utf8_pk_catalog_name;
  SQLCHAR* sqlchar_pk_category_name = nullptr;
  if (pkCatalogName) {
    utf8_pk_catalog_name =
        ConvertSQLWCHARToString(pkCatalogName, pkCatalogNameLen);
    if (!utf8_pk_catalog_name) {
      return utf8_pk_catalog_name.GetCalculatedReturnCode();
    }

    sqlchar_pk_category_name = ToSqlChar(utf8_pk_catalog_name->data());
    if (pkCatalogNameLen && pkCatalogNameLen != SQL_NTS)
      pkCatalogNameLen = utf8_pk_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_pk_schema_name;
  SQLCHAR* sqlchar_pk_schema_name = nullptr;
  if (pkSchemaName) {
    utf8_pk_schema_name =
        ConvertSQLWCHARToString(pkSchemaName, pkSchemaNameLen);
    if (!utf8_pk_schema_name) {
      return utf8_pk_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_pk_schema_name = ToSqlChar(utf8_pk_schema_name->data());
    if (pkSchemaNameLen && pkSchemaNameLen != SQL_NTS)
      pkSchemaNameLen = utf8_pk_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_pk_table_name;
  SQLCHAR* sqlchar_pk_table_name = nullptr;
  if (pkTableName) {
    utf8_pk_table_name = ConvertSQLWCHARToString(pkTableName, pkTableNameLen);
    if (!utf8_pk_table_name) {
      return utf8_pk_table_name.GetCalculatedReturnCode();
    }
    sqlchar_pk_table_name = ToSqlChar(utf8_pk_table_name->data());
    if (pkTableNameLen && pkTableNameLen != SQL_NTS)
      pkTableNameLen = utf8_pk_table_name->length();
  }

  StatusRecordOr<std::string> utf8_fk_catalog_name;
  SQLCHAR* sqlchar_fk_category_name = nullptr;
  if (fkCatalogName) {
    utf8_fk_catalog_name =
        ConvertSQLWCHARToString(fkCatalogName, fkCatalogNameLen);
    if (!utf8_fk_catalog_name) {
      return utf8_fk_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_fk_category_name = ToSqlChar(utf8_fk_catalog_name->data());
    if (fkCatalogNameLen && fkCatalogNameLen != SQL_NTS)
      fkCatalogNameLen = utf8_fk_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_fk_schema_name;
  SQLCHAR* sqlchar_fk_schema_name = nullptr;
  if (fkSchemaName) {
    utf8_fk_schema_name =
        ConvertSQLWCHARToString(fkSchemaName, fkSchemaNameLen);
    if (!utf8_fk_schema_name) {
      return utf8_fk_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_fk_schema_name = ToSqlChar(utf8_fk_schema_name->data());
    if (fkSchemaNameLen && fkSchemaNameLen != SQL_NTS)
      fkSchemaNameLen = utf8_fk_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_fk_table_name;
  SQLCHAR* sqlchar_fk_table_name = nullptr;
  if (fkTableName) {
    utf8_fk_table_name = ConvertSQLWCHARToString(fkTableName, fkTableNameLen);
    if (!utf8_fk_table_name) {
      return utf8_fk_table_name.GetCalculatedReturnCode();
    }
    sqlchar_fk_table_name = ToSqlChar(utf8_fk_table_name->data());
    if (fkTableNameLen && fkTableNameLen != SQL_NTS)
      fkTableNameLen = utf8_fk_table_name->length();
  }

  // Call to common internal function for SQLForeignKeys and SQLForeignKeysW
  // in odbc_driver_metadata.h.
  rc = google::cloud::odbc_bq_driver::SQLForeignKeysInternal(
      statementHandle, sqlchar_pk_category_name, pkCatalogNameLen,
      sqlchar_pk_schema_name, pkSchemaNameLen, sqlchar_pk_table_name,
      pkTableNameLen, sqlchar_fk_category_name, fkCatalogNameLen,
      sqlchar_fk_schema_name, fkSchemaNameLen, sqlchar_fk_table_name,
      fkTableNameLen);

  // Handle Unicode conversion of output parameters.

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
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to common internal function for SQLColumnPrivileges and
  // SQLColumnPrivilegesW in odbc_driver_metadata.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
////////////////////////////////////////
// Unicode version of SQLColumnPrivileges.
////////////////////////////////////////
// TODO(b/361047481): Add Integration Testcase for Unicode Support.
SQLRETURN SQL_API SQLColumnPrivilegesW(
    SQLHSTMT statementHandle, SQLWCHAR* catalogName, SQLSMALLINT catalogNameLen,
    SQLWCHAR* schemaName, SQLSMALLINT schemaNameLen, SQLWCHAR* tableName,
    SQLSMALLINT tableNameLen, SQLWCHAR* columnName, SQLSMALLINT columnNameLen) {
  SQLRETURN rc = SQL_SUCCESS;
  InitializeTracing("SQLColumnPrivilegesW");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Handle Unicode conversion of input parameters.

  StatusRecordOr<std::string> utf8_catalog_name;
  SQLCHAR* sqlchar_category_name = nullptr;
  if (catalogName) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalogName, catalogNameLen);
    if (!utf8_catalog_name) {
      return utf8_catalog_name.GetCalculatedReturnCode();
    }
    sqlchar_category_name = ToSqlChar(utf8_catalog_name->data());
    if (catalogNameLen && catalogNameLen != SQL_NTS)
      catalogNameLen = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schemaName) {
    utf8_schema_name = ConvertSQLWCHARToString(schemaName, schemaNameLen);
    if (!utf8_schema_name) {
      return utf8_schema_name.GetCalculatedReturnCode();
    }
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
    if (schemaNameLen && schemaNameLen != SQL_NTS)
      schemaNameLen = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  SQLCHAR* sqlchar_table_name = nullptr;
  if (tableName) {
    utf8_table_name = ConvertSQLWCHARToString(tableName, tableNameLen);
    if (!utf8_table_name) {
      return utf8_table_name.GetCalculatedReturnCode();
    }
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
    if (tableNameLen && tableNameLen != SQL_NTS)
      tableNameLen = utf8_table_name->length();
  }

  StatusRecordOr<std::string> utf8_col_name;
  SQLCHAR* sqlchar_column_name = nullptr;
  if (columnName) {
    utf8_col_name = ConvertSQLWCHARToString(columnName, columnNameLen);
    if (!utf8_col_name) {
      return utf8_col_name.GetCalculatedReturnCode();
    }
    sqlchar_column_name = ToSqlChar(utf8_col_name->data());
    if (columnNameLen && columnNameLen != SQL_NTS)
      columnNameLen = utf8_col_name->length();
  }

  // Call to common internal function for SQLColumnPrivileges and
  // SQLColumnPrivilegesW in odbc_driver_metadata.h.
  // Handle Unicode conversion of output parameters.

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
  InitializeTracing("SQLFreeStmt");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLFreeStmt in odbc_statement.h.
  rc = google::cloud::odbc_bq_driver::SQLFreeStmtInternal(statementHandle,
                                                          option);

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
  InitializeTracing("SQLEndTran");

  HandleLock lock(handle, handleType);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLEndTran in odbc_statement.h.
  rc = google::cloud::odbc_bq_driver::SQLEndTranInternal(handleType, handle,
                                                         completionType);

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Cancels the processing on a statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcancel-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQL_API SQLCancel(SQLHSTMT statementHandle) {
  SQLRETURN status = SQL_SUCCESS;
  InitializeTracing("SQLCancel");

  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLCancel in odbc_sql_results.h.
  status = google::cloud::odbc_bq_driver::SQLCancelInternal(statementHandle);

  return status;
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
  InitializeTracing("SQLCloseCursor");
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLCloseCursor in odbc_sql_results.h.
  rc = google::cloud::odbc_bq_driver::SQLCloseCursorInternal(statementHandle);

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
  InitializeTracing("SQLDisconnect");

  HandleLock lock(connectionHandle, SQL_HANDLE_DBC);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLCancel in odbc_connection.h.
  rc = google::cloud::odbc_bq_driver::SQLDisconnectInternal(connectionHandle);

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
  InitializeTracing("SQLFreeHandle");

  // Send lock request on the parent as the handle will be deleted
  HandleLock lock(handle, handleType, true);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to internal function for SQLFreeHandle in odbc_commons.h
  rc = google::cloud::odbc_bq_driver::SQLFreeHandleInternal(handleType, handle);

  return rc;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
// ODBC APIs supported in future driver releases.
//
////////////////////////////////////////////////////////////////////////////////////////////

#if !defined(_WIN32) || defined(_WIN64)

////////////////////////////////////////////////////////////////////////////////////////////
// Cancels the processing on a connection or statement.
//
// For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcancelhandle-function.
////////////////////////////////////////////////////////////////////////////////////////////
SQLRETURN SQLCancelHandle(SQLSMALLINT handleType, SQLHANDLE handle) {
  SQLRETURN rc = SQL_SUCCESS;
  SQLRETURN status;

  HandleLock lock(handle, handleType);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // passed in. Call to Trace function entry in odbc_trace.h if tracing is
  // enabled.

  // Call to internal function for SQLCancelHandle in odbc_environment.h

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

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
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }
  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLSetPos in odbc_sql_results.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}
#endif  //_WIN32

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
  HandleLock lock(statementHandle, SQL_HANDLE_STMT);
  if (!lock.isLocked()) {
    return SQL_INVALID_HANDLE;
  }

  // Call to Trace function entry in odbc_trace.h if tracing is enabled.

  // Call to internal function for SQLBulkOperations in odbc_sql_requests.h.

  // Call to Trace function exit in odbc_trace.h if tracing is enabled.

  return rc;
}

#ifdef _WIN32
////////////////////////////////////////////////////////////////////////////////////////////
//  adds, modifies, or deletes data sources from the system information.
// It may prompt the user for connection information. It can be in the driver
// DLL or a separate setup DLL. For more details see:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/configdsn-function?view=sql-server-ver16
////////////////////////////////////////////////////////////////////////////////////////////
BOOL SQL_API ConfigDSN(HWND hwndParent, WORD fRequest, LPCSTR lpszDriver,
                       LPCSTR lpszAttributes) {
  bool rc = TRUE;
  InitializeTracing("ConfigDSN");

  // Call to common internal function for ConfigDSN
  // in odbc_windows.h.
  rc = google::cloud::odbc_bq_driver::ConfigDSNInternal(
      hwndParent, fRequest, lpszDriver, lpszAttributes);

  return rc;
}
#endif  // _WIN32
// NOLINTEND
