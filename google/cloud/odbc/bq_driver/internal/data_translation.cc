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

template <typename TargetType>
odbc_internal::StatusRecord ConvertNumeric(SQLDOUBLE numeric_no,
                                           DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  auto* dest_val = reinterpret_cast<TargetType*>(dest_data.buf);
  auto status_record = CheckLimitsArithmetic<SQLDOUBLE, TargetType>(numeric_no);
  if (status_record.sql_state != SQLStates::k_22003()) {
    *dest_val = static_cast<TargetType>(numeric_no);
    if (dest_data.result_len) {
      *dest_data.result_len = sizeof(TargetType);
    }
  }
  return status_record;
}

odbc_internal::StatusRecord ConvertFromNumericDSValue(DSValue const& src_dsval,
                                                      DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  if (!dest_data.buf) {
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (dest_data.buflen < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }
  std::string str_input;
  DSValueToString(src_dsval, str_input);
  StatusRecord status_record = StatusRecord::Ok();
  StatusRecordOr<SQLDOUBLE> conversion_status = ConvertToDouble(str_input);
  if (!conversion_status) {
    return conversion_status.GetStatusRecord();
  }
  SQLDOUBLE numeric_no = *conversion_status;
  switch (dest_data.type) {
    case SQL_C_NUMERIC: {
      SQL_NUMERIC_STRUCT numst;
      status_record = GetNumericDetailsFromStr(str_input, numst);
      if (status_record.sql_state == SQLStates::k_22003()) {
        return status_record;
      }
      auto* dest_val = reinterpret_cast<SQL_NUMERIC_STRUCT*>(
          dest_data.buf);  // convert the pointer to type
      *dest_val = numst;   // fill the value
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_NUMERIC_STRUCT);
      }
      return status_record;
    }
    case SQL_C_CHAR: {
      auto status_record =
          StringValueToOutputBufferResponse(str_input.c_str(), dest_data);
      return status_record;
    }
    case SQL_C_WCHAR: {
      int src_len = str_input.length();
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(str_input);
      if (!wstr) {
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "DSValueToWchar Conversion Failed"};
        break;
      }
      WStrToOutputBufferResponse(wstr.GetValue(), dest_data.buf,
                                 dest_data.buflen, src_len, dest_data.buflen,
                                 dest_data.result_len);
      return status_record;
    }
    case SQL_C_FLOAT:
      return ConvertNumeric<SQLREAL>(numeric_no, dest_data);
    case SQL_C_DOUBLE:
      return ConvertNumeric<SQLDOUBLE>(numeric_no, dest_data);
    case SQL_C_SSHORT:
    case SQL_C_SHORT:
      return ConvertNumeric<SQLSMALLINT>(numeric_no, dest_data);
    case SQL_C_USHORT:
      return ConvertNumeric<SQLUSMALLINT>(numeric_no, dest_data);
    case SQL_C_SLONG:
    case SQL_C_LONG:
      return ConvertNumeric<SQLINTEGER>(numeric_no, dest_data);
    case SQL_C_ULONG:
      return ConvertNumeric<SQLUINTEGER>(numeric_no, dest_data);
    case SQL_C_STINYINT:
      return ConvertNumeric<SQLSCHAR>(numeric_no, dest_data);
    case SQL_C_TINYINT:
    case SQL_C_UTINYINT:
      return ConvertNumeric<SQLCHAR>(numeric_no, dest_data);
    case SQL_C_BIT: {
      if (numeric_no == 0 || numeric_no == 1) {
        *reinterpret_cast<SQLCHAR*>(dest_data.buf) =
            static_cast<SQLCHAR>(numeric_no);
        return StatusRecord::Ok();
      }
      return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
    }
    case SQL_C_SBIGINT: {
      SQLBIGINT bigint_val = std::stoll(str_input);
      *reinterpret_cast<SQLBIGINT*>(dest_data.buf) = bigint_val;
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQLBIGINT);
        return StatusRecord::Ok();
      }
      return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
    }
    case SQL_C_UBIGINT: {
      if (!str_input.empty() && str_input[0] == '-') {
        return StatusRecord{SQLStates::k_22003(),
                            "Negative value cannot be stored in unsigned type"};
      }
      try {
        SQLUBIGINT val = std::stoull(str_input);
        *reinterpret_cast<SQLUBIGINT*>(dest_data.buf) = val;
        if (dest_data.result_len) {
          *dest_data.result_len = sizeof(SQLUBIGINT);
        }
        return StatusRecord::Ok();
      } catch (std::out_of_range const&) {
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      } catch (std::invalid_argument const&) {
        return StatusRecord{SQLStates::k_22018(),
                            "Invalid character value for cast"};
      }
    }
  }

  return status_record;
}

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
    return StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
  }
  if (dest_type == SQL_C_WCHAR) {
    int src_len = src_str.length();
    StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(src_str);
    if (!wstr.Ok()) {
      return StatusRecord{SQLStates::k_HY000(),
                          "SQL_C_WCHAR Conversion Failed"};
    }

    return WStrToOutputBufferResponse(wstr.GetValue(), dest_data.buf,
                                      dest_data.buflen, src_len,
                                      dest_data.buflen, dest_data.result_len);
  }
  if (dest_type >= SQL_C_INTERVAL_YEAR &&
      dest_type <= SQL_C_INTERVAL_MINUTE_TO_SECOND) {
    auto* dest_val = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_buf);
    dest_val->interval_type = static_cast<SQLINTERVAL>(dest_type);
    dest_val->interval_sign = 1;

    std::string str = src_str;
    if (!str.empty() && str[0] == '-') {
      dest_val->interval_sign = -1;
      str = str.substr(1);
    }

    int y = 0;
    int m = 0;
    int d = 0;
    int h = 0;
    int min = 0;
    int s = 0;

    switch (dest_type) {
      case SQL_C_INTERVAL_YEAR:
        if (sscanf(str.c_str(), "%d", &y) == 1) {
          dest_val->intval.year_month.year = y;
          dest_val->intval.year_month.month = 0;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_MONTH:
        if (sscanf(str.c_str(), "%d", &m) == 1) {
          dest_val->intval.year_month.year = 0;
          dest_val->intval.year_month.month = m;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_YEAR_TO_MONTH:
        if (sscanf(str.c_str(), "%d-%d", &y, &m) == 2) {
          dest_val->intval.year_month.year = y;
          dest_val->intval.year_month.month = m;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_DAY:
        if (sscanf(str.c_str(), "%d", &d) == 1) {
          dest_val->intval.day_second.day = d;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_HOUR:
        if (sscanf(str.c_str(), "%d", &h) == 1) {
          dest_val->intval.day_second.hour = h;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_MINUTE:
        if (sscanf(str.c_str(), "%d", &min) == 1) {
          dest_val->intval.day_second.minute = min;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_SECOND:
        if (sscanf(str.c_str(), "%d", &s) == 1) {
          dest_val->intval.day_second.second = s;
          dest_val->intval.day_second.fraction = 0;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_DAY_TO_HOUR:
        if (sscanf(str.c_str(), "%d %d", &d, &h) == 2) {
          dest_val->intval.day_second.day = d;
          dest_val->intval.day_second.hour = h;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_DAY_TO_MINUTE:
        if (sscanf(str.c_str(), "%d %d:%d", &d, &h, &min) == 3) {
          dest_val->intval.day_second.day = d;
          dest_val->intval.day_second.hour = h;
          dest_val->intval.day_second.minute = min;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_DAY_TO_SECOND:
        if (sscanf(str.c_str(), "%d %d:%d:%d", &d, &h, &min, &s) == 4) {
          dest_val->intval.day_second.day = d;
          dest_val->intval.day_second.hour = h;
          dest_val->intval.day_second.minute = min;
          dest_val->intval.day_second.second = s;
          dest_val->intval.day_second.fraction = 0;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_HOUR_TO_MINUTE:
        if (sscanf(str.c_str(), "%d:%d", &h, &min) == 2) {
          dest_val->intval.day_second.hour = h;
          dest_val->intval.day_second.minute = min;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_HOUR_TO_SECOND:
        if (sscanf(str.c_str(), "%d:%d:%d", &h, &min, &s) == 3) {
          dest_val->intval.day_second.hour = h;
          dest_val->intval.day_second.minute = min;
          dest_val->intval.day_second.second = s;
          dest_val->intval.day_second.fraction = 0;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_MINUTE_TO_SECOND:
        if (sscanf(str.c_str(), "%d:%d", &min, &s) == 2) {
          dest_val->intval.day_second.minute = min;
          dest_val->intval.day_second.second = s;
          dest_val->intval.day_second.fraction = 0;
        } else {
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      default:
        return StatusRecord{SQLStates::k_22003(), "Invalid interval type"};
    }

    if (res_len) *res_len = sizeof(SQL_INTERVAL_STRUCT);
    return StatusRecord::Ok();
  }
  if (dest_type == SQL_C_TYPE_DATE) {
    auto* dest_val = reinterpret_cast<DATE_STRUCT*>(dest_buf);
    int y;
    int m;
    int d;
    char dash1;
    char dash2;
    std::istringstream ss(src_str);
    if (ss >> y >> dash1 >> m >> dash2 >> d && dash1 == '-' && dash2 == '-') {
      dest_val->year = y;
      dest_val->month = m;
      dest_val->day = d;
      if (res_len) *res_len = sizeof(DATE_STRUCT);
      return StatusRecord::Ok();
    }
    return StatusRecord{SQLStates::k_22003(), "Invalid date format"};
  }
  if (dest_type == SQL_C_TYPE_TIME) {
    auto* dest_val = reinterpret_cast<TIME_STRUCT*>(dest_buf);
    int h;
    int m;
    int s;
    char colon1;
    char colon2;
    std::istringstream ss(src_str);
    if (ss >> h >> colon1 >> m >> colon2 >> s && colon1 == ':' &&
        colon2 == ':') {
      dest_val->hour = h;
      dest_val->minute = m;
      dest_val->second = s;
      if (res_len) *res_len = sizeof(TIME_STRUCT);
      return StatusRecord::Ok();
    }
    return StatusRecord{SQLStates::k_22003(), "Invalid time format"};
  }
  if (dest_type == SQL_C_TYPE_TIMESTAMP) {
    auto* dest_val = reinterpret_cast<TIMESTAMP_STRUCT*>(dest_buf);
    if (src_str.size() >= 19 && (src_str[10] == ' ' || src_str[10] == 'T') &&
        src_str[4] == '-' && src_str[7] == '-' && src_str[13] == ':' &&
        src_str[16] == ':') {
      dest_val->year = std::stoi(src_str.substr(0, 4));
      dest_val->month = std::stoi(src_str.substr(5, 2));
      dest_val->day = std::stoi(src_str.substr(8, 2));
      dest_val->hour = std::stoi(src_str.substr(11, 2));
      dest_val->minute = std::stoi(src_str.substr(14, 2));
      dest_val->second = std::stoi(src_str.substr(17, 2));
      dest_val->fraction = 0;

      if (res_len) *res_len = sizeof(TIMESTAMP_STRUCT);
      return StatusRecord::Ok();
    }
    return StatusRecord{SQLStates::k_22003(), "Invalid timestamp format"};
  }

  if (dest_type == SQL_C_STINYINT) {
    auto* dest_val = reinterpret_cast<int8_t*>(dest_buf);
    *dest_val = static_cast<int8_t>(std::stoi(src_str));
    if (res_len) {
      *res_len = sizeof(int8_t);
      return StatusRecord::Ok();
    }
    return StatusRecord{SQLStates::k_22003(), "Invalid tinyint value"};
  }

  if (dest_type == SQL_C_UTINYINT) {
    auto* dest_val = reinterpret_cast<uint8_t*>(dest_buf);
    *dest_val = static_cast<uint8_t>(std::stoul(src_str));
    if (res_len) {
      *res_len = sizeof(uint8_t);
      return StatusRecord::Ok();
    }
    return StatusRecord{SQLStates::k_22003(), "Invalid unsigned tinyint value"};
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
      auto* dest_val = reinterpret_cast<int64_t*>(dest_buf);
      *dest_val = std::stoll(src_str);
      if (res_len) {
        *res_len = sizeof(int64_t);
        return StatusRecord::Ok();
      }
      return StatusRecord{SQLStates::k_22003(), "Invalid bigint value"};
    }
    case SQL_C_UBIGINT: {
      auto* dest_val = reinterpret_cast<uint64_t*>(dest_buf);
      *dest_val = std::stoull(src_str);
      if (res_len) {
        *res_len = sizeof(uint64_t);
        return StatusRecord::Ok();
      }
      return StatusRecord{SQLStates::k_22003(),
                          "Invalid unsigned bigint value"};
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
    case SQL_C_BIT: {
      auto* dest_val = reinterpret_cast<SQLCHAR*>(dest_buf);
      if (src_val == 0 || src_val == 1) {
        *dest_val = static_cast<SQLCHAR>(src_val);
        return StatusRecord::Ok();
      }
      return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
    }
    case SQL_C_BINARY: {
      auto* dest_val = reinterpret_cast<SQLCHAR*>(dest_buf);
      std::memcpy(dest_val, src_str.data(), src_str.size());
      reinterpret_cast<char*>(dest_val)[src_str.size()] = '\0';
      if (res_len) {
        *res_len = static_cast<SQLLEN>(src_str.size());
      }
      return StatusRecord::Ok();
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
  if (buffer_length <= 0) {
    return StatusRecord{SQLStates::k_HY090(), "Invalid Buffer length"};
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
      timestamp->fraction = 0;
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
  if (buffer_length <= 0) {
    return StatusRecord{SQLStates::k_HY090(), "Invalid Buffer length"};
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
        dest[k_timestamp_src_len] = L'\0';
      } else if (20 <= buffer_length && buffer_length <= k_timestamp_src_len) {
        if (res_len) {
          *res_len = buffer_length * sizeof(SQLWCHAR);
        }
        std::memcpy(dest, wstr_data.data(), (buffer_length) * sizeof(SQLWCHAR));
        dest[buffer_length - 1] = L'\0';
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
  if (buffer_length <= 0) {
    return StatusRecord{SQLStates::k_HY090(), "Invalid Buffer length"};
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
      timestamp->fraction = 0;
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

StatusRecord ConvertStringToJsonOutputBuffer(std::string const& src_str,
                                             DataBuffer& dest_data) {
  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  switch (dest_type) {
    case SQL_C_CHAR: {
      return StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wide_string = Utf8ToUtf16(src_str);
      if (!wide_string.Ok()) {
        return StatusRecord{SQLStates::k_HY000(),
                            "Conversion to UTF-16 failed"};
      }
      return WStrToOutputBufferResponse(
          wide_string.GetValue(), dest_buf, buffer_length, src_str.length(),
          src_str.length(), reinterpret_cast<SQLLEN*>(res_len));
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

StatusRecord ConvertFromJsonDSValue(DSValue const& src_dsval,
                                    DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);
  return ConvertStringToJsonOutputBuffer(src_str, dest_data);
}

StatusRecord ConvertFromStructDSValue(DSValue const& src_dsval,
                                      DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);

  try {
    nlohmann::json original_json = nlohmann::json::parse(src_str);
    nlohmann::json wrapped_json;
    wrapped_json["v"] = original_json;
    src_str = wrapped_json.dump();
  } catch (std::exception const& e) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid JSON in DSValue: " + std::string(e.what())};
  }

  return ConvertStringToJsonOutputBuffer(src_str, dest_data);
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
  if (buffer_length <= 0) {
    return StatusRecord{SQLStates::k_HY090(), "Invalid Buffer length"};
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

    case SQL_C_LONG:
    case SQL_C_SLONG: {
      *reinterpret_cast<SQLINTEGER*>(dest_data.buf) =
          static_cast<SQLINTEGER>(conn_bool);
      break;
    }

    case SQL_C_ULONG: {
      *reinterpret_cast<SQLUINTEGER*>(dest_data.buf) =
          static_cast<SQLUINTEGER>(conn_bool);
      break;
    }

    case SQL_C_BIT: {
      *reinterpret_cast<SQLCHAR*>(dest_data.buf) =
          conn_bool ? static_cast<SQLCHAR>(1) : static_cast<SQLCHAR>(0);
      break;
    }

    case SQL_C_DOUBLE: {
      *reinterpret_cast<SQLDOUBLE*>(dest_data.buf) = conn_bool ? 1.0 : 0.0;
      break;
    }

    case SQL_C_FLOAT: {
      *reinterpret_cast<SQLREAL*>(dest_data.buf) = conn_bool ? 1.0F : 0.0F;
      break;
    }

    case SQL_C_STINYINT:
    case SQL_C_TINYINT: {
      *reinterpret_cast<SQLSCHAR*>(dest_data.buf) =
          static_cast<SQLSCHAR>(conn_bool);
      break;
    }

    case SQL_C_UTINYINT: {
      *reinterpret_cast<SQLCHAR*>(dest_data.buf) =
          static_cast<SQLCHAR>(conn_bool);
      break;
    }

    case SQL_C_SSHORT:
    case SQL_C_SHORT: {
      *reinterpret_cast<SQLSMALLINT*>(dest_data.buf) =
          static_cast<SQLSMALLINT>(conn_bool);
      break;
    }

    case SQL_C_USHORT: {
      *reinterpret_cast<SQLUSMALLINT*>(dest_data.buf) =
          static_cast<SQLUSMALLINT>(conn_bool);
      break;
    }

    case SQL_C_SBIGINT: {
      *reinterpret_cast<SQLBIGINT*>(dest_data.buf) =
          static_cast<SQLBIGINT>(conn_bool);
      break;
    }

    case SQL_C_UBIGINT: {
      *reinterpret_cast<SQLUBIGINT*>(dest_data.buf) =
          static_cast<SQLUBIGINT>(conn_bool);
      break;
    }

    case SQL_C_NUMERIC: {
      auto* numeric = reinterpret_cast<SQL_NUMERIC_STRUCT*>(dest_data.buf);
      std::memset(numeric, 0, sizeof(SQL_NUMERIC_STRUCT));
      numeric->precision = 1;
      numeric->scale = 0;
      numeric->sign = conn_bool ? 1 : 0;
      numeric->val[0] = conn_bool ? 1 : 0;
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
      int src_len = src_str.length();
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(src_str);
      if (!wstr) {
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "Conversion to SQL_C_WCHAR failed."};
        break;
      }
      std::memset(dest_data.buf, 0, buffer_length);
      std::wstring const& wide_str = wstr.GetValue();

      status_record = WStrToOutputBufferResponse(
          wide_str, dest_data.buf, buffer_length, src_len, buffer_length,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
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

  auto hex_char_to_byte = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;  // Fallback case, should never happen if input is valid hex.
  };

  // Convert each hex pair into a byte
  for (size_t i = 0; i + 1 < conn_val.size(); i += 2) {
    uint8_t byte = (hex_char_to_byte(conn_val[i]) << 4) |
                   hex_char_to_byte(conn_val[i + 1]);
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

// This func converts a vector of SQLCHAR bytes to a UTF-16 wchar_t string,
// ensuring proper truncation handling.
StatusRecord ConvertBytesToWChar(DSValue const& conn_val,
                                 DataBuffer& dest_data) {
  StatusRecord status_record = StatusRecord::Ok();

  // Convert input bytes to a UTF-8 string
  std::string utf8_str(conn_val.begin(), conn_val.end());

  // Convert UTF-8 to UTF-16
  StatusRecordOr<std::wstring> utf16_str = Utf8ToUtf16(utf8_str);
  if (!utf16_str.Ok()) {
    return StatusRecord{SQLStates::k_01004(),
                        "UTF-8 to UTF-16 conversion failed."};
  }

  std::wstring const& utf16_value = utf16_str.GetValue();
  size_t const required_size = utf16_str.GetValue().length() * sizeof(SQLWCHAR);

  auto* buffer = reinterpret_cast<SQLWCHAR*>(dest_data.buf);

  // Handle truncation if buffer is insufficient
  if (dest_data.buflen < required_size) {
    size_t num_chars_to_copy = (dest_data.buflen / sizeof(SQLWCHAR)) - 1;
    std::memcpy(buffer, utf16_value.data(),
                num_chars_to_copy * sizeof(SQLWCHAR));
    reinterpret_cast<wchar_t*>(buffer)[utf16_value.length()] = L'\0';

    if (dest_data.result_len) {
      *dest_data.result_len = dest_data.buflen;
    }

    return StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
  }
  for (size_t i = 0; i < utf16_str.GetValue().size(); ++i) {
    buffer[i] = static_cast<SQLWCHAR>(utf16_str.GetValue()[i]);
  }

  // Set output length
  if (dest_data.result_len) {
    *dest_data.result_len = utf16_str.GetValue().size() * sizeof(SQLWCHAR);
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
    case SQL_C_WCHAR:
      return ConvertBytesToWChar(conn_val, dest_data);
    default:
      return StatusRecord{SQLStates::k_HY000(), "Unsupported conversion type"};
  }
}

// Func to convert unix timestamp data into formatted timestamp string.
// Example -[1708432245.000000, 1710944130.000425) to [2024-02-20
// 12:30:45.00000, 2024-03-20T14:15:30.000425)
StatusRecord ConvertRangeToTimestampFormat(std::string& input) {
  size_t start_pos = input.find('[');
  size_t comma_pos = input.find(',');
  size_t end_pos = input.find(')');

  // Check for invalid input format
  if (start_pos == std::string::npos || comma_pos == std::string::npos ||
      end_pos == std::string::npos) {
    return StatusRecord{SQLStates::k_01004(), "Invalid input format"};
  }

  // Parse start and end timestamps as double
  double start_ts;
  double end_ts;

  try {
    start_ts =
        std::stod(input.substr(start_pos + 1, comma_pos - start_pos - 1));
    end_ts = std::stod(input.substr(comma_pos + 1, end_pos - comma_pos - 1));
  } catch (std::invalid_argument const& e) {
    return StatusRecord{SQLStates::k_01004(), "Failed to parse timestamps"};
  }

  // Convert both timestamps
  SQL_TIMESTAMP_STRUCT start_timestamp;
  auto start_status =
      ConvertUnixTimestampToTimestampStruct(start_ts, start_timestamp);

  SQL_TIMESTAMP_STRUCT end_timestamp;
  auto end_status =
      ConvertUnixTimestampToTimestampStruct(end_ts, end_timestamp);

  // Format both timestamps
  std::string start_str = FormatTimestampToString(start_timestamp);
  std::string end_str = FormatTimestampToString(end_timestamp);

  input = "[" + FormatTimestampToString(start_timestamp) + ", " +
          FormatTimestampToString(end_timestamp) + ")";

  return StatusRecord::Ok();
}

// Func to convert date time from client library to desired format
// Example: [2024-02-20T12:30:45, 2024-03-20T14:15:30.000425) to [2024-02-20
// 12:30:45.00000, 2024-03-20T14:15:30.000425)
void NormalizeDatetimeRange(std::string& src_str) {
  std::replace(src_str.begin(), src_str.end(), 'T', ' ');

  // Ensure both timestamps have fractions
  size_t comma_pos = src_str.find(',');
  if (comma_pos != std::string::npos) {
    size_t first_fraction_pos = src_str.find('.', comma_pos - 9);
    if (first_fraction_pos == std::string::npos ||
        first_fraction_pos > comma_pos) {
      src_str.insert(comma_pos, ".000000");
      comma_pos += 7;
    }

    size_t second_fraction_pos = src_str.find('.', comma_pos + 9);
    size_t close_bracket_pos = src_str.find(')', comma_pos + 1);
    if (second_fraction_pos == std::string::npos ||
        second_fraction_pos > close_bracket_pos) {
      src_str.insert(close_bracket_pos, ".000000");
    }
  }
}

StatusRecord ConvertFromRangeDSValue(DSValue const& src_dsval,
                                     DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);
  SQLLEN buffer_length = dest_data.buflen;

  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }
  // Example: [2024-10-10, 2024-10-11)
  std::regex const date_range_regex(
      R"(\[(\d{4})-(\d{2})-(\d{2}), (\d{4})-(\d{2})-(\d{2})\))");

  // Example: [2024-02-20T12:30:45, 2024-03-20T14:15:30.000425)
  std::regex const datetime_range_regex(
      R"(^\[(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(\.\d+)?\s*,\s*(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(\.\d+)?\)$)");

  bool is_datetime_range = std::regex_match(src_str, datetime_range_regex);
  bool is_date_range = std::regex_match(src_str, date_range_regex);

  if (is_datetime_range) {
    NormalizeDatetimeRange(src_str);
  } else if (!is_date_range) {
    ConvertRangeToTimestampFormat(src_str);
  }

  switch (dest_data.type) {
    case SQL_C_CHAR: {
      return StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
    }
    case SQL_C_BINARY: {
      if (!std::regex_match(src_str, date_range_regex)) {
// Existing Driver returns timestamp range in case of binary conversion in the
// format "[value, value) " on windows
#ifdef _WIN32
        src_str.append(" ");
#else
        // whereas on linux it returns "[value, value):"
        src_str.append(":");
#endif  //_WIN32
      }
      return StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(src_str);
      if (!wstr) {
        return StatusRecord{SQLStates::k_HY000(),
                            "Conversion to SQL_C_WCHAR failed."};
      }
      SQLLEN required_size = (wstr->length() + 1) * sizeof(wchar_t);
      return WStrToOutputBufferResponse(
          wstr.GetValue(), dest_data.buf, buffer_length, src_str.length(),
          required_size, reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
