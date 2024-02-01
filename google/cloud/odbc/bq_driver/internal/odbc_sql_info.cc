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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_info.h"
#include <cstring>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;

constexpr int kUnsupportedCharBufSize = 256;

// Helper functions
namespace {
Status InvalidType(SQLUSMALLINT info_type) {
  std::string msg = "Invalid infoType: ";
  msg.append(std::to_string(info_type));
  TracePrintInternal(*(*kTraceOptsConsole), msg);
  return Status(StatusCode::kInvalidArgument, msg);
}

}  // namespace

StatusOr<SQLGetInfoSqlChar> SQLGetInfoSqlChar::GetSupportedInfoType(
    SQLUSMALLINT info_type) {
  SQLGetInfoSqlChar result;
  switch (info_type) {
    case SQL_CATALOG_NAME:
    case SQL_COLUMN_ALIAS:
    case SQL_DESCRIBE_PARAMETER:
    case SQL_EXPRESSIONS_IN_ORDERBY:
    case SQL_MULTIPLE_ACTIVE_TXN:
    case SQL_PROCEDURES:
    case SQL_ACCESSIBLE_TABLES: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kSupportedCharY));
      break;
    }
    case SQL_CATALOG_NAME_SEPARATOR: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kCatalogSeparator));
      break;
    }
    case SQL_CATALOG_TERM: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kCatalogTerm));
      break;
    }
    case SQL_COLLATION_SEQ: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDefaultCollation));
      break;
    }
    case SQL_DBMS_NAME: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDbmsName));
      break;
    }
    case SQL_DBMS_VER: {
      result.info_val = reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDbmsVer));
      break;
    }
    case SQL_DRIVER_NAME: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDriverName));
      break;
    }
    case SQL_DRIVER_ODBC_VER: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDriverOdbcVer));
      break;
    }
    case SQL_DRIVER_VER: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kDriverVer));
      break;
    }
    case SQL_IDENTIFIER_QUOTE_CHAR: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kIdentifierQuoteChar));
      break;
    }
    case SQL_SCHEMA_TERM: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kSchemaTerm));
      break;
    }
    case SQL_SEARCH_PATTERN_ESCAPE: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kSearchPatternEscape));
      break;
    }
    case SQL_SERVER_NAME: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kSqlServerName));
      break;
    }
    case SQL_TABLE_TERM: {
      result.info_val =
          reinterpret_cast<SQLCHAR*>(const_cast<char*>(kSqlTableTerm));
      break;
    }
    default: {
      return InvalidType(info_type);
    }
  }

  return result;
}

StatusOr<SQLGetInfoSqlChar> SQLGetInfoSqlChar::GetUnSupportedInfoType(
    SQLUSMALLINT info_type) {
  SQLGetInfoSqlChar result;
  switch (info_type) {
    case SQL_KEYWORDS:
    case SQL_PROCEDURE_TERM:
    case SQL_SPECIAL_CHARACTERS:
    case SQL_USER_NAME: {
      result.info_val = reinterpret_cast<SQLCHAR*>(const_cast<char*>(""));
      break;
    }
    case SQL_ACCESSIBLE_PROCEDURES:
    case SQL_DATA_SOURCE_READ_ONLY:
    case SQL_INTEGRITY:
    case SQL_LIKE_ESCAPE_CLAUSE:
    case SQL_MAX_ROW_SIZE_INCLUDES_LONG:
    case SQL_MULT_RESULT_SETS:
    case SQL_NEED_LONG_DATA_LEN:
    case SQL_ORDER_BY_COLUMNS_IN_SELECT:
    case SQL_ROW_UPDATES: {
      result.info_val = reinterpret_cast<SQLCHAR*>(const_cast<char*>("N"));
      break;
    }
    default: {
      return InvalidType(info_type);
    }
  }

  return result;
}

StatusOr<SQLGetInfoSqlUSmallInt> SQLGetInfoSqlUSmallInt::GetSupportedInfoType(
    SQLUSMALLINT info_type) {
  SQLGetInfoSqlUSmallInt result;

  switch (info_type) {
    case SQL_CATALOG_LOCATION: {
      result.info_val = static_cast<SQLUSMALLINT>(kCatalogLocation);
      break;
    }
    case SQL_CORRELATION_NAME: {
      result.info_val = static_cast<SQLUSMALLINT>(kCorrelationName);
      break;
    }
    case SQL_CURSOR_COMMIT_BEHAVIOR: {
      result.info_val = static_cast<SQLUSMALLINT>(kCursorCommitBehavior);
      break;
    }
    case SQL_CURSOR_ROLLBACK_BEHAVIOR: {
      result.info_val = static_cast<SQLUSMALLINT>(kCursorRollbackBehavior);
      break;
    }
    case SQL_GROUP_BY: {
      result.info_val = static_cast<SQLUSMALLINT>(kGroupBy);
      break;
    }
    case SQL_IDENTIFIER_CASE: {
      result.info_val = static_cast<SQLUSMALLINT>(kIdentifierCase);
      break;
    }
    case SQL_MAX_CATALOG_NAME_LEN: {
      result.info_val = static_cast<SQLUSMALLINT>(kMaxCatalogNameLen);
      break;
    }
    case SQL_MAX_COLUMNS_IN_TABLE: {
      result.info_val = static_cast<SQLUSMALLINT>(kMaxColsInTable);
      break;
    }
    case SQL_MAX_COLUMN_NAME_LEN: {
      result.info_val = static_cast<SQLUSMALLINT>(kMaxColNameLen);
      break;
    }
    case SQL_MAX_IDENTIFIER_LEN: {
      result.info_val = static_cast<SQLUSMALLINT>(kMaxIdentifierLen);
      break;
    }
    case SQL_MAX_SCHEMA_NAME_LEN: {
      result.info_val = static_cast<SQLUSMALLINT>(kMaxSchemaNameLen);
      break;
    }
    case SQL_MAX_TABLES_IN_SELECT: {
      result.info_val = static_cast<SQLUSMALLINT>(kMaxTablesInSelect);
      break;
    }
    case SQL_MAX_TABLE_NAME_LEN: {
      result.info_val = static_cast<SQLUSMALLINT>(kMaxTableNameLen);
      break;
    }
    case SQL_NULL_COLLATION: {
      result.info_val = static_cast<SQLUSMALLINT>(kNullCollation);
      break;
    }
    case SQL_QUOTED_IDENTIFIER_CASE: {
      result.info_val = static_cast<SQLUSMALLINT>(kQuotedIdentifierCase);
      break;
    }
    case SQL_TXN_CAPABLE: {
      result.info_val = static_cast<SQLUSMALLINT>(kTxnCapable);
      break;
    }
    default: {
      return InvalidType(info_type);
    }
  }

  return result;
}

StatusOr<SQLGetInfoSqlUSmallInt> SQLGetInfoSqlUSmallInt::GetUnSupportedInfoType(
    SQLUSMALLINT info_type) {
  SQLGetInfoSqlUSmallInt result;

  switch (info_type) {
    case SQL_ACTIVE_ENVIRONMENTS:
    case SQL_CONCAT_NULL_BEHAVIOR:
    case SQL_FILE_USAGE:
    case SQL_MAX_COLUMNS_IN_GROUP_BY:
    case SQL_MAX_COLUMNS_IN_INDEX:
    case SQL_MAX_COLUMNS_IN_ORDER_BY:
    case SQL_MAX_COLUMNS_IN_SELECT:
    case SQL_MAX_CONCURRENT_ACTIVITIES:
    case SQL_MAX_CURSOR_NAME_LEN:
    case SQL_MAX_DRIVER_CONNECTIONS:
    case SQL_MAX_PROCEDURE_NAME_LEN:
    case SQL_MAX_USER_NAME_LEN:
    case SQL_NON_NULLABLE_COLUMNS: {
      result.info_val = static_cast<SQLUSMALLINT>(0);
      break;
    }
    default: {
      return InvalidType(info_type);
    }
  }

  return result;
}

StatusOr<SQLGetInfoSqlUInt> SQLGetInfoSqlUInt::GetSupportedInfoType(
    SQLUSMALLINT info_type) {
  SQLGetInfoSqlUInt result;

  switch (info_type) {
    case SQL_ASYNC_MODE: {
      result.info_val = static_cast<SQLUINTEGER>(kAsyncMode);
      break;
    }
    case SQL_DEFAULT_TXN_ISOLATION: {
      result.info_val = static_cast<SQLUINTEGER>(kDefaultTxnIsolation);
      break;
    }
    case SQL_ODBC_INTERFACE_CONFORMANCE: {
      result.info_val = static_cast<SQLUINTEGER>(kOdbcInterfaceConformance);
      break;
    }
    case SQL_SQL_CONFORMANCE: {
      result.info_val = static_cast<SQLUINTEGER>(kSqlConformance);
      break;
    }
    default: {
      return InvalidType(info_type);
    }
  }

  return result;
}

StatusOr<SQLGetInfoSqlUInt> SQLGetInfoSqlUInt::GetUnSupportedInfoType(
    SQLUSMALLINT info_type) {
  SQLGetInfoSqlUInt result;

  switch (info_type) {
    case SQL_BATCH_ROW_COUNT:
    case SQL_BATCH_SUPPORT:
    case SQL_BOOKMARK_PERSISTENCE:
    case SQL_CURSOR_SENSITIVITY:
    case SQL_DDL_INDEX:
    case SQL_MAX_ASYNC_CONCURRENT_STATEMENTS:
    case SQL_MAX_BINARY_LITERAL_LEN:
    case SQL_MAX_CHAR_LITERAL_LEN:
    case SQL_MAX_INDEX_SIZE:
    case SQL_MAX_ROW_SIZE:
    case SQL_MAX_STATEMENT_LEN:
    case SQL_PARAM_ARRAY_ROW_COUNTS:
    case SQL_PARAM_ARRAY_SELECTS: {
      result.info_val = static_cast<SQLUINTEGER>(0);
      break;
    }
    default: {
      return InvalidType(info_type);
    }
  }

  return result;
}

StatusOr<SQLGetInfoBitmask> SQLGetInfoBitmask::GetSupportedInfoType(
    SQLUSMALLINT /*info_type*/) {
  // Not yet Implemented.
  return Status(StatusCode::kUnimplemented, "Not yet Implemented");
}

StatusOr<SQLGetInfoBitmask> SQLGetInfoBitmask::GetUnSupportedInfoType(
    SQLUSMALLINT info_type) {
  SQLGetInfoBitmask result;

  switch (info_type) {
    case SQL_ALTER_DOMAIN:
    case SQL_ALTER_TABLE:
    case SQL_CONVERT_BINARY:
    case SQL_CONVERT_CHAR:
    case SQL_CONVERT_DECIMAL:
    case SQL_CONVERT_FLOAT:
    case SQL_CONVERT_INTEGER:
    case SQL_CONVERT_INTERVAL_DAY_TIME:
    case SQL_CONVERT_INTERVAL_YEAR_MONTH:
    case SQL_CONVERT_LONGVARBINARY:
    case SQL_CONVERT_LONGVARCHAR:
    case SQL_CONVERT_NUMERIC:
    case SQL_CONVERT_REAL:
    case SQL_CONVERT_SMALLINT:
    case SQL_CONVERT_TINYINT:
    case SQL_CREATE_ASSERTION:
    case SQL_CREATE_CHARACTER_SET:
    case SQL_CREATE_COLLATION:
    case SQL_CREATE_DOMAIN:
    case SQL_CREATE_SCHEMA:
    case SQL_CREATE_TABLE:
    case SQL_CREATE_TRANSLATION:
    case SQL_CREATE_VIEW:
    case SQL_DROP_ASSERTION:
    case SQL_DROP_CHARACTER_SET:
    case SQL_DROP_COLLATION:
    case SQL_DROP_DOMAIN:
    case SQL_DROP_SCHEMA:
    case SQL_DROP_TABLE:
    case SQL_DROP_TRANSLATION:
    case SQL_DROP_VIEW:
    case SQL_DYNAMIC_CURSOR_ATTRIBUTES1:
    case SQL_DYNAMIC_CURSOR_ATTRIBUTES2:
    case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1:
    case SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2:
    case SQL_INDEX_KEYWORDS:
    case SQL_INFO_SCHEMA_VIEWS:
    case SQL_INSERT_STATEMENT:
    case SQL_KEYSET_CURSOR_ATTRIBUTES1:
    case SQL_KEYSET_CURSOR_ATTRIBUTES2:
    case SQL_POS_OPERATIONS:
    case SQL_SQL92_FOREIGN_KEY_DELETE_RULE:
    case SQL_SQL92_FOREIGN_KEY_UPDATE_RULE:
    case SQL_SQL92_GRANT:
    case SQL_SQL92_NUMERIC_VALUE_FUNCTIONS:
    case SQL_SQL92_REVOKE:
    case SQL_STATIC_CURSOR_ATTRIBUTES1:
    case SQL_STATIC_CURSOR_ATTRIBUTES2:
    case SQL_UNION: {
      result.info_val = static_cast<SQLUINTEGER>(0L);
      break;
    }
    default: {
      return InvalidType(info_type);
    }
  }

  return result;
}

}  // namespace google::cloud::odbc_bq_driver_internal
