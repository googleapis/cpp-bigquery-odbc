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

// Shared template functions that will be moved to a
// common data translation library.
template <typename ReturnType>
StatusOr<ReturnType> SupportedInfoType(const SQLUSMALLINT info_type) {
  return ReturnType::GetSupportedInfoType(info_type);
}

template <typename ReturnType>
StatusOr<ReturnType> UnSupportedInfoType(const SQLUSMALLINT info_type) {
  return ReturnType::GetUnSupportedInfoType(info_type);
}

struct SQLGetInfoSqlChar {
  static StatusOr<SQLGetInfoSqlChar> GetSupportedInfoType(
      const SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoSqlChar> GetUnSupportedInfoType(
      const SQLUSMALLINT info_type);

  SQLCHAR* info_val;
};

struct SQLGetInfoBitmask {
  static StatusOr<SQLGetInfoBitmask> GetSupportedInfoType(
      const SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoBitmask> GetUnSupportedInfoType(
      const SQLUSMALLINT info_type);

  SQLUINTEGER info_val;
};

struct SQLGetInfoSqlUInt {
  static StatusOr<SQLGetInfoSqlUInt> GetSupportedInfoType(
      const SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoSqlUInt> GetUnSupportedInfoType(
      const SQLUSMALLINT info_type);

  SQLUINTEGER info_val;
};

struct SQLGetInfoSqlUSmallInt {
  static StatusOr<SQLGetInfoSqlUSmallInt> GetSupportedInfoType(
      const SQLUSMALLINT info_type);
  static StatusOr<SQLGetInfoSqlUSmallInt> GetUnSupportedInfoType(
      const SQLUSMALLINT info_type);

  SQLUSMALLINT info_val;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_INFO_H
