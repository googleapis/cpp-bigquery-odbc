// Copyright 2025 Google LLC
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

//////////////////////////////////////////////////////////////////
// This file has the utilities to translate from ODBC C data type
//  to SQL Data type. ConvertFromBuffer is supposed to return a
// std::string because QueryParameter::parameter_value is a string
//////////////////////////////////////////////////////////////////

#include "google/cloud/odbc/bq_driver/internal/data_translation_inv.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using odbc_internal::SQLStates;
using odbc_internal::StatusRecord;
using odbc_internal::StatusRecordOr;

// Assuming src_data has c_type of SQL_C_CHAR or SQL_C_WCHAR, this function
// returns a string which should be set to QueryParameter::parameter_value.
StatusRecordOr<std::string> ConvertFromCharBuffer(DataBuffer& src_data,
                                                  SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN src_buflen = src_data.buflen;
  SQLLEN* src_result_len = src_data.result_len;
  SQLLEN result_len = src_buflen;
  if (src_result_len != nullptr) {
    result_len = *src_result_len;
  }

  std::string src_str;
  switch (src_data.type) {
    case SQL_C_CHAR: {
      if (src_buf == nullptr || result_len <= 0) {
        src_str = "";
        break;
      }
      src_str = std::string(static_cast<char*>(src_buf), result_len);
      break;
    }
    case SQL_C_WCHAR: {
      auto* wchar_buf = static_cast<SQLWCHAR*>(src_buf);
      if ((result_len > 0) || (result_len == SQL_NTS)) {
        if (result_len > 0) {
          result_len /= sizeof(wchar_t);
        }
        auto utf8_res = ConvertSQLWCHARToString(wchar_buf, result_len);
        if (!utf8_res) {
          LOG(ERROR) << "ConvertFromCharBuffer::ConvertSQLWCHARToString:: "
                        "UTF-8 conversion failed ";
          return StatusRecord{SQLStates::k_HY000(), "UTF-8 conversion failed"};
        }
        src_str = *utf8_res;
        break;
      }
      LOG(WARNING) << "ConvertFromCharBuffer:: Invalid buffer length: "
                   << result_len;
      return StatusRecord{SQLStates::k_HY000(), "Invalid buffer length"};
    }
    default: {
      LOG(WARNING)
          << "ConvertFromCharBuffer:: Conversion is unsupported for C-type: "
          << src_data.type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  switch (sql_type) {
    case SQL_VARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_CHAR:
    case SQL_WCHAR: {
      return src_str;
    }
    // TODO(b/345194139): Add conversion to complex data types
    default: {
      // Assume that sql_type can possibly be an arithmetic data type
      StatusRecordOr<SQLDOUBLE> double_status = ConvertToDouble(src_str);
      if (!double_status) {
        // If `src_str` cannot be converted to double, it is not an arithmetic
        // value
        LOG(ERROR) << "ConvertFromCharBuffer::ConvertToDouble:: Conversion is "
                      "unsupported";
        return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
      }
      // Try to convert the SQLDOUBLE value to the sql_type's not handled
      // explicitly by the switch-case.
      auto conv_status =
          ConvertFromArithmeticValue<SQLDOUBLE>(*double_status, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromCharBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
  }
  return StatusRecord::Ok();
}

StatusRecordOr<std::string> ParseIntervalToBuffer(SQLSMALLINT type,
                                                  SQLUINTEGER value,
                                                  SQLSMALLINT sign) {
  SQL_INTERVAL_STRUCT interval_struct = {};
  interval_struct.interval_sign = sign;
  switch (type) {
    case SQL_INTERVAL_YEAR: {
      interval_struct.interval_type = SQL_IS_YEAR;
      interval_struct.intval.year_month.year = value;
      break;
    }
    case SQL_INTERVAL_MONTH: {
      interval_struct.interval_type = SQL_IS_MONTH;
      interval_struct.intval.year_month.month = value;
      break;
    }
    case SQL_INTERVAL_DAY: {
      interval_struct.interval_type = SQL_IS_DAY;
      interval_struct.intval.day_second.day = value;
      break;
    }
    case SQL_INTERVAL_HOUR: {
      interval_struct.interval_type = SQL_IS_HOUR;
      interval_struct.intval.day_second.hour = value;
      break;
    }
    case SQL_INTERVAL_MINUTE: {
      interval_struct.interval_type = SQL_IS_MINUTE;
      interval_struct.intval.day_second.minute = value;
      break;
    }
    case SQL_INTERVAL_SECOND: {
      interval_struct.interval_type = SQL_IS_SECOND;
      interval_struct.intval.day_second.second = value;
      break;
    }
    default:
      LOG(WARNING)
          << "ParseIntervalToBuffer:: Invalid single-field interval type: "
          << type;
      return StatusRecord{SQLStates::k_HY000(),
                          "invalid single-field interval type"};
      break;
  }
  return FormatIntervalToString(interval_struct);
}

StatusRecordOr<std::string> ConvertFromNumericBuffer(DataBuffer src_data,
                                                     SQLSMALLINT sql_type) {
  auto numeric_struct = *reinterpret_cast<SQL_NUMERIC_STRUCT*>(src_data.buf);
  std::string src_str = FormatNumericToString(numeric_struct);
  auto double_val = ConvertToDouble(src_str);
  if (!double_val) {
    return double_val.GetStatusRecord();
  }
  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR: {
      if (src_str.length() > src_data.buflen) {
        return StatusRecord{SQLStates::k_22001(),
                            "String data, right truncated"};
      }
      return src_str;
    }
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_DECIMAL:
    case SQL_NUMERIC: {
      return src_str;
    }
    case SQL_REAL:
    case SQL_FLOAT: {
      auto conv_status =
          ConvertFromArithmeticValue<SQLREAL>(*double_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromNumericBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_DOUBLE: {
      auto conv_status =
          ConvertFromArithmeticValue<SQLDOUBLE>(*double_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromNumericBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_BIT: {
      if (*double_val != 0 && *double_val != 1) {
        LOG(ERROR)
            << "ConvertFromNumericBuffer:: Invalid BIT value (must be 0 or 1): "
            << *double_val;
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      }
      return std::to_string((*double_val == 0) ? 0 : 1);
    }
    case SQL_TINYINT: {
      auto conv_status =
          ConvertFromArithmeticValue<SQLCHAR>(*double_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromNumericBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_SMALLINT: {
      auto conv_status =
          ConvertFromArithmeticValue<SQLSMALLINT>(*double_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromNumericBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_INTEGER: {
      auto conv_status =
          ConvertFromArithmeticValue<SQLINTEGER>(*double_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromNumericBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_BIGINT: {
      auto conv_status =
          ConvertFromArithmeticValue<SQLBIGINT>(*double_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromNumericBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_INTERVAL_YEAR:
    case SQL_INTERVAL_MONTH:
    case SQL_INTERVAL_DAY:
    case SQL_INTERVAL_HOUR:
    case SQL_INTERVAL_MINUTE:
    case SQL_INTERVAL_SECOND: {
      return ParseIntervalToBuffer(
          sql_type, static_cast<SQLUINTEGER>(*double_val),
          static_cast<SQLSMALLINT>(numeric_struct.sign));
    }
    default:
      LOG(WARNING) << "ConvertFromNumericBuffer:: Conversion is unsupported "
                      "for SQL type: "
                   << sql_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromBinaryBuffer(DataBuffer& src_data,
                                                    SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN* src_result_len = src_data.result_len;

  auto* src_val = static_cast<uint8_t*>(src_buf);
  if (!src_val || !src_result_len || *src_result_len < 0) {
    LOG(ERROR)
        << "ConvertFromBinaryBuffer:: Invalid source buffer or result length";
    return StatusRecord{SQLStates::k_HY000(), "Invalid binary data"};
  }

  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY: {
      return Base64Encode(src_val, *src_result_len);
    }
    default:
      LOG(WARNING) << "ConvertFromBinaryBuffer:: Conversion is unsupported for "
                      "SQL type: "
                   << sql_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromBitBuffer(DataBuffer src_data,
                                                 SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLCHAR src_val = *reinterpret_cast<SQLCHAR*>(src_buf);

  if (src_val != 0 && src_val != 1) {
    LOG(ERROR) << "ConvertFromBitBuffer:: Invalid BIT value (must be 0 or 1): ";
    return StatusRecord{SQLStates::k_22003(),
                        "Invalid BIT value (must be 0 or 1)"};
  }
  switch (sql_type) {
    case SQL_CHAR:
    case SQL_BIT:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR: {
      return std::string((src_val == 0) ? "false" : "true");
    }
    case SQL_INTEGER: {
      return std::to_string(static_cast<SQLINTEGER>(src_val));
    }
    case SQL_SMALLINT: {
      return std::to_string(static_cast<SQLSMALLINT>(src_val));
    }
    case SQL_TINYINT: {
      return std::to_string(static_cast<SQLCHAR>(src_val));
    }
    case SQL_FLOAT:
    case SQL_REAL: {
      return std::to_string(static_cast<SQLREAL>(src_val));
    }
    case SQL_DOUBLE: {
      return std::to_string(static_cast<SQLDOUBLE>(src_val));
    }
    case SQL_BIGINT: {
      return std::to_string(static_cast<SQLBIGINT>(src_val));
    }
    default:
      LOG(WARNING)
          << "ConvertFromBitBuffer:: Conversion is unsupported for SQL type: "
          << sql_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromDateBuffer(DataBuffer src_data,
                                                  SQLSMALLINT sql_type) {
  SQL_DATE_STRUCT src_val = *reinterpret_cast<SQL_DATE_STRUCT*>(src_data.buf);
  if (src_val.year < 0 || src_val.month < 1 || src_val.month > 12 ||
      src_val.day < 1 || src_val.day > 31) {
    StatusRecord{SQLStates::k_HY000(), "Invalid Date data"};
  }
  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_TYPE_DATE: {
      return FormatDateToString(src_val);
      break;
    }
    case SQL_TYPE_TIMESTAMP: {
      SQL_TIMESTAMP_STRUCT timestamp_struct = {};
      timestamp_struct.day = src_val.day;
      timestamp_struct.month = src_val.month;
      timestamp_struct.year = src_val.year;

      // Use 0 for time fields
      timestamp_struct.hour = 0;
      timestamp_struct.minute = 0;
      timestamp_struct.second = 0;
      timestamp_struct.fraction = 0;
      return FormatTimestampToString(timestamp_struct);
    }
    default:
      LOG(WARNING)
          << "ConvertFromDateBuffer:: Conversion is unsupported for SQL type: "
          << sql_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromTimeBuffer(DataBuffer& src_data,
                                                  SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN* src_result_len = src_data.result_len;

  SQL_TIME_STRUCT time_struct = *reinterpret_cast<SQL_TIME_STRUCT*>(src_buf);
  if (time_struct.hour < 0 || time_struct.hour > 23 || time_struct.minute < 0 ||
      time_struct.minute > 59 || time_struct.second < 0 ||
      time_struct.second > 59) {
    LOG(ERROR) << "ConvertFromTimeBuffer:: Invalid time data";
    return StatusRecord{SQLStates::k_HY000(), "Invalid time data"};
  }

  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_TYPE_TIME: {
      return FormatTimetoString(time_struct);
    }
    default:
      LOG(WARNING)
          << "ConvertFromTimeBuffer:: Conversion is unsupported for SQL type: "
          << sql_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromIntervalBuffer(DataBuffer src_data,
                                                      SQLSMALLINT sql_type) {
  SQL_INTERVAL_STRUCT interval_struct =
      *reinterpret_cast<SQL_INTERVAL_STRUCT*>(src_data.buf);

  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR: {
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_YEAR: {
      if (interval_struct.interval_type != SQL_IS_YEAR) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Year Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Year Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_MONTH: {
      if (interval_struct.interval_type != SQL_IS_MONTH) {
        LOG(ERROR)
            << "ConvertFromIntervalBuffer:: Invalid Month Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Month Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_YEAR_TO_MONTH: {
      if (interval_struct.interval_type != SQL_IS_YEAR_TO_MONTH) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Year To Month "
                      "Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Year To Month Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_DAY: {
      if (interval_struct.interval_type != SQL_IS_DAY) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Day Interval value";
        return StatusRecord{SQLStates::k_HY000(), "Invalid Day Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_HOUR: {
      if (interval_struct.interval_type != SQL_IS_HOUR) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Hour Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Hour Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_MINUTE: {
      if (interval_struct.interval_type != SQL_IS_MINUTE) {
        LOG(ERROR)
            << "ConvertFromIntervalBuffer:: Invalid Minute Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Minute Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_SECOND: {
      if (interval_struct.interval_type != SQL_IS_SECOND) {
        LOG(ERROR)
            << "ConvertFromIntervalBuffer:: Invalid Second Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Second Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_DAY_TO_HOUR: {
      if (interval_struct.interval_type != SQL_IS_DAY_TO_HOUR) {
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Day To Hour Interval value"};
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Day To Hour Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_DAY_TO_MINUTE: {
      if (interval_struct.interval_type != SQL_IS_DAY_TO_MINUTE) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Day To Minute "
                      "Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Day To Minute Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_DAY_TO_SECOND: {
      if (interval_struct.interval_type != SQL_IS_DAY_TO_SECOND) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Day To Second "
                      "Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Day To Second Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_HOUR_TO_MINUTE: {
      if (interval_struct.interval_type != SQL_IS_HOUR_TO_MINUTE) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Hour To Minute "
                      "Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Hour To Minute Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_HOUR_TO_SECOND: {
      if (interval_struct.interval_type != SQL_IS_HOUR_TO_SECOND) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Hour To Second "
                      "Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Hour To Second Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_INTERVAL_MINUTE_TO_SECOND: {
      if (interval_struct.interval_type != SQL_IS_MINUTE_TO_SECOND) {
        LOG(ERROR) << "ConvertFromIntervalBuffer:: Invalid Minute To Second "
                      "Interval value";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid Minute To Second Interval value"};
      }
      return FormatIntervalToString(interval_struct);
    }
    case SQL_DECIMAL:
    case SQL_NUMERIC:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    case SQL_INTEGER:
    case SQL_BIGINT: {
      SQLUINTEGER value = 0;
      GetSinglePrecisionInterval(interval_struct, value);
      return std::to_string(value);
    }
    default:
      LOG(WARNING) << "ConvertFromIntervalBuffer:: Conversion is unsupported "
                      "for SQL type: "
                   << sql_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromTimestampBuffer(DataBuffer& src_data,
                                                       SQLSMALLINT sql_type) {
  auto* src_val = static_cast<TIMESTAMP_STRUCT*>(src_data.buf);
  if (!src_val || !src_data.result_len || *src_data.result_len < 0) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Data value is not a valid timestamp"};
  }

  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_TIMESTAMP:
    case SQL_TYPE_TIMESTAMP: {
      return FormatTimestampToString(*src_val);
    }
    case SQL_TYPE_DATE: {
      SQL_DATE_STRUCT date = {src_val->year, src_val->month, src_val->day};
      return FormatDateToString(date);
    }
    case SQL_TYPE_TIME: {
      SQL_TIME_STRUCT time = {src_val->hour, src_val->minute, src_val->second};
      return FormatTimetoString(time);
    }
    default:
      LOG(WARNING) << "ConvertFromTimestampBuffer:: Conversion is unsupported "
                      "for SQL type: "
                   << sql_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromBuffer(DataBuffer& src_data,
                                              SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN* res_len = src_data.result_len;

  switch (src_data.type) {
    case SQL_C_WCHAR:
    case SQL_C_CHAR: {
      auto conv_status = ConvertFromCharBuffer(src_data, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromCharBuffer:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_FLOAT: {
      auto src_val = *reinterpret_cast<SQLREAL*>(src_buf);
      auto conv_status = ConvertFromArithmeticValue<SQLREAL>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_DOUBLE: {
      auto src_val = *reinterpret_cast<SQLDOUBLE*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLDOUBLE>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_SBIGINT: {
      auto src_val = *reinterpret_cast<SQLBIGINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLBIGINT>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_UBIGINT: {
      auto src_val = *reinterpret_cast<SQLUBIGINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLUBIGINT>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_SHORT:
    case SQL_C_SSHORT: {
      auto src_val = *reinterpret_cast<SQLSMALLINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLSMALLINT>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_USHORT: {
      auto src_val = *reinterpret_cast<SQLUSMALLINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLUSMALLINT>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_LONG:
    case SQL_C_SLONG: {
      auto src_val = *reinterpret_cast<SQLINTEGER*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLINTEGER>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_ULONG: {
      auto src_val = *reinterpret_cast<SQLUINTEGER*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLUINTEGER>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_BIT: {
      auto conv_status = ConvertFromBitBuffer(src_data, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromBitBuffer:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_BINARY: {
      auto conv_status = ConvertFromBinaryBuffer(src_data, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromBinaryBuffer:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_STINYINT: {
      auto src_val = *reinterpret_cast<SQLSCHAR*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLSCHAR>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_TINYINT:
    case SQL_C_UTINYINT: {
      auto src_val = *reinterpret_cast<SQLCHAR*>(src_buf);
      auto conv_status = ConvertFromArithmeticValue<SQLCHAR>(src_val, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromArithmeticValue:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_NUMERIC: {
      auto conv_status = ConvertFromNumericBuffer(src_data, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromNumericBuffer:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_TYPE_DATE: {
      auto conv_status = ConvertFromDateBuffer(src_data, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromDateBuffer:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_TYPE_TIME: {
      auto conv_status = ConvertFromTimeBuffer(src_data, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromTimeBuffer:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_INTERVAL_YEAR:
    case SQL_C_INTERVAL_MONTH:
    case SQL_C_INTERVAL_YEAR_TO_MONTH:
    case SQL_C_INTERVAL_DAY:
    case SQL_C_INTERVAL_HOUR:
    case SQL_C_INTERVAL_MINUTE:
    case SQL_C_INTERVAL_SECOND:
    case SQL_C_INTERVAL_DAY_TO_HOUR:
    case SQL_C_INTERVAL_DAY_TO_MINUTE:
    case SQL_C_INTERVAL_DAY_TO_SECOND:
    case SQL_C_INTERVAL_HOUR_TO_MINUTE:
    case SQL_C_INTERVAL_HOUR_TO_SECOND:
    case SQL_C_INTERVAL_MINUTE_TO_SECOND: {
      auto conv_status = ConvertFromIntervalBuffer(src_data, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_TYPE_TIMESTAMP: {
      auto conv_status = ConvertFromTimestampBuffer(src_data, sql_type);
      if (!conv_status) {
        LOG(ERROR) << "ConvertFromBuffer::ConvertFromTimestampBuffer:: "
                   << conv_status.GetStatusRecord().message;
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    default: {
      LOG(WARNING)
          << "ConvertFromBuffer:: Conversion is unsupported for C-type: "
          << src_data.type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
