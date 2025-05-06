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

std::vector<DateBasicTestStruct> FetchDateConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query);

std::vector<TimeBasicTestStruct> FetchTimeConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query);

std::vector<DateTimeBasicTestStruct> FetchDateTimeConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query);

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_DATA_TRANSLATION_UTILS_H
