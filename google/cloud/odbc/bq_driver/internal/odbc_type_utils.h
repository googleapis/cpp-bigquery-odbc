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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_TYPE_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_TYPE_UTILS_H

#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include <cstring>

namespace google::cloud::odbc_bq_driver_internal {

// U usually can be SQLINTEGER and SQLSMALLINT
template <typename U>
odbc_internal::StatusRecord StringValueToOutputBufferResponse(
    char const* src, SQLPOINTER buffer_ptr, U buffer_len, U* str_len_ptr) {
  auto src_len = strlen(src);
  if (str_len_ptr) {
    *str_len_ptr = static_cast<U>(src_len);
  }
  if (!buffer_ptr) {
    return odbc_internal::StatusRecord::Ok();
  }
  if (buffer_len < 0) {
    return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY090(),
                                       "Buffer length is negative"};
  }

  char* dest = reinterpret_cast<char*>(buffer_ptr);
  auto status_record = odbc_internal::StatusRecord::Ok();

  if (src_len == 0 || buffer_len == 0) {
    *dest = '\0';
  } else if (src_len < buffer_len) {
    strncpy(dest, src, src_len);
    dest[src_len] = '\0';
  } else {
    strncpy(dest, src, (buffer_len - 1));
    dest[buffer_len - 1] = '\0';
    status_record = odbc_internal::StatusRecord{
        odbc_internal::SQLStates::k_01004(), "String data, right truncated"};
  }
  // Update the str_len_ptr to be that of the destination buffer
  // as per the spec.
  auto dest_len = strlen(dest);
  if (str_len_ptr) {
    *str_len_ptr = static_cast<U>(dest_len);
  }

  return status_record;
}

// T usually can be SQLINTEGER, SQLSMALLINT, SQLLEN, and it's unsigned values
// U usually can be SQLINTEGER and SQLSMALLINT
template <typename T, typename U>
SQLRETURN IntValueToOutputBufferResponse(T val, SQLPOINTER buffer_ptr,
                                         U* str_len_ptr) {
  if (str_len_ptr) {
    *str_len_ptr = static_cast<U>(sizeof(T));
  }
  if (buffer_ptr) {
    auto* val_ptr = reinterpret_cast<T*>(buffer_ptr);
    *val_ptr = val;
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_TYPE_UTILS_H
