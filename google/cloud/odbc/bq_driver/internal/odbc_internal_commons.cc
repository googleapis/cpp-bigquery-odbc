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

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetReference;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobCreationMode;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterType;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterValue;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
using ::google::cloud::bigquery_v2_minimal_internal::TableReference;
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using chrono_ms = std::chrono::milliseconds;
using json = nlohmann::json;

// Constants for Unix timestamp calculations
int const kSecondsPerDay = 86400;
int const kSecondsPerYear = 31536000;
int const kSecondsPerLeapYear = 31622400;  // 366 days
int const kSecondsPerHour = 3600;
int const kSecondsPerMinute = 60;
constexpr int kMaxNumericPrecision = 38;
constexpr int kMaxNumericScale = 9;

// converting the given string to Numeric number
// getting scale ,precision, sign and the value from sting parameter
odbc_internal::StatusRecord GetNumericDetailsFromStr(
    std::string const& src_dsval, SQL_NUMERIC_STRUCT& numst) {
  SQLCHAR sign = 1;
  SQLCHAR precision = 0;
  SQLSCHAR scale;
  std::string num_str;
  int integral_count = 0;
  int fractional_count = 0;
  bool fractional_truncated = false;
  auto status_record = odbc_internal::StatusRecord::Ok();
  // Handle leading whitespace
  size_t i = 0;
  while (isspace(src_dsval[i])) {
    i++;
  }
  // Check for sign
  if (src_dsval[i] == '-') {
    sign = 0;
    i++;
  }

  // Extract digits before decimal point
  while (isdigit(src_dsval[i])) {
    char ch = src_dsval[i];
    if (integral_count != 0 || ch != '0') {
      num_str += ch;
    }
    integral_count++;
    i++;
  }

  if (integral_count == 1 && num_str.empty()) {
    num_str = "0";
  }
  // Find decimal point
  if (src_dsval[i] == '.') {
    i++;
  }

  // Extract digits after decimal point
  while (isdigit(src_dsval[i])) {
    if (fractional_count < kMaxNumericScale) {
      num_str += src_dsval[i];
      fractional_count++;
    } else {
      fractional_truncated = true;
    }
    i++;
  }

  if (integral_count == 1 && num_str[0] == '0' &&
      num_str.find_first_not_of('0', 1) == std::string::npos) {
    num_str = "0";
    sign = 1;
    fractional_count = 0;
    fractional_truncated = false;
  }

  if (integral_count + fractional_count > kMaxNumericPrecision) {
    LOG(ERROR) << "GetNumericDetailsFromStr::Numeric value out of range.";
    return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
  }
  // For NUmeric data type we have limited length defined by driver itself
  // driver forces this limit by SQL_NUMERIC_STRUCT which has value of length
  // SQL_MAX_NUMERIC_LEN i.e 16
  if (integral_count >= SQL_MAX_NUMERIC_LEN) {
    scale = 0;
    precision = SQL_MAX_NUMERIC_LEN;
  } else {
    int maxlen = SQL_MAX_NUMERIC_LEN;
    int limit_scale = maxlen - integral_count;
    precision = integral_count + fractional_count;
    scale = fractional_count;
    if (scale >= limit_scale) scale = limit_scale;
    if (precision >= SQL_MAX_NUMERIC_LEN) precision = SQL_MAX_NUMERIC_LEN;
  }

  if (fractional_truncated) {
    LOG(WARNING) << "GetNumericDetailsFromStr::Fractional truncation (loss of "
                    "precision).";
    status_record = StatusRecord{SQLStates::k_01S07(),
                                 "Fractional truncation (loss of precision)"};
  }

  numst.scale = scale;
  numst.precision = precision;
  numst.sign = sign;
  uint64_t dd = std::stoull(num_str);
  memset(numst.val, 0, SQL_MAX_NUMERIC_LEN);
  memcpy(numst.val, &dd, sizeof(dd));
  return status_record;
}

bool IsLeapYear(int year) {
  return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

int DaysInMonth(int year, int month) {
  static int const kDaysInMonth[] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
  if (month == 2 && IsLeapYear(year)) {
    return 29;
  }
  return kDaysInMonth[month - 1];
}

StatusRecord ConvertUnixTimestampToTimestampStruct(
    double unix_timestamp, SQL_TIMESTAMP_STRUCT& timestamp_struct) {
  // Check for invalid timestamp (e.g., negative or non-finite)
  if (unix_timestamp < 0 || !std::isfinite(unix_timestamp)) {
    LOG(ERROR)
        << "ConvertUnixTimestampToTimestampStruct::Invalid Unix timestamp: "
        << unix_timestamp;
    return StatusRecord{SQLStates::k_01004(), "Invalid Unix timestamp"};
  }

  // Calculate whole seconds and fractional part
  auto total_seconds = static_cast<time_t>(unix_timestamp);
  int fractional_part =
      round((unix_timestamp - total_seconds) * 1000000);  // Microseconds

  // Calculate the date and time components
  int year = 1970;
  while (total_seconds >=
         (IsLeapYear(year) ? kSecondsPerLeapYear : kSecondsPerYear)) {
    total_seconds -= (IsLeapYear(year) ? kSecondsPerLeapYear : kSecondsPerYear);
    ++year;
  }

  int month = 1;
  while (total_seconds >= (DaysInMonth(year, month) * kSecondsPerDay)) {
    total_seconds -= (DaysInMonth(year, month) * kSecondsPerDay);
    ++month;
  }

  int day = total_seconds / kSecondsPerDay + 1;
  total_seconds %= kSecondsPerDay;

  int hour = total_seconds / kSecondsPerHour;
  total_seconds %= kSecondsPerHour;

  int minute = total_seconds / kSecondsPerMinute;
  total_seconds %= kSecondsPerMinute;

  int second = total_seconds;

  // Fill SQL_TIMESTAMP_STRUCT
  timestamp_struct.year = static_cast<int16_t>(year);
  timestamp_struct.month = static_cast<unsigned char>(month);
  timestamp_struct.day = static_cast<unsigned char>(day);
  timestamp_struct.hour = static_cast<unsigned char>(hour);
  timestamp_struct.minute = static_cast<unsigned char>(minute);
  timestamp_struct.second = static_cast<unsigned char>(second);
  timestamp_struct.fraction = fractional_part;

  return StatusRecord::Ok();
}

StatusRecordOr<SQL_DATE_STRUCT> ConvertStringToDateStruct(
    std::string const& date_str) {
  if (date_str.empty() || date_str.size() < SQL_DATE_LEN) {
    LOG(ERROR) << "ConvertStringToDateStruct::Invalid date string format: "
               << date_str;
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid date string format: the string is either empty or too short."};
  }
  int year = std::stoi(date_str.substr(0, 4));
  int month = std::stoi(date_str.substr(5, 2));
  int day = std::stoi(date_str.substr(8, 2));

  SQL_DATE_STRUCT date_struct;
  date_struct.year = static_cast<SQLSMALLINT>(year);
  date_struct.month = static_cast<SQLUSMALLINT>(month);
  date_struct.day = static_cast<SQLUSMALLINT>(day);
  return date_struct;
}

StatusRecord ConvertStringToIntervalStruct(
    std::string const& interval_str, SQL_INTERVAL_STRUCT& interval_struct) {
  if (interval_str.empty()) {
    LOG(ERROR)
        << "ConvertStringToIntervalStruct::Interval string can't be empty.";
    return StatusRecord{SQLStates::k_HY000(),
                        "Interval string can't be empty."};
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int fraction = 0;
  int matched_items =
      std::sscanf(interval_str.c_str(), "%d-%d %d %d:%d:%d.%d", &year, &month,
                  &day, &hour, &minute, &second, &fraction);
  if (matched_items == 6) {
    fraction = 0;
  } else if (matched_items != 7) {
    LOG(ERROR)
        << "ConvertStringToIntervalStruct::Invalid interval string format: "
        << interval_str;
    return StatusRecord{SQLStates::k_HY000(), "Invalid interval string format"};
  }

  interval_struct.interval_sign =
      (year < 0 || month < 0 || day < 0 || hour < 0 || minute < 0 || second < 0)
          ? -1
          : 1;

  if (year != 0 || month != 0) {
    if (day == 0 && hour == 0 && minute == 0 && second == 0) {
      if (year != 0 && month == 0) {
        interval_struct.interval_type = SQL_IS_YEAR;
        interval_struct.intval.year_month.year = static_cast<SQLUINTEGER>(year);
      } else if (year == 0 && month != 0) {
        interval_struct.interval_type = SQL_IS_MONTH;
        interval_struct.intval.year_month.month =
            static_cast<SQLUINTEGER>(month);
      } else if (year != 0 && month != 0) {
        interval_struct.interval_type = SQL_IS_YEAR_TO_MONTH;
        interval_struct.intval.year_month.year = static_cast<SQLUINTEGER>(year);
        interval_struct.intval.year_month.month =
            static_cast<SQLUINTEGER>(month);
      } else {
        LOG(ERROR)
            << "ConvertStringToIntervalStruct::Invalid year-month interval.";
        return StatusRecord{SQLStates::k_HY000(),
                            "Invalid year-month interval."};
      }
    } else {
      LOG(ERROR) << "ConvertStringToIntervalStruct::Year-month interval must "
                    "not include day/time.";
      return StatusRecord{SQLStates::k_HY000(),
                          "Year-month interval must not include day/time."};
    }
  } else if (day != 0 || hour != 0 || minute != 0 || second != 0) {
    if (hour == 0 && minute == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_DAY;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
    } else if (day == 0 && minute == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_HOUR;
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
    } else if (day == 0 && hour == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_MINUTE;
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
    } else if (day == 0 && hour == 0 && minute == 0) {
      interval_struct.interval_type = SQL_IS_SECOND;
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    } else if (day != 0 && hour != 0 && minute == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_DAY_TO_HOUR;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
    } else if (day != 0 && minute != 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_DAY_TO_MINUTE;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
    } else if (day == 0 && hour != 0 && minute != 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_HOUR_TO_MINUTE;
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
    } else if (day == 0 && hour != 0 && second != 0) {
      interval_struct.interval_type = SQL_IS_HOUR_TO_SECOND;
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    } else if (day == 0 && hour == 0 && minute != 0 && second != 0) {
      interval_struct.interval_type = SQL_IS_MINUTE_TO_SECOND;
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    } else {
      interval_struct.interval_type = SQL_IS_DAY_TO_SECOND;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    }
  }
  return StatusRecord::Ok();
}

StatusRecordOr<std::string> FormatDateToString(SQL_DATE_STRUCT date) {
  std::ostringstream oss;
  oss << std::setfill('0');
  oss << std::setw(4) << date.year << "-" << std::setw(2) << date.month << "-"
      << std::setw(2) << date.day;
  return oss.str();
}

std::string FormatIntervalToString(const SQL_INTERVAL_STRUCT interval) {
  char buffer[80];
  switch (interval.interval_type) {
    case SQL_IS_YEAR:
      snprintf(buffer, sizeof(buffer), "%d-0 0 0:0:0",
               interval.intval.year_month.year);
      break;
    case SQL_IS_MONTH:
      snprintf(buffer, sizeof(buffer), "0-%d 0 0:0:0",
               interval.intval.year_month.month);
      break;
    case SQL_IS_YEAR_TO_MONTH:
      snprintf(buffer, sizeof(buffer), "%d-%d 0 0:0:0",
               interval.intval.year_month.year,
               interval.intval.year_month.month);
      break;
    case SQL_IS_DAY:
      snprintf(buffer, sizeof(buffer), "0-0 %d 0:0:0",
               interval.intval.day_second.day);
      break;
    case SQL_IS_HOUR:
      snprintf(buffer, sizeof(buffer), "0-0 0 %d:0:0",
               interval.intval.day_second.hour);
      break;
    case SQL_IS_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:0",
               interval.intval.day_second.minute);
      break;
    case SQL_IS_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:0:%d.%09d",
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:0:%d",
                 interval.intval.day_second.second);
      }
      break;
    case SQL_IS_DAY_TO_HOUR:
      snprintf(buffer, sizeof(buffer), "0-0 %d %d:0:0",
               interval.intval.day_second.day, interval.intval.day_second.hour);
      break;
    case SQL_IS_DAY_TO_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:0",
               interval.intval.day_second.day, interval.intval.day_second.hour,
               interval.intval.day_second.minute);
      break;
    case SQL_IS_DAY_TO_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:%d.%09d",
                 interval.intval.day_second.day,
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:%d",
                 interval.intval.day_second.day,
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second);
      }
      break;
    case SQL_IS_HOUR_TO_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:0",
               interval.intval.day_second.hour,
               interval.intval.day_second.minute);
      break;
    case SQL_IS_HOUR_TO_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:%d.%09d",
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:%d",
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second);
      }
      break;
    case SQL_IS_MINUTE_TO_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:%d.%09d",
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:%d",
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second);
      }
      break;
    default:
      snprintf(buffer, sizeof(buffer), "Unknown interval type");
      break;
  }
  return std::string(buffer);
}

std::string FormatNumericToString(SQL_NUMERIC_STRUCT numeric) {
  uint64_t value = 0;

  for (int i = numeric.precision - 1; i >= 0; --i) {
    value = (value << 8) + numeric.val[i];
  }
  std::string result = std::to_string(value);
  if (numeric.scale > 0) {
    if (result.length() <= numeric.scale) {
      result =
          "0." + std::string(numeric.scale - result.length(), '0') + result;
    } else {
      result.insert(result.length() - numeric.scale, ".");
    }
  }
  if (numeric.sign == 0) {
    result = "-" + result;
  }
  return result;
}

StatusRecordOr<SQL_TIMESTAMP_STRUCT> ConvertStringToTimestampStruct(
    std::string const& date_str) {
  std::string cleaned_date_str = date_str;
  std::replace(cleaned_date_str.begin(), cleaned_date_str.end(), 'T', ' ');

  SQL_TIMESTAMP_STRUCT date_struct = {};
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  char fraction_str[10] = "0";

  int matched =
      std::sscanf(cleaned_date_str.c_str(), "%4d-%2d-%2d %2d:%2d:%2d.%6s",
                  &year, &month, &day, &hour, &minute, &second, fraction_str);

  if (matched < 6) {
    LOG(ERROR) << "ConvertStringToTimestampStruct::sscanf:: String not "
                  "correctly converted to timestamp. Input: "
               << cleaned_date_str;
    return StatusRecord{SQLStates::k_HY000(),
                        "String not correctly converted to timestamp"};
  }

  SQLUINTEGER fraction = 0;
  if (matched == 7) {
    int len = 0;
    for (char ch : std::string(fraction_str)) {
      if (!std::isdigit(ch)) {
        LOG(ERROR) << "ConvertStringToTimestampStruct:: Fractional part is not "
                      "a valid number. Input: "
                   << cleaned_date_str;
        return StatusRecord{SQLStates::k_HY000(),
                            "Fractional part is not a valid number"};
      }
      fraction = fraction * 10 + (ch - '0');
      ++len;
    }
    for (; len < 6; ++len) {
      fraction *= 10;
    }
  }

  date_struct.year = static_cast<SQLSMALLINT>(year);
  date_struct.month = static_cast<SQLUSMALLINT>(month);
  date_struct.day = static_cast<SQLUSMALLINT>(day);
  date_struct.hour = static_cast<SQLUSMALLINT>(hour);
  date_struct.minute = static_cast<SQLUSMALLINT>(minute);
  date_struct.second = static_cast<SQLUSMALLINT>(second);
  date_struct.fraction = fraction;

  return date_struct;
}

StatusRecordOr<ResultSet> ProcessResultSetRows(
    TableSchema const& schema, std::vector<RowData> const& rows) {
  ResultSet result_set;
  // Populate the schema for each row. The row schema
  // indicates how they should converted back for the application buffers in
  // SQLFetch.
  for (int i = 0; i < schema.fields.size(); i++) {
    TableFieldSchema table_field_schema = schema.fields[i];
    ColumnSchema col_schema;
    col_schema.col_index = i;
    StatusRecordOr<BQDataType> type_status_record =
        ConvertDSType(table_field_schema.type);
    if (!type_status_record.Ok()) {
      LOG(ERROR) << "ProcessResultSetRows::ConvertDSType:: "
                 << type_status_record.GetStatusRecord().message;
      return type_status_record.GetStatusRecord();
    }

    col_schema.col_type = *type_status_record;
    col_schema.is_mode_repeated = (table_field_schema.mode == "REPEATED");
    result_set.row_schema.emplace_back(col_schema);
  }
  // Populate the data for each row.
  for (auto const& row : rows) {
    DSRow rs_row;
    int i = 0;
    for (auto const& col : row.columns) {
      BQDataType col_type;
      if (result_set.row_schema[i].is_mode_repeated)
        col_type = kArray;
      else
        col_type = result_set.row_schema[i].col_type;
      std::string data = col.value;
      if (col.is_null) {
        rs_row.emplace_back(kNullValue);
      } else if (!data.empty()) {
        DSValue row_val;
        switch (col_type) {
          case BQDataType::kNumeric:
          case BQDataType::kBigNumeric: {
            NumericToDSValue(data, row_val);
            break;
          }
          case BQDataType::kString: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kInt64: {
            SQLBIGINT l_data;
            try {
              l_data = std::stoll(data);
            } catch (std::exception const& ex) {
              return StatusRecord{SQLStates::k_HY000(),
                                  "data cannot be parsed as long long"};
            }
            ArithmeticToDSValue<SQLBIGINT>(l_data, row_val);
            break;
          }
          case BQDataType::kFloat64: {
            SQLDOUBLE d_data;
            try {
              d_data = std::stod(data);
            } catch (std::exception const& ex) {
              return StatusRecord{SQLStates::k_HY000(),
                                  "data cannot be parsed as double"};
            }
            ArithmeticToDSValue<SQLDOUBLE>(d_data, row_val);
            break;
          }
          case BQDataType::kJson:
          case BQDataType::kStruct: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kArray: {
            BQDataType array_type = result_set.row_schema[i].col_type;
            ArrayJsonToDSValue(data, row_val, array_type);
            break;
          }
          case BQDataType::kDate: {
            auto date_struct = ConvertStringToDateStruct(data);
            if (!date_struct.Ok()) {
              return date_struct.GetStatusRecord();
            }
            DateToDSValue(date_struct.GetValue(), row_val);
            break;
          }
          case BQDataType::kTime: {
            SQL_TIME_STRUCT t_data = ConvertToTimeStruct(data);
            TimeToDSValue(t_data, row_val);
            break;
          }
          case BQDataType::kTimeStamp: {
            double unix_timestamp;
            try {
              unix_timestamp = std::stod(data);
            } catch (std::exception const& ex) {
              return StatusRecord{SQLStates::k_HY000(),
                                  "data cannot be parsed as double"};
            }
            SQL_TIMESTAMP_STRUCT time_struct;
            ConvertUnixTimestampToTimestampStruct(unix_timestamp, time_struct);
            TimestampToDSValue(time_struct, row_val);
            break;
          }
          case BQDataType::kInterval: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kDatetime: {
            auto time_struct = ConvertStringToTimestampStruct(data);
            if (!time_struct.Ok()) {
              return time_struct.GetStatusRecord();
            }
            TimestampToDSValue(time_struct.GetValue(), row_val);
            break;
          }
          case BQDataType::kBytes: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kBool: {
            bool bool_val = false;
            std::transform(data.begin(), data.end(), data.begin(), ::tolower);
            if (data == "1" || data == "true" || data == "yes") {
              bool_val = true;
            } else if (data == "0" || data == "false" || data == "no") {
              bool_val = false;
            }
            BooleanToDSValue(bool_val, row_val);
            break;
          }
          case BQDataType::kGeography:
          case BQDataType::kRange: {
            StringToDSValue(data, row_val);
            break;
          }
          default: {
            return StatusRecord{SQLStates::k_HY000(),
                                "Invalid or unsupported col BQ data type"};
          }
        }
        rs_row.emplace_back(row_val);
      } else {
        DSValue empty_value;
        StringToDSValue("", empty_value);
        rs_row.emplace_back(empty_value);
      }
      i++;
    }
    result_set.rows.emplace_back(rs_row);
  }
  return result_set;
}

StatusRecordOr<ResultSet> ProcessPostQueryResults(
    PostQueryResults const& post_query_results) {
  if (!post_query_results.job_complete) {
    // If this method is being called then the assumption is PostQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    LOG(ERROR) << "ProcessPostQueryResults:: Unexpected value for "
                  "job_complete: expecting true.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(post_query_results.schema,
                              post_query_results.rows);
}

StatusRecordOr<ResultSet> ProcessGetQueryResults(
    GetQueryResults const& get_query_results) {
  if (!get_query_results.job_complete) {
    // If this method is being called then the assumption is GetQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    LOG(ERROR) << "ProcessGetQueryResults:: Unexpected value for job_complete: "
                  "expecting true.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(get_query_results.schema, get_query_results.rows);
}

StatusRecordOr<ResultSet> ProcessQueryResults(DSResults const& query_results) {
  // If the variant holds `ResultSet`(case of HT API), return it directly
  if (absl::holds_alternative<ResultSet>(query_results.data_source_results)) {
    return absl::get<ResultSet>(query_results.data_source_results);
  }
  if (absl::holds_alternative<PostQueryResults>(
          query_results.data_source_results)) {
    return ProcessPostQueryResults(
        absl::get<PostQueryResults>(query_results.data_source_results));
  }
  if (absl::holds_alternative<GetQueryResults>(
          query_results.data_source_results)) {
    return ProcessGetQueryResults(
        absl::get<GetQueryResults>(query_results.data_source_results));
  }
  LOG(ERROR) << "ProcessPostQueryResults:: Unexpected value for job_complete: "
                "expecting true.";
  return StatusRecord{SQLStates::k_HY000(), "Invalid query results object"};
}

StatusRecordOr<std::vector<RowData>> GetRowsResults(
    DSResults const& query_results) {
  if (absl::holds_alternative<PostQueryResults>(
          query_results.data_source_results)) {
    auto results =
        absl::get<PostQueryResults>(query_results.data_source_results);
    if (!results.job_complete) {
      LOG(ERROR) << "GetRowsResults:: Unexpected value for job_complete in "
                    "PostQueryResults: expecting true.";
      return StatusRecord{
          SQLStates::k_HY000(),
          "Internal Error: Unexpected value for job_complete: expecting true"};
    }
    return results.rows;
  }
  if (absl::holds_alternative<GetQueryResults>(
          query_results.data_source_results)) {
    auto results =
        absl::get<GetQueryResults>(query_results.data_source_results);
    if (!results.job_complete) {
      LOG(ERROR) << "GetRowsResults:: Unexpected value for job_complete in "
                    "GetQueryResults: expecting true.";
      return StatusRecord{
          SQLStates::k_HY000(),
          "Internal Error: Unexpected value for job_complete: expecting true"};
    }
    return results.rows;
  }
  LOG(ERROR) << "GetRowsResults:: Invalid query results object type.";
  return StatusRecord{SQLStates::k_HY000(), "Invalid query results object"};
}

StatusRecordOr<Job> CancelBQJob(ConnectionHandle& conn_handle,
                                std::string const& job_id,
                                std::string const& location) {
  // validate we have a job.
  if (job_id.empty()) {
    LOG(ERROR) << "CancelBQJob:: Invalid or empty job id.";
    return StatusRecord{SQLStates::k_HY000(), "Invalid or empty job id"};
  }
  // Validate the  connection handle.
  if (!conn_handle.IsConnected()) {
    LOG(ERROR) << "CancelBQJob:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  // Validate we have a bq client.
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR) << "CancelBQJob:: Invalid or null BQ Client within the "
                  "connection handle.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }
  // validate we have a project_id.
  std::string project_id = conn_handle.GetDsn().catalog;
  if (project_id.empty()) {
    LOG(ERROR)
        << "CancelBQJob:: Invalid or empty catalog in connection handle.";
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid or empty catalog in connection handle"};
  }

  Options options;
  return bq_client->CancelJob(project_id, job_id, location, options);
}

StatusRecordOr<PostQueryResults> PostQueryWithoutResults(
    ConnectionHandle& conn_handle, PostQueryRequest const& post_query_request) {
  // Validate the  connection handle.
  if (!conn_handle.IsConnected()) {
    LOG(ERROR)
        << "PostQueryWithoutResults:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR)
        << "PostQueryWithoutResults:: Invalid or null BQ Client within the "
           "connection handle.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }
  // For now , we use default options.
  // We can set timeout here as needed later.
  Options options;
  auto pq_status = bq_client->PostQuery(post_query_request, options);
  if (!pq_status) {
    LOG(ERROR) << "PostQueryWithoutResults::PostQuery:: "
               << pq_status.GetStatusRecord().message;
    return pq_status.GetStatusRecord();
  }
  if (!conn_handle.IsSessionStarted() &&
      !pq_status->session_info.session_id.empty()) {
    conn_handle.SetSessionId(pq_status->session_info.session_id);
  }
  return pq_status;
}

odbc_internal::StatusRecordOr<TableSchema> BuildTableSchemaFromRowSchema(
    RowSchema& row_schema,
    std::map<std::string, ColumnSchema> const& metadata_schema) {
  if (row_schema.empty()) {
    LOG(ERROR) << "BuildTableSchemaFromRowSchema:: Row schema is empty.";
    return StatusRecord{SQLStates::k_HY000(),
                        "row schema should not be less than 0"};
  }

  std::unordered_map<int, std::string> index_to_name_map;
  for (auto const& [col_name, col_schema] : metadata_schema) {
    index_to_name_map[col_schema.col_index] = col_name;
  }

  // we need to sort row_schema by col_index in ascending order.
  std::sort(row_schema.begin(), row_schema.end(),
            [](ColumnSchema const& a, ColumnSchema const& b) {
              return a.col_index < b.col_index;
            });

  TableSchema schema;
  for (auto& row : row_schema) {
    TableFieldSchema field;

    auto it = index_to_name_map.find(row.col_index);
    if (it == index_to_name_map.end()) {
      LOG(ERROR)
          << "BuildTableSchemaFromRowSchema:: No matching col_index found: "
          << row.col_index;
      return StatusRecord{
          SQLStates::k_HY000(),
          "No matching col_index found: " + std::to_string(row.col_index)};
    }
    field.name = it->second;
    auto result = GetDataTypeInStr(row.col_type);
    if (!result) {
      LOG(ERROR) << "BuildTableSchemaFromRowSchema::GetDataTypeInStr:: "
                 << result.GetStatusRecord().message;
      return StatusRecord{SQLStates::k_HY000(),
                          result.GetStatusRecord().message};
    }
    field.type = *result;
    field.mode = row.is_mode_repeated ? "REPEATED" : "NULLABLE";
    if(field.name == "COLUMN_SIZE" || field.name =="BUFFER_LENGTH" || field.name=="NUM_PREC_RADIX" || field.name=="CHAR_OCTET_LENGTH"|| field.name=="ORDINAL_POSITION"){
      field.type = "INTEGER";
    }
    if(field.name == "COLUMN_NAME" || field.name == "DATA_TYPE" || field.name =="TYPE_NAME" || field.name=="NULLABLE"||
    field.name == "SQL_DATA_TYPE" || field.name=="ORDINAL_POSITION"){
      field.mode = " NON NULLABLE";
    }
    schema.fields.push_back(std::move(field));
  }
  return schema;
}
StatusRecordOr<BQDataType> OdbcTypeToBqType(SQLSMALLINT sql_type) {
  switch (sql_type) {
    case SQL_WVARCHAR:
    case SQL_VARCHAR:
      return BQDataType::kString;
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT:
      return BQDataType::kInt64;
    case SQL_DOUBLE:
    case SQL_REAL:
      return BQDataType::kFloat64;
    case SQL_NUMERIC:
      return BQDataType::kNumeric;
    case SQL_BIT:
      return BQDataType::kBool;
    case SQL_TYPE_DATE:
      return BQDataType::kDate;
    case SQL_TYPE_TIME:
      return BQDataType::kTime;
    case SQL_TYPE_TIMESTAMP:
      return BQDataType::kTimeStamp;
    case SQL_VARBINARY:
      return BQDataType::kBytes;
    default:
      return StatusRecord{SQLStates::k_HYC00(),
                          "Unsupported ODBC SQL type: " + std::to_string(sql_type)};
  }
}

StatusRecordOr<BQDataType> ConvertDSType(std::string const& type) {
  if (type == "STRING") {
    return BQDataType::kString;
  }
  if (type == "INTEGER" || type == "INT64") {
    return BQDataType::kInt64;
  }
  if (type == "BOOL" || type == "BOOLEAN") {
    return BQDataType::kBool;
  }
  if (type == "FLOAT64" || type == "FLOAT") {
    return BQDataType::kFloat64;
  }
  if (type == "DECIMAL" || type == "NUMERIC") {
    return BQDataType::kNumeric;
  }
  if (type == "BYTES") {
    return BQDataType::kBytes;
  }
  if (type == "DATE") {
    return BQDataType::kDate;
  }
  if (type == "DATETIME") {
    return BQDataType::kDatetime;
  }
  if (type == "TIME") {
    return BQDataType::kTime;
  }
  if (type == "TIMESTAMP") {
    return BQDataType::kTimeStamp;
  }
  if (type == "BIGNUMERIC") {
    return BQDataType::kBigNumeric;
  }
  if (type == "RANGE") {
    return BQDataType::kRange;
  }
  if (type == "STRUCT" || type == "RECORD") {
    return BQDataType::kStruct;
  }
  if (type == "JSON") {
    return BQDataType::kJson;
  }
  if (type == "NULL") {
    return BQDataType::kNull;
  }
  if (type == "INTERVAL") {
    return BQDataType::kInterval;
  }
  if (type == "GEOGRAPHY") {
    return BQDataType::kGeography;
  }
  if (type == "ARRAY") {
    return BQDataType::kArray;
  }
  std::string err_msg = "Invalid Data Type: ";
  err_msg.append(type);
  LOG(ERROR) << "ConvertDSType:: " << err_msg;
  return StatusRecord{SQLStates::k_HY000(), err_msg};
}

StatusRecordOr<QueryParameter> ConstructStringQueryParameter(
    std::string const& parameter_name, std::string const& parameter_value) {
  if (parameter_name.empty()) {
    LOG(ERROR)
        << "ConstructStringQueryParameter:: Invalid (empty) parameter name.";
    return StatusRecord{SQLStates::k_HY000(), "Invalid parameter name"};
  }

  QueryParameter query_param;
  QueryParameterType query_param_type;
  QueryParameterValue query_param_value;

  query_param_type.type = "STRING";
  query_param_value.value = parameter_value;
  query_param.name = parameter_name;
  query_param.parameter_type = query_param_type;
  query_param.parameter_value = query_param_value;

  return query_param;
}

StatusRecordOr<QueryParameter> ConstructStringArrayQueryParameter(
    std::string const& parameter_name,
    std::vector<std::string> const& parameter_values) {
  if (parameter_name.empty()) {
    LOG(ERROR) << "ConstructStringArrayQueryParameter:: Invalid (empty) "
                  "parameter name.";
    return StatusRecord{SQLStates::k_HY000(), "Invalid parameter name"};
  }
  if (parameter_values.empty()) {
    LOG(ERROR)
        << "ConstructStringArrayQueryParameter:: Empty parameter values.";
    return StatusRecord{SQLStates::k_HY000(), "Empty parameter values"};
  }

  QueryParameter query_param;
  QueryParameterType query_param_type;
  QueryParameterType query_param_array_type;
  QueryParameterValue query_param_value;

  query_param_array_type.type = "STRING";
  query_param_type.type = "ARRAY";
  query_param_type.array_type =
      std::make_shared<QueryParameterType>(query_param_array_type);
  for (auto const& param_val : parameter_values) {
    QueryParameterValue query_param_array_value;
    query_param_array_value.value = param_val;
    query_param_value.array_values.push_back(query_param_array_value);
  }
  query_param.name = parameter_name;
  query_param.parameter_type = query_param_type;
  query_param.parameter_value = query_param_value;

  return query_param;
}

StatusRecordOr<std::vector<QueryParameter>> ConstructStringQueryParameters(
    std::map<std::string, std::string> const& params) {
  std::vector<QueryParameter> query_params;
  for (auto const& [parameter_name, parameter_value] : params) {
    auto query_parameter_response =
        ConstructStringQueryParameter(parameter_name, parameter_value);
    if (!query_parameter_response) {
      LOG(ERROR)
          << "ConstructStringQueryParameters::ConstructStringQueryParameter:: "
          << query_parameter_response.GetStatusRecord().message;
      return query_parameter_response.GetStatusRecord();
    }
    query_params.emplace_back(*query_parameter_response);
  }
  return query_params;
}

PostQueryRequest ConstructBasicPostQueryRequest(
    ConnectionHandle const& conn_handle, std::string const& query_str,
    int query_timeout) {
  std::string catalog = conn_handle.GetDsn().catalog;
  std::string default_dataset = conn_handle.GetDsn().default_dataset;
  bool is_bq_legacy_sql = conn_handle.GetDsn().is_bq_legacy_sql;
  bool is_job_creation_required = conn_handle.GetDsn().is_job_creation_required;
  std::string session_location = conn_handle.GetDsn().session_location;
  bool is_query_cache = conn_handle.GetDsn().is_query_cache;
  PostQueryRequest post_request;
  QueryRequest query_request;
  // Construct query request.
  query_request.set_dry_run(false);
  query_request.set_query(query_str);
  query_request.set_timeout(std::chrono::milliseconds(query_timeout * 1000));
  query_request.set_use_legacy_sql(is_bq_legacy_sql);
  query_request.set_use_query_cache(is_query_cache);
  if (is_job_creation_required) {
    query_request.set_job_creation_mode(JobCreationMode::Required());
  }
  if (!default_dataset.empty()) {
    DatasetReference ds_ref;
    // Set dataset info.
    ds_ref.project_id = catalog;
    ds_ref.dataset_id = default_dataset;
    query_request.set_default_dataset(ds_ref);
  }
  std::string psc = conn_handle.GetDsn().psc;
  std::string psc_location = GetLocationfromPSC(psc);
  if (!psc_location.empty()) {
    query_request.set_location(psc_location);
  }

  std::vector<ConnectionProperty> combined_properties =
      conn_handle.GetDsn().connection_properties;

  // If session started, add session_id
  if (conn_handle.IsSessionStarted()) {
    combined_properties.push_back(
        ConnectionProperty{"session_id", conn_handle.GetSessionId()});
  } else if (conn_handle.GetDsn().sessions_enabled) {
    query_request.set_create_session(true);
    query_request.set_location(session_location);
  }

  // Now set all at once
  query_request.set_connection_properties(combined_properties);

  // Set billing info and query request.
  post_request.set_project_id(catalog);
  post_request.set_query_request(query_request);
  return post_request;
}
odbc_internal::StatusRecordOr<TableSchema> BuildTableSchemaFromMetadataMap(
    RowSchema& row_schema,
    std::map<int, OdbcColumnSpec> const& metadata_schema) {
  if (row_schema.empty()) {
    LOG(ERROR) << "BuildTableSchemaFromMetadataMap:: Row schema is empty.";
    return StatusRecord{SQLStates::k_HY000(),
                        "row schema should not be empty"};
  }

  std::unordered_map<int, OdbcColumnSpec> index_to_spec_map = {};
  for (auto const& [index, spec] : metadata_schema) {
    index_to_spec_map[index] = spec;
  }

  std::sort(row_schema.begin(), row_schema.end());

  TableSchema schema;
  for (auto& row : row_schema) {
    TableFieldSchema field;

    auto it = index_to_spec_map.find(row.col_index);
    if (it == index_to_spec_map.end()) {
      LOG(ERROR)
          << "BuildTableSchemaFromMetadataMap:: No matching col_index found: "
          << row.col_index;
      return StatusRecord{
          SQLStates::k_HY000(),
          "No matching col_index found: " + std::to_string(row.col_index)};
    }

    field.name = it->second.odbc_column_name;

    auto bq_type_result = OdbcTypeToBqType(it->second.odbc_data_type);
    if (!bq_type_result) {
      LOG(ERROR) << "BuildTableSchemaFromMetadataMap::OdbcTypeToBqType:: "
                 << bq_type_result.GetStatusRecord().message;
      return bq_type_result.GetStatusRecord();
    }

    auto string_type_result = GetDataTypeInStr(*bq_type_result);
    if (!string_type_result) {
      LOG(ERROR) << "BuildTableSchemaFromMetadataMap::GetDataTypeInStr:: "
                 << string_type_result.GetStatusRecord().message;
      return string_type_result.GetStatusRecord();
    }
    field.type = *string_type_result;

    field.mode = row.is_mode_repeated ? "REPEATED" : "NULLABLE";
    schema.fields.push_back(std::move(field));
  }
  return schema;
}

odbc_internal::StatusRecordOr<PostQueryRequest>
ConstructNamedParametersPostQueryRequest(
    std::string const& catalog, std::string const& dataset,
    std::string const& named_query,
    std::vector<QueryParameter> const& named_query_params) {
  if (catalog.empty()) {
    LOG(ERROR) << "ConstructNamedParametersPostQueryRequest:: Cannot construct "
                  "request: catalog name is required.";
    return StatusRecord{SQLStates::k_HY090(),
                        "Cannot construct named parameter query "
                        "request: catalog name is required"};
  }
  if (dataset.empty()) {
    LOG(ERROR) << "ConstructNamedParametersPostQueryRequest:: Cannot construct "
                  "request: dataset name is required.";
    return StatusRecord{SQLStates::k_HY090(),
                        "Cannot construct named parameter query "
                        "request: dataset name is required"};
  }
  if (named_query.empty()) {
    LOG(ERROR) << "ConstructNamedParametersPostQueryRequest:: Cannot construct "
                  "request: parameterized query is required.";
    return StatusRecord{SQLStates::k_HY090(),
                        "Cannot construct named parameter query "
                        "request: parameterized query is required"};
  }
  PostQueryRequest post_request;
  QueryRequest query_request;
  DatasetReference ds_ref;
  // Set dataset info.
  ds_ref.project_id = catalog;
  ds_ref.dataset_id = dataset;
  // Construct query request.
  query_request.set_dry_run(false);
  query_request.set_default_dataset(ds_ref);
  query_request.set_query(named_query);
  // Following are specific to parameterized queries.
  query_request.set_parameter_mode("NAMED");
  query_request.set_query_parameters(named_query_params);
  query_request.set_use_legacy_sql(false);
  // Set billing info and query request.
  post_request.set_project_id(catalog);
  post_request.set_query_request(query_request);
  return post_request;
}

odbc_internal::StatusRecordOr<std::string> GetDataTypeInStr(BQDataType type) {
  switch (type) {
    case BQDataType::kArray:
      return std::string("ARRAY");
    case BQDataType::kBigNumeric:
      return std::string("BIGNUMERIC");
    case BQDataType::kNumeric:
      return std::string("NUMERIC");
    case BQDataType::kBytes:
      return std::string("BYTES");
    case BQDataType::kInt64:
      return std::string("INT64");
    case BQDataType::kDate:
      return std::string("DATE");
    case BQDataType::kFloat64:
      return std::string("FLOAT64");
    case BQDataType::kInterval:
      return std::string("INTERVAL");
    case BQDataType::kGeography:
      return std::string("GEOGRAPHY");
    case BQDataType::kDatetime:
      return std::string("DATETIME");
    case BQDataType::kTime:
      return std::string("TIME");
    case BQDataType::kBool:
      return std::string("BOOL");
    case BQDataType::kString:
      return std::string("STRING");
    case BQDataType::kRange:
      return std::string("RANGE");
    case BQDataType::kStruct:
      return std::string("STRUCT");
    case BQDataType::kJson:
      return std::string("JSON");
    case BQDataType::kTimeStamp:
      return std::string("TIMESTAMP");
    case BQDataType::kNull:
      return std::string("NULL");
    default:
      std::string err_msg = "Invalid BQ Data Type: ";
      err_msg.append(std::to_string(type));
      LOG(ERROR) << "GetDataTypeInStr:: " << err_msg;
      return StatusRecord{SQLStates::k_HY000(), err_msg};
  }
}

odbc_internal::StatusRecordOr<SQLSMALLINT> GetSQLDataType(
    std::string const& type, bool isArray) {
  if (isArray) {
    return SQL_VARCHAR;
  }
  if (type == "STRING") {
    return SQL_VARCHAR;
  }
  if (type == "INTEGER" || type == "INT64") {
    return SQL_BIGINT;
  }
  if (type == "BOOL" || type == "BOOLEAN") {
    return SQL_BIT;
  }
  if (type == "FLOAT64" || type == "FLOAT") {
    return SQL_DOUBLE;
  }
  if (type == "DECIMAL" || type == "NUMERIC" || type == "BIGNUMERIC") {
    return SQL_NUMERIC;
  }
  if (type == "BYTES") {
    return SQL_VARBINARY;
  }
  if (type == "DATE") {
    return SQL_TYPE_DATE;
  }
  if (type == "DATETIME") {
    return SQL_TYPE_TIMESTAMP;
  }
  if (type == "TIME") {
    return SQL_TYPE_TIME;
  }
  if (type == "TIMESTAMP") {
    return SQL_TYPE_TIMESTAMP;
  }
  if (type == "STRUCT" || type == "RECORD") {
    return SQL_VARCHAR;
  }
  if (type == "JSON") {
    return SQL_VARCHAR;
  }
  if (type == "INTERVAL") {
    return SQL_VARCHAR;
  }
  if (type == "GEOGRAPHY") {
    return SQL_VARCHAR;
  }
  if (type == "ARRAY") {
    return SQL_VARCHAR;
  }
  if (type == "RANGE") {
    return SQL_VARCHAR;
  }
  std::string err_msg = "Invalid Data Type: ";
  err_msg.append(type);
  LOG(ERROR) << "GetSQLDataType:: " << err_msg;
  return StatusRecord{SQLStates::k_HY000(), err_msg};
}

bool operator==(ColumnSchema const& lhs, ColumnSchema const& rhs) {
  return (lhs.col_index == rhs.col_index && lhs.col_type == rhs.col_type);
}
bool operator>(ColumnSchema const& lhs, ColumnSchema const& rhs) {
  return (lhs.col_index > rhs.col_index);
}
bool operator<(ColumnSchema const& lhs, ColumnSchema const& rhs) {
  return (lhs.col_index < rhs.col_index);
}

// verify the parameters for the connection and return any missing parameter as
// a string.
odbc_internal::StatusRecordOr<std::string> GetMissingAttributesStr(
    ConnectionHandle* conn_handle) {
  Dsn dsn = conn_handle->GetDsn();
  std::ostringstream missing;

  if (dsn.o_auth_mechanism.empty() && dsn.catalog.empty()) {
    missing << "Catalog:"
            << "Catalog=?;";
    missing << "OAuthMechanism:"
            << "OAuthMechanism=?;";
  } else {
    if (dsn.catalog.empty()) {
      missing << "Catalog:"
              << "Catalog=?;";
    }
    if (dsn.o_auth_mechanism.empty()) {
      missing << "OAuthMechanism:"
              << "OAuthMechanism=?;";
    }
    if (!dsn.o_auth_mechanism.empty() && dsn.key_file_path.empty()) {
      missing << "KeyFilePath:"
              << "KeyFilePath=?;";
    }
  }
  std::string missing_str = missing.str();
  if (!missing_str.empty()) {
    return missing_str;
  }
  return StatusRecord::Ok();
}

odbc_internal::StatusRecord ValidateAllowedAttributes(
    ConnectionHandle* conn_handle, Section const& attributes) {
  StatusRecord status_record = StatusRecord::Ok();
  Dsn dsn_fields = conn_handle->GetDsn();

  // TODO(b/384384699): Support ListProjectsParent as part of DSN from the UI
  std::unordered_map<std::string, std::string> dsn_map = {
      {"DRIVER", dsn_fields.driver},
      {"CATALOG", dsn_fields.catalog},
      {"DSN", dsn_fields.dsn_name},
      {"KEYFILEPATH", dsn_fields.key_file_path},
      {"OAUTHMECHANISM", dsn_fields.o_auth_mechanism}};

  for (auto const& [key, _] : attributes) {
    auto it = dsn_map.find(key);
    if (it != dsn_map.end()) {
      if (!it->second.empty()) {
        LOG(ERROR) << "ValidateAllowedAttributes:: Connection Attribute '"
                   << key << "' already found!";
        status_record = StatusRecord{
            SQLStates::k_HY000(), "Connection Error: Connection Attribute '" +
                                      key + "' already found!"};
      }
    } else {
      LOG(ERROR)
          << "ValidateAllowedAttributes:: Non-requested connection attribute '"
          << key << "' in ConnectionString.";
      status_record = StatusRecord{
          SQLStates::k_HY000(),
          "Connection Error: Non Requested connection attribute '" + key +
              "' in ConnectionString"};
    }
  }
  return status_record;
}
#ifdef _WIN32
std::string EncryptPassword(std::string const& password) {
  DATA_BLOB input, output;
  std::string hex_result;
  input.pbData = (BYTE*)password.data();
  input.cbData = static_cast<DWORD>(password.size());

  if (CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                       &output)) {
    std::vector<uint8_t> encrypted_data(output.pbData,
                                        output.pbData + output.cbData);
    BytesToHex(encrypted_data, hex_result);
    LocalFree(output.pbData);  // Free memory allocated by CryptProtectData
    hex_result = hex_result.substr(2);
  } else {
    LOG(ERROR) << "EncryptPassword::CryptProtectData:: Failed to encrypt "
                  "password. Error code: "
               << GetLastError();
    hex_result.clear();  // Ensures an empty string in case of failure
  }
  return hex_result;
}

std::string DecryptPassword(std::string const& encrypted_hex) {
  std::vector<uint8_t> encrypted_data = HexToBytes(encrypted_hex);
  DATA_BLOB input, output;
  std::string decrypted_password;
  input.pbData = encrypted_data.data();
  input.cbData = static_cast<DWORD>(encrypted_data.size());

  if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0,
                         &output)) {
    decrypted_password.assign(reinterpret_cast<char*>(output.pbData),
                              output.cbData);
    LocalFree(output.pbData);  // Free memory allocated by CryptUnprotectData
  } else {
    LOG(ERROR) << "DecryptPassword::CryptUnprotectData:: Failed to decrypt "
                  "password. Error code: "
               << GetLastError();
    decrypted_password.clear();  // Ensures an empty string in case of failure
  }
  return decrypted_password;
}
#endif  //_WIN32

}  // namespace google::cloud::odbc_bq_driver_internal
