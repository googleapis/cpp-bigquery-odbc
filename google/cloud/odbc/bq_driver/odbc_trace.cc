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

#include "odbc_trace.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConvertSQLWCHARToString;
using google::cloud::odbc_bq_driver_internal::FormatSqlChar;
using google::cloud::odbc_bq_driver_internal::FormatSqlHandle;
using google::cloud::odbc_bq_driver_internal::FormatSqlHandleType;
using google::cloud::odbc_bq_driver_internal::FormatSqlInteger;
using google::cloud::odbc_bq_driver_internal::FormatSqlLen;
using google::cloud::odbc_bq_driver_internal::FormatSqlPointer;
using google::cloud::odbc_bq_driver_internal::FormatSqlSetPosiRow;
using google::cloud::odbc_bq_driver_internal::FormatSqlSmallInt;
using google::cloud::odbc_bq_driver_internal::FormatSqlULen;
using google::cloud::odbc_bq_driver_internal::FormatSqlUSmallInt;
using google::cloud::odbc_bq_driver_internal::FormatString;
using google::cloud::odbc_bq_driver_internal::ToCStr;
#ifdef _WIN32
using google::cloud::odbc_bq_driver_internal::FormatHWND;
using google::cloud::odbc_bq_driver_internal::FormatRequest;
#endif  // _WIN32
using ::google::cloud::odbc_internal::StatusRecordOr;

constexpr int kAuthBufSize = 2048;

// Following functionality still needs to be implemented for the
// entry functions.
//
// 1) Different levels of logging.
// 2) Ensure no secret data gets removed
//    from connection string if present. (e.g. secret, tokens etc)
// 3) Implement unicode functions.

void TraceFunctionEntry_SQLAllocHandle(SQLSMALLINT handle_type,
                                       SQLHANDLE input_handle,
                                       SQLHANDLE* output_handle,
                                       TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLAllocHandle_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(handle_type)),
                              ToCStr(FormatSqlHandle(input_handle)),
                              ToCStr(FormatSqlHandle(output_handle)));
    } else {
      CollectAndPrintArgs("SQLAllocHandle_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(handle_type)),
                          ToCStr(FormatSqlHandle(input_handle)),
                          ToCStr(FormatSqlHandle(output_handle)));
    }
  }
}

void TraceFunctionExit_SQLAllocHandle(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLAllocHandle_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLDriverConnect(
    SQLHDBC connection_handle, SQLHWND window_handle,
    SQLCHAR* in_connection_str, SQLSMALLINT in_connection_str_len,
    SQLCHAR* out_conn_str, SQLSMALLINT out_conn_str_buf_len,
    SQLSMALLINT* out_conn_str_len, SQLUSMALLINT driver_completion,
    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLDriverConnect_Entry", opts, 9,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)),
                              ToCStr(FormatSqlHandle(window_handle)),
                              ToCStr(FormatSqlChar(in_connection_str)),
                              ToCStr(FormatSqlSmallInt(in_connection_str_len)),
                              ToCStr(FormatSqlChar(out_conn_str)),
                              ToCStr(FormatSqlSmallInt(out_conn_str_buf_len)),
                              ToCStr(FormatSqlSmallInt(out_conn_str_len)),
                              ToCStr(FormatSqlUSmallInt(driver_completion)));
    } else {
      CollectAndPrintArgs("SQLDriverConnect_Entry", opts, 9,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)),
                          ToCStr(FormatSqlHandle(window_handle)),
                          ToCStr(FormatSqlChar(in_connection_str)),
                          ToCStr(FormatSqlSmallInt(in_connection_str_len)),
                          ToCStr(FormatSqlChar(out_conn_str)),
                          ToCStr(FormatSqlSmallInt(out_conn_str_buf_len)),
                          ToCStr(FormatSqlSmallInt(out_conn_str_len)),
                          ToCStr(FormatSqlUSmallInt(driver_completion)));
    }
  }
}

void TraceFunctionExit_SQLDriverConnect(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLDriverConnect_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLDriverConnectW(
    SQLHDBC connection_handle, SQLHWND window_handle,
    SQLWCHAR* in_connection_str, SQLSMALLINT in_connection_str_len,
    SQLWCHAR* out_conn_str, SQLSMALLINT out_conn_str_buf_len,
    SQLSMALLINT* out_conn_str_len, SQLUSMALLINT driver_completion,
    TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_in_connection_str;
  auto* out_conn_str_temp = reinterpret_cast<SQLCHAR*>(out_conn_str);
  std::wstring in_connection_wstr(
      reinterpret_cast<wchar_t const*>(in_connection_str));
  auto in_connection_wstr_len = wcslen(in_connection_wstr.data());
  if (in_connection_wstr_len > 0) {
    utf8_in_connection_str =
        ConvertSQLWCHARToString(in_connection_str, in_connection_str_len);
    if (!utf8_in_connection_str) {
      TracePrintInternal(opts,
                         utf8_in_connection_str.GetStatusRecord().message);
      return;
    }
  }

  TraceFunctionEntry_SQLDriverConnect(connection_handle, window_handle,
                                      ToSqlChar(utf8_in_connection_str->data()),
                                      in_connection_str_len, out_conn_str_temp,
                                      out_conn_str_buf_len, out_conn_str_len,
                                      driver_completion, opts);
}

void TraceFunctionExit_SQLDriverConnectW(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLDriverConnectW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLBrowseConnect(SQLHDBC connection_handle,
                                         SQLCHAR* in_conn_str,
                                         SQLSMALLINT in_conn_str_len,
                                         SQLCHAR* out_conn_str,
                                         SQLSMALLINT out_conn_str_buf_len,
                                         SQLSMALLINT* out_conn_str_len,
                                         TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLBrowseConnect_Entry", opts, 7,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)),
                              ToCStr(FormatSqlChar(in_conn_str)),
                              ToCStr(FormatSqlSmallInt(in_conn_str_len)),
                              ToCStr(FormatSqlChar(out_conn_str)),
                              ToCStr(FormatSqlSmallInt(out_conn_str_buf_len)),
                              ToCStr(FormatSqlSmallInt(out_conn_str_len)));
    } else {
      CollectAndPrintArgs("SQLBrowseConnect_Entry", opts, 7,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)),
                          ToCStr(FormatSqlChar(in_conn_str)),
                          ToCStr(FormatSqlSmallInt(in_conn_str_len)),
                          ToCStr(FormatSqlChar(out_conn_str)),
                          ToCStr(FormatSqlSmallInt(out_conn_str_buf_len)),
                          ToCStr(FormatSqlSmallInt(out_conn_str_len)));
    }
  }
}

void TraceFunctionExit_SQLBrowseConnect(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLBrowseConnect_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLBrowseConnectW(SQLHDBC connection_handle,
                                          SQLWCHAR* in_conn_str,
                                          SQLSMALLINT in_conn_str_len,
                                          SQLWCHAR* out_conn_str,
                                          SQLSMALLINT out_conn_str_buf_len,
                                          SQLSMALLINT* out_conn_str_len,
                                          TraceOptions& opts) {
  auto* out_conn_str_temp = reinterpret_cast<SQLCHAR*>(out_conn_str);
  StatusRecordOr<std::string> utf8_in_connection_str =
      ConvertSQLWCHARToString(in_conn_str, in_conn_str_len);
  if (!utf8_in_connection_str) {
    TracePrintInternal(opts, utf8_in_connection_str.GetStatusRecord().message);
    return;
  }
  in_conn_str_len = utf8_in_connection_str->length();

  TraceFunctionEntry_SQLBrowseConnect(
      connection_handle, ToSqlChar(utf8_in_connection_str->data()),
      in_conn_str_len, out_conn_str_temp, out_conn_str_buf_len,
      out_conn_str_len, opts);
}

void TraceFunctionExit_SQLBrowseConnectW(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLBrowseConnectW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLConnect(
    SQLHDBC connection_handle, SQLCHAR* server_name,
    SQLSMALLINT server_name_len, SQLCHAR* user_name, SQLSMALLINT user_name_len,
    const SQLCHAR* auth_str, SQLSMALLINT auth_str_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    // Not printing auth string.
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      if (auth_str && auth_str_len > 0) {
        CollectAndPrintArgsFile("SQLConnect_Entry", opts, 6,
                                ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                                ToCStr(FormatSqlHandle(connection_handle)),
                                ToCStr(FormatSqlChar(server_name)),
                                ToCStr(FormatSqlSmallInt(server_name_len)),
                                ToCStr(FormatSqlChar(user_name)),
                                ToCStr(FormatSqlSmallInt(user_name_len)),
                                ToCStr(FormatString("****")),
                                ToCStr(FormatSqlSmallInt(auth_str_len)));
      } else {
        CollectAndPrintArgsFile("SQLConnect_Entry", opts, 6,
                                ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                                ToCStr(FormatSqlHandle(connection_handle)),
                                ToCStr(FormatSqlChar(server_name)),
                                ToCStr(FormatSqlSmallInt(server_name_len)),
                                ToCStr(FormatSqlChar(user_name)),
                                ToCStr(FormatSqlSmallInt(user_name_len)));
      }
    } else {
      if (auth_str && auth_str_len > 0) {
        CollectAndPrintArgs("SQLConnect_Entry", opts, 6,
                            ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                            ToCStr(FormatSqlHandle(connection_handle)),
                            ToCStr(FormatSqlChar(server_name)),
                            ToCStr(FormatSqlSmallInt(server_name_len)),
                            ToCStr(FormatSqlChar(user_name)),
                            ToCStr(FormatSqlSmallInt(user_name_len)),
                            ToCStr(FormatString("****")),
                            ToCStr(FormatSqlSmallInt(auth_str_len)));
      } else {
        CollectAndPrintArgs("SQLConnect_Entry", opts, 6,
                            ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                            ToCStr(FormatSqlHandle(connection_handle)),
                            ToCStr(FormatSqlChar(server_name)),
                            ToCStr(FormatSqlSmallInt(server_name_len)),
                            ToCStr(FormatSqlChar(user_name)),
                            ToCStr(FormatSqlSmallInt(user_name_len)));
      }
    }
  }
}

void TraceFunctionExit_SQLConnect(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLConnect_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLConnectW(
    SQLHDBC connection_handle, SQLWCHAR* server_name,
    SQLSMALLINT server_name_len, SQLWCHAR* user_name, SQLSMALLINT user_name_len,
    const SQLWCHAR* auth_str, SQLSMALLINT auth_str_len, TraceOptions& opts) {
  std::string server;
  std::string user;
  std::string auth;
  std::wstring server_wstr(reinterpret_cast<wchar_t const*>(server_name));
  auto server_len = server_wstr.length();
  if (server_len > 0) {
    StatusRecordOr<std::string> utf8_server_name =
        ConvertSQLWCHARToString(server_name, server_name_len);
    if (!utf8_server_name) {
      TracePrintInternal(opts, utf8_server_name.GetStatusRecord().message);
      return;
    }
    server = *utf8_server_name;
    server_name_len = utf8_server_name->length();
  }
  std::wstring user_wstr(reinterpret_cast<wchar_t const*>(user_name));
  auto user_len = user_wstr.length();
  if (user_len > 0) {
    StatusRecordOr<std::string> utf8_user_name =
        ConvertSQLWCHARToString(user_name, user_name_len);
    if (!utf8_user_name) {
      TracePrintInternal(opts, utf8_user_name.GetStatusRecord().message);
      return;
    }
    user = *utf8_user_name;
    user_name_len = utf8_user_name->length();
  }
  SQLWCHAR auth_string[kAuthBufSize];
  for (int i = 0; i < auth_str_len; ++i) auth_string[i] = *(auth_str + i);
  if (auth_str_len > 0) {
    StatusRecordOr<std::string> utf8_auth_str =
        ConvertSQLWCHARToString(auth_string, auth_str_len);
    if (!utf8_auth_str) {
      TracePrintInternal(opts, utf8_auth_str.GetStatusRecord().message);
      return;
    }
    auth = *utf8_auth_str;
    auth_str_len = utf8_auth_str->length();
  }
  TraceFunctionEntry_SQLConnect(connection_handle, ToSqlChar(server.data()),
                                server_name_len, ToSqlChar(user.data()),
                                user_name_len, ToSqlChar(auth.data()),
                                auth_str_len, opts);
}

void TraceFunctionExit_SQLConnectW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLConnectW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetInfo(SQLHDBC connection_handle,
                                   SQLUSMALLINT info_type,
                                   SQLPOINTER info_value,
                                   SQLSMALLINT info_value_buf_len,
                                   SQLSMALLINT* info_value_str_len,
                                   TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetInfo_Entry", opts, 6,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)),
                              ToCStr(FormatSqlUSmallInt(info_type)),
                              ToCStr(FormatSqlPointer(info_value)),
                              ToCStr(FormatSqlSmallInt(info_value_buf_len)),
                              ToCStr(FormatSqlSmallInt(info_value_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetInfo_Entry", opts, 6,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)),
                          ToCStr(FormatSqlUSmallInt(info_type)),
                          ToCStr(FormatSqlPointer(info_value)),
                          ToCStr(FormatSqlSmallInt(info_value_buf_len)),
                          ToCStr(FormatSqlSmallInt(info_value_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetInfo(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetInfo_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetInfoW(SQLHDBC connection_handle,
                                    SQLUSMALLINT info_type,
                                    SQLPOINTER info_value,
                                    SQLSMALLINT info_value_buf_len,
                                    SQLSMALLINT* info_value_str_len,
                                    TraceOptions& opts) {
  TraceFunctionEntry_SQLGetInfo(connection_handle, info_type, info_value,
                                info_value_buf_len, info_value_str_len, opts);
}

void TraceFunctionExit_SQLGetInfoW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetInfoW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetFunctions(SQLHDBC connection_handle,
                                        SQLUSMALLINT fn_id,
                                        SQLUSMALLINT* supported_fn,
                                        TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetFunctions_Entry", opts, 4,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)),
                              ToCStr(FormatSqlUSmallInt(fn_id)),
                              ToCStr(FormatSqlUSmallInt(supported_fn)));
    } else {
      CollectAndPrintArgs("SQLGetFunctions_Entry", opts, 4,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)),
                          ToCStr(FormatSqlUSmallInt(fn_id)),
                          ToCStr(FormatSqlUSmallInt(supported_fn)));
    }
  }
}

void TraceFunctionExit_SQLGetFunctions(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetFunctions_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetTypeInfo(SQLHSTMT statement_handle,
                                       SQLSMALLINT data_type,
                                       TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetTypeInfo_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlSmallInt(data_type)));
    } else {
      CollectAndPrintArgs("SQLGetTypeInfo_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlSmallInt(data_type)));
    }
  }
}

void TraceFunctionExit_SQLGetTypeInfo(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetTypeInfo_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetConnectAttr(SQLHDBC connection_handle,
                                          SQLINTEGER attr, SQLPOINTER value,
                                          SQLINTEGER value_str_len,
                                          TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSetConnectAttr_Entry", opts, 5,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)),
                              ToCStr(FormatSqlInteger(attr)),
                              ToCStr(FormatSqlPointer(value)),
                              ToCStr(FormatSqlInteger(value_str_len)));
    } else {
      CollectAndPrintArgs("SQLSetConnectAttr_Entry", opts, 5,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)),
                          ToCStr(FormatSqlInteger(attr)),
                          ToCStr(FormatSqlPointer(value)),
                          ToCStr(FormatSqlInteger(value_str_len)));
    }
  }
}

void TraceFunctionExit_SQLSetConnectAttr(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLSetConnectAttr_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetConnectAttrW(SQLHDBC connection_handle,
                                           SQLINTEGER attr, SQLPOINTER value,
                                           SQLINTEGER value_str_len,
                                           TraceOptions& opts) {
  TraceFunctionEntry_SQLSetConnectAttr(connection_handle, attr, value,
                                       value_str_len, opts);
}

void TraceFunctionExit_SQLSetConnectAttrW(SQLRETURN ret_code,
                                          TraceOptions& opts) {
  ExitInternal("SQLSetConnectAttrW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetConnectAttr(SQLHDBC connection_handle,
                                          SQLINTEGER attr, SQLPOINTER value,
                                          SQLINTEGER value_buf_len,
                                          SQLINTEGER* value_str_len,
                                          TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetConnectAttr_Entry", opts, 6,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)),
                              ToCStr(FormatSqlInteger(attr)),
                              ToCStr(FormatSqlPointer(value)),
                              ToCStr(FormatSqlInteger(value_buf_len)),
                              ToCStr(FormatSqlInteger(value_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetConnectAttr_Entry", opts, 6,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)),
                          ToCStr(FormatSqlInteger(attr)),
                          ToCStr(FormatSqlPointer(value)),
                          ToCStr(FormatSqlInteger(value_buf_len)),
                          ToCStr(FormatSqlInteger(value_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetConnectAttr(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLGetConnectAttr_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetConnectAttrW(SQLHDBC connection_handle,
                                           SQLINTEGER attr, SQLPOINTER value,
                                           SQLINTEGER value_buf_len,
                                           SQLINTEGER* value_str_len,
                                           TraceOptions& opts) {
  TraceFunctionEntry_SQLGetConnectAttr(connection_handle, attr, value,
                                       value_buf_len, value_str_len, opts);
}

void TraceFunctionExit_SQLGetConnectAttrW(SQLRETURN ret_code,
                                          TraceOptions& opts) {
  ExitInternal("SQLGetConnectAttrW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetStmtAttr(SQLHSTMT statement_handle,
                                       SQLINTEGER attr, SQLPOINTER value,
                                       SQLINTEGER value_str_len,
                                       TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSetStmtAttr_Entry", opts, 5,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlInteger(attr)),
                              ToCStr(FormatSqlPointer(value)),
                              ToCStr(FormatSqlInteger(value_str_len)));
    } else {
      CollectAndPrintArgs("SQLSetStmtAttr_Entry", opts, 5,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlInteger(attr)),
                          ToCStr(FormatSqlPointer(value)),
                          ToCStr(FormatSqlInteger(value_str_len)));
    }
  }
}

void TraceFunctionExit_SQLSetStmtAttr(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLSetStmtAttr_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetStmtAttrW(SQLHSTMT statement_handle,
                                        SQLINTEGER attr, SQLPOINTER value,
                                        SQLINTEGER value_str_len,
                                        TraceOptions& opts) {
  TraceFunctionEntry_SQLSetStmtAttr(statement_handle, attr, value,
                                    value_str_len, opts);
}

void TraceFunctionExit_SQLSetStmtAttrW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLSetStmtAttrW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetStmtAttr(SQLHSTMT statement_handle,
                                       SQLINTEGER attr, SQLPOINTER value,
                                       SQLINTEGER value_buf_len,
                                       SQLINTEGER* value_str_len,
                                       TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetStmtAttr_Entry", opts, 6,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlInteger(attr)),
                              ToCStr(FormatSqlPointer(value)),
                              ToCStr(FormatSqlInteger(value_buf_len)),
                              ToCStr(FormatSqlInteger(value_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetStmtAttr_Entry", opts, 6,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlInteger(attr)),
                          ToCStr(FormatSqlPointer(value)),
                          ToCStr(FormatSqlInteger(value_buf_len)),
                          ToCStr(FormatSqlInteger(value_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetStmtAttr(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetStmtAttr_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetStmtAttrW(SQLHSTMT statement_handle,
                                        SQLINTEGER attr, SQLPOINTER value,
                                        SQLINTEGER value_buf_len,
                                        SQLINTEGER* value_str_len,
                                        TraceOptions& opts) {
  TraceFunctionEntry_SQLGetStmtAttr(statement_handle, attr, value,
                                    value_buf_len, value_str_len, opts);
}

void TraceFunctionExit_SQLGetStmtAttrW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetStmtAttrW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetEnvAttr(SQLHENV env_handle, SQLINTEGER attr,
                                      SQLPOINTER value,
                                      SQLINTEGER value_str_len,
                                      TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSetEnvAttr_Entry", opts, 5,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_ENV)),
                              ToCStr(FormatSqlHandle(env_handle)),
                              ToCStr(FormatSqlInteger(attr)),
                              ToCStr(FormatSqlPointer(value)),
                              ToCStr(FormatSqlInteger(value_str_len)));
    } else {
      CollectAndPrintArgs("SQLSetEnvAttr_Entry", opts, 5,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_ENV)),
                          ToCStr(FormatSqlHandle(env_handle)),
                          ToCStr(FormatSqlInteger(attr)),
                          ToCStr(FormatSqlPointer(value)),
                          ToCStr(FormatSqlInteger(value_str_len)));
    }
  }
}

void TraceFunctionExit_SQLSetEnvAttr(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLSetEnvAttr_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetEnvAttr(SQLHENV env_handle, SQLINTEGER attr,
                                      SQLPOINTER value,
                                      SQLINTEGER value_buf_len,
                                      SQLINTEGER* value_str_len,
                                      TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetEnvAttr_Entry", opts, 6,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_ENV)),
                              ToCStr(FormatSqlHandle(env_handle)),
                              ToCStr(FormatSqlInteger(attr)),
                              ToCStr(FormatSqlPointer(value)),
                              ToCStr(FormatSqlInteger(value_buf_len)),
                              ToCStr(FormatSqlInteger(value_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetEnvAttr_Entry", opts, 6,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_ENV)),
                          ToCStr(FormatSqlHandle(env_handle)),
                          ToCStr(FormatSqlInteger(attr)),
                          ToCStr(FormatSqlPointer(value)),
                          ToCStr(FormatSqlInteger(value_buf_len)),
                          ToCStr(FormatSqlInteger(value_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetEnvAttr(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetEnvAttr_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDescField(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_id,
    SQLPOINTER out_desc_val, SQLINTEGER out_desc_val_buf_len,
    SQLINTEGER* out_desc_val_str_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetDescField_Entry", opts, 7,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                              ToCStr(FormatSqlHandle(desc_handle)),
                              ToCStr(FormatSqlSmallInt(rec_no)),
                              ToCStr(FormatSqlSmallInt(field_id)),
                              ToCStr(FormatSqlPointer(out_desc_val)),
                              ToCStr(FormatSqlInteger(out_desc_val_buf_len)),
                              ToCStr(FormatSqlInteger(out_desc_val_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetDescField_Entry", opts, 7,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                          ToCStr(FormatSqlHandle(desc_handle)),
                          ToCStr(FormatSqlSmallInt(rec_no)),
                          ToCStr(FormatSqlSmallInt(field_id)),
                          ToCStr(FormatSqlPointer(out_desc_val)),
                          ToCStr(FormatSqlInteger(out_desc_val_buf_len)),
                          ToCStr(FormatSqlInteger(out_desc_val_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetDescField(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetDescField_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDescFieldW(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_id,
    SQLPOINTER out_desc_val, SQLINTEGER out_desc_val_buf_len,
    SQLINTEGER* out_desc_val_str_len, TraceOptions& opts) {
  TraceFunctionEntry_SQLGetDescField(desc_handle, rec_no, field_id,
                                     out_desc_val, out_desc_val_buf_len,
                                     out_desc_val_str_len, opts);
}

void TraceFunctionExit_SQLGetDescFieldW(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLGetDescFieldW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDescRec(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLCHAR* name,
    SQLSMALLINT name_buf_len, SQLSMALLINT* name_str_len, SQLSMALLINT* desc_type,
    SQLSMALLINT* desc_sub_type, SQLLEN* desc_oct_len, SQLSMALLINT* desc_prec,
    SQLSMALLINT* desc_sc, SQLSMALLINT* nullable, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetDescRec_Entry", opts, 12,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                              ToCStr(FormatSqlHandle(desc_handle)),
                              ToCStr(FormatSqlSmallInt(rec_no)),
                              ToCStr(FormatSqlChar(name)),
                              ToCStr(FormatSqlSmallInt(name_buf_len)),
                              ToCStr(FormatSqlSmallInt(name_str_len)),
                              ToCStr(FormatSqlSmallInt(desc_type)),
                              ToCStr(FormatSqlSmallInt(desc_sub_type)),
                              ToCStr(FormatSqlLen(desc_oct_len)),
                              ToCStr(FormatSqlSmallInt(desc_prec)),
                              ToCStr(FormatSqlSmallInt(desc_sc)),
                              ToCStr(FormatSqlSmallInt(nullable)));
    } else {
      CollectAndPrintArgs("SQLGetDescRec_Entry", opts, 12,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                          ToCStr(FormatSqlHandle(desc_handle)),
                          ToCStr(FormatSqlSmallInt(rec_no)),
                          ToCStr(FormatSqlChar(name)),
                          ToCStr(FormatSqlSmallInt(name_buf_len)),
                          ToCStr(FormatSqlSmallInt(name_str_len)),
                          ToCStr(FormatSqlSmallInt(desc_type)),
                          ToCStr(FormatSqlSmallInt(desc_sub_type)),
                          ToCStr(FormatSqlLen(desc_oct_len)),
                          ToCStr(FormatSqlSmallInt(desc_prec)),
                          ToCStr(FormatSqlSmallInt(desc_sc)),
                          ToCStr(FormatSqlSmallInt(nullable)));
    }
  }
}

void TraceFunctionExit_SQLGetDescRec(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetDescRec_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDescRecW(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLWCHAR* name,
    SQLSMALLINT name_buf_len, SQLSMALLINT* name_str_len, SQLSMALLINT* desc_type,
    SQLSMALLINT* desc_sub_type, SQLLEN* desc_oct_len, SQLSMALLINT* desc_prec,
    SQLSMALLINT* desc_sc, SQLSMALLINT* nullable, TraceOptions& opts) {
  auto* name_temp = reinterpret_cast<SQLCHAR*>(name);
  TraceFunctionEntry_SQLGetDescRec(
      desc_handle, rec_no, name_temp, name_buf_len, name_str_len, desc_type,
      desc_sub_type, desc_oct_len, desc_prec, desc_sc, nullable, opts);
}

void TraceFunctionExit_SQLGetDescRecW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetDescRecW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetDescField(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_identifier,
    SQLPOINTER desc_val, SQLINTEGER desc_val_buf_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSetDescField_Entry", opts, 6,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                              ToCStr(FormatSqlHandle(desc_handle)),
                              ToCStr(FormatSqlSmallInt(rec_no)),
                              ToCStr(FormatSqlSmallInt(field_identifier)),
                              ToCStr(FormatSqlPointer(desc_val)),
                              ToCStr(FormatSqlInteger(desc_val_buf_len)));
    } else {
      CollectAndPrintArgs("SQLSetDescField_Entry", opts, 6,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                          ToCStr(FormatSqlHandle(desc_handle)),
                          ToCStr(FormatSqlSmallInt(rec_no)),
                          ToCStr(FormatSqlSmallInt(field_identifier)),
                          ToCStr(FormatSqlPointer(desc_val)),
                          ToCStr(FormatSqlInteger(desc_val_buf_len)));
    }
  }
}

void TraceFunctionExit_SQLSetDescField(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLSetDescField_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetDescFieldW(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_identifier,
    SQLPOINTER desc_val, SQLINTEGER desc_val_buf_len, TraceOptions& opts) {
  TraceFunctionEntry_SQLSetDescField(desc_handle, rec_no, field_identifier,
                                     desc_val, desc_val_buf_len, opts);
}

void TraceFunctionExit_SQLSetDescFieldW(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLSetDescFieldW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetDescRec(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT desc_type,
    SQLSMALLINT desc_sub_type, SQLLEN desc_oct_len, SQLSMALLINT desc_prec,
    SQLSMALLINT desc_sc, SQLPOINTER desc_data, SQLLEN* desc_oct_len_ptr,
    SQLLEN* desc_ind, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSetDescRec_Entry", opts, 11,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                              ToCStr(FormatSqlHandle(desc_handle)),
                              ToCStr(FormatSqlSmallInt(rec_no)),
                              ToCStr(FormatSqlSmallInt(desc_type)),
                              ToCStr(FormatSqlSmallInt(desc_sub_type)),
                              ToCStr(FormatSqlLen(desc_oct_len)),
                              ToCStr(FormatSqlSmallInt(desc_prec)),
                              ToCStr(FormatSqlSmallInt(desc_sc)),
                              ToCStr(FormatSqlPointer(desc_data)),
                              ToCStr(FormatSqlLen(desc_oct_len_ptr)),
                              ToCStr(FormatSqlLen(desc_ind)));
    } else {
      CollectAndPrintArgs("SQLSetDescRec_Entry", opts, 11,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                          ToCStr(FormatSqlHandle(desc_handle)),
                          ToCStr(FormatSqlSmallInt(rec_no)),
                          ToCStr(FormatSqlSmallInt(desc_type)),
                          ToCStr(FormatSqlSmallInt(desc_sub_type)),
                          ToCStr(FormatSqlLen(desc_oct_len)),
                          ToCStr(FormatSqlSmallInt(desc_prec)),
                          ToCStr(FormatSqlSmallInt(desc_sc)),
                          ToCStr(FormatSqlPointer(desc_data)),
                          ToCStr(FormatSqlLen(desc_oct_len_ptr)),
                          ToCStr(FormatSqlLen(desc_ind)));
    }
  }
}

void TraceFunctionExit_SQLSetDescRec(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLSetDescRec_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLCopyDesc(SQLHDESC src_desc_handle,
                                    SQLHDESC target_desc_handle,
                                    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLCopyDesc_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                              ToCStr(FormatSqlHandle(src_desc_handle)),
                              ToCStr(FormatSqlHandle(target_desc_handle)));
    } else {
      CollectAndPrintArgs("SQLCopyDesc_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DESC)),
                          ToCStr(FormatSqlHandle(src_desc_handle)),
                          ToCStr(FormatSqlHandle(target_desc_handle)));
    }
  }
}

void TraceFunctionExit_SQLCopyDesc(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLCopyDesc_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLPrepare(SQLHSTMT statement_handle, SQLCHAR* stmt_txt,
                                   SQLINTEGER stmt_txt_len,
                                   TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLPrepare_Entry", opts, 4,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(stmt_txt)),
                              ToCStr(FormatSqlInteger(stmt_txt_len)));
    } else {
      CollectAndPrintArgs("SQLPrepare_Entry", opts, 4,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(stmt_txt)),
                          ToCStr(FormatSqlInteger(stmt_txt_len)));
    }
  }
}

void TraceFunctionExit_SQLPrepare(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLPrepare_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLPrepareW(SQLHSTMT statement_handle,
                                    SQLWCHAR* stmt_txt, SQLINTEGER stmt_txt_len,
                                    TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_stmt_txt;
  if (stmt_txt_len > 0 || stmt_txt_len == SQL_NTS) {
    utf8_stmt_txt = ConvertSQLWCHARToString(stmt_txt, stmt_txt_len);
    if (!utf8_stmt_txt) {
      TracePrintInternal(opts, utf8_stmt_txt.GetStatusRecord().message);
      return;
    }
  }
  TraceFunctionEntry_SQLPrepare(
      statement_handle, ToSqlChar(utf8_stmt_txt->data()), stmt_txt_len, opts);
}

void TraceFunctionExit_SQLPrepareW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLPrepareW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLBindParameter(
    SQLHSTMT statement_handle, SQLUSMALLINT param_num, SQLSMALLINT param_type,
    SQLSMALLINT param_c_type, SQLSMALLINT param_sql_type, SQLULEN param_col_sz,
    SQLSMALLINT param_scale, SQLPOINTER param_data_val,
    SQLLEN param_data_val_buf_len, SQLLEN* param_data_val_str_len,
    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLBindParameter_Entry", opts, 11,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(param_num)),
                              ToCStr(FormatSqlSmallInt(param_type)),
                              ToCStr(FormatSqlSmallInt(param_c_type)),
                              ToCStr(FormatSqlSmallInt(param_sql_type)),
                              ToCStr(FormatSqlULen(param_col_sz)),
                              ToCStr(FormatSqlSmallInt(param_scale)),
                              ToCStr(FormatSqlPointer(param_data_val)),
                              ToCStr(FormatSqlLen(param_data_val_buf_len)),
                              ToCStr(FormatSqlLen(param_data_val_str_len)));
    } else {
      CollectAndPrintArgs("SQLBindParameter_Entry", opts, 11,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(param_num)),
                          ToCStr(FormatSqlSmallInt(param_type)),
                          ToCStr(FormatSqlSmallInt(param_c_type)),
                          ToCStr(FormatSqlSmallInt(param_sql_type)),
                          ToCStr(FormatSqlULen(param_col_sz)),
                          ToCStr(FormatSqlSmallInt(param_scale)),
                          ToCStr(FormatSqlPointer(param_data_val)),
                          ToCStr(FormatSqlLen(param_data_val_buf_len)),
                          ToCStr(FormatSqlLen(param_data_val_str_len)));
    }
  }
}

void TraceFunctionExit_SQLBindParameter(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLBindParameter_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetCursorName(SQLHSTMT statement_handle,
                                         SQLCHAR* cur_name,
                                         SQLSMALLINT cur_name_buf_len,
                                         SQLSMALLINT* cur_name_str_len,
                                         TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetCursorName_Entry", opts, 5,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(cur_name)),
                              ToCStr(FormatSqlSmallInt(cur_name_buf_len)),
                              ToCStr(FormatSqlSmallInt(cur_name_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetCursorName_Entry", opts, 5,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(cur_name)),
                          ToCStr(FormatSqlSmallInt(cur_name_buf_len)),
                          ToCStr(FormatSqlSmallInt(cur_name_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetCursorName(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLGetCursorName_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetCursorNameW(SQLHSTMT statement_handle,
                                          SQLWCHAR* cur_name,
                                          SQLSMALLINT cur_name_buf_len,
                                          SQLSMALLINT* cur_name_str_len,
                                          TraceOptions& opts) {
  auto* cur_name_temp = reinterpret_cast<SQLCHAR*>(cur_name);

  TraceFunctionEntry_SQLGetCursorName(statement_handle, cur_name_temp,
                                      cur_name_buf_len, cur_name_str_len, opts);
}

void TraceFunctionExit_SQLGetCursorNameW(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLGetCursorNameW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetCursorName(SQLHSTMT statement_handle,
                                         SQLCHAR* cur_name,
                                         SQLSMALLINT cur_name_len,
                                         TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSetCursorName_Entry", opts, 4,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(cur_name)),
                              ToCStr(FormatSqlSmallInt(cur_name_len)));
    } else {
      CollectAndPrintArgs("SQLSetCursorName_Entry", opts, 4,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(cur_name)),
                          ToCStr(FormatSqlSmallInt(cur_name_len)));
    }
  }
}

void TraceFunctionExit_SQLSetCursorName(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLSetCursorName_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetCursorNameW(SQLHSTMT statement_handle,
                                          SQLWCHAR* cur_name,
                                          SQLSMALLINT cur_name_len,
                                          TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_cur_name;
  if (cur_name_len > 0 || cur_name_len == SQL_NTS) {
    utf8_cur_name = ConvertSQLWCHARToString(cur_name, cur_name_len);
    if (!utf8_cur_name) {
      TracePrintInternal(opts, utf8_cur_name.GetStatusRecord().message);
      return;
    }
    cur_name_len = utf8_cur_name->length();
  }
  TraceFunctionEntry_SQLSetCursorName(
      statement_handle, ToSqlChar(utf8_cur_name->data()), cur_name_len, opts);
}

void TraceFunctionExit_SQLSetCursorNameW(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLSetCursorNameW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLExecute(SQLHSTMT statement_handle,
                                   TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLExecute_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)));
    } else {
      CollectAndPrintArgs("SQLExecute_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)));
    }
  }
}

void TraceFunctionExit_SQLExecute(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLExecute_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLExecDirect(SQLHSTMT statement_handle,
                                      SQLCHAR* stmt_txt,
                                      SQLINTEGER stmt_txt_len,
                                      TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLExecDirect_Entry", opts, 4,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(stmt_txt)),
                              ToCStr(FormatSqlInteger(stmt_txt_len)));
    } else {
      CollectAndPrintArgs("SQLExecDirect_Entry", opts, 4,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(stmt_txt)),
                          ToCStr(FormatSqlInteger(stmt_txt_len)));
    }
  }
}

void TraceFunctionExit_SQLExecDirect(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLExecDirect_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLExecDirectW(SQLHSTMT statement_handle,
                                       SQLWCHAR* stmt_txt,
                                       SQLINTEGER stmt_txt_len,
                                       TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_stmt_txt =
      ConvertSQLWCHARToString(stmt_txt, stmt_txt_len);
  if (!utf8_stmt_txt) {
    TracePrintInternal(opts, utf8_stmt_txt.GetStatusRecord().message);
    return;
  }
  if (stmt_txt_len != SQL_NTS) stmt_txt_len = utf8_stmt_txt->length();

  TraceFunctionEntry_SQLExecDirect(
      statement_handle, ToSqlChar(utf8_stmt_txt->data()), stmt_txt_len, opts);
}

void TraceFunctionExit_SQLExecDirectW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLExecDirectW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLNativeSql(
    SQLHDBC connection_handle, SQLCHAR* in_stmt_txt, SQLINTEGER in_stmt_txt_len,
    SQLCHAR* out_stmt_txt, SQLINTEGER out_stmt_txt_buf_len,
    SQLINTEGER* out_stmt_txt_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLNativeSql_Entry", opts, 7,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)),
                              ToCStr(FormatSqlChar(in_stmt_txt)),
                              ToCStr(FormatSqlInteger(in_stmt_txt_len)),
                              ToCStr(FormatSqlChar(out_stmt_txt)),
                              ToCStr(FormatSqlInteger(out_stmt_txt_buf_len)),
                              ToCStr(FormatSqlInteger(out_stmt_txt_len)));
    } else {
      CollectAndPrintArgs("SQLNativeSql_Entry", opts, 7,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)),
                          ToCStr(FormatSqlChar(in_stmt_txt)),
                          ToCStr(FormatSqlInteger(in_stmt_txt_len)),
                          ToCStr(FormatSqlChar(out_stmt_txt)),
                          ToCStr(FormatSqlInteger(out_stmt_txt_buf_len)),
                          ToCStr(FormatSqlInteger(out_stmt_txt_len)));
    }
  }
}

void TraceFunctionExit_SQLNativeSql(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLNativeSql_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLNativeSqlW(SQLHDBC connection_handle,
                                      SQLWCHAR* in_stmt_txt,
                                      SQLINTEGER in_stmt_txt_len,
                                      SQLWCHAR* out_stmt_txt,
                                      SQLINTEGER out_stmt_txt_buf_len,
                                      SQLINTEGER* out_stmt_txt_len,
                                      TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_in_stmt_txt =
      ConvertSQLWCHARToString(in_stmt_txt, in_stmt_txt_len);
  if (!utf8_in_stmt_txt) {
    TracePrintInternal(opts, utf8_in_stmt_txt.GetStatusRecord().message);
    return;
  }
  in_stmt_txt_len = utf8_in_stmt_txt->length();
  auto* out_stmt_txt_sqlchar = reinterpret_cast<SQLCHAR*>(out_stmt_txt);

  TraceFunctionEntry_SQLNativeSql(
      connection_handle, ToSqlChar(utf8_in_stmt_txt->data()), in_stmt_txt_len,
      out_stmt_txt_sqlchar, out_stmt_txt_buf_len, out_stmt_txt_len, opts);
}

void TraceFunctionExit_SQLNativeSqlW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLNativeSqlW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLNumParams(SQLHSTMT statement_handle,
                                     SQLSMALLINT* param_count,
                                     TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLNumParams_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlSmallInt(param_count)));
    } else {
      CollectAndPrintArgs("SQLNumParams_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlSmallInt(param_count)));
    }
  }
}

void TraceFunctionExit_SQLNumParams(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLNumParams_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLParamData(SQLHSTMT statement_handle,
                                     SQLPOINTER* param_or_tgt_val,
                                     TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLParamData_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlPointer(param_or_tgt_val)));
    } else {
      CollectAndPrintArgs("SQLParamData_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlPointer(param_or_tgt_val)));
    }
  }
}

void TraceFunctionExit_SQLParamData(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLParamData_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLPutData(SQLHSTMT statement_handle,
                                   SQLPOINTER param_data, SQLLEN param_data_len,
                                   TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLPutData_Entry", opts, 4,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlPointer(param_data)),
                              ToCStr(FormatSqlLen(param_data_len)));
    } else {
      CollectAndPrintArgs("SQLPutData_Entry", opts, 4,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlPointer(param_data)),
                          ToCStr(FormatSqlLen(param_data_len)));
    }
  }
}

void TraceFunctionExit_SQLPutData(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLPutData_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLDescribeParam(
    SQLHSTMT statement_handle, SQLUSMALLINT param_num,
    SQLSMALLINT* param_sql_type, SQLULEN* param_sz, SQLSMALLINT* param_scale,
    SQLSMALLINT* param_nullable, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLDescribeParam_Entry", opts, 7,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(param_num)),
                              ToCStr(FormatSqlSmallInt(param_sql_type)),
                              ToCStr(FormatSqlULen(param_sz)),
                              ToCStr(FormatSqlSmallInt(param_scale)),
                              ToCStr(FormatSqlSmallInt(param_nullable)));
    } else {
      CollectAndPrintArgs("SQLDescribeParam_Entry", opts, 7,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(param_num)),
                          ToCStr(FormatSqlSmallInt(param_sql_type)),
                          ToCStr(FormatSqlULen(param_sz)),
                          ToCStr(FormatSqlSmallInt(param_scale)),
                          ToCStr(FormatSqlSmallInt(param_nullable)));
    }
  }
}

void TraceFunctionExit_SQLDescribeParam(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLDescribeParam_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetData(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLSMALLINT target_c_type,
    SQLPOINTER target_val, SQLLEN target_val_buf_len,
    SQLLEN* target_val_str_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetData_Entry", opts, 7,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(col_num)),
                              ToCStr(FormatSqlSmallInt(target_c_type)),
                              ToCStr(FormatSqlPointer(target_val)),
                              ToCStr(FormatSqlLen(target_val_buf_len)),
                              ToCStr(FormatSqlLen(target_val_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetData_Entry", opts, 7,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(col_num)),
                          ToCStr(FormatSqlSmallInt(target_c_type)),
                          ToCStr(FormatSqlPointer(target_val)),
                          ToCStr(FormatSqlLen(target_val_buf_len)),
                          ToCStr(FormatSqlLen(target_val_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetData(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetData_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLNumResultCols(SQLHSTMT statement_handle,
                                         SQLSMALLINT* col_count,
                                         TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLNumResultCols_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlSmallInt(col_count)));
    } else {
      CollectAndPrintArgs("SQLNumResultCols_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlSmallInt(col_count)));
    }
  }
}

void TraceFunctionExit_SQLNumResultCols(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLNumResultCols_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLFetch(SQLHSTMT statement_handle,
                                 TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLFetch_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)));
    } else {
      CollectAndPrintArgs("SQLFetch_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)));
    }
  }
}

void TraceFunctionExit_SQLFetch(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLFetch_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLExtendedFetch(SQLHSTMT statement_handle,
                                         SQLUSMALLINT fetch_orientation,
                                         SQLLEN fetch_offset,
                                         SQLULEN* row_count,
                                         SQLUSMALLINT* row_status_arr,
                                         TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLExtendedFetch_Entry", opts, 6,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(fetch_orientation)),
                              ToCStr(FormatSqlLen(fetch_offset)),
                              ToCStr(FormatSqlULen(row_count)),
                              ToCStr(FormatSqlUSmallInt(row_status_arr)));
    } else {
      CollectAndPrintArgs("SQLExtendedFetch_Entry", opts, 6,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(fetch_orientation)),
                          ToCStr(FormatSqlLen(fetch_offset)),
                          ToCStr(FormatSqlULen(row_count)),
                          ToCStr(FormatSqlUSmallInt(row_status_arr)));
    }
  }
}

void TraceFunctionExit_SQLExtendedFetch(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLExtendedFetch_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColAttribute(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLColAttribute_Entry", opts, 8,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(col_num)),
                              ToCStr(FormatSqlUSmallInt(field_identifier)),
                              ToCStr(FormatSqlPointer(char_attr)),
                              ToCStr(FormatSqlSmallInt(char_attr_buf_len)),
                              ToCStr(FormatSqlSmallInt(char_attr_str_len)),
                              ToCStr(FormatSqlLen(numeric_attr)));
    } else {
      CollectAndPrintArgs("SQLColAttribute_Entry", opts, 8,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(col_num)),
                          ToCStr(FormatSqlUSmallInt(field_identifier)),
                          ToCStr(FormatSqlPointer(char_attr)),
                          ToCStr(FormatSqlSmallInt(char_attr_buf_len)),
                          ToCStr(FormatSqlSmallInt(char_attr_str_len)),
                          ToCStr(FormatSqlLen(numeric_attr)));
    }
  }
}

void TraceFunctionExit_SQLColAttribute(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLColAttribute_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColAttributeW(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts) {
  TraceFunctionEntry_SQLColAttribute(
      statement_handle, col_num, field_identifier, char_attr, char_attr_buf_len,
      char_attr_str_len, numeric_attr, opts);
}

void TraceFunctionExit_SQLColAttributeW(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLColAttributeW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColAttributes(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLColAttributes_Entry", opts, 8,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(col_num)),
                              ToCStr(FormatSqlUSmallInt(field_identifier)),
                              ToCStr(FormatSqlPointer(char_attr)),
                              ToCStr(FormatSqlSmallInt(char_attr_buf_len)),
                              ToCStr(FormatSqlSmallInt(char_attr_str_len)),
                              ToCStr(FormatSqlLen(numeric_attr)));
    } else {
      CollectAndPrintArgs("SQLColAttributes_Entry", opts, 8,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(col_num)),
                          ToCStr(FormatSqlUSmallInt(field_identifier)),
                          ToCStr(FormatSqlPointer(char_attr)),
                          ToCStr(FormatSqlSmallInt(char_attr_buf_len)),
                          ToCStr(FormatSqlSmallInt(char_attr_str_len)),
                          ToCStr(FormatSqlLen(numeric_attr)));
    }
  }
}

void TraceFunctionExit_SQLColAttributes(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLColAttributes_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColAttributesW(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts) {
  TraceFunctionEntry_SQLColAttributes(
      statement_handle, col_num, field_identifier, char_attr, char_attr_buf_len,
      char_attr_str_len, numeric_attr, opts);
}

void TraceFunctionExit_SQLColAttributesW(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLColAttributesW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLDescribeCol(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLCHAR* col_name,
    SQLSMALLINT col_name_buf_len, SQLSMALLINT* col_name_len,
    SQLSMALLINT* col_sql_data_type, SQLULEN* col_sz, SQLSMALLINT* dec_digits,
    SQLSMALLINT* col_nullable, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile(
          "SQLDescribeCol_Entry", opts, 10,
          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
          ToCStr(FormatSqlHandle(statement_handle)),
          ToCStr(FormatSqlUSmallInt(col_num)), ToCStr(FormatSqlChar(col_name)),
          ToCStr(FormatSqlSmallInt(col_name_buf_len)),
          ToCStr(FormatSqlSmallInt(col_name_len)),
          ToCStr(FormatSqlSmallInt(col_sql_data_type)),
          ToCStr(FormatSqlULen(col_sz)), ToCStr(FormatSqlSmallInt(dec_digits)),
          ToCStr(FormatSqlSmallInt(col_nullable)));
    } else {
      CollectAndPrintArgs(
          "SQLDescribeCol_Entry", opts, 10,
          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
          ToCStr(FormatSqlHandle(statement_handle)),
          ToCStr(FormatSqlUSmallInt(col_num)), ToCStr(FormatSqlChar(col_name)),
          ToCStr(FormatSqlSmallInt(col_name_buf_len)),
          ToCStr(FormatSqlSmallInt(col_name_len)),
          ToCStr(FormatSqlSmallInt(col_sql_data_type)),
          ToCStr(FormatSqlULen(col_sz)), ToCStr(FormatSqlSmallInt(dec_digits)),
          ToCStr(FormatSqlSmallInt(col_nullable)));
    }
  }
}

void TraceFunctionExit_SQLDescribeCol(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLDescribeCol_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLDescribeColW(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLWCHAR* col_name,
    SQLSMALLINT col_name_buf_len, SQLSMALLINT* col_name_len,
    SQLSMALLINT* col_sql_data_type, SQLULEN* col_sz, SQLSMALLINT* dec_digits,
    SQLSMALLINT* col_nullable, TraceOptions& opts) {
  auto* col_name_temp = reinterpret_cast<SQLCHAR*>(col_name);
  TraceFunctionEntry_SQLDescribeCol(
      statement_handle, col_num, col_name_temp, col_name_buf_len, col_name_len,
      col_sql_data_type, col_sz, dec_digits, col_nullable, opts);
}

void TraceFunctionExit_SQLDescribeColW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLDescribeColW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLBindCol(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLSMALLINT target_c_type,
    SQLPOINTER target_val, SQLLEN target_val_buf_len,
    SQLLEN* target_val_str_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLBindCol_Entry", opts, 7,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(col_num)),
                              ToCStr(FormatSqlSmallInt(target_c_type)),
                              ToCStr(FormatSqlPointer(target_val)),
                              ToCStr(FormatSqlLen(target_val_buf_len)),
                              ToCStr(FormatSqlLen(target_val_str_len)));
    } else {
      CollectAndPrintArgs("SQLBindCol_Entry", opts, 7,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(col_num)),
                          ToCStr(FormatSqlSmallInt(target_c_type)),
                          ToCStr(FormatSqlPointer(target_val)),
                          ToCStr(FormatSqlLen(target_val_buf_len)),
                          ToCStr(FormatSqlLen(target_val_str_len)));
    }
  }
}

void TraceFunctionExit_SQLBindCol(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLBindCol_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLRowCount(SQLHSTMT statement_handle,
                                    SQLLEN* row_count, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLRowCount_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlLen(row_count)));
    } else {
      CollectAndPrintArgs("SQLRowCount_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlLen(row_count)));
    }
  }
}

void TraceFunctionExit_SQLRowCount(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLRowCount_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLFetchScroll(SQLHSTMT statement_handle,
                                       SQLSMALLINT fetch_orientation,
                                       SQLLEN fetch_offset,
                                       TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLFetchScroll_Entry", opts, 4,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlSmallInt(fetch_orientation)),
                              ToCStr(FormatSqlLen(fetch_offset)));
    } else {
      CollectAndPrintArgs("SQLFetchScroll_Entry", opts, 4,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlSmallInt(fetch_orientation)),
                          ToCStr(FormatSqlLen(fetch_offset)));
    }
  }
}

void TraceFunctionExit_SQLFetchScroll(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLFetchScroll_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLMoreResults(SQLHSTMT statement_handle,
                                       TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLMoreResults_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)));
    } else {
      CollectAndPrintArgs("SQLMoreResults_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)));
    }
  }
}

void TraceFunctionExit_SQLMoreResults(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLMoreResults_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDiagField(
    SQLSMALLINT handle_type, SQLHANDLE handle, SQLSMALLINT rec_no,
    SQLSMALLINT diag_id, SQLPOINTER diag_info, SQLSMALLINT diag_info_buf_len,
    SQLSMALLINT* diag_info_str_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLGetDiagField_Entry", opts, 7,
                              ToCStr(FormatSqlHandleType(handle_type)),
                              ToCStr(FormatSqlHandle(handle)),
                              ToCStr(FormatSqlSmallInt(rec_no)),
                              ToCStr(FormatSqlSmallInt(diag_id)),
                              ToCStr(FormatSqlPointer(diag_info)),
                              ToCStr(FormatSqlSmallInt(diag_info_buf_len)),
                              ToCStr(FormatSqlSmallInt(diag_info_str_len)));
    } else {
      CollectAndPrintArgs("SQLGetDiagField_Entry", opts, 7,
                          ToCStr(FormatSqlHandleType(handle_type)),
                          ToCStr(FormatSqlHandle(handle)),
                          ToCStr(FormatSqlSmallInt(rec_no)),
                          ToCStr(FormatSqlSmallInt(diag_id)),
                          ToCStr(FormatSqlPointer(diag_info)),
                          ToCStr(FormatSqlSmallInt(diag_info_buf_len)),
                          ToCStr(FormatSqlSmallInt(diag_info_str_len)));
    }
  }
}

void TraceFunctionExit_SQLGetDiagField(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetDiagField_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDiagFieldW(
    SQLSMALLINT handle_type, SQLHANDLE handle, SQLSMALLINT rec_no,
    SQLSMALLINT diag_id, SQLPOINTER diag_info, SQLSMALLINT diag_info_buf_len,
    SQLSMALLINT* diag_info_str_len, TraceOptions& opts) {
  TraceFunctionEntry_SQLGetDiagField(handle_type, handle, rec_no, diag_id,
                                     diag_info, diag_info_buf_len,
                                     diag_info_str_len, opts);
}

void TraceFunctionExit_SQLGetDiagFieldW(SQLRETURN ret_code,
                                        TraceOptions& opts) {
  ExitInternal("SQLGetDiagFieldW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDiagRec(SQLSMALLINT handle_type, SQLHANDLE handle,
                                      SQLSMALLINT rec_no, SQLCHAR* sql_state,
                                      SQLINTEGER* native_err, SQLCHAR* msg_txt,
                                      SQLSMALLINT msg_txt_buf_len,
                                      SQLSMALLINT* msg_txt_len,
                                      TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile(
          "SQLGetDiagRec_Entry", opts, 8,
          ToCStr(FormatSqlHandleType(handle_type)),
          ToCStr(FormatSqlHandle(handle)), ToCStr(FormatSqlSmallInt(rec_no)),
          ToCStr(FormatSqlChar(sql_state)),
          ToCStr(FormatSqlInteger(native_err)), ToCStr(FormatSqlChar(msg_txt)),
          ToCStr(FormatSqlSmallInt(msg_txt_buf_len)),
          ToCStr(FormatSqlSmallInt(msg_txt_len)));
    } else {
      CollectAndPrintArgs(
          "SQLGetDiagRec_Entry", opts, 8,
          ToCStr(FormatSqlHandleType(handle_type)),
          ToCStr(FormatSqlHandle(handle)), ToCStr(FormatSqlSmallInt(rec_no)),
          ToCStr(FormatSqlChar(sql_state)),
          ToCStr(FormatSqlInteger(native_err)), ToCStr(FormatSqlChar(msg_txt)),
          ToCStr(FormatSqlSmallInt(msg_txt_buf_len)),
          ToCStr(FormatSqlSmallInt(msg_txt_len)));
    }
  }
}

void TraceFunctionExit_SQLGetDiagRec(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetDiagRec_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLGetDiagRecW(
    SQLSMALLINT handle_type, SQLHANDLE handle, SQLSMALLINT rec_no,
    SQLWCHAR* sql_state, SQLINTEGER* native_err, SQLWCHAR* msg_txt,
    SQLSMALLINT msg_txt_buf_len, SQLSMALLINT* msg_txt_len, TraceOptions& opts) {
  auto* sql_state_sqlchar = reinterpret_cast<SQLCHAR*>(sql_state);
  auto* msg_txt_sqlchar = reinterpret_cast<SQLCHAR*>(msg_txt);
  TraceFunctionEntry_SQLGetDiagRec(
      handle_type, handle, rec_no, sql_state_sqlchar, native_err,
      msg_txt_sqlchar, msg_txt_buf_len, msg_txt_len, opts);
}

void TraceFunctionExit_SQLGetDiagRecW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLGetDiagRecW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColumns(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLColumns_Entry", opts, 10,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(table_name)),
                              ToCStr(FormatSqlSmallInt(table_name_len)),
                              ToCStr(FormatSqlChar(col_name)),
                              ToCStr(FormatSqlSmallInt(col_name_len)));
    } else {
      CollectAndPrintArgs("SQLColumns_Entry", opts, 10,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(table_name)),
                          ToCStr(FormatSqlSmallInt(table_name_len)),
                          ToCStr(FormatSqlChar(col_name)),
                          ToCStr(FormatSqlSmallInt(col_name_len)));
    }
  }
}

void TraceFunctionExit_SQLColumns(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLColumns_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColumnsW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLWCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  if (table_name_len > 0 || table_name_len == SQL_NTS) {
    utf8_table_name = ConvertSQLWCHARToString(table_name, table_name_len);
    if (!utf8_table_name) {
      TracePrintInternal(opts, utf8_table_name.GetStatusRecord().message);
      return;
    }
    table_name_len = utf8_table_name->length();
  }

  StatusRecordOr<std::string> utf8_col_name;
  if (col_name_len > 0 || col_name_len == SQL_NTS) {
    utf8_col_name = ConvertSQLWCHARToString(col_name, col_name_len);
    if (!utf8_col_name) {
      TracePrintInternal(opts, utf8_col_name.GetStatusRecord().message);
      return;
    }
    col_name_len = utf8_col_name->length();
  }

  TraceFunctionEntry_SQLColumns(
      statement_handle, ToSqlChar(utf8_catalog_name->data()), catalog_name_len,
      ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_table_name->data()), table_name_len,
      ToSqlChar(utf8_col_name->data()), col_name_len, opts);
}

void TraceFunctionExit_SQLColumnsW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLColumnsW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLTables(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLCHAR* table_type, SQLSMALLINT table_type_len,
    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLTables_Entry", opts, 10,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(table_name)),
                              ToCStr(FormatSqlSmallInt(table_name_len)),
                              ToCStr(FormatSqlChar(table_type)),
                              ToCStr(FormatSqlSmallInt(table_type_len)));
    } else {
      CollectAndPrintArgs("SQLTables_Entry", opts, 10,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(table_name)),
                          ToCStr(FormatSqlSmallInt(table_name_len)),
                          ToCStr(FormatSqlChar(table_type)),
                          ToCStr(FormatSqlSmallInt(table_type_len)));
    }
  }
}

void TraceFunctionExit_SQLTables(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLTables_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLTablesW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLWCHAR* table_type,
    SQLSMALLINT table_type_len, TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if ((catalog_name_len > 0 || catalog_name_len == SQL_NTS) &&
      catalog_name[0] != '\0') {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }
  SQLCHAR* sqlchar_category_name = nullptr;
  if (catalog_name) {
    sqlchar_category_name = ToSqlChar(utf8_catalog_name->data());
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if ((schema_name_len > 0 || schema_name_len == SQL_NTS) &&
      schema_name[0] != '\0') {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }
  SQLCHAR* sqlchar_schema_name = nullptr;
  if (schema_name) {
    sqlchar_schema_name = ToSqlChar(utf8_schema_name->data());
  }

  StatusRecordOr<std::string> utf8_table_name;
  if ((table_name_len > 0 || table_name_len == SQL_NTS) &&
      table_name[0] != '\0') {
    utf8_table_name = ConvertSQLWCHARToString(table_name, table_name_len);
    if (!utf8_table_name) {
      TracePrintInternal(opts, utf8_table_name.GetStatusRecord().message);
      return;
    }
    table_name_len = utf8_table_name->length();
  }
  SQLCHAR* sqlchar_table_name = nullptr;
  if (table_name) {
    sqlchar_table_name = ToSqlChar(utf8_table_name->data());
  }

  StatusRecordOr<std::string> utf8_table_type;
  if ((table_type_len > 0 || table_type_len == SQL_NTS) &&
      table_type[0] != '\0') {
    utf8_table_type = ConvertSQLWCHARToString(table_type, table_type_len);
    if (!utf8_table_type) {
      TracePrintInternal(opts, utf8_table_type.GetStatusRecord().message);
      return;
    }
    table_type_len = utf8_table_type->length();
  }
  SQLCHAR* sqlchar_table_type = nullptr;
  if (table_type) {
    sqlchar_table_type = ToSqlChar(utf8_table_type->data());
  }

  TraceFunctionEntry_SQLTables(
      statement_handle, sqlchar_category_name, catalog_name_len,
      sqlchar_schema_name, schema_name_len, sqlchar_table_name, table_name_len,
      sqlchar_table_type, table_type_len, opts);
}

void TraceFunctionExit_SQLTablesW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLTablesW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLPrimaryKeys(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLPrimaryKeys_Entry", opts, 8,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(table_name)),
                              ToCStr(FormatSqlSmallInt(table_name_len)));
    } else {
      CollectAndPrintArgs("SQLPrimaryKeys_Entry", opts, 8,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(table_name)),
                          ToCStr(FormatSqlSmallInt(table_name_len)));
    }
  }
}

void TraceFunctionExit_SQLPrimaryKeys(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLPrimaryKeys_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLPrimaryKeysW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  if (table_name_len > 0 || table_name_len == SQL_NTS) {
    utf8_table_name = ConvertSQLWCHARToString(table_name, table_name_len);
    if (!utf8_table_name) {
      TracePrintInternal(opts, utf8_table_name.GetStatusRecord().message);
      return;
    }
    table_name_len = utf8_table_name->length();
  }

  TraceFunctionEntry_SQLPrimaryKeys(
      statement_handle, ToSqlChar(utf8_catalog_name->data()), catalog_name_len,
      ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_table_name->data()), table_name_len, opts);
}

void TraceFunctionExit_SQLPrimaryKeysW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLPrimaryKeysW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLProcedureColumns(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* proc_name, SQLSMALLINT proc_name_len,
    SQLCHAR* col_name, SQLSMALLINT col_name_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLProcedureColumns_Entry", opts, 10,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(proc_name)),
                              ToCStr(FormatSqlSmallInt(proc_name_len)),
                              ToCStr(FormatSqlChar(col_name)),
                              ToCStr(FormatSqlSmallInt(col_name_len)));
    } else {
      CollectAndPrintArgs("SQLProcedureColumns_Entry", opts, 10,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(proc_name)),
                          ToCStr(FormatSqlSmallInt(proc_name_len)),
                          ToCStr(FormatSqlChar(col_name)),
                          ToCStr(FormatSqlSmallInt(col_name_len)));
    }
  }
}

void TraceFunctionExit_SQLProcedureColumns(SQLRETURN ret_code,
                                           TraceOptions& opts) {
  ExitInternal("SQLProcedureColumns_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLProcedureColumnsW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* proc_name, SQLSMALLINT proc_name_len,
    SQLWCHAR* col_name, SQLSMALLINT col_name_len, TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_proc_name;
  if (proc_name_len > 0 || proc_name_len == SQL_NTS) {
    utf8_proc_name = ConvertSQLWCHARToString(proc_name, proc_name_len);
    if (!utf8_proc_name) {
      TracePrintInternal(opts, utf8_proc_name.GetStatusRecord().message);
      return;
    }
    proc_name_len = utf8_proc_name->length();
  }

  StatusRecordOr<std::string> utf8_col_name;
  if (col_name_len > 0 || col_name_len == SQL_NTS) {
    utf8_col_name = ConvertSQLWCHARToString(col_name, col_name_len);
    if (!utf8_col_name) {
      TracePrintInternal(opts, utf8_col_name.GetStatusRecord().message);
      return;
    }
    col_name_len = utf8_col_name->length();
  }

  TraceFunctionEntry_SQLProcedureColumns(
      statement_handle, ToSqlChar(utf8_catalog_name->data()), catalog_name_len,
      ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_proc_name->data()), proc_name_len,
      ToSqlChar(utf8_col_name->data()), col_name_len, opts);
}

void TraceFunctionExit_SQLProcedureColumnsW(SQLRETURN ret_code,
                                            TraceOptions& opts) {
  ExitInternal("SQLProcedureColumnsW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLProcedures(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* proc_name, SQLSMALLINT proc_name_len,
    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLProcedures_Entry", opts, 8,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(proc_name)),
                              ToCStr(FormatSqlSmallInt(proc_name_len)));
    } else {
      CollectAndPrintArgs("SQLProcedures_Entry", opts, 8,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(proc_name)),
                          ToCStr(FormatSqlSmallInt(proc_name_len)));
    }
  }
}

void TraceFunctionExit_SQLProcedures(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLProcedures_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLProceduresW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* proc_name, SQLSMALLINT proc_name_len,
    TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_proc_name;
  if (proc_name_len > 0 || proc_name_len == SQL_NTS) {
    utf8_proc_name = ConvertSQLWCHARToString(proc_name, proc_name_len);
    if (!utf8_proc_name) {
      TracePrintInternal(opts, utf8_proc_name.GetStatusRecord().message);
      return;
    }
    proc_name_len = utf8_proc_name->length();
  }

  TraceFunctionEntry_SQLProcedures(
      statement_handle, ToSqlChar(utf8_catalog_name->data()), catalog_name_len,
      ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_proc_name->data()), proc_name_len, opts);
}

void TraceFunctionExit_SQLProceduresW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLProceduresW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSpecialColumns(
    SQLHSTMT statement_handle, SQLUSMALLINT id_type, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT min_rowid_scope,
    SQLUSMALLINT col_nullable, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSpecialColumns_Entry", opts, 11,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(id_type)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(table_name)),
                              ToCStr(FormatSqlSmallInt(table_name_len)),
                              ToCStr(FormatSqlUSmallInt(min_rowid_scope)),
                              ToCStr(FormatSqlUSmallInt(col_nullable)));
    } else {
      CollectAndPrintArgs("SQLSpecialColumns_Entry", opts, 11,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(id_type)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(table_name)),
                          ToCStr(FormatSqlSmallInt(table_name_len)),
                          ToCStr(FormatSqlUSmallInt(min_rowid_scope)),
                          ToCStr(FormatSqlUSmallInt(col_nullable)));
    }
  }
}

void TraceFunctionExit_SQLSpecialColumns(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLSpecialColumns_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSpecialColumnsW(
    SQLHSTMT statement_handle, SQLUSMALLINT id_type, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT min_rowid_scope,
    SQLUSMALLINT col_nullable, TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  if (table_name_len > 0 || table_name_len == SQL_NTS) {
    utf8_table_name = ConvertSQLWCHARToString(table_name, table_name_len);
    if (!utf8_table_name) {
      TracePrintInternal(opts, utf8_table_name.GetStatusRecord().message);
      return;
    }
    table_name_len = utf8_table_name->length();
  }

  TraceFunctionEntry_SQLSpecialColumns(
      statement_handle, id_type, ToSqlChar(utf8_catalog_name->data()),
      catalog_name_len, ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_table_name->data()), table_name_len, min_rowid_scope,
      col_nullable, opts);
}

void TraceFunctionExit_SQLSpecialColumnsW(SQLRETURN ret_code,
                                          TraceOptions& opts) {
  ExitInternal("SQLSpecialColumnsW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLStatistics(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT index_type, SQLUSMALLINT reserved,
    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLStatistics_Entry", opts, 10,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(table_name)),
                              ToCStr(FormatSqlSmallInt(table_name_len)),
                              ToCStr(FormatSqlUSmallInt(index_type)),
                              ToCStr(FormatSqlUSmallInt(reserved)));
    } else {
      CollectAndPrintArgs("SQLStatistics_Entry", opts, 10,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(table_name)),
                          ToCStr(FormatSqlSmallInt(table_name_len)),
                          ToCStr(FormatSqlUSmallInt(index_type)),
                          ToCStr(FormatSqlUSmallInt(reserved)));
    }
  }
}

void TraceFunctionExit_SQLStatistics(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLStatistics_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLStatisticsW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT index_type, SQLUSMALLINT reserved,
    TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  if (table_name_len > 0 || table_name_len == SQL_NTS) {
    utf8_table_name = ConvertSQLWCHARToString(table_name, table_name_len);
    if (!utf8_table_name) {
      TracePrintInternal(opts, utf8_table_name.GetStatusRecord().message);
      return;
    }
    table_name_len = utf8_table_name->length();
  }

  TraceFunctionEntry_SQLStatistics(
      statement_handle, ToSqlChar(utf8_catalog_name->data()), catalog_name_len,
      ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_table_name->data()), table_name_len, index_type, reserved,
      opts);
}

void TraceFunctionExit_SQLStatisticsW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLStatisticsW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLTablePrivileges(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLTablePrivileges_Entry", opts, 8,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(table_name)),
                              ToCStr(FormatSqlSmallInt(table_name_len)));
    } else {
      CollectAndPrintArgs("SQLTablePrivileges_Entry", opts, 8,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(table_name)),
                          ToCStr(FormatSqlSmallInt(table_name_len)));
    }
  }
}

void TraceFunctionExit_SQLTablePrivileges(SQLRETURN ret_code,
                                          TraceOptions& opts) {
  ExitInternal("SQLTablePrivileges_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLTablePrivilegesW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  if (table_name_len > 0 || table_name_len == SQL_NTS) {
    utf8_table_name = ConvertSQLWCHARToString(table_name, table_name_len);
    if (!utf8_table_name) {
      TracePrintInternal(opts, utf8_table_name.GetStatusRecord().message);
      return;
    }
    table_name_len = utf8_table_name->length();
  }

  TraceFunctionEntry_SQLTablePrivileges(
      statement_handle, ToSqlChar(utf8_catalog_name->data()), catalog_name_len,
      ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_table_name->data()), table_name_len, opts);
}

void TraceFunctionExit_SQLTablePrivilegesW(SQLRETURN ret_code,
                                           TraceOptions& opts) {
  ExitInternal("SQLTablePrivilegesW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLForeignKeys(
    SQLHSTMT statement_handle, SQLCHAR* pk_catalog_name,
    SQLSMALLINT pk_catalog_name_len, SQLCHAR* pk_schema_name,
    SQLSMALLINT pk_schema_name_len, SQLCHAR* pk_table_name,
    SQLSMALLINT pk_table_name_len, SQLCHAR* fk_catalog_name,
    SQLSMALLINT fk_catalog_name_len, SQLCHAR* fk_schema_name,
    SQLSMALLINT fk_schema_name_len, SQLCHAR* fk_table_name,
    SQLSMALLINT fk_table_name_len, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLForeignKeys_Entry", opts, 14,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(pk_catalog_name)),
                              ToCStr(FormatSqlSmallInt(pk_catalog_name_len)),
                              ToCStr(FormatSqlChar(pk_schema_name)),
                              ToCStr(FormatSqlSmallInt(pk_schema_name_len)),
                              ToCStr(FormatSqlChar(pk_table_name)),
                              ToCStr(FormatSqlSmallInt(pk_table_name_len)),
                              ToCStr(FormatSqlChar(fk_catalog_name)),
                              ToCStr(FormatSqlSmallInt(fk_catalog_name_len)),
                              ToCStr(FormatSqlChar(fk_schema_name)),
                              ToCStr(FormatSqlSmallInt(fk_schema_name_len)),
                              ToCStr(FormatSqlChar(fk_table_name)),
                              ToCStr(FormatSqlSmallInt(fk_table_name_len)));
    } else {
      CollectAndPrintArgs("SQLForeignKeys_Entry", opts, 14,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(pk_catalog_name)),
                          ToCStr(FormatSqlSmallInt(pk_catalog_name_len)),
                          ToCStr(FormatSqlChar(pk_schema_name)),
                          ToCStr(FormatSqlSmallInt(pk_schema_name_len)),
                          ToCStr(FormatSqlChar(pk_table_name)),
                          ToCStr(FormatSqlSmallInt(pk_table_name_len)),
                          ToCStr(FormatSqlChar(fk_catalog_name)),
                          ToCStr(FormatSqlSmallInt(fk_catalog_name_len)),
                          ToCStr(FormatSqlChar(fk_schema_name)),
                          ToCStr(FormatSqlSmallInt(fk_schema_name_len)),
                          ToCStr(FormatSqlChar(fk_table_name)),
                          ToCStr(FormatSqlSmallInt(fk_table_name_len)));
    }
  }
}

void TraceFunctionExit_SQLForeignKeys(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLForeignKeys_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLForeignKeysW(
    SQLHSTMT statement_handle, SQLWCHAR* pk_catalog_name,
    SQLSMALLINT pk_catalog_name_len, SQLWCHAR* pk_schema_name,
    SQLSMALLINT pk_schema_name_len, SQLWCHAR* pk_table_name,
    SQLSMALLINT pk_table_name_len, SQLWCHAR* fk_catalog_name,
    SQLSMALLINT fk_catalog_name_len, SQLWCHAR* fk_schema_name,
    SQLSMALLINT fk_schema_name_len, SQLWCHAR* fk_table_name,
    SQLSMALLINT fk_table_name_len, TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_pk_catalog_name;
  if ((pk_catalog_name_len > 0 || pk_catalog_name_len == SQL_NTS) &&
      pk_catalog_name[0] != '\0') {
    utf8_pk_catalog_name =
        ConvertSQLWCHARToString(pk_catalog_name, pk_catalog_name_len);
    if (!utf8_pk_catalog_name) {
      TracePrintInternal(opts, utf8_pk_catalog_name.GetStatusRecord().message);
      return;
    }
    pk_catalog_name_len = utf8_pk_catalog_name->length();
  }
  SQLCHAR* sqlchar_pk_category_name = nullptr;
  if (pk_catalog_name) {
    sqlchar_pk_category_name = ToSqlChar(utf8_pk_catalog_name->data());
  }

  StatusRecordOr<std::string> utf8_pk_schema_name;
  if ((pk_schema_name_len > 0 || pk_schema_name_len == SQL_NTS) &&
      pk_schema_name[0] != '\0') {
    utf8_pk_schema_name =
        ConvertSQLWCHARToString(pk_schema_name, pk_schema_name_len);
    if (!utf8_pk_schema_name) {
      TracePrintInternal(opts, utf8_pk_schema_name.GetStatusRecord().message);
      return;
    }
    pk_schema_name_len = utf8_pk_schema_name->length();
  }
  SQLCHAR* sqlchar_pk_schema_name = nullptr;
  if (pk_schema_name) {
    sqlchar_pk_schema_name = ToSqlChar(utf8_pk_schema_name->data());
  }

  StatusRecordOr<std::string> utf8_pk_table_name;
  if ((pk_table_name_len > 0 || pk_table_name_len == SQL_NTS) &&
      pk_table_name[0] != '\0') {
    utf8_pk_table_name =
        ConvertSQLWCHARToString(pk_table_name, pk_table_name_len);
    if (!utf8_pk_table_name) {
      TracePrintInternal(opts, utf8_pk_table_name.GetStatusRecord().message);
      return;
    }
    pk_table_name_len = utf8_pk_table_name->length();
  }
  SQLCHAR* sqlchar_pk_table_name = nullptr;
  if (pk_table_name) {
    sqlchar_pk_table_name = ToSqlChar(utf8_pk_table_name->data());
  }

  StatusRecordOr<std::string> utf8_fk_catalog_name;
  if ((fk_catalog_name_len > 0 || fk_catalog_name_len == SQL_NTS) &&
      fk_catalog_name[0] != '\0') {
    utf8_fk_catalog_name =
        ConvertSQLWCHARToString(fk_catalog_name, fk_catalog_name_len);
    if (!utf8_fk_catalog_name) {
      TracePrintInternal(opts, utf8_fk_catalog_name.GetStatusRecord().message);
      return;
    }
    fk_catalog_name_len = utf8_fk_catalog_name->length();
  }
  SQLCHAR* sqlchar_fk_category_name = nullptr;
  if (fk_catalog_name) {
    sqlchar_fk_category_name = ToSqlChar(utf8_fk_catalog_name->data());
  }

  StatusRecordOr<std::string> utf8_fk_schema_name;
  if ((fk_schema_name_len > 0 || fk_schema_name_len == SQL_NTS) &&
      fk_schema_name[0] != '\0') {
    utf8_fk_schema_name =
        ConvertSQLWCHARToString(fk_schema_name, fk_schema_name_len);
    if (!utf8_fk_schema_name) {
      TracePrintInternal(opts, utf8_fk_schema_name.GetStatusRecord().message);
      return;
    }
    fk_schema_name_len = utf8_fk_schema_name->length();
  }
  SQLCHAR* sqlchar_fk_schema_name = nullptr;
  if (fk_schema_name) {
    sqlchar_fk_schema_name = ToSqlChar(utf8_fk_schema_name->data());
  }

  StatusRecordOr<std::string> utf8_fk_table_name;
  if ((fk_table_name_len > 0 || fk_table_name_len == SQL_NTS) &&
      fk_table_name[0] != '\0') {
    utf8_fk_table_name =
        ConvertSQLWCHARToString(fk_table_name, fk_table_name_len);
    if (!utf8_fk_table_name) {
      TracePrintInternal(opts, utf8_fk_table_name.GetStatusRecord().message);
      return;
    }
    fk_table_name_len = utf8_fk_table_name->length();
  }
  SQLCHAR* sqlchar_fk_table_name = nullptr;
  if (fk_table_name) {
    sqlchar_fk_table_name = ToSqlChar(utf8_fk_table_name->data());
  }

  TraceFunctionEntry_SQLForeignKeys(
      statement_handle, sqlchar_pk_category_name, pk_catalog_name_len,
      sqlchar_pk_schema_name, pk_schema_name_len, sqlchar_pk_table_name,
      pk_table_name_len, sqlchar_fk_category_name, fk_catalog_name_len,
      sqlchar_fk_schema_name, fk_schema_name_len, sqlchar_fk_table_name,
      fk_table_name_len, opts);
}

void TraceFunctionExit_SQLForeignKeysW(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLForeignKeysW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColumnPrivileges(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLColumnPrivileges_Entry", opts, 10,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlChar(catalog_name)),
                              ToCStr(FormatSqlSmallInt(catalog_name_len)),
                              ToCStr(FormatSqlChar(schema_name)),
                              ToCStr(FormatSqlSmallInt(schema_name_len)),
                              ToCStr(FormatSqlChar(table_name)),
                              ToCStr(FormatSqlSmallInt(table_name_len)),
                              ToCStr(FormatSqlChar(col_name)),
                              ToCStr(FormatSqlSmallInt(col_name_len)));
    } else {
      CollectAndPrintArgs("SQLColumnPrivileges_Entry", opts, 10,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlChar(catalog_name)),
                          ToCStr(FormatSqlSmallInt(catalog_name_len)),
                          ToCStr(FormatSqlChar(schema_name)),
                          ToCStr(FormatSqlSmallInt(schema_name_len)),
                          ToCStr(FormatSqlChar(table_name)),
                          ToCStr(FormatSqlSmallInt(table_name_len)),
                          ToCStr(FormatSqlChar(col_name)),
                          ToCStr(FormatSqlSmallInt(col_name_len)));
    }
  }
}

void TraceFunctionExit_SQLColumnPrivileges(SQLRETURN ret_code,
                                           TraceOptions& opts) {
  ExitInternal("SQLColumnPrivileges_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLColumnPrivilegesW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLWCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts) {
  StatusRecordOr<std::string> utf8_catalog_name;
  if (catalog_name_len > 0 || catalog_name_len == SQL_NTS) {
    utf8_catalog_name = ConvertSQLWCHARToString(catalog_name, catalog_name_len);
    if (!utf8_catalog_name) {
      TracePrintInternal(opts, utf8_catalog_name.GetStatusRecord().message);
      return;
    }
    catalog_name_len = utf8_catalog_name->length();
  }

  StatusRecordOr<std::string> utf8_schema_name;
  if (schema_name_len > 0 || schema_name_len == SQL_NTS) {
    utf8_schema_name = ConvertSQLWCHARToString(schema_name, schema_name_len);
    if (!utf8_schema_name) {
      TracePrintInternal(opts, utf8_schema_name.GetStatusRecord().message);
      return;
    }
    schema_name_len = utf8_schema_name->length();
  }

  StatusRecordOr<std::string> utf8_table_name;
  if (table_name_len > 0 || table_name_len == SQL_NTS) {
    utf8_table_name = ConvertSQLWCHARToString(table_name, table_name_len);
    if (!utf8_table_name) {
      TracePrintInternal(opts, utf8_table_name.GetStatusRecord().message);
      return;
    }
    table_name_len = utf8_table_name->length();
  }

  StatusRecordOr<std::string> utf8_col_name;
  if (col_name_len > 0 || col_name_len == SQL_NTS) {
    utf8_col_name = ConvertSQLWCHARToString(col_name, col_name_len);
    if (!utf8_col_name) {
      TracePrintInternal(opts, utf8_col_name.GetStatusRecord().message);
      return;
    }
    col_name_len = utf8_col_name->length();
  }

  TraceFunctionEntry_SQLColumnPrivileges(
      statement_handle, ToSqlChar(utf8_catalog_name->data()), catalog_name_len,
      ToSqlChar(utf8_schema_name->data()), schema_name_len,
      ToSqlChar(utf8_table_name->data()), table_name_len,
      ToSqlChar(utf8_col_name->data()), col_name_len, opts);
}

void TraceFunctionExit_SQLColumnPrivilegesW(SQLRETURN ret_code,
                                            TraceOptions& opts) {
  ExitInternal("SQLColumnPrivilegesW_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLFreeStmt(SQLHSTMT statement_handle,
                                    SQLUSMALLINT option, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLFreeStmt_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(option)));
    } else {
      CollectAndPrintArgs("SQLFreeStmt_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(option)));
    }
  }
}

void TraceFunctionExit_SQLFreeStmt(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLFreeStmt_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLEndTran(SQLSMALLINT handle_type, SQLHANDLE handle,
                                   SQLSMALLINT completion_type,
                                   TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLEndTran_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(handle_type)),
                              ToCStr(FormatSqlHandle(handle)),
                              ToCStr(FormatSqlSmallInt(completion_type)));
    } else {
      CollectAndPrintArgs("SQLEndTran_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(handle_type)),
                          ToCStr(FormatSqlHandle(handle)),
                          ToCStr(FormatSqlSmallInt(completion_type)));
    }
  }
}

void TraceFunctionExit_SQLEndTran(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLEndTran_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLCancel(SQLHSTMT statement_handle,
                                  TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLCancel_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)));
    } else {
      CollectAndPrintArgs("SQLCancel_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)));
    }
  }
}

void TraceFunctionExit_SQLCancel(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLCancel_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLCloseCursor(SQLHSTMT statement_handle,
                                       TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLCloseCursor_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)));
    } else {
      CollectAndPrintArgs("SQLCloseCursor_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)));
    }
  }
}

void TraceFunctionExit_SQLCloseCursor(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLCloseCursor_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLDisconnect(SQLHDBC connection_handle,
                                      TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLDisconnect_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                              ToCStr(FormatSqlHandle(connection_handle)));
    } else {
      CollectAndPrintArgs("SQLDisconnect_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_DBC)),
                          ToCStr(FormatSqlHandle(connection_handle)));
    }
  }
}

void TraceFunctionExit_SQLDisconnect(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLDisconnect_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLFreeHandle(SQLSMALLINT handle_type, SQLHANDLE handle,
                                      TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLFreeHandle_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(handle_type)),
                              ToCStr(FormatSqlHandle(handle)));
    } else {
      CollectAndPrintArgs("SQLFreeHandle_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(handle_type)),
                          ToCStr(FormatSqlHandle(handle)));
    }
  }
}

void TraceFunctionExit_SQLFreeHandle(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLFreeHandle_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLCancelHandle(SQLSMALLINT handle_type,
                                        SQLHANDLE handle, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLCancelHandle_Entry", opts, 2,
                              ToCStr(FormatSqlHandleType(handle_type)),
                              ToCStr(FormatSqlHandle(handle)));
    } else {
      CollectAndPrintArgs("SQLCancelHandle_Entry", opts, 2,
                          ToCStr(FormatSqlHandleType(handle_type)),
                          ToCStr(FormatSqlHandle(handle)));
    }
  }
}

void TraceFunctionExit_SQLCancelHandle(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLCancelHandle_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLSetPos(SQLHSTMT statement_handle,
                                  SQLSETPOSIROW row_number, SQLUSMALLINT op,
                                  SQLUSMALLINT lock_type, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLSetPos_Entry", opts, 5,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlSetPosiRow(row_number)),
                              ToCStr(FormatSqlUSmallInt(op)),
                              ToCStr(FormatSqlUSmallInt(lock_type)));
    } else {
      CollectAndPrintArgs("SQLSetPos_Entry", opts, 5,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlSetPosiRow(row_number)),
                          ToCStr(FormatSqlUSmallInt(op)),
                          ToCStr(FormatSqlUSmallInt(lock_type)));
    }
  }
}

void TraceFunctionExit_SQLSetPos(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("SQLSetPos_Exit", ret_code, opts);
}

void TraceFunctionEntry_SQLBulkOperations(SQLHSTMT statement_handle,
                                          SQLSMALLINT op, TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("SQLBulkOperations_Entry", opts, 3,
                              ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                              ToCStr(FormatSqlHandle(statement_handle)),
                              ToCStr(FormatSqlUSmallInt(op)));
    } else {
      CollectAndPrintArgs("SQLBulkOperations_Entry", opts, 3,
                          ToCStr(FormatSqlHandleType(SQL_HANDLE_STMT)),
                          ToCStr(FormatSqlHandle(statement_handle)),
                          ToCStr(FormatSqlUSmallInt(op)));
    }
  }
}

void TraceFunctionExit_SQLBulkOperations(SQLRETURN ret_code,
                                         TraceOptions& opts) {
  ExitInternal("SQLBulkOperations_Exit", ret_code, opts);
}
#ifdef _WIN32
void TraceFunctionEntry_ConfigDSN(HWND hwndParent, WORD fRequest,
                                  LPCSTR lpszDriver, LPCSTR lpszAttributes,
                                  TraceOptions& opts) {
  if (opts.logging_enabled) {
    if (opts.is_file_closed) {
      opts.trace_file.open(opts.log_file,
                           std::ofstream::out | std::ofstream::app);
      opts.is_file_closed = false;
    }
    if (opts.trace_file.is_open()) {
      CollectAndPrintArgsFile("ConfigDSN_Entry", opts, 4,
                              ToCStr(FormatHWND(hwndParent)),
                              ToCStr(FormatRequest(fRequest)),
                              ToCStr(lpszDriver), ToCStr(lpszAttributes));
    } else {
      CollectAndPrintArgs("ConfigDSN_Entry", opts, 4,
                          ToCStr(FormatHWND(hwndParent)),
                          ToCStr(FormatRequest(fRequest)), ToCStr(lpszDriver),
                          ToCStr(lpszAttributes));
    }
  }
}

void TraceFunctionExit_ConfigDSN(SQLRETURN ret_code, TraceOptions& opts) {
  ExitInternal("ConfigDSN_Exit", ret_code, opts);
}
#endif  // _WIN32

}  // namespace google::cloud::odbc_bq_driver
