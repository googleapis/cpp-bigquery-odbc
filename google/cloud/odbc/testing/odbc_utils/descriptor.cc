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

#include "google/cloud/odbc/testing/odbc_utils/descriptor.h"
#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

void RandomizeDefaultValues(SQLHDESC desc, SQLUSMALLINT param_number) {
  SQLRETURN status;
  status = SQLSetDescField(desc, param_number, SQL_DESC_PRECISION,
                           (SQLPOINTER)kPrecisionUnchanged, NULL);
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails("SetDescField", desc, SQL_HANDLE_DESC);
    throw std::runtime_error("SetDescField failed with status: " +
                             std::to_string(status));
  }
  status = SQLSetDescField(desc, param_number, SQL_DESC_SCALE,
                           (SQLPOINTER)kScaleUnchanged, NULL);
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails("SetDescField", desc, SQL_HANDLE_DESC);
    throw std::runtime_error("SetDescField failed with status: " +
                             std::to_string(status));
  }
  status =
      SQLSetDescField(desc, param_number, SQL_DESC_DATETIME_INTERVAL_PRECISION,
                      (SQLPOINTER)kDatetimePrecisionUnchanged, NULL);
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails("SetDescField", desc, SQL_HANDLE_DESC);
    throw std::runtime_error("SetDescField failed with status: " +
                             std::to_string(status));
  }
  status = SQLSetDescField(desc, param_number, SQL_DESC_LENGTH,
                           (SQLPOINTER)kLengthUnchanged, NULL);
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails("SetDescField", desc, SQL_HANDLE_DESC);
    throw std::runtime_error("SetDescField failed with status: " +
                             std::to_string(status));
  }
  status = SQLSetDescField(desc, param_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                           (SQLPOINTER)kDatetimeCodeUnchanged, NULL);
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails("SetDescField", desc, SQL_HANDLE_DESC);
    throw std::runtime_error("SetDescField failed with status: " +
                             std::to_string(status));
  }
}

void GetDescField(SQLHDESC descriptor_handle, SQLSMALLINT rec_number,
                  SQLSMALLINT field_identifier, SQLPOINTER out_value,
                  SQLINTEGER value_buffer_len, SQLINTEGER* value_string_len,
                  bool use_ansi) {
  SQLRETURN status;
  if (use_ansi) {
    status = SQLGetDescFieldA(descriptor_handle, rec_number, field_identifier,
                              out_value, value_buffer_len, value_string_len);
  } else {
    status = SQLGetDescField(descriptor_handle, rec_number, field_identifier,
                             out_value, value_buffer_len, value_string_len);
  }
  if (!SQL_SUCCEEDED(status)) {
    GetErrorDetails("GetDescField", descriptor_handle, SQL_HANDLE_DESC,
                    use_ansi);
    throw std::runtime_error("GetDescField failed with status: " +
                             std::to_string(status));
  }
}

void CheckType(SQLHDESC desc, SQLSMALLINT type_expected, bool use_ansi) {
  SQLSMALLINT type;
  GetDescField(desc, 1, SQL_DESC_TYPE, &type, 0, NULL, use_ansi);
  EXPECT_EQ(type_expected, type);
}

void CheckConciseType(SQLHDESC desc, SQLSMALLINT concise_type_expected,
                      bool use_ansi) {
  SQLSMALLINT concise_type;
  GetDescField(desc, 1, SQL_DESC_CONCISE_TYPE, &concise_type, 0, NULL,
               use_ansi);
  EXPECT_EQ(concise_type_expected, concise_type);
}

void CheckDatetimeIntervalPrecision(
    SQLHDESC desc, SQLSMALLINT datetime_interval_precision_expected,
    bool use_ansi) {
  SQLINTEGER datetime_interval_precision = 0;
  GetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_PRECISION,
               &datetime_interval_precision, 0, NULL, use_ansi);
  EXPECT_EQ(datetime_interval_precision_expected, datetime_interval_precision);
}

void CheckPrecision(SQLHDESC desc, SQLSMALLINT precision_expected,
                    bool use_ansi) {
  SQLSMALLINT precision = 0;
  GetDescField(desc, 1, SQL_DESC_PRECISION, &precision, 0, NULL, use_ansi);
  EXPECT_EQ(precision_expected, precision);
}

void CheckLength(SQLHDESC desc, SQLULEN length_expected, bool use_ansi) {
  SQLULEN length = 0;
  GetDescField(desc, 1, SQL_DESC_LENGTH, &length, 0, NULL, use_ansi);
  //  std::cout << "For i = " + std::to_string(length_expected) + " actual value
  //  is " + std::to_string(length) + "\n";
  EXPECT_EQ(length_expected, length);
}

void CheckDatetimeIntervalCode(SQLHDESC desc,
                               SQLSMALLINT datetime_interval_code_expected,
                               bool use_ansi) {
  SQLSMALLINT datetime_interval_code = 0;
  GetDescField(desc, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
               &datetime_interval_code, 0, NULL, use_ansi);
  EXPECT_EQ(datetime_interval_code_expected, datetime_interval_code);
}

void CheckScale(SQLHDESC desc, SQLSMALLINT scale_expected, bool use_ansi) {
  SQLSMALLINT scale = 0;
  GetDescField(desc, 1, SQL_DESC_SCALE, &scale, 0, NULL, use_ansi);
  EXPECT_EQ(scale_expected, scale);
}

}  // namespace google::cloud::odbc_tests
