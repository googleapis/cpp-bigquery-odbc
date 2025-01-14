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

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

StatusRecord ConvertFromJsonDSValue(DSValue const& src_dsval,
                                    DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);
  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  switch (dest_type) {
    case SQL_C_CHAR: {
      StatusRecord status_record =
          StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
      return status_record;
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wide_string = Utf8ToUtf16(src_str);
      if (!wide_string.Ok()) {
        StatusRecord status_record =
            StatusRecord{SQLStates::k_HY000(), "Conversion Failed"};
        break;
      }
      return WStrToOutputBufferResponse(
          wide_string.GetValue(), dest_buf, buffer_length, src_str.length(),
          src_str.length(), reinterpret_cast<SQLLEN*>(dest_data.result_len));
      break;
    }
    // TODO(b\367841053): SQL_C_BINARY to be done later
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

template <typename T>
inline void HandleIntervalArthmeticConversion(
    T* dest_buf, SQL_INTERVAL_STRUCT interval, SQLLEN* res_len,
    odbc_internal::StatusRecord& status_record) {
  SQLUINTEGER value = 0;
  GetSinglePrecisionInterval(interval, value);
  // Get the maximum value for the type T.
  if (value <= static_cast<SQLUINTEGER>(std::numeric_limits<T>::max())) {
    *dest_buf = static_cast<T>(value);
    if (res_len) {
      *res_len = sizeof(T);
    }
  } else {
    status_record = odbc_internal::StatusRecord{
        odbc_internal::SQLStates::k_22003(), "Data truncated"};
  }
}

odbc_internal::StatusRecord ConvertFromIntervalDSValue(DSValue const& src_dsval,
                                                       DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  SQL_INTERVAL_STRUCT conn_interval = {};
  std::string interval_src_str;

  DSValueToString(src_dsval, interval_src_str);
  ConvertStringToIntervalStruct(interval_src_str, conn_interval);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  auto* res_len = reinterpret_cast<SQLLEN*>(dest_data.result_len);

  constexpr int kIntervalCharLength = 30;
  int interval_src_len = interval_src_str.length();

  if (!dest_buf) {
    return StatusRecord::Ok();
  }
  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  StatusRecord status_record = StatusRecord::Ok();
  switch (dest_type) {
    case SQL_C_CHAR: {
      char* dest = reinterpret_cast<char*>(dest_buf);
      auto whole_digit_count = GetWholeDigitCount(interval_src_str);
      if (buffer_length > kIntervalCharLength) {
        if (res_len) {
          *res_len = interval_src_str.length();
        }
        std::strncpy(dest, interval_src_str.c_str(), interval_src_len);
        dest[interval_src_len] = '\0';
      } else if (buffer_length > whole_digit_count) {
        if (res_len) {
          *res_len = interval_src_str.length();
        }
        std::strncpy(dest, interval_src_str.c_str(), interval_src_len);
        dest[interval_src_len] = '\0';
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        strncpy(dest, "Y-M D H:M:S", buffer_length - 1);
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(interval_src_str);
      int interval_char_length = wstr.GetValue().length();
      auto whole_digit_count = GetWholeDigitCount(interval_src_str);
      if (!wstr) {
        status_record =
            StatusRecord{SQLStates::k_HY000(), wstr.GetStatusRecord().message};
        break;
      }
      return WStrIntervalBufferResponse(
          wstr.GetValue(), dest_buf, buffer_length, interval_char_length,
          whole_digit_count, reinterpret_cast<SQLLEN*>(dest_data.result_len));
      break;
    }
    case SQL_C_STINYINT: {
      auto* dest = reinterpret_cast<SQLSCHAR*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLSCHAR>(dest, conn_interval, res_len,
                                                  status_record);
      break;
    }
    case SQL_C_UTINYINT: {
      auto* dest = reinterpret_cast<SQLCHAR*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLCHAR>(dest, conn_interval, res_len,
                                                 status_record);
      break;
    }
    case SQL_C_SSHORT: {
      auto* dest = reinterpret_cast<SQLSMALLINT*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLSMALLINT>(dest, conn_interval,
                                                     res_len, status_record);
      break;
    }
    case SQL_C_USHORT: {
      auto* dest = reinterpret_cast<SQLUSMALLINT*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLUSMALLINT>(dest, conn_interval,
                                                      res_len, status_record);
      break;
    }
    case SQL_C_ULONG: {
      auto* dest = reinterpret_cast<SQLUINTEGER*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLUINTEGER>(dest, conn_interval,
                                                     res_len, status_record);
      break;
    }
    case SQL_C_SBIGINT: {
      auto* dest = reinterpret_cast<SQLBIGINT*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLBIGINT>(dest, conn_interval, res_len,
                                                   status_record);
      break;
    }
    case SQL_C_NUMERIC: {
      auto* dest = reinterpret_cast<SQL_NUMERIC_STRUCT*>(dest_buf);
      SQLUINTEGER value = 0;
      GetSinglePrecisionInterval(conn_interval, value);
      std::memset(dest, 0, sizeof(SQL_NUMERIC_STRUCT));
      dest->sign = 1;

      int byte_index = 0;
      while (value > 0 && byte_index < SQL_MAX_NUMERIC_LEN) {
        dest->val[byte_index++] = static_cast<SQLCHAR>(value & 0xFF);
        value >>= 8;
      }
      if (value > 0) {
        status_record =
            odbc_internal::StatusRecord{odbc_internal::SQLStates::k_22003(),
                                        "Value out of range for SQL_C_NUMERIC"};
      } else {
        dest->precision = 10;
        dest->scale = 0;
        if (res_len) {
          *res_len = sizeof(SQL_NUMERIC_STRUCT);
        }
      }
      break;
    }
    case SQL_C_INTERVAL_YEAR:
    case SQL_C_INTERVAL_MONTH:
    case SQL_C_INTERVAL_DAY:
    case SQL_C_INTERVAL_HOUR:
    case SQL_C_INTERVAL_MINUTE:
    case SQL_C_INTERVAL_SECOND:
    case SQL_C_INTERVAL_YEAR_TO_MONTH:
    case SQL_C_INTERVAL_DAY_TO_HOUR:
    case SQL_C_INTERVAL_DAY_TO_MINUTE:
    case SQL_C_INTERVAL_DAY_TO_SECOND:
    case SQL_C_INTERVAL_HOUR_TO_MINUTE:
    case SQL_C_INTERVAL_HOUR_TO_SECOND:
    case SQL_C_INTERVAL_MINUTE_TO_SECOND: {
      if (kIntervalCharLength < buffer_length) {
        return IntervalToOutputBufferResponse(conn_interval, dest_buf,
                                              buffer_length, res_len);
      }
      status_record = StatusRecord{SQLStates::k_01S07(), "Data truncated"};
      break;
    }
    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
  return status_record;
}

StatusRecord ConvertFromBooleanDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  bool conn_bool = false; 

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;

  if (!dest_buf) {
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  DSValueToBoolean(src_dsval, conn_bool);
  StatusRecord status_record = StatusRecord::Ok();
  switch (dest_type) {

   case SQL_C_CHAR: {
      auto* dest = reinterpret_cast<char*>(dest_buf);
      if (buffer_length < 2) { 
        if (buffer_length > 0) {
          dest[0] = '\0'; 
        }
        status_record = StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
      } else {
        dest[0] = conn_bool ? '1' : '0';
        dest[1] = '\0'; 
      }
      break;
    }

    case SQL_C_WCHAR: {
      auto* dest = reinterpret_cast<wchar_t*>(dest_buf);
      size_t wchar_len = buffer_length / sizeof(wchar_t);
      if (wchar_len < 2) { 
        if (wchar_len > 0) {
          dest[0] = L'\0'; 
        }
        status_record = StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
      } else {
        std::wstring value = conn_bool ? L"1" : L"0"; ;
        std::wcsncpy(dest, value.c_str(), wchar_len - 1);
        dest[wchar_len - 1] = L'\0'; 
      }
      break;
    }

    case SQL_C_BINARY: {
      if (buffer_length < sizeof(bool)) {
        std::memcpy(dest_buf, &conn_bool, buffer_length); // Copy only what fits
        status_record = StatusRecord{SQLStates::k_01004(), "Binary data, right truncated"};
      } else {
        std::memcpy(dest_buf, &conn_bool, sizeof(bool));
      }
      break;
    }

    case SQL_C_LONG: {
      if (buffer_length < sizeof(SQLINTEGER)) {
        status_record = StatusRecord{SQLStates::k_01004(), "Long integer data, right truncated"};
      } else {
        *reinterpret_cast<SQLINTEGER*>(dest_buf) = static_cast<SQLINTEGER>(conn_bool) ;
      }
      break;
    }

  case SQL_C_BIT: {
    if (buffer_length < sizeof(SQLCHAR)) {
        status_record = StatusRecord{SQLStates::k_01004(), "Bit data, right truncated"};
    } else {
       *reinterpret_cast<SQLCHAR*>(dest_buf) = conn_bool ? static_cast<SQLCHAR>(1) : static_cast<SQLCHAR>(0);
    }
    break;
}

case SQL_C_DOUBLE: {
    if (buffer_length < sizeof(SQLDOUBLE)) {
        status_record = StatusRecord{SQLStates::k_01004(), "Double data, right truncated"};
    } else {
        *reinterpret_cast<SQLDOUBLE*>(dest_buf) = conn_bool ? 1.0 : 0.0;
    }
    break;
}

    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }

  return status_record;
}

}  // namespace google::cloud::odbc_bq_driver_internal
