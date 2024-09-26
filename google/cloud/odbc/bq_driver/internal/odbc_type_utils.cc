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
    *(static_cast<SQLPOINTER*>(out_buf)) = ptr;
  }
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLINTEGER>(sizeof(SQLPOINTER));
  }
  return SQL_SUCCESS;
}

odbc_internal::StatusRecord IntervalToOutputBufferResponse(
    const SQL_INTERVAL_STRUCT& conn_interval, SQLPOINTER dest_buf,
    SQLLEN buffer_length, SQLLEN* result_len) {
  auto status_record = odbc_internal::StatusRecord::Ok();
  auto* dest_interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_buf);
  if (buffer_length >= sizeof(SQL_INTERVAL_STRUCT)) {
    *dest_interval = conn_interval;
    if (result_len) {
      *result_len = sizeof(SQL_INTERVAL_STRUCT);
    }
    return status_record;
  }
  status_record = odbc_internal::StatusRecord{
      odbc_internal::SQLStates::k_01S07(), "Interval data, right truncated"};
  return status_record;
}

odbc_internal::StatusRecord WStrIntervalBufferResponse(
    std::wstring wstr, SQLPOINTER dest_buf, SQLLEN buffer_length,
    SQLINTEGER char_len, SQLINTEGER whole_digits_count, SQLLEN* res_len) {
  auto status_record = odbc_internal::StatusRecord::Ok();
  std::vector<SQLWCHAR> wstr_data(wstr.begin(), wstr.end());
  wstr_data.emplace_back(L'\0');

  auto* dest = static_cast<SQLWCHAR*>(dest_buf);
  if (buffer_length > char_len) {
    if (res_len) {
      *res_len = char_len * sizeof(SQLWCHAR);
    }
    std::memcpy(dest, wstr_data.data(), (char_len) * sizeof(SQLWCHAR));
  } else if (buffer_length > whole_digits_count) {
    if (res_len) {
      *res_len = buffer_length * sizeof(SQLWCHAR);
    }
    std::memcpy(dest, wstr_data.data(), (buffer_length) * sizeof(SQLWCHAR));
    status_record = odbc_internal::StatusRecord{
        google::cloud::odbc_internal::SQLStates::k_01004(), "Data truncated"};
  } else {
    status_record = odbc_internal::StatusRecord{
        google::cloud::odbc_internal::SQLStates::k_22003(),
        "Buffer length is insufficient"};
  }
  return status_record;
}
}  // namespace google::cloud::odbc_bq_driver_internal
