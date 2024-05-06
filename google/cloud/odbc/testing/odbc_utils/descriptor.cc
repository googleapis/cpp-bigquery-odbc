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
                           (SQLPOINTER)kLength, NULL);
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

}  // namespace google::cloud::odbc_tests
