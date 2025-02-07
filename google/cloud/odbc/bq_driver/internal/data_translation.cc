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

odbc_internal::StatusRecord ConvertFromStringDSValue(DSValue const& src_dsval,
                                                     DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  using odbc_internal::StatusRecordOr;

  std::string src_str;
  DSValueToString(src_dsval, src_str);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN* res_len = dest_data.result_len;

  if (dest_type == SQL_C_CHAR) {
    StatusRecord status_record =
        StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
    return status_record;
  }

  // TODO(sachinpro): This assumes that SQLDOUBLE is a safe container for all
  // arithmetic types, which is not true for int64 which has a range(-2^63 to
  // +2^63 -1). Integer range of SQLDOUBLE is (-2^54 to +2^54 -1). Here we
  // should use SQLDOUBLE for floating point values and int64 for pure integers
  StatusRecordOr<SQLDOUBLE> conversion_status = ConvertToDouble(src_str);
  if (!conversion_status) {
    return conversion_status.GetStatusRecord();
  }
  SQLDOUBLE src_val = *conversion_status;

  // TODO(b/343404637): Handle all arithmetic types
  switch (dest_type) {
    case SQL_C_FLOAT: {
      auto* dest_val = reinterpret_cast<SQLREAL*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLREAL>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLREAL>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLREAL);
        }
      }
      return status_record;
    }
    case SQL_C_DOUBLE: {
      auto* dest_val = reinterpret_cast<SQLDOUBLE*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLDOUBLE>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLDOUBLE>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLDOUBLE);
        }
      }
      return status_record;
    }
    case SQL_C_SBIGINT: {
      auto* dest_val = reinterpret_cast<SQLBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLBIGINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLBIGINT);
        }
      }
      return status_record;
    }
    case SQL_C_UBIGINT: {
      auto* dest_val = reinterpret_cast<SQLUBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLUBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUBIGINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLUBIGINT);
        }
      }
      return status_record;
    }
    case SQL_C_SSHORT: {
      auto* dest_val = reinterpret_cast<SQLSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLSMALLINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLSMALLINT);
        }
      }
      return status_record;
    }
    case SQL_C_USHORT: {
      auto* dest_val = reinterpret_cast<SQLUSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLUSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUSMALLINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLUSMALLINT);
        }
      }
      return status_record;
    }
    case SQL_C_SLONG: {
      auto* dest_val = reinterpret_cast<SQLINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLINTEGER>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLINTEGER);
        }
      }
      return status_record;
    }
    case SQL_C_ULONG: {
      auto* dest_val = reinterpret_cast<SQLUINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLUINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUINTEGER>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLUINTEGER);
        }
      }
      return status_record;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

odbc_internal::StatusRecord ConvertFromTimeDSValue(DSValue const& src_dsval,
                                                   DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  SQL_TIME_STRUCT dest_time;
  DSValueToTime(src_dsval, dest_time);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  constexpr int kTimeWcharLength = kTimeCharLength;
  constexpr int kTimeBinaryLength = sizeof(SQL_TIME_STRUCT);

  if (!dest_buf) {
    return StatusRecord::Ok();
  }
  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_type) {
    case SQL_C_CHAR: {
      auto* dest = reinterpret_cast<char*>(dest_buf);
      if (buffer_length < kTimeCharLength) {
        strncpy(dest, "HH:MM:SS", buffer_length - 1);
        dest[buffer_length - 1] = '\0';
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
        if (res_len) {
          *res_len = buffer_length;
        }
      } else {
        snprintf(dest, buffer_length, "%02d:%02d:%02d.000000", dest_time.hour,
                 dest_time.minute, dest_time.second);
        if (res_len) {
          *res_len = kTimeCharLength;
        }
      }
      break;
    }
    case SQL_C_TYPE_TIME: {
      return TimeToOutputBufferResponse(
          dest_time, dest_buf, buffer_length,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }

    case SQL_C_TYPE_TIMESTAMP: {
      auto* timestamp = reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(dest_buf);
      timestamp->year = 0;
      timestamp->month = 0;
      timestamp->day = 0;
      timestamp->hour = dest_time.hour;
      timestamp->minute = dest_time.minute;
      timestamp->second = dest_time.second;
      if (res_len) {
        *res_len = sizeof(SQL_TIMESTAMP_STRUCT);
      }
      break;
    }

    case SQL_C_WCHAR: {
      std::string time_src_str;
      time_src_str = FormatTimetoString(dest_time);
      time_src_str.append(".000000");
      SQLINTEGER k_time_src_len = time_src_str.length();
      SQLINTEGER supp_max_len = 9;
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(time_src_str);
      if (!wstr) {
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "DSValueToWchar Conversion Failed"};
        break;
      }
      return WStrToOutputBufferResponse(
          wstr.GetValue(), dest_buf, buffer_length, k_time_src_len,
          supp_max_len, reinterpret_cast<SQLLEN*>(dest_data.result_len));
      break;
    }
    case SQL_C_BINARY: {
      if (buffer_length < kTimeBinaryLength) {
        memcpy(dest_buf, &dest_time, buffer_length);
        status_record =
            StatusRecord{SQLStates::k_01004(), "Binary data, right truncated"};
        if (res_len) {
          *res_len = buffer_length;
        }
      } else {
        memcpy(dest_buf, &dest_time, kTimeBinaryLength);
        if (res_len) {
          *res_len = kTimeBinaryLength;
        }
      }
      break;
    }
    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }

  return status_record;
}

odbc_internal::StatusRecord ConvertFromTimestampDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  using odbc_internal::StatusRecordOr;

  SQL_TIMESTAMP_STRUCT timestamp_src_struct;
  DSValueToTimestamp(src_dsval, timestamp_src_struct);

  std::string timestamp_src_str;
  timestamp_src_str = FormatTimestampToString(timestamp_src_struct);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  // Define length variables
  int k_timestamp_src_len = timestamp_src_str.length();
  constexpr int kTimestampBinaryLength = sizeof(SQL_TIMESTAMP_STRUCT);

  if (!dest_buf) {
    return StatusRecord::Ok();
  }
  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_type) {
    case SQL_C_CHAR: {
      auto* dest = reinterpret_cast<char*>(dest_buf);
      if (buffer_length > k_timestamp_src_len) {
        if (res_len) {
          *res_len = k_timestamp_src_len;
        }
        std::strncpy(dest, timestamp_src_str.c_str(), k_timestamp_src_len);
        dest[k_timestamp_src_len] = '\0';
      } else if (20 <= buffer_length && buffer_length <= k_timestamp_src_len) {
        if (res_len) {
          *res_len = buffer_length;
        }
        std::strncpy(dest, timestamp_src_str.c_str(), buffer_length - 1);
        dest[buffer_length - 1] = '\0';
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }

    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(timestamp_src_str);
      if (!wstr) {
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "DSValueToWchar Conversion Failed"};
        break;
      }
      std::vector<SQLWCHAR> wstr_data(wstr->begin(), wstr->end());
      wstr_data.emplace_back(L'\0');

      auto* dest = reinterpret_cast<SQLWCHAR*>(dest_buf);
      if (buffer_length > k_timestamp_src_len) {
        if (res_len) {
          *res_len = k_timestamp_src_len * sizeof(SQLWCHAR);
        }
        std::memcpy(dest, wstr_data.data(),
                    (k_timestamp_src_len) * sizeof(SQLWCHAR));
      } else if (20 <= buffer_length && buffer_length <= k_timestamp_src_len) {
        if (res_len) {
          *res_len = buffer_length * sizeof(SQLWCHAR);
        }
        std::memcpy(dest, wstr_data.data(), (buffer_length) * sizeof(SQLWCHAR));
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }

    case SQL_C_BINARY: {
      if (kTimestampBinaryLength <= buffer_length) {
        if (res_len) {
          *res_len = kTimestampBinaryLength;
        }
        timestamp_src_struct.fraction = timestamp_src_struct.fraction * 1000;
        std::memcpy(dest_buf, &timestamp_src_struct, kTimestampBinaryLength);

      } else {
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }

    case SQL_C_TYPE_DATE: {
      auto* date = reinterpret_cast<SQL_DATE_STRUCT*>(dest_buf);
      if (res_len) {
        *res_len = sizeof(SQL_DATE_STRUCT);
      }
      if (timestamp_src_struct.hour == 0 && timestamp_src_struct.minute == 0 &&
          timestamp_src_struct.second == 0) {
        date->year = timestamp_src_struct.year;
        date->month = timestamp_src_struct.month;
        date->day = timestamp_src_struct.day;
      } else {
        date->year = timestamp_src_struct.year;
        date->month = timestamp_src_struct.month;
        date->day = timestamp_src_struct.day;
        status_record =
            StatusRecord{SQLStates::k_01S07(), "Date data, right truncated"};
      }
      break;
    }

    case SQL_C_TYPE_TIME: {
      auto* time = reinterpret_cast<SQL_TIME_STRUCT*>(dest_buf);
      if (res_len) {
        *res_len = sizeof(SQL_TIME_STRUCT);
      }
      if (timestamp_src_struct.fraction == 0) {
        time->hour = timestamp_src_struct.hour;
        time->minute = timestamp_src_struct.minute;
        time->second = timestamp_src_struct.second;
      } else {
        time->hour = timestamp_src_struct.hour;
        time->minute = timestamp_src_struct.minute;
        time->second = timestamp_src_struct.second;
        status_record =
            StatusRecord{SQLStates::k_01S07(), "Time data, right truncated"};
      }
      break;
    }

    case SQL_C_TYPE_TIMESTAMP: {
      return TimestampToOutputBufferResponse(
          timestamp_src_struct, dest_buf, buffer_length,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }

    default:
      status_record =
          StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
  return status_record;
}

odbc_internal::StatusRecord ConvertFromDateDSValue(DSValue const& src_dsval,
                                                   DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  using odbc_internal::StatusRecordOr;

  SQL_DATE_STRUCT conn_date;

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  if (!dest_buf) {
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  DSValueToDate(src_dsval, conn_date);

  constexpr int kDateCharLength = SQL_DATE_LEN;
  constexpr int kDateWcharLength = kDateCharLength;
  constexpr int kDateBinaryLength = sizeof(SQL_DATE_STRUCT);

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_type) {
    case SQL_C_TYPE_DATE: {
      return DateToOutputBufferResponse(
          conn_date, dest_buf, buffer_length,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }

    case SQL_C_TYPE_TIMESTAMP: {
      auto* timestamp = reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(dest_buf);
      timestamp->year = conn_date.year;
      timestamp->month = conn_date.month;
      timestamp->day = conn_date.day;
      timestamp->hour = 0;
      timestamp->minute = 0;
      timestamp->second = 0;
      if (res_len) {
        *res_len = sizeof(SQL_TIMESTAMP_STRUCT);
      }
      break;
    }
    case SQL_C_CHAR: {
      auto* dest = reinterpret_cast<char*>(dest_buf);
      if (buffer_length < kDateCharLength) {
        strncpy(dest, "YYYY-MM-DD", buffer_length - 1);
        dest[buffer_length - 1] = '\0';
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
        if (res_len) {
          *res_len = buffer_length;
        }
      } else {
        snprintf(dest, buffer_length, "%04d-%02d-%02d", conn_date.year,
                 conn_date.month, conn_date.day);
        if (res_len) {
          *res_len = kDateCharLength;
        }
      }
      break;
    }

    case SQL_C_BINARY: {
      if (buffer_length < kDateBinaryLength) {
        memcpy(dest_buf, &conn_date, buffer_length);
        status_record =
            StatusRecord{SQLStates::k_01004(), "Binary data, right truncated"};
        if (res_len) {
          *res_len = buffer_length;
        }
      } else {
        memcpy(dest_buf, &conn_date, kDateBinaryLength);
        if (res_len) {
          *res_len = kDateBinaryLength;
        }
      }
      break;
    }
    case SQL_C_WCHAR: {
      auto* dest = reinterpret_cast<wchar_t*>(dest_buf);
      if (buffer_length < kDateWcharLength * sizeof(wchar_t)) {
        wcsncpy(dest, L"YYYY-MM-DD", (buffer_length / sizeof(wchar_t)) - 1);
        dest[(buffer_length / sizeof(wchar_t)) - 1] = L'\0';
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
        if (res_len) {
          *res_len = buffer_length;
        }
      } else {
        char buffer[11];
        snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", conn_date.year,
                 conn_date.month, conn_date.day);
        std::string formatted_date = buffer;
        StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(formatted_date);
        if (!wstr) {
          return StatusRecord{SQLStates::k_HY000(),
                              "DSValueToWchar Conversion Failed"};
          break;
        }
        std::vector<SQLWCHAR> wstr_data(wstr->begin(), wstr->end());
        wstr_data.emplace_back(L'\0');
        std::memcpy(dest_buf, wstr_data.data(),
                    (wstr_data.size() + 1) * sizeof(SQLWCHAR));
        if (res_len) {
          *res_len = kDateWcharLength;
        }
        break;
      }
    }
    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }

  return status_record;
}

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
    case SQL_C_BINARY: {
      return StringValueToOutputBufferResponse<SQLLEN>(
          src_str.c_str(), dest_buf, buffer_length, res_len);
    }

    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

StatusRecord ConvertFromArrayDSValue(DSValue const& src_dsval,
                                     DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_data.type) {
    case SQL_C_CHAR: {
      return StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wide_string = Utf8ToUtf16(src_str);
      if (!wide_string.Ok()) {
        return StatusRecord{SQLStates::k_HY000(), "Conversion Failed"};
      }
      return WStrToOutputBufferResponse(
          *wide_string, dest_data.buf, dest_data.buflen, src_str.length(),
          src_str.length(), reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }
    case SQL_C_BINARY: {
      if (dest_data.buflen < src_str.length()) {
        std::memcpy(dest_data.buf, src_str.c_str(), dest_data.buflen - 1);
        if (dest_data.result_len) {
          *dest_data.result_len = dest_data.buflen - 1;
        }
        return StatusRecord{SQLStates::k_01004(),
                            "Binary data, right truncated"};
      }
      std::memcpy(dest_data.buf, src_str.c_str(), src_str.length());
      if (dest_data.result_len) {
        *dest_data.result_len = src_str.length();
      }
      break;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return status_record;
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

StatusRecord ConvertFromBooleanDSValue(DSValue const& src_dsval,
                                       DataBuffer& dest_data) {
  bool conn_bool = false;

  if (!dest_data.buf) {
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }

  if (dest_data.buflen < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  DSValueToBoolean(src_dsval, conn_bool);
  StatusRecord status_record = StatusRecord::Ok();
  switch (dest_data.type) {
    case SQL_C_CHAR: {
      auto* dest = reinterpret_cast<char*>(dest_data.buf);
      if (dest_data.buflen < 2) {
        if (dest_data.buflen > 0) {
          dest[0] = '\0';
        }
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
      } else {
        dest[0] = conn_bool ? '1' : '0';
        dest[1] = '\0';
      }
      break;
    }

    case SQL_C_WCHAR: {
      auto* dest = reinterpret_cast<wchar_t*>(dest_data.buf);
      size_t wchar_len = dest_data.buflen / sizeof(wchar_t);
      if (wchar_len < 2) {
        if (wchar_len > 0) {
          dest[0] = L'\0';
        }
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
      } else {
        std::wstring value = conn_bool ? L"1" : L"0";
        std::wcsncpy(dest, value.c_str(), wchar_len - 1);
        dest[wchar_len - 1] = L'\0';
      }
      break;
    }

    case SQL_C_BINARY: {
      if (dest_data.buflen < sizeof(bool)) {
        std::memcpy(dest_data.buf, &conn_bool, dest_data.buflen);
        status_record =
            StatusRecord{SQLStates::k_01004(), "Binary data, right truncated"};
      } else {
        std::memcpy(dest_data.buf, &conn_bool, sizeof(bool));
      }
      break;
    }

    case SQL_C_LONG: {
      if (dest_data.buflen < sizeof(SQLINTEGER)) {
        status_record = StatusRecord{SQLStates::k_01004(),
                                     "Long integer data, right truncated"};
      } else {
        *reinterpret_cast<SQLINTEGER*>(dest_data.buf) =
            static_cast<SQLINTEGER>(conn_bool);
      }
      break;
    }

    case SQL_C_BIT: {
      if (dest_data.buflen < sizeof(SQLCHAR)) {
        status_record =
            StatusRecord{SQLStates::k_01004(), "Bit data, right truncated"};
      } else {
        *reinterpret_cast<SQLCHAR*>(dest_data.buf) =
            conn_bool ? static_cast<SQLCHAR>(1) : static_cast<SQLCHAR>(0);
      }
      break;
    }

    case SQL_C_DOUBLE: {
      if (dest_data.buflen < sizeof(SQLDOUBLE)) {
        status_record =
            StatusRecord{SQLStates::k_01004(), "Double data, right truncated"};
      } else {
        *reinterpret_cast<SQLDOUBLE*>(dest_data.buf) = conn_bool ? 1.0 : 0.0;
      }
      break;
    }

    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }

  return status_record;
}

StatusRecord ConvertFromGeographyDSValue(DSValue const& src_dsval,
                                         DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);
  SQLLEN buffer_length = dest_data.buflen;

  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_data.type) {
    case SQL_C_CHAR:
    case SQL_C_BINARY: {
      StatusRecord status_record =
          StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
      break;
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(src_str);
      if (!wstr) {
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "Conversion to SQL_C_WCHAR failed."};
        break;
      }
      std::memset(dest_data.buf, 0, buffer_length);

      return WStrToOutputBufferResponse(
          wstr.GetValue(), dest_data.buf, buffer_length, src_str.length(),
          src_str.length(), reinterpret_cast<SQLLEN*>(dest_data.result_len));
      break;
    }

    default: {
      status_record =
          StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return status_record;
}

// This func converts a vector of SQLCHAR bytes (hex-encoded) to binary data,
// handling truncation if needed.
StatusRecord ConvertBytesToBinary(DSValue const& conn_val,
                                  DataBuffer& dest_data) {
  DSValue binary_data;

  // Reserves space to optimize memory allocation as each hex pair forms one
  // byte.
  binary_data.reserve(conn_val.size() / 2);

  StatusRecord status_record = StatusRecord::Ok();

  // Convert each hex pair into a byte
  for (auto i = 0; i < conn_val.size(); i += 2) {
    uint8_t byte = (conn_val[i] - '0') * 16 + (conn_val[i + 1] - '0');
    binary_data.push_back(byte);
  }

  // Handle buffer truncation scenario
  if (dest_data.buflen < binary_data.size()) {
    std::memcpy(dest_data.buf, binary_data.data(), dest_data.buflen);
    if (dest_data.result_len) {
      *dest_data.result_len = dest_data.buflen;
    }
    status_record =
        StatusRecord{SQLStates::k_01004(), "Binary data, right truncated"};
  } else {
    std::memcpy(dest_data.buf, binary_data.data(), binary_data.size());
    if (dest_data.result_len) {
      *dest_data.result_len = binary_data.size();
    }
  }
  return status_record;
}

// This func converts a vector of SQLCHAR bytes to a standard char string,
// ensuring null termination and handling truncation.
StatusRecord ConvertBytesToChar(DSValue const& conn_val,
                                DataBuffer& dest_data) {
  auto* dest = reinterpret_cast<char*>(dest_data.buf);
  StatusRecord status_record = StatusRecord::Ok();

  // Check for truncation and copy data accordingly
  if (dest_data.buflen < static_cast<SQLLEN>(conn_val.size()) + 1) {
    std::memcpy(dest, conn_val.data(), dest_data.buflen - 1);
    dest[dest_data.buflen - 1] = '\0';
    if (dest_data.result_len) {
      *dest_data.result_len = dest_data.buflen;
    }
    status_record =
        StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
  } else {
    std::memcpy(dest, conn_val.data(), conn_val.size());
    if (dest_data.result_len) {
      *dest_data.result_len = conn_val.size();
    }
  }
  return status_record;
}

StatusRecord Base64Decode(DSValue const& ascii_values, DSValue& source) {
  static std::string const kBaseChars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  source.clear();
  int val = 0;
  int val_b = -8;

  for (char c : ascii_values) {
    if (!absl::StrContains(kBaseChars, c)) {
      continue;  // Skip any non-Base64 characters
    }

    val = (val << 6) + kBaseChars.find(c);
    val_b += 6;

    if (val_b >= 0) {
      source.push_back(static_cast<uint8_t>((val >> val_b) & 0xFF));
      val_b -= 8;
    }
  }

  return StatusRecord::Ok();
}

// Func to convert base64 encoded into its ASCII hexadecimal value
StatusRecord Base64ToASCIIHexFormat(DSValue const& bytes, DSValue& output) {
  output.clear();

  for (uint8_t byte : bytes) {
    std::ostringstream hex_stream;
    hex_stream << std::hex << std::uppercase << std::setfill('0')
               << std::setw(2) << static_cast<int>(byte);

    for (char ch : hex_stream.str()) {
      output.push_back(static_cast<uint8_t>(ch));
    }
  }
  return StatusRecord::Ok();
}

StatusRecord ConvertFromBytesDSValue(DSValue const& src_dsval,
                                     DataBuffer& dest_data) {
  DSValue base_value;
  Base64Decode(src_dsval, base_value);

  DSValue conn_val;
  Base64ToASCIIHexFormat(base_value, conn_val);

  SQLLEN src_length = static_cast<SQLLEN>(conn_val.size());

  if (!dest_data.buf) {
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (dest_data.buflen < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  switch (dest_data.type) {
    case SQL_C_BINARY:
      return ConvertBytesToBinary(conn_val, dest_data);
    case SQL_C_CHAR:
      return ConvertBytesToChar(conn_val, dest_data);
    // TODO(@khushikathuria008): SQL_C_WCHAR will come in part 2 of this PR.
    default:
      return StatusRecord{SQLStates::k_HY000(), "Unsupported conversion type"};
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
