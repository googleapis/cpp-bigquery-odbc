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
                         (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_UINTEGER);
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

// Gets Info about the driver and populates conn.metadata
SQLRETURN GetDriverInfo2(std::shared_ptr<ConnectionHandle> conn) {
  SQLCHAR sqlCharBuf[kBufferLength];
  SQLUSMALLINT sqlUSmallIntBuf;
  SQLUINTEGER sqlUIntegerBuf;
  SQLUINTEGER sqlBitmaskBuf;
  SQLSMALLINT out_len;
  SQLRETURN status;

  std::map<SQLUSMALLINT, std::string> const kCharMap = {
      {SQL_ACCESSIBLE_PROCEDURES, "SQL_ACCESSIBLE_PROCEDURES"},
      {SQL_ACCESSIBLE_TABLES, "SQL_ACCESSIBLE_TABLES"},
      {SQL_DATA_SOURCE_NAME, "SQL_DATA_SOURCE_NAME"},
      {SQL_ODBC_VER, "SQL_ODBC_VER"},
      {SQL_DATABASE_NAME, "SQL_DATABASE_NAME"},
      {SQL_DRIVER_NAME, "SQL_DRIVER_NAME"},
      {SQL_DRIVER_ODBC_VER, "SQL_DRIVER_ODBC_VER"},
      {SQL_CATALOG_NAME, "SQL_CATALOG_NAME"},
      {SQL_CATALOG_NAME_SEPARATOR, "SQL_CATALOG_NAME_SEPARATOR"},
      {SQL_CATALOG_TERM, "SQL_CATALOG_TERM"},
      {SQL_COLLATION_SEQ, "SQL_COLLATION_SEQ"},
      {SQL_COLUMN_ALIAS, "SQL_COLUMN_ALIAS"},
      {SQL_DATA_SOURCE_READ_ONLY, "SQL_DATA_SOURCE_READ_ONLY"},
      {SQL_DBMS_NAME, "SQL_DBMS_NAME"},
      {SQL_DBMS_VER, "SQL_DBMS_VER"},
      {SQL_DESCRIBE_PARAMETER, "SQL_DESCRIBE_PARAMETER"},
      {SQL_DM_VER, "SQL_DM_VER"},
      {SQL_DRIVER_VER, "SQL_DRIVER_VER"},
      {SQL_EXPRESSIONS_IN_ORDERBY, "SQL_EXPRESSIONS_IN_ORDERBY"},
      {SQL_IDENTIFIER_QUOTE_CHAR, "SQL_IDENTIFIER_QUOTE_CHAR"},
      {SQL_INTEGRITY, "SQL_INTEGRITY"},
      {SQL_KEYWORDS, "SQL_KEYWORDS"},
      {SQL_LIKE_ESCAPE_CLAUSE, "SQL_LIKE_ESCAPE_CLAUSE"},
      {SQL_MAX_ROW_SIZE_INCLUDES_LONG, "SQL_MAX_ROW_SIZE_INCLUDES_LONG"},
      {SQL_MULT_RESULT_SETS, "SQL_MULT_RESULT_SETS"},
      {SQL_MULTIPLE_ACTIVE_TXN, "SQL_MULTIPLE_ACTIVE_TXN"},
      {SQL_DRIVER_VER, "SQL_DRIVER_VER"},
      {SQL_NEED_LONG_DATA_LEN, "SQL_NEED_LONG_DATA_LEN"},
      {SQL_ORDER_BY_COLUMNS_IN_SELECT, "SQL_ORDER_BY_COLUMNS_IN_SELECT"},
      {SQL_PROCEDURE_TERM, "SQL_PROCEDURE_TERM"},
      {SQL_PROCEDURES, "SQL_PROCEDURES"},
      {SQL_ROW_UPDATES, "SQL_ROW_UPDATES"},
      {SQL_SCHEMA_TERM, "SQL_SCHEMA_TERM"},
      {SQL_SEARCH_PATTERN_ESCAPE, "SQL_SEARCH_PATTERN_ESCAPE"},
      {SQL_SERVER_NAME, "SQL_SERVER_NAME"},
      {SQL_SPECIAL_CHARACTERS, "SQL_SPECIAL_CHARACTERS"},
      {SQL_TABLE_TERM, "SQL_TABLE_TERM"},
      {SQL_USER_NAME, "SQL_USER_NAME"},
      {SQL_XOPEN_CLI_YEAR, "SQL_XOPEN_CLI_YEAR"}};

  std::map<SQLUSMALLINT, std::string> const kBitMaskMap = {
      {SQL_ALTER_TABLE, "SQL_ALTER_TABLE"},
      {SQL_ALTER_DOMAIN, "SQL_ALTER_DOMAIN"},
      {SQL_AGGREGATE_FUNCTIONS, "SQL_AGGREGATE_FUNCTIONS"},
      {SQL_STRING_FUNCTIONS, "SQL_STRING_FUNCTIONS"},
      {SQL_CATALOG_USAGE, "SQL_CATALOG_USAGE"},
      {SQL_CONVERT_BIGINT, "SQL_CONVERT_BIGINT"},
      {SQL_CONVERT_BINARY, "SQL_CONVERT_BINARY"},
      {SQL_CONVERT_BIT, "SQL_CONVERT_BIT"},
      {SQL_CONVERT_CHAR, "SQL_CONVERT_CHAR"},
      {SQL_CONVERT_DATE, "SQL_CONVERT_DATE"},
      {SQL_CONVERT_DECIMAL, "SQL_CONVERT_DECIMAL"},
      {SQL_CONVERT_DOUBLE, "SQL_CONVERT_DOUBLE"},
      {SQL_CONVERT_FLOAT, "SQL_CONVERT_FLOAT"},
      {SQL_CONVERT_INTEGER, "SQL_CONVERT_INTEGER"},
      {SQL_CONVERT_INTERVAL_YEAR_MONTH, "SQL_CONVERT_INTERVAL_YEAR_MONTH"},
      {SQL_CONVERT_INTERVAL_DAY_TIME, "SQL_CONVERT_INTERVAL_DAY_TIME"},
      {SQL_CONVERT_LONGVARBINARY, "SQL_CONVERT_LONGVARBINARY"},
      {SQL_CONVERT_LONGVARCHAR, "SQL_CONVERT_LONGVARCHAR"},
      {SQL_CONVERT_NUMERIC, "SQL_CONVERT_NUMERIC"},
      {SQL_CONVERT_REAL, "SQL_CONVERT_REAL"},
      {SQL_CONVERT_SMALLINT, "SQL_CONVERT_SMALLINT"},
      {SQL_CONVERT_TIME, "SQL_CONVERT_TIME"},
      {SQL_CONVERT_TIMESTAMP, "SQL_CONVERT_TIMESTAMP"},
      {SQL_CONVERT_TINYINT, "SQL_CONVERT_TINYINT"},
      {SQL_CONVERT_VARBINARY, "SQL_CONVERT_VARBINARY"},
      {SQL_CONVERT_VARCHAR, "SQL_CONVERT_VARCHAR"},
      {SQL_CONVERT_FUNCTIONS, "SQL_CONVERT_FUNCTIONS"},
      {SQL_CREATE_ASSERTION, "SQL_CREATE_ASSERTION"},
      {SQL_CREATE_CHARACTER_SET, "SQL_CREATE_CHARACTER_SET"},
      {SQL_CREATE_COLLATION, "SQL_CREATE_COLLATION"},
      {SQL_CREATE_DOMAIN, "SQL_CREATE_DOMAIN"},
      {SQL_CREATE_SCHEMA, "SQL_CREATE_SCHEMA"},
      {SQL_CREATE_TABLE, "SQL_CREATE_TABLE"},
      {SQL_CREATE_TRANSLATION, "SQL_CREATE_TRANSLATION"},
      {SQL_CREATE_VIEW, "SQL_CREATE_VIEW"},
      {SQL_DATETIME_LITERALS, "SQL_DATETIME_LITERALS"},
      {SQL_DROP_ASSERTION, "SQL_DROP_ASSERTION"},
      {SQL_DROP_CHARACTER_SET, "SQL_DROP_CHARACTER_SET"},
      {SQL_DROP_COLLATION, "SQL_DROP_COLLATION"},
      {SQL_DROP_DOMAIN, "SQL_DROP_DOMAIN"},
      {SQL_DROP_SCHEMA, "SQL_DROP_SCHEMA"},
      {SQL_DROP_TABLE, "SQL_DROP_TABLE"},
      {SQL_DROP_TRANSLATION, "SQL_DROP_TRANSLATION"},
      {SQL_DROP_VIEW, "SQL_DROP_VIEW"},
      {SQL_DYNAMIC_CURSOR_ATTRIBUTES1, "SQL_DYNAMIC_CURSOR_ATTRIBUTES1"},
      {SQL_DYNAMIC_CURSOR_ATTRIBUTES2, "SQL_DYNAMIC_CURSOR_ATTRIBUTES2"},
      {SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1,
       "SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1"},
      {SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2,
       "SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2"},
      {SQL_GETDATA_EXTENSIONS, "SQL_GETDATA_EXTENSIONS"},
      {SQL_INDEX_KEYWORDS, "SQL_INDEX_KEYWORDS"},
      {SQL_INFO_SCHEMA_VIEWS, "SQL_INFO_SCHEMA_VIEWS"},
      {SQL_INSERT_STATEMENT, "SQL_INSERT_STATEMENT"},
      {SQL_KEYSET_CURSOR_ATTRIBUTES1, "SQL_KEYSET_CURSOR_ATTRIBUTES1"},
      {SQL_KEYSET_CURSOR_ATTRIBUTES2, "SQL_KEYSET_CURSOR_ATTRIBUTES2"},
      {SQL_NUMERIC_FUNCTIONS, "SQL_NUMERIC_FUNCTIONS"},
      {SQL_OJ_CAPABILITIES, "SQL_OJ_CAPABILITIES"},
      {SQL_POS_OPERATIONS, "SQL_POS_OPERATIONS"},
      {SQL_SCHEMA_USAGE, "SQL_SCHEMA_USAGE"},
      {SQL_SCROLL_OPTIONS, "SQL_SCROLL_OPTIONS"},
      {SQL_SQL92_DATETIME_FUNCTIONS, "SQL_SQL92_DATETIME_FUNCTIONS"},
      {SQL_SQL92_FOREIGN_KEY_DELETE_RULE, "SQL_SQL92_FOREIGN_KEY_DELETE_RULE"},
      {SQL_SQL92_FOREIGN_KEY_UPDATE_RULE, "SQL_SQL92_FOREIGN_KEY_UPDATE_RULE"},
      {SQL_SQL92_GRANT, "SQL_SQL92_GRANT"},
      {SQL_SQL92_NUMERIC_VALUE_FUNCTIONS, "SQL_SQL92_NUMERIC_VALUE_FUNCTIONS"},
      {SQL_SQL92_PREDICATES, "SQL_SQL92_PREDICATES"},
      {SQL_SQL92_RELATIONAL_JOIN_OPERATORS,
       "SQL_SQL92_RELATIONAL_JOIN_OPERATORS"},
      {SQL_SQL92_REVOKE, "SQL_SQL92_REVOKE"},
      {SQL_SQL92_ROW_VALUE_CONSTRUCTOR, "SQL_SQL92_ROW_VALUE_CONSTRUCTOR"},
      {SQL_SQL92_STRING_FUNCTIONS, "SQL_SQL92_STRING_FUNCTIONS"},
      {SQL_SQL92_VALUE_EXPRESSIONS, "SQL_SQL92_VALUE_EXPRESSIONS"},
      {SQL_STANDARD_CLI_CONFORMANCE, "SQL_STANDARD_CLI_CONFORMANCE"},
      {SQL_STATIC_CURSOR_ATTRIBUTES1, "SQL_STATIC_CURSOR_ATTRIBUTES1"},
      {SQL_STATIC_CURSOR_ATTRIBUTES2, "SQL_STATIC_CURSOR_ATTRIBUTES2"},
      {SQL_SUBQUERIES, "SQL_SUBQUERIES"},
      {SQL_SYSTEM_FUNCTIONS, "SQL_SYSTEM_FUNCTIONS"},
      {SQL_TIMEDATE_ADD_INTERVALS, "SQL_TIMEDATE_ADD_INTERVALS"},
      {SQL_TIMEDATE_DIFF_INTERVALS, "SQL_TIMEDATE_DIFF_INTERVALS"},
      {SQL_TIMEDATE_FUNCTIONS, "SQL_TIMEDATE_FUNCTIONS"},
      {SQL_TXN_ISOLATION_OPTION, "SQL_TXN_ISOLATION_OPTION"},
      {SQL_UNION, "SQL_UNION"}};

  std::map<SQLUSMALLINT, std::string> const kSqlUSmallIntMap = {
      {SQL_QUOTED_IDENTIFIER_CASE, "SQL_QUOTED_IDENTIFIER_CASE"},
      {SQL_ACTIVE_ENVIRONMENTS, "SQL_ACTIVE_ENVIRONMENTS"},
      {SQL_CATALOG_LOCATION, "SQL_CATALOG_LOCATION"},
      {SQL_CONCAT_NULL_BEHAVIOR, "SQL_CONCAT_NULL_BEHAVIOR"},
      {SQL_CORRELATION_NAME, "SQL_CORRELATION_NAME"},
      {SQL_CURSOR_COMMIT_BEHAVIOR, "SQL_CURSOR_COMMIT_BEHAVIOR"},
      {SQL_CURSOR_ROLLBACK_BEHAVIOR, "SQL_CURSOR_ROLLBACK_BEHAVIOR"},
      {SQL_FILE_USAGE, "SQL_FILE_USAGE"},
      {SQL_GROUP_BY, "SQL_GROUP_BY"},
      {SQL_IDENTIFIER_CASE, "SQL_IDENTIFIER_CASE"},
      {SQL_MAX_CATALOG_NAME_LEN, "SQL_MAX_CATALOG_NAME_LEN"},
      {SQL_MAX_COLUMN_NAME_LEN, "SQL_MAX_COLUMN_NAME_LEN"},
      {SQL_MAX_COLUMNS_IN_GROUP_BY, "SQL_MAX_COLUMNS_IN_GROUP_BY"},
      {SQL_MAX_COLUMNS_IN_INDEX, "SQL_MAX_COLUMNS_IN_INDEX"},
      {SQL_MAX_COLUMNS_IN_ORDER_BY, "SQL_MAX_COLUMNS_IN_ORDER_BY"},
      {SQL_MAX_COLUMNS_IN_SELECT, "SQL_MAX_COLUMNS_IN_SELECT"},
      {SQL_MAX_COLUMNS_IN_TABLE, "SQL_MAX_COLUMNS_IN_TABLE"},
      {SQL_MAX_CONCURRENT_ACTIVITIES, "SQL_MAX_CONCURRENT_ACTIVITIES"},
      {SQL_MAX_CURSOR_NAME_LEN, "SQL_MAX_CURSOR_NAME_LEN"},
      {SQL_MAX_DRIVER_CONNECTIONS, "SQL_MAX_DRIVER_CONNECTIONS"},
      {SQL_MAX_IDENTIFIER_LEN, "SQL_MAX_IDENTIFIER_LEN"},
      {SQL_MAX_PROCEDURE_NAME_LEN, "SQL_MAX_PROCEDURE_NAME_LEN"},
      {SQL_MAX_SCHEMA_NAME_LEN, "SQL_MAX_SCHEMA_NAME_LEN"},
      {SQL_MAX_TABLE_NAME_LEN, "SQL_MAX_TABLE_NAME_LEN"},
      {SQL_MAX_TABLES_IN_SELECT, "SQL_MAX_TABLES_IN_SELECT"},
      {SQL_MAX_USER_NAME_LEN, "SQL_MAX_USER_NAME_LEN"},
      {SQL_NON_NULLABLE_COLUMNS, "SQL_NON_NULLABLE_COLUMNS"},
      {SQL_NULL_COLLATION, "SQL_NULL_COLLATION"},
      {SQL_TXN_CAPABLE, "SQL_TXN_CAPABLE"}};

  std::map<SQLUSMALLINT, std::string> const kSqlUIntegerMap = {
      {SQL_ASYNC_MODE, "SQL_ASYNC_MODE"},
      {SQL_BATCH_ROW_COUNT, "SQL_BATCH_ROW_COUNT"},
      {SQL_BATCH_SUPPORT, "SQL_BATCH_SUPPORT"},
      {SQL_BOOKMARK_PERSISTENCE, "SQL_BOOKMARK_PERSISTENCE"},
      {SQL_CURSOR_SENSITIVITY, "SQL_CURSOR_SENSITIVITY"},
      {SQL_DDL_INDEX, "SQL_DDL_INDEX"},
      {SQL_DEFAULT_TXN_ISOLATION, "SQL_DEFAULT_TXN_ISOLATION"},
      {SQL_MAX_ASYNC_CONCURRENT_STATEMENTS,
       "SQL_MAX_ASYNC_CONCURRENT_STATEMENTS"},
      {SQL_MAX_BINARY_LITERAL_LEN, "SQL_MAX_BINARY_LITERAL_LEN"},
      {SQL_MAX_CHAR_LITERAL_LEN, "SQL_MAX_CHAR_LITERAL_LEN"},
      {SQL_MAX_INDEX_SIZE, "SQL_MAX_INDEX_SIZE"},
      {SQL_MAX_ROW_SIZE, "SQL_MAX_ROW_SIZE"},
      {SQL_MAX_STATEMENT_LEN, "SQL_MAX_STATEMENT_LEN"},
      {SQL_ODBC_INTERFACE_CONFORMANCE, "SQL_ODBC_INTERFACE_CONFORMANCE"},
      {SQL_PARAM_ARRAY_ROW_COUNTS, "SQL_PARAM_ARRAY_ROW_COUNTS"},
      {SQL_PARAM_ARRAY_SELECTS, "SQL_PARAM_ARRAY_SELECTS"},
      {SQL_SQL_CONFORMANCE, "SQL_SQL_CONFORMANCE"}};

  std::map<std::string, std::string> unsupported_char;
  std::map<std::string, std::string> supported_char;
  std::map<std::string, std::string> unsupported_sqlusmallint;
  std::map<std::string, std::string> supported_sqlusmallint;
  std::map<std::string, std::string> unsupported_sqluinteger;
  std::map<std::string, std::string> supported_sqluinteger;
  std::map<std::string, std::string> unsupported_bitmask;
  std::map<std::string, std::string> supported_bitmask;
  for (auto elem : kCharMap) {
    auto info_type = std::get<0>(elem);
    auto info_name = std::get<1>(elem);
    status = SQLGetInfo(conn->hdbc, info_type, sqlCharBuf, sizeof(sqlCharBuf),
                        &out_len);
    if (SQL_SUCCEEDED(status)) {
      if (status == SQL_SUCCESS_WITH_INFO) {
        std::runtime_error("Buffer size is not enough for " + info_name +
                           " InfoType");
      }
      std::string val = (char*)sqlCharBuf;
      if (val == "N" || val == "") {
        unsupported_char.insert({info_name, val});
      } else {
        supported_char.insert({info_name, val});
      }
    }
  }

  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "SUPPORTED SQLCHAR " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : supported_char) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
  }
  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "UNSUPPORTED SQLCHAR " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : unsupported_char) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
  }
  for (auto elem : kSqlUSmallIntMap) {
    auto info_type = std::get<0>(elem);
    auto info_name = std::get<1>(elem);
    status = SQLGetInfo(conn->hdbc, info_type, (SQLPOINTER)&sqlUSmallIntBuf,
                        sizeof(sqlUSmallIntBuf), nullptr);
    if (SQL_SUCCEEDED(status)) {
      if (status == SQL_SUCCESS_WITH_INFO) {
        std::runtime_error("Buffer size is not enough for " + info_name +
                           " InfoType");
      }
      if (sqlUSmallIntBuf == 0 || sqlUSmallIntBuf == 0L) {
        unsupported_sqlusmallint.insert(
            {info_name, std::to_string(sqlUSmallIntBuf)});
      } else {
        supported_sqlusmallint.insert(
            {info_name, std::to_string(sqlUSmallIntBuf)});
      }
    }
  }
  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "SUPPORTED SQLUSMALLINT " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : supported_sqlusmallint) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
  }
  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "UNSUPPORTED SQLUSMALLINT " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : unsupported_sqlusmallint) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
  }
  for (auto elem : kSqlUIntegerMap) {
    auto info_type = std::get<0>(elem);
    auto info_name = std::get<1>(elem);
    status = SQLGetInfo(conn->hdbc, info_type, (SQLPOINTER)&sqlUIntegerBuf,
                        sizeof(sqlUIntegerBuf), nullptr);
    if (SQL_SUCCEEDED(status)) {
      if (status == SQL_SUCCESS_WITH_INFO) {
        std::runtime_error("Buffer size is not enough for " + info_name +
                           " InfoType");
      }
      if (sqlUIntegerBuf == 0 || sqlUIntegerBuf == 0L) {
        unsupported_sqluinteger.insert(
            {info_name, std::to_string(sqlUIntegerBuf)});
      } else {
        supported_sqluinteger.insert(
            {info_name, std::to_string(sqlUIntegerBuf)});
      }
    }
  }

  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "SUPPORTED SQLUINTEGER " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : supported_sqluinteger) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
  }
  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "UNSUPPORTED SQLUINTEGER " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : unsupported_sqluinteger) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
  }

  for (auto elem : kBitMaskMap) {
    auto info_type = std::get<0>(elem);
    auto info_name = std::get<1>(elem);
    status = SQLGetInfo(conn->hdbc, info_type, (SQLPOINTER)&sqlBitmaskBuf,
                        sizeof(sqlBitmaskBuf), nullptr);
    if (SQL_SUCCEEDED(status)) {
      if (status == SQL_SUCCESS_WITH_INFO) {
        std::runtime_error("Buffer size is not enough for " + info_name +
                           " InfoType");
      }
      if (sqlBitmaskBuf == 0 || sqlBitmaskBuf == 0L) {
        unsupported_bitmask.insert({info_name, std::to_string(sqlBitmaskBuf)});
      } else {
        supported_bitmask.insert({info_name, std::to_string(sqlBitmaskBuf)});
      }
    }
  }

  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "SUPPORTED BITMASK " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : supported_bitmask) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
  }
  std::cout << std::endl;
  std::cout << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << "UNSUPPORTED BITMASK " << std::endl;
  std::cout << "************************************" << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;

  for (auto elem : unsupported_bitmask) {
    std::string type = std::get<0>(elem);
    std::string val = std::get<1>(elem);
    std ::cout << type << "=" << val << std::endl;
    std::cout << std::endl;
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
