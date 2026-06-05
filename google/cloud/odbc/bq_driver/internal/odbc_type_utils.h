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

#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include <cstdint>
#include <cstring>
#include <map>
#include <string_view>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

struct DataBuffer {
  // C data type of the data the application expects
  SQLSMALLINT type;

  // Pointer to the buffer provided by the application
  SQLPOINTER buf;

  // Length of the buffer provided by the application
  SQLLEN buflen;

  // Length of the result populated by the driver
  SQLLEN* result_len;
};

struct Interval {
  SQLSMALLINT concise_sql_type;
  SQLSMALLINT concise_c_type;
  SQLSMALLINT datetime_interval_code;
};

static std::vector<Interval> const kDatetimeTypes = {
    {SQL_TYPE_DATE, SQL_C_TYPE_DATE, SQL_CODE_DATE},
    {SQL_TYPE_TIME, SQL_C_TYPE_TIME, SQL_CODE_TIME},
    {SQL_TYPE_TIMESTAMP, SQL_C_TYPE_TIMESTAMP, SQL_CODE_TIMESTAMP},
};

static std::vector<Interval> const kIntervalTypes = {
    {SQL_INTERVAL_MONTH, SQL_C_INTERVAL_MONTH, SQL_CODE_MONTH},
    {SQL_INTERVAL_YEAR, SQL_C_INTERVAL_YEAR, SQL_CODE_YEAR},
    {SQL_INTERVAL_YEAR_TO_MONTH, SQL_C_INTERVAL_YEAR_TO_MONTH,
     SQL_CODE_YEAR_TO_MONTH},
    {SQL_INTERVAL_DAY, SQL_C_INTERVAL_DAY, SQL_CODE_DAY},
    {SQL_INTERVAL_HOUR, SQL_C_INTERVAL_HOUR, SQL_CODE_HOUR},
    {SQL_INTERVAL_MINUTE, SQL_C_INTERVAL_MINUTE, SQL_CODE_MINUTE},
    {SQL_INTERVAL_SECOND, SQL_C_INTERVAL_SECOND, SQL_CODE_SECOND},
    {SQL_INTERVAL_DAY_TO_HOUR, SQL_C_INTERVAL_DAY_TO_HOUR,
     SQL_CODE_DAY_TO_HOUR},
    {SQL_INTERVAL_DAY_TO_MINUTE, SQL_C_INTERVAL_DAY_TO_MINUTE,
     SQL_CODE_DAY_TO_MINUTE},
    {SQL_INTERVAL_DAY_TO_SECOND, SQL_C_INTERVAL_DAY_TO_SECOND,
     SQL_CODE_DAY_TO_SECOND},
    {SQL_INTERVAL_HOUR_TO_MINUTE, SQL_C_INTERVAL_HOUR_TO_MINUTE,
     SQL_CODE_HOUR_TO_MINUTE},
    {SQL_INTERVAL_HOUR_TO_SECOND, SQL_C_INTERVAL_HOUR_TO_SECOND,
     SQL_CODE_HOUR_TO_SECOND},
    {SQL_INTERVAL_MINUTE_TO_SECOND, SQL_C_INTERVAL_MINUTE_TO_SECOND,
     SQL_CODE_MINUTE_TO_SECOND},
};

static std::vector<int> const kOtherSQLSupportedTypes = {
    SQL_CHAR,     SQL_VARCHAR,      SQL_LONGVARCHAR,   SQL_WCHAR,
    SQL_WVARCHAR, SQL_WLONGVARCHAR, SQL_DECIMAL,       SQL_NUMERIC,
    SQL_SMALLINT, SQL_INTEGER,      SQL_REAL,          SQL_FLOAT,
    SQL_DOUBLE,   SQL_BIT,          SQL_TINYINT,       SQL_BIGINT,
    SQL_BINARY,   SQL_VARBINARY,    SQL_LONGVARBINARY, SQL_GUID};

static std::vector<int> const kOtherCSupportedTypes = {
    SQL_C_CHAR,    SQL_C_WCHAR,    SQL_C_SSHORT,      SQL_C_USHORT,
    SQL_C_SLONG,   SQL_C_ULONG,    SQL_C_FLOAT,       SQL_C_DOUBLE,
    SQL_C_BIT,     SQL_C_STINYINT, SQL_C_UTINYINT,    SQL_C_SBIGINT,
    SQL_C_UBIGINT, SQL_C_BINARY,   SQL_C_VARBOOKMARK, SQL_C_NUMERIC,
    SQL_C_GUID};

// NOLINTBEGIN(performance-no-int-to-ptr)
template <typename T>
inline SQLPOINTER ToSqlPointer(T x) {
  return reinterpret_cast<SQLPOINTER>(x);
}
// NOLINTEND(performance-no-int-to-ptr)

// U usually can be SQLINTEGER, SQLSMALLINT or SQLLEN
template <typename U>
odbc_internal::StatusRecord StringValueToOutputBufferResponse(
    std::string_view src, SQLPOINTER buffer_ptr, U buffer_len, U* str_len_ptr) {
  auto src_len = src.length();
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
    std::memcpy(dest, src.data(), src_len);
    dest[src_len] = '\0';
  } else {
    std::memcpy(dest, src.data(), (buffer_len - 1));
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

inline odbc_internal::StatusRecord StringValueToOutputBufferResponse(
    std::string_view src, DataBuffer& dest_data) {
  return StringValueToOutputBufferResponse<SQLLEN>(
      src, dest_data.buf, dest_data.buflen, dest_data.result_len);
}

inline odbc_internal::StatusRecord TimestampToOutputBufferResponse(
    const SQL_TIMESTAMP_STRUCT& conn_timestamp, SQLPOINTER dest_buf,
    SQLLEN* result_len) {
  auto* dest_timestamp = reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(dest_buf);

  if (result_len) {
    *result_len = sizeof(SQL_TIMESTAMP_STRUCT);
  }

  dest_timestamp->year = conn_timestamp.year;
  dest_timestamp->month = conn_timestamp.month;
  dest_timestamp->day = conn_timestamp.day;
  dest_timestamp->hour = conn_timestamp.hour;
  dest_timestamp->minute = conn_timestamp.minute;
  dest_timestamp->second = conn_timestamp.second;
  dest_timestamp->fraction = conn_timestamp.fraction;

  return odbc_internal::StatusRecord::Ok();
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

// Writes `count` wide characters from `src` directly into `dest` using the
// current wire encoding. `dest` must point to caller-owned storage of at
// least `count * WireWcharSize()` bytes. The caller writes its own NUL
// terminator (`WriteWireNul` below) if it wants one.
//
// When the wire SQLWCHAR width matches `sizeof(wchar_t)` (Windows and the
// iODBC build — the common case), this is a single memcpy of the wstring's
// raw bytes
inline void WriteWideToWireBuffer(std::wstring const& src, void* dest,
                                  size_t count) {
  if (count > src.size()) count = src.size();
#if !defined(_WIN32)
  if (IsRuntimeWireUtf16Le()) {
    auto* d = static_cast<uint16_t*>(dest);
    for (size_t i = 0; i < count; ++i) {
      d[i] = static_cast<uint16_t>(src[i]);
    }
    return;
  }
  if constexpr (sizeof(SQLWCHAR) != sizeof(wchar_t)) {
    // e.g. unixODBC: SQLWCHAR is 2 bytes, wchar_t is 4 bytes
    auto* d = static_cast<SQLWCHAR*>(dest);
    for (size_t i = 0; i < count; ++i) {
      d[i] = static_cast<SQLWCHAR>(src[i]);
    }
    return;
  }
#endif
  // Wire SQLWCHAR width matches wchar_t — single memcpy.
  std::memcpy(dest, src.data(), count * sizeof(SQLWCHAR));
}

// Writes a single wire-format NUL terminator (one code unit, 2 or 4 bytes)
// at byte offset `char_index * WireWcharSize()` from `dest`.
inline void WriteWireNul(void* dest, size_t char_index) {
  size_t const wire_sz = WireWcharSize();
  std::memset(static_cast<uint8_t*>(dest) + (char_index * wire_sz), 0, wire_sz);
}

inline odbc_internal::StatusRecord WStrToOutputBufferResponse(
    std::wstring const& wstr, SQLPOINTER dest_buf, SQLLEN buffer_length,
    SQLINTEGER src_len, SQLINTEGER supp_max_len, SQLLEN* res_len) {
  auto status_record = odbc_internal::StatusRecord::Ok();
  size_t const wire_sz = WireWcharSize();

  if (wstr.empty()) {
    if (dest_buf && buffer_length > 0) {
      WriteWireNul(dest_buf, 0);
    }
    if (res_len) {
      *res_len = 0;
    }
    return status_record;
  }

  if (buffer_length > src_len) {
    if (res_len) {
      *res_len = src_len * static_cast<SQLLEN>(wire_sz);
    }
    WriteWideToWireBuffer(wstr, dest_buf, src_len);
    WriteWireNul(dest_buf, src_len);
  } else if (supp_max_len <= buffer_length && buffer_length <= src_len) {
    if (res_len) {
      *res_len = buffer_length * static_cast<SQLLEN>(wire_sz);
    }
    WriteWideToWireBuffer(wstr, dest_buf, buffer_length - 1);
    WriteWireNul(dest_buf, buffer_length - 1);
    status_record = odbc_internal::StatusRecord{
        google::cloud::odbc_internal::SQLStates::k_01004(), "Data truncated"};
  } else {
    status_record = odbc_internal::StatusRecord{
        google::cloud::odbc_internal::SQLStates::k_22003(),
        "Buffer length is insufficient"};
  }
  return status_record;
}

SQLRETURN AddressToPointer(SQLPOINTER ptr, SQLPOINTER out_buf,
                           SQLINTEGER* str_len_ptr);

SQLRETURN AddressToPointer(SQLPOINTER ptr, SQLPOINTER out_buf,
                           SQLSMALLINT* str_len_ptr);

odbc_internal::StatusRecord WStrIntervalBufferResponse(
    std::wstring const& wstr, SQLPOINTER dest_buf, SQLLEN buffer_length,
    SQLINTEGER char_len, SQLINTEGER whole_digits_count, SQLLEN* res_len);
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_TYPE_UTILS_H
