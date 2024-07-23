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
#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_tests {

void SetAttributes(std::shared_ptr<ODBCHandles> conn, int timeout,
                   bool use_ansi = false) {
  auto status = SQLAllocHandle(SQL_HANDLE_ENV, NULL, &conn->henv);
  CheckError(status, "SQLAllocHandle", conn);

  status = SQLSetEnvAttr(conn->henv, SQL_ATTR_ODBC_VERSION,
                         (SQLPOINTER)SQL_OV_ODBC3, 0);
  CheckError(status, "SQLSetEnvAttr", conn);

  status = SQLAllocHandle(SQL_HANDLE_DBC, conn->henv, &conn->hdbc);
  CheckError(status, "SQLAllocHandle", conn);

  if (use_ansi) {
    status = SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_LOGIN_TIMEOUT,
                                (SQLPOINTER)10, 0);
    CheckError(status, "SQLSetConnectAttr", conn, use_ansi);

    status = SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                                (SQLPOINTER)timeout, 0);
    CheckError(status, "SQLSetConnectAttr", conn, use_ansi);
  } else {
    status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_LOGIN_TIMEOUT,
                               (SQLPOINTER)10, 0);
    CheckError(status, "SQLSetConnectAttr", conn, use_ansi);

    status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                               (SQLPOINTER)timeout, 0);
    CheckError(status, "SQLSetConnectAttr", conn, use_ansi);
  }
}

SQLRETURN Connect(std::string conn_str, std::shared_ptr<ODBCHandles> conn,
                  int timeout, bool use_ansi) {
  SQLSMALLINT buflen;
  SQLCHAR data_source[kBufferLength];
  SQLSMALLINT out_len;
  SQLRETURN status;

  SetAttributes(conn, timeout, use_ansi);

  StrToChar((char*)data_source, conn_str);

  if (use_ansi) {
    status = SQLDriverConnectA(conn->hdbc, 0, (SQLCHAR*)data_source, SQL_NTS,
                               (SQLCHAR*)conn->outdsn, NumSqlChar(conn->outdsn),
                               &buflen, SQL_DRIVER_COMPLETE);
  } else {
    status = SQLDriverConnect(conn->hdbc, 0, (SQLCHAR*)data_source, SQL_NTS,
                              (SQLCHAR*)conn->outdsn, NumSqlChar(conn->outdsn),
                              &buflen, SQL_DRIVER_COMPLETE);
  }
  CheckError(status, "SQLDriverConnect", conn, use_ansi);

  conn->connected = true;

  PrintDriverVerName(conn, use_ansi);

  // Allocate statement handle
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
  CheckError(status, "SQLAllocHandle", conn);
  return status;
}

SQLRETURN ConnectDsn(std::string dsn, std::shared_ptr<ODBCHandles> conn,
                     int timeout, bool use_ansi) {
  SQLSMALLINT buflen;
  SQLSMALLINT out_len;
  SQLRETURN status;

  SetAttributes(conn, timeout, use_ansi);
  if (use_ansi) {
    status =
        SQLConnectA(conn->hdbc, (SQLCHAR*)dsn.c_str(), SQL_NTS,
                    (SQLCHAR*)conn->outdsn, NumSqlChar(conn->outdsn), NULL, 0);
  } else {
    status =
        SQLConnect(conn->hdbc, (SQLCHAR*)dsn.c_str(), SQL_NTS,
                   (SQLCHAR*)conn->outdsn, NumSqlChar(conn->outdsn), NULL, 0);
  }

  CheckError(status, "SQLConnect", conn, use_ansi);
  conn->connected = true;

  PrintDriverVerName(conn, use_ansi);

  // Allocate statement handle
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
  CheckError(status, "SQLAllocHandle", conn);
  return status;
}

// Disconnect from the database
SQLRETURN Disconnect(std::shared_ptr<ODBCHandles> conn) {
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
SQLRETURN GetDriverInfo(std::shared_ptr<ODBCHandles> conn, bool use_ansi) {
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
    if (use_ansi) {
      status = SQLGetInfoA(conn->hdbc, info_type, buf, sizeof(buf), &out_len);
    } else {
      status = SQLGetInfo(conn->hdbc, info_type, buf, sizeof(buf), &out_len);
    }
    CheckError(status, "SqlGetInfo(" + info_name + ")", conn, use_ansi);
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

// TODO(#10): Remove printf and support logging
// Prints if the environment is ODBC3
SQLRETURN GetEnvInfo(std::shared_ptr<ODBCHandles> conn) {
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
SQLRETURN PrintDriverVerName(std::shared_ptr<ODBCHandles> conn, bool use_ansi) {
  SQLCHAR driver_info[kBufferLength];
  SQLSMALLINT out_len;
  SQLRETURN status;
  if (use_ansi) {
    status = SQLGetInfoA(conn->hdbc, SQL_DRIVER_VER, driver_info,
                         NumSqlChar(driver_info), &out_len);
  } else {
    status = SQLGetInfo(conn->hdbc, SQL_DRIVER_VER, driver_info,
                        NumSqlChar(driver_info), &out_len);
  }
  CheckError(status, "SQLGetInfo", conn, use_ansi);

  printf("Driver: %s", driver_info);
  if (use_ansi) {
    status = SQLGetInfoA(conn->hdbc, SQL_DRIVER_NAME, driver_info,
                         NumSqlChar(driver_info), &out_len);

  } else {
    status = SQLGetInfo(conn->hdbc, SQL_DRIVER_NAME, driver_info,
                        NumSqlChar(driver_info), &out_len);
  }
  CheckError(status, "SQLGetInfo", conn, use_ansi);
  printf(" (%s) \n\n", driver_info);
  return status;
}

}  // namespace google::cloud::odbc_tests
