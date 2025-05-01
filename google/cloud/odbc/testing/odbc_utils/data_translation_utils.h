// Copyright 2025 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_DATA_TRANSLATION_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_DATA_TRANSLATION_UTILS_H

#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

struct DateBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_DATE_STRUCT value;
  // Optional string representation of the returned value
  std::optional<std::string> return_str_val;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

struct TimeBasicTestStruct {
  // The target C type
  SQLSMALLINT target_c_type;
  // The value that should be returned
  SQL_TIME_STRUCT value;
  // Optional string representation of the returned value
  std::optional<std::string> return_val_str;
  // The status that should be returned for this C Type
  SQLRETURN status;
};

struct DateTimeBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_TIMESTAMP_STRUCT value;
  // Optional string representation of the returned value
  std::optional<std::string> return_val_str;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

struct IntervalBasicTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // The value that should be returned by SQLGetData if it succeeds
  SQL_INTERVAL_STRUCT interval_value;
  // Optional string representation of the returned value
  std::optional<std::string> return_val_str;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

struct IntervalArthemeticTestStruct {
  // The target C type SQLGetData will convert SQL type to
  SQLSMALLINT target_c_type;
  // Union of possible C types that can represent result of interval arithmetic.
  union Value {
    int8_t int_t;
    uint8_t unit_t;
    SQLSMALLINT sql_smallint;
    SQLUSMALLINT sql_usmallint;
    SQLUINTEGER sql_uninteger;
    SQLBIGINT sql_bigint;
    SQL_NUMERIC_STRUCT numeric;
  } value;
  // The status that should be returned by SQLGetData for this C Type
  SQLRETURN status;
};

std::vector<DateBasicTestStruct> const kConversionFromDateTestData{
    {SQL_C_CHAR, {2024, 2, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_TYPE_DATE, {2024, 3, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2024, 4, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_WCHAR, {2024, 7, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_BINARY, {2024, 5, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_USHORT, {2024, 6, 20}, std::nullopt, SQL_ERROR},
    {SQL_C_DOUBLE, {2024, 1, 20}, std::nullopt, SQL_ERROR},
};

std::vector<TimeBasicTestStruct> const kConversionFromTimeTestData{
    {SQL_C_CHAR, {11, 20, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_TYPE_TIME, {22, 45, 54}, std::nullopt, SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP, {2, 36, 29}, std::nullopt, SQL_SUCCESS},
    {SQL_C_WCHAR, {19, 07, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_BINARY, {04, 06, 07}, std::nullopt, SQL_SUCCESS},
};

std::vector<DateTimeBasicTestStruct> const kConversionFromDateTimeTestData{
    {SQL_C_WCHAR,
     {2024, 02, 20, 10, 20, 30, 123112},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_BINARY,
     {2024, 03, 20, 00, 00, 00, 000000},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_TYPE_DATE,
     {2024, 04, 20, 10, 20, 30, 123112},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_TYPE_TIME,
     {2024, 05, 20, 10, 2, 30, 123112},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_TYPE_TIMESTAMP,
     {2024, 06, 20, 11, 2, 30, 12311},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_CHAR, {2024, 2, 20}, std::nullopt, SQL_SUCCESS},
    {SQL_C_SLONG, {2024, 01, 20, 10, 20, 30, 123112}, std::nullopt, SQL_ERROR},
    {SQL_C_DOUBLE, {2024, 1, 20}, std::nullopt, SQL_ERROR},
    {SQL_C_USHORT, {2024, 6, 20}, std::nullopt, SQL_ERROR},
};

// TODO(b/368251064): Remove designated identifiers to support C++17.
std::vector<IntervalBasicTestStruct> const kConversionYearMonthIntervalTestData{
    {SQL_C_CHAR,
     {SQL_IS_YEAR, 1, {.year_month = {3, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_YEAR,
     {SQL_IS_YEAR, 1, {.year_month = {5, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_MONTH,
     {SQL_IS_MONTH, 1, {.year_month = {0, 8}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_DOUBLE,
     {SQL_IS_YEAR, 1, {.year_month = {9, 0}}},
     std::nullopt,
     SQL_ERROR},
    {SQL_C_WCHAR,
     {SQL_IS_YEAR_TO_MONTH, 1, {.year_month = {2, 5}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_YEAR_TO_MONTH,
     {SQL_IS_YEAR_TO_MONTH, 1, {.year_month = {1, 6}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_FLOAT,
     {SQL_IS_MONTH, 1, {.year_month = {0, 9}}},
     std::nullopt,
     SQL_ERROR},
};

// TODO(b/368251064): Remove designated identifiers to support C++17.
std::vector<IntervalBasicTestStruct> const kConversionDaySecondIntervalTestData{
    {SQL_C_CHAR,
     {SQL_IS_DAY, 1, {.day_second = {5, 0, 0, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_WCHAR,
     {SQL_IS_HOUR, 1, {.day_second = {0, 2, 0, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_DAY,
     {SQL_IS_DAY, 1, {.day_second = {15, 0, 0, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_FLOAT,
     {SQL_IS_MINUTE, 1, {.day_second = {0, 0, 45, 0, 0}}},
     std::nullopt,
     SQL_ERROR},
    {SQL_C_INTERVAL_HOUR,
     {SQL_IS_HOUR, 1, {.day_second = {0, 20, 0, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_MINUTE,
     {SQL_IS_MINUTE, 1, {.day_second = {0, 0, 45, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_SECOND,
     {SQL_IS_SECOND, 1, {.day_second = {0, 0, 0, 10, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_DOUBLE,
     {SQL_IS_DAY_TO_HOUR, 1, {.day_second = {10, 14, 0, 0, 0}}},
     std::nullopt,
     SQL_ERROR},
    {SQL_C_INTERVAL_DAY_TO_HOUR,
     {SQL_IS_DAY_TO_HOUR, 1, {.day_second = {10, 14, 0, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_DAY_TO_MINUTE,
     {SQL_IS_DAY_TO_MINUTE, 1, {.day_second = {1, 5, 30, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_DAY_TO_SECOND,
     {SQL_IS_DAY_TO_SECOND, 1, {.day_second = {2, 1, 2, 20, 500}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_HOUR_TO_MINUTE,
     {SQL_IS_HOUR_TO_MINUTE, 1, {.day_second = {0, 9, 45, 0, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_INTERVAL_HOUR_TO_SECOND,
     {SQL_IS_HOUR_TO_SECOND, 1, {.day_second = {0, 11, 10, 25, 0}}},
     std::nullopt,
     SQL_SUCCESS},
    {SQL_C_BIT,
     {SQL_IS_DAY_TO_SECOND, 1, {.day_second = {2, 1, 2, 20, 500}}},
     std::nullopt,
     SQL_ERROR},
    {SQL_C_INTERVAL_MINUTE_TO_SECOND,
     {SQL_IS_MINUTE_TO_SECOND, 1, {.day_second = {0, 0, 50, 10, 100}}},
     std::nullopt,
     SQL_SUCCESS},
};

// TODO(b/368251064): Remove designated identifiers to support C++17.
std::vector<IntervalBasicTestStruct> const
    kConversionFromSinglePrecisionIntervalData{
        {SQL_C_STINYINT,
         {SQL_IS_YEAR, 1, {.year_month = {1, 0}}},
         std::nullopt,
         SQL_SUCCESS},
        {SQL_C_UTINYINT,
         {SQL_IS_DAY, 1, {.day_second = {6, 0, 0, 0, 0}}},
         std::nullopt,
         SQL_SUCCESS},
        {SQL_C_SSHORT,
         {SQL_IS_HOUR, 1, {.day_second = {0, 12, 0, 0, 0}}},
         std::nullopt,
         SQL_SUCCESS},
        {SQL_C_USHORT,
         {SQL_IS_MINUTE, 1, {.day_second = {0, 0, 20, 0, 0}}},
         std::nullopt,
         SQL_SUCCESS},
        {SQL_C_ULONG,
         {SQL_IS_DAY, 1, {.day_second = {4, 0, 0, 0, 0}}},
         std::nullopt,
         SQL_SUCCESS},
        {SQL_C_SBIGINT,
         {SQL_IS_HOUR, 1, {.day_second = {0, 3, 0, 0, 0}}},
         std::nullopt,
         SQL_SUCCESS},
        {SQL_C_NUMERIC,
         {SQL_IS_MONTH, 1, {.year_month = {0, 8}}},
         std::nullopt,
         SQL_SUCCESS},
    };

std::vector<DateBasicTestStruct> FetchDateConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query);

std::vector<TimeBasicTestStruct> FetchTimeConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query);

std::vector<DateTimeBasicTestStruct> FetchDateTimeConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query);

std::vector<IntervalArthemeticTestStruct> FetchIntervalArtheConvertResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query);

std::vector<IntervalBasicTestStruct> FetchIntervalConversionResults(
    std::shared_ptr<ODBCHandles> conn, std::string const& query,
    std::vector<IntervalBasicTestStruct> test_data);

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_DATA_TRANSLATION_UTILS_H
