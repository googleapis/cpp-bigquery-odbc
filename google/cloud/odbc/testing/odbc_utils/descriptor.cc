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

namespace google::cloud::odbc_tests {

SQLRETURN GetDescField(SQLHDESC descriptor_handle, SQLSMALLINT rec_number,
                       SQLSMALLINT field_identifier, SQLPOINTER out_value,
                       SQLINTEGER value_buffer_len,
                       SQLINTEGER* value_string_len, bool use_ansi) {
  if (use_ansi) {
    return SQLGetDescFieldA(descriptor_handle, rec_number, field_identifier,
                            out_value, value_buffer_len, value_string_len);
  }
  return SQLGetDescField(descriptor_handle, rec_number, field_identifier,
                         out_value, value_buffer_len, value_string_len);
}

}  // namespace google::cloud::odbc_tests
