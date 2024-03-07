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

#include "connection.h"

namespace google::cloud::odbc_tests {

void SetAttributes(std::shared_ptr<ConnectionHandle> conn, int timeout) {
  auto status = SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv);
  CheckError(status, "SQLAllocHandle", conn);

  status = SQLSetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION,
                         (SQLPOINTER)SQL_OV_ODBC3, 0);
  CheckError(status, "SQLSetEnvAttr", conn);

  status = SQLAllocHandle(SQL_HANDLE_DBC, conn->henv, &conn->hdbc);
  CheckError(status, "SQLAllocHandle", conn);

  // Set the application name
  status = SQLSetConnectAttr(conn->hdbc, SQL_APPLICATION_NAME,
                             (SQLPOINTER)("odbctest"), SQL_NTS);
  CheckError(status, "SQLSetConnectAttr", conn);

  status = SQLSetConnectAttr(conn->hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
  CheckError(status, "SQLSetConnectAttr", conn);

  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                             (SQLPOINTER)timeout, 0);
  CheckError(status, "SQLSetConnectAttr", conn);
}

SQLRETURN Connect(std::string conn_str, std::shared_ptr<ConnectionHandle> conn,
                  int timeout) {
  SQLSMALLINT buflen;
  SQLCHAR data_source[kBufferLength];
  SQLSMALLINT out_len;
  SQLRETURN status;

  SetAttributes(conn, timeout);

  StrToChar((char*)data_source, conn_str);

  status = SQLDriverConnect(conn->hdbc, 0, (SQLCHAR*)data_source, SQL_NTS,
                            (SQLCHAR*)conn->outdsn, NumSqlChar(conn->outdsn),
                            &buflen, SQL_DRIVER_COMPLETE);
  CheckError(status, "SQLDriverConnect", conn);
  conn->connected = true;

  PrintDriverVerName(conn);

  // Allocate statement handle
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
  CheckError(status, "SQLAllocHandle", conn);
  return status;
}

SQLRETURN ConnectDsn(std::string dsn, std::shared_ptr<ConnectionHandle> conn,
                     int timeout) {
  SQLSMALLINT buflen;
  SQLSMALLINT out_len;
  SQLRETURN status;

  SetAttributes(conn, timeout);

  status =
      SQLConnect(conn->hdbc, (SQLCHAR*)dsn.c_str(), SQL_NTS,
                 (SQLCHAR*)conn->outdsn, NumSqlChar(conn->outdsn), NULL, 0);
  CheckError(status, "SQLConnect", conn);
  conn->connected = true;

  PrintDriverVerName(conn);

  // Allocate statement handle
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
  CheckError(status, "SQLAllocHandle", conn);
  return status;
}

// Disconnect from the database
SQLRETURN Disconnect(std::shared_ptr<ConnectionHandle> conn) {
  SQLRETURN status;
  if (conn->hstmt) {
    // Not checking for error after SQLCloseCursor because it fails when no
    // cursor is open.
    SQLCloseCursor(conn->hstmt);
    status = SQLFreeHandle(SQL_HANDLE_STMT, conn->hstmt);
    CheckError(status, "SQLFreeHandle", conn);
  }
  if (conn->connected) {
    status = SQLDisconnect(conn->hdbc);
    CheckError(status, "SQLDisconnect", conn);
  }
  if (conn->hdbc) {
    status = SQLFreeHandle(SQL_HANDLE_DBC, conn->hdbc);
    CheckError(status, "SQLFreeHandle", conn);
  }
  if (conn->henv) {
    status = SQLFreeHandle(SQL_HANDLE_ENV, conn->henv);
    CheckError(status, "SQLFreeHandle", conn);
  }
  return 0;
}

// Gets Info about the driver and populates conn.metadata
SQLRETURN GetDriverInfo(std::shared_ptr<ConnectionHandle> conn) {
  SQLCHAR buf[kBufferLength];
  SQLSMALLINT out_len;
  SQLRETURN status;

  std::vector<std::tuple<SQLUSMALLINT, std::string, std::string*>> const
      kMetadataFieldsMap{
          {SQL_DATA_SOURCE_NAME, "SQL_DATA_SOURCE_NAME",
           &conn->metadata.dsn_name},
          {SQL_ODBC_VER, "SQL_ODBC_VER", &conn->metadata.db_odbc_ver},
          {SQL_DATABASE_NAME, "SQL_DATABASE_NAME", &conn->metadata.project_id},
          {SQL_DRIVER_NAME, "SQL_DRIVER_NAME", &conn->metadata.driver_name},
          {SQL_DRIVER_ODBC_VER, "SQL_DRIVER_ODBC_VER",
           &conn->metadata.driver_odbc_ver},
          {SQL_DRIVER_VER, "SQL_DRIVER_VER", &conn->metadata.driver_ver}};

  for (auto elem : kMetadataFieldsMap) {
    auto info_type = std::get<0>(elem);
    auto info_name = std::get<1>(elem);
    auto metadata_field_ptr = std::get<2>(elem);
    status = SQLGetInfo(conn->hdbc, info_type, buf, sizeof(buf), &out_len);
    CheckError(status, "SqlGetInfo(" + info_name + ")", conn);
    if (SQL_SUCCEEDED(status)) {
      if (status == SQL_SUCCESS_WITH_INFO) {
        std::runtime_error("Buffer size is not enough for " + info_name +
                           " InfoType");
      }
      std::string val = (char*)buf;
      *metadata_field_ptr = val;
    }
  }

  return status;
}

SQLRETURN SetConnectAttributeAfterConnection(
    std::shared_ptr<ConnectionHandle> conn) {
  SQLCHAR buf[kBufferLength];
  SQLSMALLINT out_len;
  SQLUINTEGER timeout = 30;
  SQLUINTEGER packet_size = 20;
  SQLINTEGER translate_dll_opt = 1;
  std::string catalog_name = "bigquery-devtools-drivers";
  std::string trace_file_name = "trace-file-test";
  std::string translate_lib_name = "translate-lib-name";
  SQLCHAR* catalog =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(catalog_name.c_str()));
  SQLCHAR* trace_file =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(trace_file_name.c_str()));
  SQLCHAR* translate_lib =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(translate_lib_name.c_str()));
  SQLRETURN status;

  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ACCESS_MODE,
                             (SQLPOINTER)SQL_MODE_READ_ONLY, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ACCESS_MODE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ACCESS_MODE");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ACCESS_MODE, Info Val: SQL_MODE_READ_ONLY"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ACCESS_MODE,
                             (SQLPOINTER)SQL_MODE_READ_WRITE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ACCESS_MODE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ACCESS_MODE");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ACCESS_MODE, Info Val: SQL_MODE_READ_WRITE"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                             (SQLPOINTER)SQL_ASYNC_ENABLE_OFF, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ASYNC_ENABLE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ASYNC_ENABLE");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ASYNC_ENABLE, Info Val: SQL_ASYNC_ENABLE_OFF"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                             (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ASYNC_ENABLE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ASYNC_ENABLE");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ASYNC_ENABLE, Info Val: SQL_ASYNC_ENABLE_ON"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_AUTOCOMMIT,
                             (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_AUTOCOMMIT");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_AUTOCOMMIT, Info Val: SQL_AUTOCOMMIT_OFF"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_AUTOCOMMIT,
                             (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_AUTOCOMMIT");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_AUTOCOMMIT, Info Val: SQL_AUTOCOMMIT_ON"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                             (SQLPOINTER)catalog, catalog_name.length());
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_CURRENT_CATALOG)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_CURRENT_CATALOG");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_CURRENT_CATALOG, Info Val:"
        << catalog << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                             (SQLPOINTER)timeout, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_CONNECTION_TIMEOUT");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_CONNECTION_TIMEOUT, Info Val:"
        << timeout << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_LOGIN_TIMEOUT,
                             (SQLPOINTER)timeout, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_LOGIN_TIMEOUT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_LOGIN_TIMEOUT");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_LOGIN_TIMEOUT, Info Val:"
        << timeout << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_METADATA_ID,
                             (SQLPOINTER)SQL_TRUE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_METADATA_ID)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_METADATA_ID");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_METADATA_ID, Info Val: SQL_TRUE"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_METADATA_ID,
                             (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_METADATA_ID)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_METADATA_ID");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_METADATA_ID, Info Val: SQL_FALSE"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_TXN_ISOLATION,
                             (SQLPOINTER)SQL_TXN_SERIALIZABLE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_TXN_ISOLATION)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_TXN_ISOLATION");
    }
    std::cout
        << "SetConnAttribute - AFTER_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_TXN_ISOLATION, Info Val: SQL_TXN_SERIALIZABLE"
        << std::endl;
  }
  return status;
}

SQLRETURN SetConnectAttributeBeforeConnection(
    std::shared_ptr<ConnectionHandle> conn) {
  SQLCHAR buf[kBufferLength];
  SQLSMALLINT out_len;
  SQLUINTEGER timeout = 30;
  SQLUINTEGER packet_size = 20;
  SQLINTEGER translate_dll_opt = 1;
  std::string catalog_name = "bigquery-devtools-drivers";
  std::string trace_file_name = "trace-file-test";
  std::string translate_lib_name = "translate-lib-name";
  SQLCHAR* catalog =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(catalog_name.c_str()));
  SQLCHAR* trace_file =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(trace_file_name.c_str()));
  SQLCHAR* translate_lib =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>(translate_lib_name.c_str()));
  SQLRETURN status;

  status = SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv);
  CheckError(status, "SQLAllocHandleWithEnv", conn);

  status = SQLSetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION,
                         (SQLPOINTER)SQL_OV_ODBC3, 0);
  CheckError(status, "SQLSetEnvAttr", conn);

  status = SQLAllocHandle(SQL_HANDLE_DBC, conn->henv, &conn->hdbc);
  CheckError(status, "SQLAllocHandleWithConn", conn);

  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ACCESS_MODE,
                             (SQLPOINTER)SQL_MODE_READ_ONLY, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ACCESS_MODE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ACCESS_MODE");
    }
    std::cout
        << "SetConnAttribute- BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ACCESS_MODE, Info Val: SQL_MODE_READ_ONLY"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ACCESS_MODE,
                             (SQLPOINTER)SQL_MODE_READ_WRITE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ACCESS_MODE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ACCESS_MODE");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ACCESS_MODE, Info Val: SQL_MODE_READ_WRITE"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                             (SQLPOINTER)SQL_ASYNC_ENABLE_OFF, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ASYNC_ENABLE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ASYNC_ENABLE");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ASYNC_ENABLE, Info Val: SQL_ASYNC_ENABLE_OFF"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE,
                             (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_ASYNC_ENABLE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_ASYNC_ENABLE");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_ASYNC_ENABLE, Info Val: SQL_ASYNC_ENABLE_ON"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_AUTOCOMMIT,
                             (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_AUTOCOMMIT");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_AUTOCOMMIT, Info Val: SQL_AUTOCOMMIT_OFF"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_AUTOCOMMIT,
                             (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_AUTOCOMMIT");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_AUTOCOMMIT, Info Val: SQL_AUTOCOMMIT_ON"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                             (SQLPOINTER)catalog, catalog_name.length());
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_CURRENT_CATALOG)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_CURRENT_CATALOG");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_CURRENT_CATALOG, Info Val:"
        << catalog << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                             (SQLPOINTER)timeout, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_CONNECTION_TIMEOUT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_CONNECTION_TIMEOUT");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_CONNECTION_TIMEOUT, Info Val:"
        << timeout << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_LOGIN_TIMEOUT,
                             (SQLPOINTER)timeout, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_LOGIN_TIMEOUT)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_LOGIN_TIMEOUT");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_LOGIN_TIMEOUT, Info Val:"
        << timeout << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_METADATA_ID,
                             (SQLPOINTER)SQL_TRUE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_METADATA_ID)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_METADATA_ID");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_METADATA_ID, Info Val: SQL_TRUE"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_METADATA_ID,
                             (SQLPOINTER)SQL_FALSE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_METADATA_ID)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_METADATA_ID");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION -  Successful for Info Name: "
           "SQL_ATTR_METADATA_ID, Info Val: SQL_FALSE"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_PACKET_SIZE,
                             (SQLPOINTER)packet_size, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_PACKET_SIZE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_PACKET_SIZE");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_PACKET_SIZE, Info Val:"
        << packet_size << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_TRACE,
                             (SQLPOINTER)SQL_OPT_TRACE_ON, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_TRACE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_TRACE");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_TRACE, Info Val: SQL_OPT_TRACE_ON"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_TRACE,
                             (SQLPOINTER)SQL_OPT_TRACE_OFF, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_TRACE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_TRACE");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_TRACE, Info Val: SQL_OPT_TRACE_OFF"
        << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_TRACEFILE,
                             (SQLPOINTER)trace_file, trace_file_name.length());
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_TRACEFILE)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_TRACEFILE");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_TRACEFILE, Info Val:"
        << trace_file << std::endl;
  }
  status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_TXN_ISOLATION,
                             (SQLPOINTER)SQL_TXN_SERIALIZABLE, 0);
  CheckError(status, "SQLSetConnectAttr(SQL_ATTR_TXN_ISOLATION)", conn);
  if (SQL_SUCCEEDED(status)) {
    if (status == SQL_SUCCESS_WITH_INFO) {
      std::runtime_error(
          "SQLSetConnectAttr returned SQL_SUCCESS_WITH_INFO for info name: "
          "SQL_ATTR_TXN_ISOLATION");
    }
    std::cout
        << "SetConnAttribute - BEFORE_CONNECTION - Successful for Info Name: "
           "SQL_ATTR_TXN_ISOLATION, Info Val: SQL_TXN_SERIALIZABLE"
        << std::endl;
  }
  return status;
}

// TODO(#10): Remove printf and support logging
// Prints if the environment is ODBC3
SQLRETURN GetEnvInfo(std::shared_ptr<ConnectionHandle> conn) {
  SQLUINTEGER buf;
  auto status = SQLGetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION,
                              (SQLPOINTER)&buf, SQL_IS_UINTEGER, NULL);
  if (SQL_SUCCEEDED(status) && buf == SQL_OV_ODBC3) {
    printf("****************************************\n");
    printf("Environment is ODBC3\n");
    printf("****************************************\n\n");
    return status;
  }
  return status;
}

// TODO(#10): Remove printf and support logging
// Print the version and the name of the connected driver
SQLRETURN PrintDriverVerName(std::shared_ptr<ConnectionHandle> conn) {
  SQLCHAR driver_info[kBufferLength];
  SQLSMALLINT out_len;
  SQLRETURN status;
  status = SQLGetInfo(conn->hdbc, SQL_DRIVER_VER, driver_info,
                      NumSqlChar(driver_info), &out_len);
  CheckError(status, "SQLGetInfo", conn);
  printf("Driver: %s", driver_info);
  status = SQLGetInfo(conn->hdbc, SQL_DRIVER_NAME, driver_info,
                      NumSqlChar(driver_info), &out_len);
  CheckError(status, "SQLGetInfo", conn);
  printf(" (%s) \n\n", driver_info);
  return status;
}

}  // namespace google::cloud::odbc_tests
