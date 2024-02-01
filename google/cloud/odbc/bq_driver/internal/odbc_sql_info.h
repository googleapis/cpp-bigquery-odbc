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
