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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_INFO_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_INFO_H

#include "google/cloud/odbc/bq_driver/internal/odbc_includes.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

// Constants specific to SQLGetInfo Information type
// values for SQLGetInfoSqlChar.
constexpr char const* kSupportedCharY = "Y";
constexpr char const* kCatalogSeparator = ".";
constexpr char const* kCatalogTerm = "Project";
constexpr char const* kDefaultCollation = "UTF-16LE_BINARY";
constexpr char const* kDbmsName = "BigQuery";
constexpr char const* kDbmsVer = "2";
constexpr char const* kDriverName = "Google ODBC Driver For BigQuery";
constexpr char const* kDriverOdbcVer = "03.80";
// TODO: Revisit thiswhen a proper versioning is defined for the Google Driver.
// Similar to
// https://github.com/googleapis/google-cloud-cpp/blob/main/google/cloud/version.cc
constexpr char const* kDriverVer = "1.0.0.0000";
constexpr char const* kIdentifierQuoteChar = "`";
constexpr char const* kSchemaTerm = "Dataset";
constexpr char const* kSearchPatternEscape = "\\";
constexpr char const* kSqlServerName = "Google";
constexpr char const* kSqlTableTerm = "Table";

// Constants specific to SQLGetInfo Information type
// values for SQLGetInfoSqlUSmallInt value.
constexpr SQLUSMALLINT kCatalogLocation = SQL_CL_START;
constexpr SQLUSMALLINT kCorrelationName = SQL_CN_ANY;
constexpr SQLUSMALLINT kCursorCommitBehavior = SQL_CB_CLOSE;
constexpr SQLUSMALLINT kCursorRollbackBehavior = SQL_CB_CLOSE;
constexpr SQLUSMALLINT kGroupBy = SQL_GB_GROUP_BY_CONTAINS_SELECT;
constexpr SQLUSMALLINT kIdentifierCase = SQL_IC_SENSITIVE;
constexpr SQLUSMALLINT kMaxCatalogNameLen = 128;
constexpr SQLUSMALLINT kMaxColsInTable = 10000;
constexpr SQLUSMALLINT kMaxColNameLen = 128;
constexpr SQLUSMALLINT kMaxIdentifierLen = 255;
constexpr SQLUSMALLINT kMaxSchemaNameLen = 1024;
constexpr SQLUSMALLINT kMaxTablesInSelect = 1000;
constexpr SQLUSMALLINT kMaxTableNameLen = 1024;
constexpr SQLUSMALLINT kNullCollation = SQL_NC_LOW;
constexpr SQLUSMALLINT kQuotedIdentifierCase = SQL_IC_SENSITIVE;
constexpr SQLUSMALLINT kTxnCapable = SQL_TC_DML;

// Constants specific to SQLGetInfo Information type
// values for SQLGetInfoSqlUInteger value.
constexpr SQLUINTEGER kAsyncMode = SQL_AM_STATEMENT;
constexpr SQLUINTEGER kDefaultTxnIsolation = SQL_TXN_SERIALIZABLE;
constexpr SQLUINTEGER kOdbcInterfaceConformance = SQL_OIC_CORE;
constexpr SQLUINTEGER kSqlConformance = SQL_OIC_CORE;

// Constants specific to SQLGetInfo Information type
// values for SQLGetInfoSqlUInteger Bitmask value.
constexpr SQLUINTEGER kAggregateFns = SQL_AF_ALL | SQL_AF_AVG | SQL_AF_COUNT |
                                      SQL_AF_DISTINCT | SQL_AF_MAX |
                                      SQL_AF_MIN | SQL_AF_SUM;
constexpr SQLUINTEGER kCatalogUsage = SQL_CU_DML_STATEMENTS;
constexpr SQLUINTEGER kValueExpressions = SQL_SVE_CAST;
constexpr SQLUINTEGER kCliConformance = SQL_SCC_ISO92_CLI;
constexpr SQLUINTEGER kTxnSerializable = SQL_TXN_SERIALIZABLE;
constexpr SQLUINTEGER kConvertBigInt =
    SQL_CVT_DOUBLE | SQL_CVT_VARCHAR | SQL_CVT_BIT | SQL_CVT_BIGINT;
constexpr SQLUINTEGER kConvertBit =
    SQL_CVT_VARCHAR | SQL_CVT_BIT | SQL_CVT_BIGINT;
constexpr SQLUINTEGER kConvertDate =
    SQL_CVT_VARCHAR | SQL_CVT_DATE | SQL_CVT_TIMESTAMP;
constexpr SQLUINTEGER kScrollOptions = SQL_SO_FORWARD_ONLY;
constexpr SQLUINTEGER kConvertDouble =
    SQL_CVT_VARCHAR | SQL_CVT_DOUBLE | SQL_CVT_BIGINT;
constexpr SQLUINTEGER kConvertFn = SQL_FN_CVT_CONVERT | SQL_FN_CVT_CAST;
constexpr SQLUINTEGER kConvertTime = SQL_CVT_VARCHAR | SQL_CVT_TIME;
constexpr SQLUINTEGER kConvertTimestamp =
    SQL_CVT_VARCHAR | SQL_CVT_TIME | SQL_CVT_TIMESTAMP;
constexpr SQLUINTEGER kConvertVarBinary = SQL_CVT_VARCHAR | SQL_CVT_VARBINARY;
constexpr SQLUINTEGER kConvertVarChar =
    SQL_CVT_DOUBLE | SQL_CVT_VARCHAR | SQL_CVT_BIT | SQL_CVT_BIGINT |
    SQL_CVT_VARBINARY | SQL_CVT_DATE | SQL_CVT_TIME | SQL_CVT_TIMESTAMP;
constexpr SQLUINTEGER kDateTimeLiterals = SQL_DL_SQL92_TIMESTAMP;
constexpr SQLUINTEGER kGetDataExtns =
    SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER | SQL_GD_BLOCK | SQL_GD_BOUND;
constexpr SQLUINTEGER kSchemaUsage =
    SQL_SU_DML_STATEMENTS | SQL_SU_PROCEDURE_INVOCATION |
    SQL_SU_TABLE_DEFINITION | SQL_SU_INDEX_DEFINITION |
    SQL_SU_PRIVILEGE_DEFINITION;
constexpr SQLUINTEGER kOJCapabilities =
    SQL_OJ_LEFT | SQL_OJ_RIGHT | SQL_OJ_FULL | SQL_OJ_NESTED |
    SQL_OJ_NOT_ORDERED | SQL_OJ_INNER | SQL_OJ_ALL_COMPARISON_OPS;
constexpr SQLUINTEGER kDateTimeFns =
    SQL_SDF_CURRENT_DATE | SQL_SDF_CURRENT_TIME | SQL_SDF_CURRENT_TIMESTAMP;
constexpr SQLUINTEGER kRowValueCtr = SQL_SRVC_VALUE_EXPRESSION | SQL_SRVC_NULL |
                                     SQL_SRVC_DEFAULT | SQL_SRVC_ROW_SUBQUERY;
constexpr SQLUINTEGER kSystemFns = SQL_FN_SYS_IFNULL;
constexpr SQLUINTEGER kTimeDateFns =
    SQL_FN_TD_NOW | SQL_FN_TD_CURDATE | SQL_FN_TD_DAYOFMONTH |
    SQL_FN_TD_DAYOFWEEK | SQL_FN_TD_DAYOFYEAR | SQL_FN_TD_MONTH |
    SQL_FN_TD_QUARTER | SQL_FN_TD_WEEK | SQL_FN_TD_YEAR | SQL_FN_TD_CURTIME |
    SQL_FN_TD_HOUR | SQL_FN_TD_MINUTE | SQL_FN_TD_SECOND |
    SQL_FN_TD_TIMESTAMPADD | SQL_FN_TD_TIMESTAMPDIFF | SQL_FN_TD_DAYNAME |
    SQL_FN_TD_MONTHNAME | SQL_FN_TD_CURRENT_DATE | SQL_FN_TD_CURRENT_TIME |
    SQL_FN_TD_CURRENT_TIMESTAMP | SQL_FN_TD_EXTRACT;
constexpr SQLUINTEGER kTimeDateAddIntervals =
    SQL_FN_TSI_SECOND | SQL_FN_TSI_MINUTE | SQL_FN_TSI_HOUR | SQL_FN_TSI_DAY |
    SQL_FN_TSI_WEEK | SQL_FN_TSI_MONTH | SQL_FN_TSI_QUARTER | SQL_FN_TSI_YEAR;
constexpr SQLUINTEGER kTimeDateDiffIntervals =
    SQL_FN_TSI_SECOND | SQL_FN_TSI_MINUTE | SQL_FN_TSI_HOUR | SQL_FN_TSI_DAY |
    SQL_FN_TSI_MONTH | SQL_FN_TSI_QUARTER | SQL_FN_TSI_YEAR;
constexpr SQLUINTEGER kSql92StrFns =
    SQL_SSF_LOWER | SQL_SSF_UPPER | SQL_SSF_SUBSTRING | SQL_SSF_TRANSLATE |
    SQL_SSF_TRIM_BOTH | SQL_SSF_TRIM_LEADING | SQL_SSF_TRIM_TRAILING;
constexpr SQLUINTEGER kSubQueries = SQL_SQ_COMPARISON | SQL_SQ_EXISTS |
                                    SQL_SQ_IN | SQL_SQ_QUANTIFIED |
                                    SQL_SQ_CORRELATED_SUBQUERIES;
constexpr SQLUINTEGER kStrFns =
    SQL_FN_STR_ASCII | SQL_FN_STR_CHAR | SQL_FN_STR_CHAR_LENGTH |
    SQL_FN_STR_CHARACTER_LENGTH | SQL_FN_STR_CONCAT | SQL_FN_STR_LENGTH |
    SQL_FN_STR_LTRIM | SQL_FN_STR_OCTET_LENGTH | SQL_FN_STR_POSITION |
    SQL_FN_STR_REPEAT | SQL_FN_STR_REPLACE | SQL_FN_STR_RTRIM |
    SQL_FN_STR_SUBSTRING;
constexpr SQLUINTEGER kPredicates =
    SQL_SP_EXISTS | SQL_SP_ISNOTNULL | SQL_SP_ISNULL | SQL_SP_UNIQUE |
    SQL_SP_LIKE | SQL_SP_IN | SQL_SP_BETWEEN | SQL_SP_COMPARISON |
    SQL_SP_QUANTIFIED_COMPARISON;
constexpr SQLUINTEGER kJoinOperators =
    SQL_SRJO_CROSS_JOIN | SQL_SRJO_FULL_OUTER_JOIN | SQL_SRJO_INNER_JOIN |
    SQL_SRJO_LEFT_OUTER_JOIN | SQL_SRJO_RIGHT_OUTER_JOIN;
constexpr SQLUINTEGER kNumericFns =
    SQL_FN_NUM_ABS | SQL_FN_NUM_ACOS | SQL_FN_NUM_ASIN | SQL_FN_NUM_ATAN |
    SQL_FN_NUM_ATAN2 | SQL_FN_NUM_CEILING | SQL_FN_NUM_COS | SQL_FN_NUM_COT |
    SQL_FN_NUM_EXP | SQL_FN_NUM_FLOOR | SQL_FN_NUM_LOG | SQL_FN_NUM_LOG10 |
    SQL_FN_NUM_MOD | SQL_FN_NUM_POWER | SQL_FN_NUM_ROUND | SQL_FN_NUM_SIGN |
    SQL_FN_NUM_SIN | SQL_FN_NUM_SQRT | SQL_FN_NUM_TAN | SQL_FN_NUM_TRUNCATE;

// Shared template functions that will be moved to a
// common data translation library.
template <typename ReturnType>
StatusOr<ReturnType> SupportedInfoType(SQLUSMALLINT info_type) {
  return ReturnType::GetSupportedInfoType(info_type);
}

template <typename ReturnType>
StatusOr<ReturnType> UnSupportedInfoType(SQLUSMALLINT info_type) {
  return ReturnType::GetUnSupportedInfoType(info_type);
}

struct SQLGetInfoSqlChar {
  static StatusOr<SQLGetInfoSqlChar> GetSupportedInfoType(
      SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoSqlChar> GetUnSupportedInfoType(
      SQLUSMALLINT info_type);

  SQLCHAR* info_val;
};

struct SQLGetInfoBitmask {
  static StatusOr<SQLGetInfoBitmask> GetSupportedInfoType(
      SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoBitmask> GetUnSupportedInfoType(
      SQLUSMALLINT info_type);

  SQLUINTEGER info_val;
};

struct SQLGetInfoSqlUInt {
  static StatusOr<SQLGetInfoSqlUInt> GetSupportedInfoType(
      SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoSqlUInt> GetUnSupportedInfoType(
      SQLUSMALLINT info_type);

  SQLUINTEGER info_val;
};

struct SQLGetInfoSqlUSmallInt {
  static StatusOr<SQLGetInfoSqlUSmallInt> GetSupportedInfoType(
      SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoSqlUSmallInt> GetUnSupportedInfoType(
      SQLUSMALLINT info_type);

  SQLUSMALLINT info_val;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_INFO_H
