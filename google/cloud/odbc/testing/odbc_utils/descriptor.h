// Copyright 2024 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_DESCRIPTOR_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_DESCRIPTOR_H

#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

inline constexpr int kPrecisionUnchanged = 111;
inline constexpr int kScaleUnchanged = 112;
inline constexpr int kDatetimePrecisionUnchanged = 113;
inline constexpr int kLength = 114;
inline constexpr int kDatetimeCodeUnchanged = SQL_CODE_MINUTE_TO_SECOND;

void RandomizeDefaultValues(SQLHDESC desc, SQLUSMALLINT param_number);

void GetDescField(SQLHDESC descriptor_handle, SQLSMALLINT rec_number,
                  SQLSMALLINT field_identifier, SQLPOINTER out_value,
                  SQLINTEGER value_buffer_len, SQLINTEGER* value_string_len,
                  bool use_ansi);

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_DESCRIPTOR_H
