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

namespace google::cloud::odbc_bq_driver_internal {

// During program execution out_buf is never NULL
SQLRETURN AddressToPointer(SQLPOINTER ptr, SQLPOINTER out_buf,
                           SQLINTEGER* str_len_ptr) {
  if (out_buf) {
    auto* buf = static_cast<std::size_t*>(out_buf);
    *buf = reinterpret_cast<std::size_t>(ptr);
  }
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLINTEGER>(sizeof(std::size_t));
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver_internal
