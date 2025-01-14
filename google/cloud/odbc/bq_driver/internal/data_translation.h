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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

constexpr int kTimeCharLength = 8;

// Checks if an arithmetic value can be converted to another accurately.
template <typename SrcType, typename DestType>
inline odbc_internal::StatusRecord CheckLimitsArithmetic(SrcType value) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  if (!std::is_arithmetic_v<SrcType> || !std::is_arithmetic_v<DestType>) {
    return odbc_internal::StatusRecord{
        SQLStates::k_HY000(), "Invalid datatypes for conversion check!"};
  }

  // Special case for same type
  if constexpr (std::is_same_v<SrcType, DestType>) {
    return StatusRecord::Ok();
  }

  // Special case for boolean to numeric
  if constexpr (std::is_same_v<SrcType, bool>) {
    return StatusRecord::Ok();  // bool can always be converted to a number (0
                                // or 1)
  }
  bool status = static_cast<double>(value) >= static_cast<double>(std::numeric_limits<DestType>::lowest()) &&
                 static_cast<double>(value) <= static_cast<double>(std::numeric_limits<DestType>::max());
  if (!status) {
    return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
  }

  // Special case for floating point to integer
  if constexpr (std::is_floating_point_v<SrcType> &&
                std::is_integral_v<DestType>) {
    bool status =
        (value == static_cast<DestType>(value));  // Check for truncation
    if (!status) {
      return StatusRecord{SQLStates::k_01S07(), "Fractional truncation"};
    }
  }
  return StatusRecord::Ok();
}

// Assuming that DSValue hosts fixed-length arithmetic data, this converts it to
// the destination data type in the DataBuffer
template <typename SrcType>
inline odbc_internal::StatusRecord ConvertFromArithmeticDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  if (!std::is_arithmetic_v<SrcType>) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid datatypes for conversion check!"};
  }

  SrcType src_val;
  std::memcpy(&src_val, src_dsval.data(),
              sizeof(SrcType));  // Get the src value

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN* res_len = dest_data.result_len;
  // Ref:
  // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16
  // to understand the ODBC C data types and their typedefs
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
          CheckLimitsArithmetic<SrcType, SQLDOUBLE>(src_val);
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
          CheckLimitsArithmetic<SrcType, SQLBIGINT>(src_val);
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
          CheckLimitsArithmetic<SrcType, SQLUBIGINT>(src_val);
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
          CheckLimitsArithmetic<SrcType, SQLSMALLINT>(src_val);
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
          CheckLimitsArithmetic<SrcType, SQLUSMALLINT>(src_val);
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
          CheckLimitsArithmetic<SrcType, SQLINTEGER>(src_val);
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
          CheckLimitsArithmetic<SrcType, SQLUINTEGER>(src_val);
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
    case SQL_C_CHAR: {
      std::string str = std::to_string(src_val);
      StatusRecord status_record =
          StringValueToOutputBufferResponse(str.c_str(), dest_data);
      return status_record;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

// Converts a string to SQLDOUBLE and returns StatusRecord if it failed
// TODO(sachinpro): Make sure double is a good enough container during translation
// wherever ConvertToDouble is used.
inline odbc_internal::StatusRecordOr<SQLDOUBLE> ConvertToDouble(
    std::string const& str) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  char* endptr;
  errno = 0;
  SQLDOUBLE result = std::strtod(str.c_str(), &endptr);

  if (endptr == str.c_str() || *endptr != '\0' || errno == ERANGE) {
    // Conversion failed or overflow/underflow occurred
    return StatusRecord{SQLStates::k_HY000(), "Invalid conversion"};
  }

  // Check for NaN or infinity
  if (!std::isfinite(result)) {
    return StatusRecord{SQLStates::k_HY000(), "Value is NaN"};
  }
  return result;
}

// Assuming that DSValue hosts string data, this converts it to the destination
// data type in the DataBuffer
inline odbc_internal::StatusRecord ConvertFromStringDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
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

inline odbc_internal::StatusRecord ConvertFromTimeDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
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

inline odbc_internal::StatusRecord ConvertFromTimestampDSValue(
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

inline odbc_internal::StatusRecord ConvertFromDateDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
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
                                    DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromIntervalDSValue(DSValue const& src_dsval,
                                                       DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromBooleanDSValue(DSValue const& src_dsval,
                                                      DataBuffer& dest_data);
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
