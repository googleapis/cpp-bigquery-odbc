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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_TYPE_UTILS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_TYPE_UTILS_H

#include "google/cloud/odbc/internal/diagnostic_records.h"

namespace google::cloud::odbc_bq_driver_internal {

odbc_internal::StatusRecord WriteStringToBufferOutput(char const* src,
                                                      SQLPOINTER buffer_ptr,
                                                      SQLSMALLINT buffer_len,
                                                      SQLSMALLINT* str_len_ptr);

template <typename T>
SQLRETURN WriteToBufferOutput(T val, SQLPOINTER buffer_ptr,
                              SQLSMALLINT* str_len_ptr) {
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLSMALLINT>(sizeof(T));
  }
  if (buffer_ptr) {
    auto* val_ptr = reinterpret_cast<T*>(buffer_ptr);
    *val_ptr = val;
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_TYPE_UTILS_H
