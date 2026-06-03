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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

// During program execution out_buf is never NULL
SQLRETURN AddressToPointer(SQLPOINTER ptr, SQLPOINTER out_buf,
                           SQLINTEGER* str_len_ptr) {
  if (out_buf) {
    *(static_cast<SQLPOINTER*>(out_buf)) = ptr;
  }
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLINTEGER>(sizeof(SQLPOINTER));
  }
  return SQL_SUCCESS;
}

// During program execution out_buf is never NULL
SQLRETURN AddressToPointer(SQLPOINTER ptr, SQLPOINTER out_buf,
                           SQLSMALLINT* str_len_ptr) {
  if (out_buf) {
    *(static_cast<SQLPOINTER*>(out_buf)) = ptr;
  }
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLSMALLINT>(sizeof(SQLPOINTER));
  }
  return SQL_SUCCESS;
}

odbc_internal::StatusRecord WStrIntervalBufferResponse(
    std::wstring const& wstr, SQLPOINTER dest_buf, SQLLEN buffer_length,
    SQLINTEGER char_len, SQLINTEGER whole_digits_count, SQLLEN* res_len) {
  auto status_record = odbc_internal::StatusRecord::Ok();
  size_t const wire_sz = WireWcharSize();

  if (buffer_length > char_len) {
    if (res_len) {
      *res_len = static_cast<SQLLEN>(char_len) * static_cast<SQLLEN>(wire_sz);
    }
    WriteWideToWireBuffer(wstr, dest_buf, static_cast<size_t>(char_len));
    WriteWireNul(dest_buf, static_cast<size_t>(char_len));
  } else if (buffer_length > whole_digits_count) {
    if (res_len) {
      *res_len = buffer_length * static_cast<SQLLEN>(wire_sz);
    }
    WriteWideToWireBuffer(wstr, dest_buf,
                          static_cast<size_t>(buffer_length - 1));
    WriteWireNul(dest_buf, static_cast<size_t>(buffer_length - 1));
    status_record = odbc_internal::StatusRecord{
        google::cloud::odbc_internal::SQLStates::k_01004(), "Data truncated"};
  } else {
    LOG(ERROR) << "WStrIntervalBufferResponse:: Buffer length is insufficient.";
    status_record = odbc_internal::StatusRecord{
        google::cloud::odbc_internal::SQLStates::k_22003(),
        "Buffer length is insufficient"};
  }
  return status_record;
}
}  // namespace google::cloud::odbc_bq_driver_internal
