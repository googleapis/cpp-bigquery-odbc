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
auto constexpr kSupportedCharY = "Y";
auto constexpr kCatalogSeparator = ".";
auto constexpr kCatalogTerm = "Project";
auto constexpr kDefaultCollation = "UTF-16LE_BINARY";
auto constexpr kDbmsName = "BigQuery";
auto constexpr kDbmsVer = "2";
auto constexpr kDriverName = "Google ODBC Driver For BigQuery";
auto constexpr kDriverOdbcVer = "03.80";
auto constexpr kDriverVer = "1.0.0.0000";
auto constexpr kIdentifierQuoteChar = "`";
auto constexpr kSchemaTerm = "Dataset";
auto constexpr kSearchPatternEscape = "\\";
auto constexpr kSqlServerName = "Google";
auto constexpr kSqlTableTerm = "Table";

// Constants specific to SQLGetInfo Information type
// values for SQLGetInfoSqlUSmallInt value.
auto constexpr kCatalogLocation = SQL_CL_START;
auto constexpr kCorrelationName = SQL_CN_ANY;
auto constexpr kCursorCommitBehavior = SQL_CB_CLOSE;
auto constexpr kCursorRollbackBehavior = SQL_CB_CLOSE;
auto constexpr kGroupBy = SQL_GB_GROUP_BY_CONTAINS_SELECT;
auto constexpr kIdentifierCase = SQL_IC_SENSITIVE;
auto constexpr kMaxCatalogNameLen = 128;
auto constexpr kMaxColsInTable = 10000;
auto constexpr kMaxColNameLen = 128;
auto constexpr kMaxIdentifierLen = 255;
auto constexpr kMaxSchemaNameLen = 1024;
auto constexpr kMaxTablesInSelect = 1000;
auto constexpr kMaxTableNameLen = 1024;
auto constexpr kNullCollation = SQL_NC_LOW;
auto constexpr kQuotedIdentifierCase = SQL_IC_SENSITIVE;
auto constexpr kTxnCapable = SQL_TC_DML;

// Constants specific to SQLGetInfo Information type
// values for SQLGetInfoSqlUInteger value.
auto constexpr kAsyncMode = SQL_AM_STATEMENT;
auto constexpr kDefaultTxnIsolation = SQL_TXN_SERIALIZABLE;
auto constexpr kOdbcInterfaceConformance = SQL_OIC_CORE;
auto constexpr kSqlConformance = SQL_OIC_CORE;

// Constants specific to SQLGetInfo Information type
// values for SQLGetInfoSqlUInteger bitmask value.

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
