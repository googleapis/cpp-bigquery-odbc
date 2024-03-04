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

#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include <cstring>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

StatusRecord WriteStringToBufferOutput(char const* src, SQLPOINTER buffer_ptr,
                                       SQLSMALLINT buffer_len,
                                       SQLSMALLINT* str_len_ptr) {
  SQLSMALLINT src_len = strlen(src);
  *str_len_ptr = static_cast<SQLSMALLINT>(src_len);
  if (!buffer_ptr) {
    return StatusRecord::Ok();
  }

  char* dest = reinterpret_cast<char*>(buffer_ptr);

  if (src_len == 0 || buffer_len == 0) {
    *dest = '\0';
  } else if (src_len < buffer_len) {
    strncpy(dest, src, src_len);
    dest[src_len] = '\0';
  } else {
    strncpy(dest, src, (buffer_len - 1));
    dest[buffer_len - 1] = '\0';
    return StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
  }
  return StatusRecord::Ok();
}

SQLRETURN WriteSQLINTEGERToBufferOutput(SQLINTEGER val, SQLPOINTER buffer_ptr,
                                        SQLSMALLINT* str_len_ptr) {
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLSMALLINT>(sizeof(SQLINTEGER));
  }
  if (buffer_ptr) {
    auto* val_ptr = reinterpret_cast<SQLINTEGER*>(buffer_ptr);
    *val_ptr = val;
  }
  return SQL_SUCCESS;
}

SQLRETURN WriteSQLLENToBufferOutput(SQLLEN val, SQLPOINTER buffer_ptr,
                                    SQLSMALLINT* str_len_ptr) {
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLSMALLINT>(sizeof(SQLLEN));
  }
  if (buffer_ptr) {
    auto* val_ptr = reinterpret_cast<SQLLEN*>(buffer_ptr);
    *val_ptr = val;
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver_internal
