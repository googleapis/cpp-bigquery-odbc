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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bq_driver_internal::IsLengthSensitiveType;
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
  if (!status_record.ok())
    LOG(ERROR) << "ConvertNumeric::CheckLimitsArithmetic:: "
               << status_record.message;
  return status_record;
}

odbc_internal::StatusRecord ConvertFromNumericDSValue(DSValue const& src_dsval,
                                                      DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  if (!dest_data.buf) {
    LOG(ERROR) << "ConvertFromNumericDSValue::Destination buffer is null";
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (dest_data.buflen < 0) {
    LOG(ERROR) << "ConvertFromNumericDSValue::Buffer length is negative";
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }
  std::string str_input;
  DSValueToString(src_dsval, str_input);
  StatusRecord status_record = StatusRecord::Ok();
  StatusRecordOr<SQLDOUBLE> conversion_status = ConvertToDouble(str_input);
  if (!conversion_status) {
    LOG(ERROR) << "ConvertFromNumericDSValue::ConvertToDouble:: "
               << conversion_status.GetStatusRecord().message;
    return conversion_status.GetStatusRecord();
  }
  SQLDOUBLE numeric_no = *conversion_status;
  switch (dest_data.type) {
    case SQL_C_NUMERIC: {
      SQL_NUMERIC_STRUCT numst;
      try {
        status_record = GetNumericDetailsFromStr(str_input, numst);
      } catch (std::exception const& e) {
        LOG(ERROR) << "ConvertFromNumericDSValue::GetNumericDetailsFromStr:: "
                   << "Invalid character value for cast: " << e.what();
        return StatusRecord{SQLStates::k_22018(),
                            "Invalid character value for cast"};
      }
      if (status_record.sql_state == SQLStates::k_22003()) {
        LOG(ERROR) << "ConvertFromNumericDSValue::GetNumericDetailsFromStr:: "
                   << status_record.message;
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
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(str_input);
      if (!wstr) {
        LOG(ERROR) << "ConvertFromNumericDSValue::Utf8ToUtf16:: "
                   << wstr.GetStatusRecord().message;
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "DSValueToWchar Conversion Failed"};
        break;
      }
      SQLLEN wchar_capacity = dest_data.buflen / WireWcharSize();
      auto src_len = static_cast<SQLINTEGER>(wstr->length());
      SQLINTEGER required_chars = src_len + 1;
      WStrToOutputBufferResponse(wstr.GetValue(), dest_data.buf, wchar_capacity,
                                 src_len, required_chars, dest_data.result_len);
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
      LOG(ERROR) << "ConvertFromNumericDSValue::Numeric value out of range for "
                    "BIT type.";
      return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
    }
    case SQL_C_SBIGINT: {
      try {
        SQLBIGINT bigint_val = std::stoll(str_input);
        *reinterpret_cast<SQLBIGINT*>(dest_data.buf) = bigint_val;
      } catch (std::invalid_argument const&) {
        LOG(ERROR) << "ConvertFromNumericDSValue::stoll:: Invalid character "
                      "value for cast";
        return StatusRecord{SQLStates::k_22018(),
                            "Invalid character value for cast"};
      } catch (std::out_of_range const&) {
        LOG(ERROR)
            << "ConvertFromNumericDSValue::stoll:: Numeric value out of range";
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      }
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQLBIGINT);
      }
      return StatusRecord::Ok();
    }
    case SQL_C_UBIGINT: {
      if (!str_input.empty() && str_input[0] == '-') {
        LOG(ERROR) << "ConvertFromNumericDSValue::Negative value cannot be "
                      "stored in unsigned type";
        return StatusRecord{SQLStates::k_22003(),
                            "Negative value cannot be stored in unsigned type"};
      }
      try {
        SQLUBIGINT val = std::stoull(str_input);
        *reinterpret_cast<SQLUBIGINT*>(dest_data.buf) = val;
      } catch (std::out_of_range const&) {
        LOG(ERROR)
            << "ConvertFromNumericDSValue::stoull:: Numeric value out of range";
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      } catch (std::invalid_argument const&) {
        LOG(ERROR) << "ConvertFromNumericDSValue::stoull:: Invalid character "
                      "value for cast";
        return StatusRecord{SQLStates::k_22018(),
                            "Invalid character value for cast"};
      }
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQLUBIGINT);
      }
      return StatusRecord::Ok();
    }
    case SQL_C_BINARY: {
      auto val = static_cast<int32_t>(numeric_no);

      // Determine the minimum number of bytes required to represent `val` in
      // binary. This is based on signed integer value ranges:
      size_t byte_count = 1;
      if (val >= -128 && val <= 127) {
        byte_count = 1;  // Fits in 1 byte (int8_t)
      } else if (val >= -32768 && val <= 32767) {
        byte_count = 2;  // Fits in 2 bytes (int16_t)
      } else if (val >= -8388608 && val <= 8388607) {
        byte_count = 3;  // Fits in 3 bytes (24-bit signed)
      } else {
        byte_count = 4;  // Fits in 4 bytes (int32_t)
      }

      // Check if the destination buffer is large enough to hold the binary
      // data.
      if (dest_data.buflen < static_cast<SQLLEN>(byte_count)) {
        LOG(ERROR)
            << "ConvertFromNumericDSValue::Buffer too small for binary data";
        return StatusRecord{SQLStates::k_22003(),
                            "Buffer too small for binary data"};
      }

      // Write the value to the buffer in little-endian order (least significant
      // byte first).
      auto* out = reinterpret_cast<uint8_t*>(dest_data.buf);
      for (size_t i = 0; i < byte_count; ++i) {
        out[i] = static_cast<uint8_t>((val >> (8 * i)) & 0xFF);
      }

      // Set the number of bytes written if result_len pointer is provided.
      if (dest_data.result_len) {
        *dest_data.result_len = static_cast<SQLLEN>(byte_count);
      }

      return StatusRecord::Ok();
    }

    case SQL_C_INTERVAL_YEAR: {
      auto* interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_data.buf);
      interval->intval.year_month.year = static_cast<SQLUINTEGER>(numeric_no);
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_INTERVAL_STRUCT);
      }
      return StatusRecord::Ok();
    }

    case SQL_C_INTERVAL_MONTH: {
      auto* interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_data.buf);
      interval->intval.year_month.month = static_cast<SQLUINTEGER>(numeric_no);
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_INTERVAL_STRUCT);
      }
      return StatusRecord::Ok();
    }

    case SQL_C_INTERVAL_DAY: {
      auto* interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_data.buf);
      interval->intval.day_second.day = static_cast<SQLUINTEGER>(numeric_no);
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_INTERVAL_STRUCT);
      }
      return StatusRecord::Ok();
    }

    case SQL_C_INTERVAL_HOUR: {
      auto* interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_data.buf);
      interval->intval.day_second.hour = static_cast<SQLUINTEGER>(numeric_no);
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_INTERVAL_STRUCT);
      }
      return StatusRecord::Ok();
    }

    case SQL_C_INTERVAL_MINUTE: {
      auto* interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_data.buf);
      interval->intval.day_second.minute = static_cast<SQLUINTEGER>(numeric_no);
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_INTERVAL_STRUCT);
      }
      return StatusRecord::Ok();
    }

    case SQL_C_INTERVAL_SECOND: {
      auto* interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_data.buf);
      interval->intval.day_second.second = static_cast<SQLUINTEGER>(numeric_no);
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_INTERVAL_STRUCT);
      }
      return StatusRecord::Ok();
    }
    default: {
      LOG(WARNING)
          << "ConvertFromNumericDSValue::Conversion is unsupported for C-type: "
          << dest_data.type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return status_record;
}

odbc_internal::StatusRecord ConvertFromStringDSValue(DSValue const& src_dsval,
                                                     DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  using odbc_internal::StatusRecordOr;

  std::string_view src_view(src_dsval.data(), src_dsval.size());

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN* res_len = dest_data.result_len;

  if (dest_type == SQL_C_CHAR) {
    return StringValueToOutputBufferResponse(src_view, dest_data);
  }
  if (dest_type == SQL_C_WCHAR) {
    if (!dest_data.buf) {
      LOG(ERROR) << "ConvertFromStringDSValue::SQL_C_WCHAR: "
                    "Destination buffer is null";
      return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
    }
    if (dest_data.buflen < 0) {
      LOG(ERROR) << "ConvertFromStringDSValue::SQL_C_WCHAR: "
                    "Buffer length is negative";
      return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
    }
    StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(src_view);
    if (!wstr.Ok()) {
      LOG(ERROR) << "ConvertFromStringDSValue::Utf8ToUtf16:: "
                 << wstr.GetStatusRecord().message;
      return StatusRecord{SQLStates::k_HY000(),
                          "SQL_C_WCHAR Conversion Failed"};
    }
    std::wstring wide_str = wstr.GetValue();
    if (!wide_str.empty() && wide_str.back() == L'\0') {
      wide_str.pop_back();
    }

    auto src_len = static_cast<SQLINTEGER>(wide_str.length());
    SQLLEN wchar_capacity = dest_data.buflen / WireWcharSize();
    SQLINTEGER required_chars = src_len + 1;
    return WStrToOutputBufferResponse(wide_str, dest_data.buf, wchar_capacity,
                                      src_len, required_chars,
                                      dest_data.result_len);
  }

  std::string src_str(src_view);

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
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_YEAR: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_MONTH:
        if (sscanf(str.c_str(), "%d", &m) == 1) {
          dest_val->intval.year_month.year = 0;
          dest_val->intval.year_month.month = m;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_MONTH: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_YEAR_TO_MONTH:
        if (sscanf(str.c_str(), "%d-%d", &y, &m) == 2) {
          dest_val->intval.year_month.year = y;
          dest_val->intval.year_month.month = m;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_YEAR_TO_MONTH: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_DAY:
        if (sscanf(str.c_str(), "%d", &d) == 1) {
          dest_val->intval.day_second.day = d;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_DAY: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_HOUR:
        if (sscanf(str.c_str(), "%d", &h) == 1) {
          dest_val->intval.day_second.hour = h;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_HOUR: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_MINUTE:
        if (sscanf(str.c_str(), "%d", &min) == 1) {
          dest_val->intval.day_second.minute = min;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_MINUTE: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_SECOND:
        if (sscanf(str.c_str(), "%d", &s) == 1) {
          dest_val->intval.day_second.second = s;
          dest_val->intval.day_second.fraction = 0;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_SECOND: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_DAY_TO_HOUR:
        if (sscanf(str.c_str(), "%d %d", &d, &h) == 2) {
          dest_val->intval.day_second.day = d;
          dest_val->intval.day_second.hour = h;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_DAY_TO_HOUR: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_DAY_TO_MINUTE:
        if (sscanf(str.c_str(), "%d %d:%d", &d, &h, &min) == 3) {
          dest_val->intval.day_second.day = d;
          dest_val->intval.day_second.hour = h;
          dest_val->intval.day_second.minute = min;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_DAY_TO_MINUTE: "
                     << str;
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
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_DAY_TO_SECOND: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_HOUR_TO_MINUTE:
        if (sscanf(str.c_str(), "%d:%d", &h, &min) == 2) {
          dest_val->intval.day_second.hour = h;
          dest_val->intval.day_second.minute = min;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_HOUR_TO_MINUTE: "
                     << str;
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
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_HOUR_TO_SECOND: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      case SQL_C_INTERVAL_MINUTE_TO_SECOND:
        if (sscanf(str.c_str(), "%d:%d", &min, &s) == 2) {
          dest_val->intval.day_second.minute = min;
          dest_val->intval.day_second.second = s;
          dest_val->intval.day_second.fraction = 0;
        } else {
          LOG(ERROR) << "ConvertFromStringDSValue::sscanf:: Invalid interval "
                        "format for SQL_C_INTERVAL_MINUTE_TO_SECOND: "
                     << str;
          return StatusRecord{SQLStates::k_22003(), "Invalid interval format"};
        }
        break;
      default:
        LOG(ERROR) << "ConvertFromStringDSValue:: Invalid interval type: "
                   << dest_type;
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
    LOG(ERROR) << "ConvertFromStringDSValue:: Invalid date format: " << src_str;
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
    LOG(ERROR) << "ConvertFromStringDSValue:: Invalid time format: " << src_str;
    return StatusRecord{SQLStates::k_22003(), "Invalid time format"};
  }
  if (dest_type == SQL_C_TYPE_TIMESTAMP) {
    auto* dest_val = reinterpret_cast<TIMESTAMP_STRUCT*>(dest_buf);
    if (src_str.size() >= 19 && (src_str[10] == ' ' || src_str[10] == 'T') &&
        src_str[4] == '-' && src_str[7] == '-' && src_str[13] == ':' &&
        src_str[16] == ':') {
      try {
        dest_val->year = std::stoi(src_str.substr(0, 4));
        dest_val->month = std::stoi(src_str.substr(5, 2));
        dest_val->day = std::stoi(src_str.substr(8, 2));
        dest_val->hour = std::stoi(src_str.substr(11, 2));
        dest_val->minute = std::stoi(src_str.substr(14, 2));
        dest_val->second = std::stoi(src_str.substr(17, 2));
        dest_val->fraction = 0;
      } catch (std::exception const&) {
        LOG(ERROR) << "ConvertFromStringDSValue:: Invalid timestamp format: "
                   << src_str;
        return StatusRecord{SQLStates::k_22003(), "Invalid timestamp format"};
      }

      if (res_len) *res_len = sizeof(TIMESTAMP_STRUCT);
      return StatusRecord::Ok();
    }
    LOG(ERROR) << "ConvertFromStringDSValue:: Invalid timestamp format: "
               << src_str;
    return StatusRecord{SQLStates::k_22003(), "Invalid timestamp format"};
  }

  if (dest_type == SQL_C_STINYINT || dest_type == SQL_C_TINYINT) {
    auto* dest_val = reinterpret_cast<SQLSCHAR*>(dest_buf);
    try {
      *dest_val = static_cast<SQLSCHAR>(std::stoi(src_str));
    } catch (std::invalid_argument const&) {
      LOG(ERROR) << "ConvertFromStringDSValue::stoi:: Invalid tinyint value: "
                 << src_str;
      return StatusRecord{SQLStates::k_22018(),
                          "Invalid character value for cast"};
    } catch (std::out_of_range const&) {
      LOG(ERROR) << "ConvertFromStringDSValue::stoi:: Tinyint value out of "
                    "range: "
                 << src_str;
      return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
    }
    if (res_len) {
      *res_len = sizeof(SQLSCHAR);
    }
    return StatusRecord::Ok();
  }

  if (dest_type == SQL_C_UTINYINT) {
    auto* dest_val = reinterpret_cast<SQLCHAR*>(dest_buf);
    if (!src_str.empty() && src_str[0] == '-') {
      LOG(ERROR) << "ConvertFromStringDSValue::stoull:: Negative value cannot "
                    "be stored in unsigned tinyint: "
                 << src_str;
      return StatusRecord{SQLStates::k_22003(),
                          "Negative value cannot be stored in unsigned type"};
    }
    try {
      *dest_val = static_cast<SQLCHAR>(std::stoul(src_str));
    } catch (std::invalid_argument const&) {
      LOG(ERROR)
          << "ConvertFromStringDSValue::stoul:: Invalid unsigned tinyint "
             "value: "
          << src_str;
      return StatusRecord{SQLStates::k_22018(),
                          "Invalid character value for cast"};
    } catch (std::out_of_range const&) {
      LOG(ERROR)
          << "ConvertFromStringDSValue::stoul:: Unsigned tinyint value out of "
             "range: "
          << src_str;
      return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
    }
    if (res_len) {
      *res_len = sizeof(SQLCHAR);
    }
    return StatusRecord::Ok();
  }

  // TODO(sachinpro): This assumes that SQLDOUBLE is a safe container for all
  // arithmetic types, which is not true for int64 which has a range(-2^63 to
  // +2^63 -1). Integer range of SQLDOUBLE is (-2^54 to +2^54 -1). Here we
  // should use SQLDOUBLE for floating point values and int64 for pure integers
  StatusRecordOr<SQLDOUBLE> conversion_status = ConvertToDouble(src_str);
  if (!conversion_status) {
    LOG(ERROR) << "ConvertFromStringDSValue::ConvertToDouble:: "
               << conversion_status.GetStatusRecord().message;
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
      try {
        *dest_val = std::stoll(src_str);
      } catch (std::invalid_argument const&) {
        LOG(ERROR) << "ConvertFromStringDSValue::stoll:: Invalid bigint value: "
                   << src_str;
        return StatusRecord{SQLStates::k_22018(),
                            "Invalid character value for cast"};
      } catch (std::out_of_range const&) {
        LOG(ERROR)
            << "ConvertFromStringDSValue::stoll:: Bigint value out of range: "
            << src_str;
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      }
      if (res_len) {
        *res_len = sizeof(int64_t);
      }
      return StatusRecord::Ok();
    }
    case SQL_C_UBIGINT: {
      auto* dest_val = reinterpret_cast<uint64_t*>(dest_buf);
      if (!src_str.empty() && src_str[0] == '-') {
        LOG(ERROR)
            << "ConvertFromStringDSValue::stoull:: Negative value cannot "
               "be stored in unsigned bigint: "
            << src_str;
        return StatusRecord{SQLStates::k_22003(),
                            "Negative value cannot be stored in unsigned type"};
      }
      try {
        *dest_val = std::stoull(src_str);
      } catch (std::invalid_argument const&) {
        LOG(ERROR) << "ConvertFromStringDSValue::stoull:: Invalid unsigned "
                      "bigint value: "
                   << src_str;
        return StatusRecord{SQLStates::k_22018(),
                            "Invalid character value for cast"};
      } catch (std::out_of_range const&) {
        LOG(ERROR)
            << "ConvertFromStringDSValue::stoull:: Unsigned bigint value "
               "out of range: "
            << src_str;
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      }
      if (res_len) {
        *res_len = sizeof(uint64_t);
      }
      return StatusRecord::Ok();
    }
    case SQL_C_SHORT:
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
    case SQL_C_LONG:
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
    case SQL_C_NUMERIC: {
      SQL_NUMERIC_STRUCT numst;
      StatusRecord status_record;
      try {
        status_record = GetNumericDetailsFromStr(src_str, numst);
      } catch (std::exception const& e) {
        LOG(ERROR) << "ConvertFromStringDSValue::GetNumericDetailsFromStr:: "
                   << "Invalid character value for cast: " << e.what();
        return StatusRecord{SQLStates::k_22018(),
                            "Invalid character value for cast"};
      }
      if (status_record.sql_state == SQLStates::k_22003()) {
        return status_record;
      }
      auto* dest_val = reinterpret_cast<SQL_NUMERIC_STRUCT*>(dest_data.buf);
      *dest_val = numst;
      if (dest_data.result_len) {
        *dest_data.result_len = sizeof(SQL_NUMERIC_STRUCT);
      }
      return status_record;
    }
    default: {
      LOG(WARNING)
          << "ConvertFromStringDSValue::Conversion is unsupported for C-type: "
          << dest_type;
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
  if (IsLengthSensitiveType(dest_type) && buffer_length <= 0) {
    LOG(ERROR) << "ConvertFromTimeDSValue:: Invalid Buffer length: "
               << buffer_length;
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
        snprintf(dest, buffer_length, "%02d:%02d:%02d", dest_time.hour,
                 dest_time.minute, dest_time.second);
        if (res_len) {
          *res_len = kTimeCharLength;
        }
      }
      break;
    }
    case SQL_C_TYPE_TIME: {
      auto* dest = reinterpret_cast<SQL_TIME_STRUCT*>(dest_buf);
      *dest = dest_time;
      if (res_len) {
        *res_len = sizeof(SQL_TIME_STRUCT);
      }
      break;
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
      SQLINTEGER k_time_src_len = time_src_str.length();
      SQLINTEGER supp_max_len = 9;
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(time_src_str);
      if (!wstr) {
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "DSValueToWchar Conversion Failed"};
        break;
      }
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      SQLLEN required_chars = static_cast<SQLLEN>(wstr->length()) + 1;
      return WStrToOutputBufferResponse(
          wstr.GetValue(), dest_buf, wchar_capacity, k_time_src_len,
          required_chars, reinterpret_cast<SQLLEN*>(dest_data.result_len));
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

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  // Define length variables
  constexpr int kTimestampBinaryLength = sizeof(SQL_TIMESTAMP_STRUCT);

  if (!dest_buf) {
    return StatusRecord::Ok();
  }
  if (IsLengthSensitiveType(dest_type) && buffer_length <= 0) {
    LOG(ERROR) << "ConvertFromTimestampDSValue:: Invalid Buffer length: "
               << buffer_length;
    return StatusRecord{SQLStates::k_HY090(), "Invalid Buffer length"};
  }

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_type) {
    case SQL_C_CHAR: {
      std::string timestamp_src_str =
          FormatTimestampToString(timestamp_src_struct);
      int k_timestamp_src_len = timestamp_src_str.length();
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
        LOG(WARNING)
            << "ConvertFromTimestampDSValue:: Data truncated for SQL_C_CHAR.";
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        LOG(ERROR) << "ConvertFromTimestampDSValue:: Buffer length is "
                      "insufficient for SQL_C_CHAR.";
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }

    case SQL_C_WCHAR: {
      std::string timestamp_src_str =
          FormatTimestampToString(timestamp_src_struct);
      int k_timestamp_src_len = timestamp_src_str.length();
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(timestamp_src_str);
      if (!wstr) {
        LOG(ERROR)
            << "ConvertFromTimestampDSValue:: DSValueToWchar Conversion Failed";
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "DSValueToWchar Conversion Failed"};
        break;
      }
      auto write_terminator = [&](SQLLEN char_index) {
        auto* p = static_cast<uint8_t*>(dest_buf) +
                  (char_index * WireWcharSize());
        std::memset(p, 0, WireWcharSize());
      };
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      if (wchar_capacity > k_timestamp_src_len) {
        if (res_len) {
          *res_len = k_timestamp_src_len * WireWcharSize();
        }
        WriteWideToWireBuffer(*wstr, dest_buf, k_timestamp_src_len);
        write_terminator(k_timestamp_src_len);
      } else if (20 <= wchar_capacity &&
                 wchar_capacity <= k_timestamp_src_len) {
        if (res_len) {
          *res_len = wchar_capacity * WireWcharSize();
        }
        WriteWideToWireBuffer(*wstr, dest_buf, wchar_capacity);
        write_terminator(wchar_capacity - 1);
        LOG(WARNING)
            << "ConvertFromTimestampDSValue:: Data truncated for SQL_C_WCHAR.";
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        LOG(ERROR) << "ConvertFromTimestampDSValue:: Buffer length is "
                      "insufficient for SQL_C_WCHAR.";
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
        LOG(ERROR) << "ConvertFromTimestampDSValue:: Buffer length is "
                      "insufficient for SQL_C_BINARY.";
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
        LOG(WARNING) << "ConvertFromTimestampDSValue:: Date data, right "
                        "truncated (time part was present).";
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
        LOG(WARNING) << "ConvertFromTimestampDSValue:: Time data, right "
                        "truncated (fractional part was present).";
        status_record =
            StatusRecord{SQLStates::k_01S07(), "Time data, right truncated"};
      }
      break;
    }

    case SQL_C_TYPE_TIMESTAMP: {
      return TimestampToOutputBufferResponse(
          timestamp_src_struct, dest_buf,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }

    default:
      LOG(ERROR) << "ConvertFromTimestampDSValue:: Conversion is unsupported "
                    "for C-type: "
                 << dest_type;
      status_record =
          StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
  return status_record;
}

odbc_internal::StatusRecord ConvertFromDatetimeDSValue(DSValue const& src_dsval,
                                                       DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  using odbc_internal::StatusRecordOr;

  // Datetime uses SQL_TIMESTAMP_STRUCT struct
  SQL_TIMESTAMP_STRUCT datetime_src_struct;
  DSValueToDatetime(src_dsval, datetime_src_struct);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  // Define length variables
  constexpr int kDatetimeBinaryLength = sizeof(SQL_TIMESTAMP_STRUCT);

  if (!dest_buf) {
    return StatusRecord::Ok();
  }
  if (IsLengthSensitiveType(dest_type) && buffer_length <= 0) {
    LOG(ERROR) << "ConvertFromDatetimeDSValue:: Invalid Buffer length: "
               << buffer_length;
    return StatusRecord{SQLStates::k_HY090(), "Invalid Buffer length"};
  }

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_type) {
    case SQL_C_CHAR: {
      std::string datetime_src_str =
          FormatDatetimeToString(datetime_src_struct);
      int k_datetime_src_len = datetime_src_str.length();
      auto* dest = reinterpret_cast<char*>(dest_buf);
      if (buffer_length > k_datetime_src_len) {
        if (res_len) {
          *res_len = k_datetime_src_len;
        }
        std::strncpy(dest, datetime_src_str.c_str(), k_datetime_src_len);
        dest[k_datetime_src_len] = '\0';
      } else if (20 <= buffer_length && buffer_length <= k_datetime_src_len) {
        if (res_len) {
          *res_len = buffer_length;
        }
        std::strncpy(dest, datetime_src_str.c_str(), buffer_length - 1);
        dest[buffer_length - 1] = '\0';
        LOG(WARNING)
            << "ConvertFromDatetimeDSValue:: Data truncated for SQL_C_CHAR.";
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        LOG(ERROR) << "ConvertFromDatetimeDSValue:: Buffer length is "
                      "insufficient for SQL_C_CHAR.";
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }
    case SQL_C_WCHAR: {
      std::string datetime_src_str =
          FormatDatetimeToString(datetime_src_struct);
      int k_datetime_src_len = datetime_src_str.length();
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(datetime_src_str);
      if (!wstr) {
        LOG(ERROR)
            << "ConvertFromDatetimeDSValue:: DSValueToWchar Conversion Failed";
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "DSValueToWchar Conversion Failed"};
        break;
      }
      auto write_terminator = [&](SQLLEN char_index) {
        auto* p = static_cast<uint8_t*>(dest_buf) +
                  (char_index * WireWcharSize());
        std::memset(p, 0, WireWcharSize());
      };
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      if (wchar_capacity > k_datetime_src_len) {
        if (res_len) {
          *res_len = k_datetime_src_len * WireWcharSize();
        }
        WriteWideToWireBuffer(*wstr, dest_buf, k_datetime_src_len);
        write_terminator(k_datetime_src_len);
      } else if (20 <= wchar_capacity && wchar_capacity <= k_datetime_src_len) {
        if (res_len) {
          *res_len = wchar_capacity * WireWcharSize();
        }
        WriteWideToWireBuffer(*wstr, dest_buf, wchar_capacity);
        write_terminator(wchar_capacity - 1);
        LOG(WARNING)
            << "ConvertFromDatetimeDSValue:: Data truncated for SQL_C_WCHAR.";
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        LOG(ERROR) << "ConvertFromDatetimeDSValue:: Buffer length is "
                      "insufficient for SQL_C_WCHAR.";
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }
    case SQL_C_BINARY: {
      if (kDatetimeBinaryLength <= buffer_length) {
        if (res_len) {
          *res_len = kDatetimeBinaryLength;
        }
        datetime_src_struct.fraction = datetime_src_struct.fraction * 1000;
        std::memcpy(dest_buf, &datetime_src_struct, kDatetimeBinaryLength);

      } else {
        LOG(ERROR) << "ConvertFromDatetimeDSValue:: Buffer length is "
                      "insufficient for SQL_C_BINARY.";
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
      if (datetime_src_struct.hour == 0 && datetime_src_struct.minute == 0 &&
          datetime_src_struct.second == 0) {
        date->year = datetime_src_struct.year;
        date->month = datetime_src_struct.month;
        date->day = datetime_src_struct.day;
      } else {
        date->year = datetime_src_struct.year;
        date->month = datetime_src_struct.month;
        date->day = datetime_src_struct.day;
        LOG(WARNING) << "ConvertFromDatetimeDSValue:: Date data, right "
                        "truncated (time part was present).";
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
      if (datetime_src_struct.fraction == 0) {
        time->hour = datetime_src_struct.hour;
        time->minute = datetime_src_struct.minute;
        time->second = datetime_src_struct.second;
      } else {
        time->hour = datetime_src_struct.hour;
        time->minute = datetime_src_struct.minute;
        time->second = datetime_src_struct.second;
        LOG(WARNING) << "ConvertFromDatetimeDSValue:: Time data, right "
                        "truncated (fractional part was present).";
        status_record =
            StatusRecord{SQLStates::k_01S07(), "Time data, right truncated"};
      }
      break;
    }
    case SQL_C_TYPE_TIMESTAMP: {
      return TimestampToOutputBufferResponse(
          datetime_src_struct, dest_buf,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }
    default:
      LOG(ERROR) << "ConvertFromDatetimeDSValue:: Conversion is unsupported "
                    "for C-type: "
                 << dest_type;
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
    LOG(ERROR) << "ConvertFromDateDSValue:: Destination buffer is null";
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (IsLengthSensitiveType(dest_type) && buffer_length <= 0) {
    LOG(ERROR) << "ConvertFromDateDSValue:: Invalid Buffer length: "
               << buffer_length;
    return StatusRecord{SQLStates::k_HY090(), "Invalid Buffer length"};
  }

  DSValueToDate(src_dsval, conn_date);

  constexpr int kDateCharLength = SQL_DATE_LEN;
  constexpr int kDateWcharLength = kDateCharLength;
  constexpr int kDateBinaryLength = sizeof(SQL_DATE_STRUCT);

  StatusRecord status_record = StatusRecord::Ok();

  switch (dest_type) {
    case SQL_C_TYPE_DATE: {
      auto* dest = reinterpret_cast<SQL_DATE_STRUCT*>(dest_buf);
      *dest = conn_date;
      if (res_len) {
        *res_len = sizeof(SQL_DATE_STRUCT);
      }
      break;
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
      char buffer[11];
      snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", conn_date.year,
               conn_date.month, conn_date.day);
      std::string formatted_date = buffer;
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(formatted_date);
      if (!wstr) {
        LOG(ERROR)
            << "ConvertFromDateDSValue:: DSValueToWchar Conversion Failed";
        return StatusRecord{SQLStates::k_HY000(),
                            "DSValueToWchar Conversion Failed"};
      }
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      auto src_len = static_cast<SQLINTEGER>(wstr->length());
      SQLINTEGER required_chars = src_len + 1;
      return WStrToOutputBufferResponse(
          wstr.GetValue(), dest_buf, wchar_capacity, src_len, required_chars,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }
    default:
      LOG(ERROR)
          << "ConvertFromDateDSValue:: Conversion is unsupported for C-type: "
          << dest_type;
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
        LOG(ERROR) << "ConvertStringToJsonOutputBuffer::Utf8ToUtf16:: "
                   << wide_string.GetStatusRecord().message;
        return StatusRecord{SQLStates::k_HY000(),
                            "Conversion to UTF-16 failed"};
      }
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      auto src_len = static_cast<SQLINTEGER>(wide_string->length());
      SQLINTEGER required_chars = src_len + 1;
      return WStrToOutputBufferResponse(wide_string.GetValue(), dest_buf,
                                        wchar_capacity, src_len, required_chars,
                                        reinterpret_cast<SQLLEN*>(res_len));
    }
    case SQL_C_BINARY: {
      return StringValueToOutputBufferResponse<SQLLEN>(
          src_str.c_str(), dest_buf, buffer_length, res_len);
    }
    default: {
      LOG(ERROR) << "ConvertStringToJsonOutputBuffer:: Conversion is "
                    "unsupported for C-type: "
                 << dest_type;
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
    // If the try above failed, it implies that the json didn't
    //  follow the rest API response.
    // It might be because the application is using HTAPI.
    // We can return the string response as it is, in this case.
    return ConvertStringToJsonOutputBuffer(src_str, dest_data);
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
      SQLLEN wchar_capacity = dest_data.buflen / WireWcharSize();
      auto src_len = static_cast<SQLINTEGER>(wide_string->length());
      SQLINTEGER required_chars = src_len + 1;
      return WStrToOutputBufferResponse(
          *wide_string, dest_data.buf, wchar_capacity, src_len, required_chars,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
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
  auto status = ConvertStringToIntervalStruct(interval_src_str, conn_interval);
  if (!status.ok()) {
    LOG(ERROR) << "ConvertFromIntervalDSValue::ConvertStringToIntervalStruct:: "
               << status.message;
    return StatusRecord{status.sql_state, status.message};
  }

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  auto* res_len = reinterpret_cast<SQLLEN*>(dest_data.result_len);

  constexpr int kIntervalCharLength = 30;
  int interval_src_len = interval_src_str.length();

  if (!dest_buf) {
    return StatusRecord::Ok();
  }
  if (IsLengthSensitiveType(dest_type) && buffer_length <= 0) {
    LOG(ERROR) << "ConvertFromIntervalDSValue:: Invalid Buffer length: "
               << buffer_length;
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
        LOG(WARNING)
            << "ConvertFromIntervalDSValue:: Data truncated for SQL_C_CHAR.";
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        strncpy(dest, "Y-M D H:M:S", buffer_length - 1);
        LOG(ERROR) << "ConvertFromIntervalDSValue:: Buffer length is "
                      "insufficient for SQL_C_CHAR.";
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(interval_src_str);
      auto whole_digit_count = GetWholeDigitCount(interval_src_str);
      if (!wstr) {
        LOG(ERROR) << "ConvertFromIntervalDSValue::Utf8ToUtf16:: "
                   << wstr.GetStatusRecord().message;
        status_record =
            StatusRecord{SQLStates::k_HY000(), wstr.GetStatusRecord().message};
        break;
      }
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      auto interval_char_length =
          static_cast<SQLINTEGER>(wstr.GetValue().length());
      return WStrIntervalBufferResponse(
          wstr.GetValue(), dest_buf, wchar_capacity, interval_char_length,
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
        LOG(ERROR) << "ConvertFromIntervalDSValue:: Value out of range for "
                      "SQL_C_NUMERIC.";
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
      auto* dest_interval = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_buf);
      *dest_interval = conn_interval;
      if (res_len) {
        *res_len = sizeof(SQL_INTERVAL_STRUCT);
      }
      break;
    }
    default:
      LOG(ERROR) << "ConvertFromIntervalDSValue:: Conversion is unsupported "
                    "for C-type: "
                 << dest_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
  return status_record;
}

StatusRecord ConvertFromBooleanDSValue(DSValue const& src_dsval,
                                       DataBuffer& dest_data) {
  bool conn_bool = false;
  std::string src_str;

  if (!dest_data.buf) {
    LOG(ERROR) << "ConvertFromBooleanDSValue:: Destination buffer is null.";
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }

  if (dest_data.buflen < 0) {
    LOG(ERROR) << "ConvertFromBooleanDSValue:: Buffer length is negative.";
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  if (dest_data.type == SQL_C_CHAR || dest_data.type == SQL_C_WCHAR) {
    src_str = std::string(src_dsval.begin(), src_dsval.end());
  } else {
    DSValueToBoolean(src_dsval, conn_bool);
  }
  StatusRecord status_record = StatusRecord::Ok();
  switch (dest_data.type) {
    case SQL_C_CHAR: {
      auto* dest = reinterpret_cast<char*>(dest_data.buf);
      SQLLEN len = src_str.size();
      if (dest_data.buflen < len) {
        std::memcpy(dest, src_str.c_str(), dest_data.buflen - 1);
        dest[dest_data.buflen - 1] = '\0';
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
        LOG(WARNING)
            << "ConvertFromBooleanDSValue:: String data, right truncated";
      } else {
        std::memcpy(dest, src_str.c_str(), len);
        dest[len] = '\0';
      }
      break;
    }

    case SQL_C_WCHAR: {
      auto* dest = reinterpret_cast<wchar_t*>(dest_data.buf);
      std::wstring wstr(src_str.begin(), src_str.end());
      SQLLEN wstr_len = wstr.size();
      size_t wchar_len = dest_data.buflen / sizeof(wchar_t);

      if (wchar_len < wstr_len + 1) {
        std::wcsncpy(dest, wstr.c_str(), wchar_len - 1);
        dest[wchar_len - 1] = L'\0';
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
        LOG(WARNING)
            << "ConvertFromBooleanDSValue:: String data, right truncated";
      } else {
        std::wcsncpy(dest, wstr.c_str(), wstr_len);
        dest[wstr_len] = L'\0';
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
      LOG(ERROR) << "ConvertFromBooleanDSValue:: Unsupported type: "
                 << dest_data.type;
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
    LOG(ERROR) << "ConvertFromGeographyDSValue:: Buffer length is negative.";
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
        LOG(ERROR) << "ConvertFromGeographyDSValue:: UTF-8 to UTF-16 "
                      "conversion failed: ";
        status_record = StatusRecord{SQLStates::k_HY000(),
                                     "Conversion to SQL_C_WCHAR failed."};
        break;
      }
      std::memset(dest_data.buf, 0, buffer_length);
      std::wstring const& wide_str = wstr.GetValue();
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      SQLLEN src_len = static_cast<SQLLEN>(wide_str.length());
      SQLLEN required_chars = src_len + 1;
      status_record = WStrToOutputBufferResponse(
          wide_str, dest_data.buf, wchar_capacity, src_len, required_chars,
          reinterpret_cast<SQLLEN*>(dest_data.result_len));
      break;
    }
    default: {
      LOG(ERROR) << "ConvertFromGeographyDSValue:: Unsupported type: "
                 << dest_data.type;
      status_record =
          StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
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

// This func converts a vector of SQLCHAR bytes (hex-encoded) to binary data,
// handling truncation if needed.
StatusRecord ConvertBytesToBinary(DSValue const& conn_val,
                                  DataBuffer& dest_data) {
  DSValue binary_data;
  DSValue base_value;
  DSValue hex_value;

  Base64Decode(conn_val, base_value);
  Base64ToASCIIHexFormat(base_value, hex_value);
  SQLLEN src_length = static_cast<SQLLEN>(hex_value.size());

  // Reserves space to optimize memory allocation as each hex pair forms one
  // byte.
  binary_data.reserve(hex_value.size() / 2);

  StatusRecord status_record = StatusRecord::Ok();

  auto hex_char_to_byte = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;  // Fallback case, should never happen if input is valid hex.
  };

  // Convert each hex pair into a byte
  for (size_t i = 0; i + 1 < hex_value.size(); i += 2) {
    uint8_t byte = (hex_char_to_byte(hex_value[i]) << 4) |
                   hex_char_to_byte(hex_value[i + 1]);
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
    LOG(WARNING) << "ConvertBytesToChar:: String data, right truncated.";
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
    LOG(ERROR) << "ConvertBytesToWChar:: UTF-8 to UTF-16 conversion failed: ";
    return StatusRecord{SQLStates::k_01004(),
                        "UTF-8 to UTF-16 conversion failed."};
  }

  std::wstring const& utf16_value = utf16_str.GetValue();
  size_t const required_size = utf16_value.length() * WireWcharSize();

  auto write_terminator = [&](size_t char_index) {
    auto* p = static_cast<uint8_t*>(dest_data.buf) +
              (char_index * WireWcharSize());
    std::memset(p, 0, WireWcharSize());
  };

  // Handle truncation if buffer is insufficient
  if (static_cast<size_t>(dest_data.buflen) < required_size) {
    size_t num_chars_to_copy = (dest_data.buflen / WireWcharSize()) - 1;
    WriteWideToWireBuffer(utf16_value, dest_data.buf, num_chars_to_copy);
    write_terminator(num_chars_to_copy);

    if (dest_data.result_len) {
      *dest_data.result_len = dest_data.buflen;
    }
    LOG(WARNING) << "ConvertBytesToWChar:: String data, right truncated.";
    return StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
  }
  WriteWideToWireBuffer(utf16_value, dest_data.buf, utf16_value.size());
  size_t buffer_chars = dest_data.buflen / WireWcharSize();
  if (utf16_value.size() < buffer_chars) {
    write_terminator(utf16_value.size());
  }

  // Set output length
  if (dest_data.result_len) {
    *dest_data.result_len = utf16_value.size() * WireWcharSize();
  }
  return status_record;
}

StatusRecord ConvertFromBytesDSValue(DSValue const& src_dsval,
                                     DataBuffer& dest_data) {
  if (!dest_data.buf) {
    LOG(ERROR) << "ConvertFromBytesDSValue:: Destination buffer is null";
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (dest_data.buflen < 0) {
    LOG(ERROR) << "ConvertFromBytesDSValue:: Buffer length is negative: "
               << dest_data.buflen;
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  switch (dest_data.type) {
    case SQL_C_BINARY:
      return ConvertBytesToBinary(src_dsval, dest_data);
    case SQL_C_CHAR:
      return ConvertBytesToChar(src_dsval, dest_data);
    case SQL_C_WCHAR:
      return ConvertBytesToWChar(src_dsval, dest_data);
    default:
      LOG(ERROR) << "Unsupported conversion type for bytes DSValue: "
                 << dest_data.type;
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
    LOG(ERROR) << "Invalid input format for timestamp range: " << input;
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
    LOG(ERROR) << "Failed to parse timestamps: " << e.what();
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
  // Ensure both timestamps have fractions
  size_t comma_pos = src_str.find(',');
  if (comma_pos != std::string::npos) {
    size_t first_fraction_pos = src_str.find('.', comma_pos - 9);
    if (first_fraction_pos != std::string::npos &&
        first_fraction_pos < comma_pos) {
      size_t digits = comma_pos - first_fraction_pos - 1;
      if (digits < 6) {
        src_str.insert(comma_pos, std::string(6 - digits, '0'));
        comma_pos += (6 - digits);
      } else if (digits > 6) {
        src_str.erase(first_fraction_pos + 7, digits - 6);
        comma_pos -= (digits - 6);
      }
    }

    size_t second_fraction_pos = src_str.find('.', comma_pos + 9);
    size_t close_bracket_pos = src_str.find(')', comma_pos + 1);
    if (second_fraction_pos != std::string::npos &&
        second_fraction_pos < close_bracket_pos) {
      size_t digits = close_bracket_pos - second_fraction_pos - 1;
      if (digits < 6) {
        src_str.insert(close_bracket_pos, std::string(6 - digits, '0'));
      } else if (digits > 6) {
        src_str.erase(second_fraction_pos + 7, digits - 6);
      }
    }
  }
}

namespace {
// Example: [2024-10-10, 2024-10-11)
re2::RE2 const kDateRangeRegex(R"(\[\d{4}-\d{2}-\d{2}, \d{4}-\d{2}-\d{2}\))");

// Example: [2024-02-20T12:30:45, 2024-03-20T14:15:30.000425)
re2::RE2 const kDatetimeRangeRegex(
    R"(\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?\s*,\s*\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?\))");
}  // namespace

StatusRecord ConvertFromRangeDSValue(DSValue const& src_dsval,
                                     DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);
  SQLLEN buffer_length = dest_data.buflen;

  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  bool is_datetime_range = re2::RE2::FullMatch(src_str, kDatetimeRangeRegex);
  bool is_date_range = re2::RE2::FullMatch(src_str, kDateRangeRegex);

  if (is_datetime_range) {
    NormalizeDatetimeRange(src_str);
  } else if (!is_date_range) {
    auto status = ConvertRangeToTimestampFormat(src_str);
    if (!status.ok()) {
      return status;
    }
  }

  switch (dest_data.type) {
    case SQL_C_CHAR: {
      return StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
    }
    case SQL_C_BINARY: {
      if (!is_date_range) {
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
        LOG(ERROR) << "Failed to convert string to wide string: "
                   << wstr.GetStatusRecord().message;
        return StatusRecord{SQLStates::k_HY000(),
                            "Conversion to SQL_C_WCHAR failed."};
      }
      SQLLEN wchar_capacity = buffer_length / WireWcharSize();
      SQLLEN src_len = static_cast<SQLLEN>(wstr->length());
      SQLLEN required_chars = src_len + 1;
      return WStrToOutputBufferResponse(
          wstr.GetValue(), dest_data.buf, wchar_capacity, src_len,
          required_chars, reinterpret_cast<SQLLEN*>(dest_data.result_len));
    }
    default: {
      LOG(ERROR) << "Unsupported conversion type for range DSValue: "
                 << dest_data.type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
