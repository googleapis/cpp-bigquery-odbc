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
    SQLUSMALLINT /*info_type*/) {
  // Not yet Implemented.
  return Status(StatusCode::kUnimplemented, "Not yet Implemented");
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
    SQLUSMALLINT /*info_type*/) {
  // Not yet Implemented.
  return Status(StatusCode::kUnimplemented, "Not yet Implemented");
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
    SQLUSMALLINT /*info_type*/) {
  // Not yet Implemented.
  return Status(StatusCode::kUnimplemented, "Not yet Implemented");
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
