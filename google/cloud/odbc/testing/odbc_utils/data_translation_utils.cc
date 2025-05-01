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

#include "google/cloud/odbc/testing/odbc_utils/data_translation_utils.h"

namespace google::cloud::odbc_tests {

std::vector<DateBasicTestStruct> FetchDateConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  std::vector<DateBasicTestStruct> results;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromDateTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    DateBasicTestStruct result;
    result.target_c_type = expected.target_c_type;
    result.value = {0, 0, 0};
    result.return_str_val = std::nullopt;
    result.status = SQL_ERROR;

    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    if (!SQL_SUCCEEDED(status)) {
      results.emplace_back(result);
      continue;
    }
    result.status = status;
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        result.return_str_val = reinterpret_cast<char*>(data);
        break;
      }
      case SQL_C_WCHAR: {
        result.return_str_val =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), 10);
        break;
      }
      case SQL_C_BINARY: {
        if (strlen_or_ind == sizeof(SQL_DATE_STRUCT)) {
          result.value = *reinterpret_cast<SQL_DATE_STRUCT*>(data);
          result.return_str_val = FormatDate(result.value);
        }
        break;
      }
      case SQL_C_TYPE_DATE: {
        result.value = *reinterpret_cast<SQL_DATE_STRUCT*>(data);
        break;
      }
      case SQL_C_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT* ret_ts_struct =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        result.value.year = ret_ts_struct->year;
        result.value.month = ret_ts_struct->month;
        result.value.day = ret_ts_struct->day;
        break;
      }
      default:
        break;
    }
    results.emplace_back(result);
  }
  return results;
}

std::vector<TimeBasicTestStruct> FetchTimeConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query) {
  SQLRETURN status;
  std::uint8_t data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  std::vector<TimeBasicTestStruct> results;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromTimeTestData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    TimeBasicTestStruct result;
    result.target_c_type = expected.target_c_type;
    result.value = {00, 00, 00};
    result.return_val_str = std::nullopt;
    result.status = SQL_ERROR;

    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }

    result.status = status;
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        result.return_val_str = std::string(reinterpret_cast<char*>(data));
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        result.return_val_str =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        break;
      }
      case SQL_C_BINARY: {
        SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
        result.return_val_str = FormatTimetoString(*time);
        break;
      }
      case SQL_C_TYPE_TIME: {
        result.value = *reinterpret_cast<SQL_TIME_STRUCT*>(data);
        break;
      }
      case SQL_C_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT* ret_ts_struct =
            reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        result.value.hour = ret_ts_struct->hour;
        result.value.minute = ret_ts_struct->minute;
        result.value.second = ret_ts_struct->second;
        break;
      }
      default:
        break;
    }
    results.emplace_back(result);
  }
  return results;
}

std::vector<DateTimeBasicTestStruct> FetchDateTimeConversionResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query) {
  SQLRETURN status;
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  std::vector<DateTimeBasicTestStruct> results;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  for (auto const& expected : kConversionFromDateTimeTestData) {
    SQLPOINTER data[kBufferLength] = {0};
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);

    DateTimeBasicTestStruct result;
    result.target_c_type = expected.target_c_type;
    result.value = {};
    result.return_val_str = std::nullopt;
    result.status = SQL_ERROR;

    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }

    result.status = status;
    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        result.return_val_str = reinterpret_cast<char*>(data);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        result.return_val_str =
            ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(data), length);
        break;
      }
      case SQL_C_BINARY: {
        result.return_val_str =
            FormatTimeStamp(*reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data));
        break;
      }
      case SQL_C_TYPE_DATE: {
        SQL_DATE_STRUCT* date = reinterpret_cast<SQL_DATE_STRUCT*>(data);
        result.value.year = date->year;
        result.value.month = date->month;
        result.value.day = date->day;
        break;
      }
      case SQL_C_TYPE_TIMESTAMP: {
        result.value = *reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(data);
        break;
      }
      case SQL_C_TYPE_TIME: {
        SQL_TIME_STRUCT* time = reinterpret_cast<SQL_TIME_STRUCT*>(data);
        result.value.hour = time->hour;
        result.value.minute = time->minute;
        result.value.second = time->second;
        break;
      }
      case SQL_C_SLONG:
      case SQL_C_DOUBLE:
      case SQL_C_USHORT: {
        result.status = expected.status;
        break;
      }
      default:
        break;
    }
    results.emplace_back(result);
  }
  return results;
}

std::vector<IntervalBasicTestStruct> FetchIntervalConversionResults(
    std::shared_ptr<ODBCHandles> conn, std::string const& query,
    std::vector<IntervalBasicTestStruct> test_data) {
  SQLRETURN status;
  char read_stmt[kBufferLength];
  SQLCHAR data_char[kBufferLength];
  SQLLEN strlen_or_ind;
  StrToChar(read_stmt, query.c_str());

  std::vector<IntervalBasicTestStruct> results;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  for (auto const& expected : test_data) {
    std::cout << "target_c_type 1: " << expected.target_c_type << std::endl;
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data_char,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    status = SQLFetch(conn->hstmt);

    if (status == SQL_NO_DATA) {
      break;
    }

    IntervalBasicTestStruct result;
    result.target_c_type = expected.target_c_type;
    result.interval_value = {};
    result.return_val_str = std::nullopt;
    result.status = SQL_ERROR;

    result.status = status;
    SQL_INTERVAL_STRUCT* return_val =
        reinterpret_cast<SQL_INTERVAL_STRUCT*>(data_char);

    switch (expected.target_c_type) {
      case SQL_C_CHAR: {
        result.return_val_str = reinterpret_cast<char*>(data_char);
        break;
      }
      case SQL_C_WCHAR: {
        SQLINTEGER length = strlen_or_ind / sizeof(SQLWCHAR);
        result.return_val_str = ConvertSQLWCHARToString(
            reinterpret_cast<SQLWCHAR*>(data_char), length);
        break;
      }
      case SQL_C_INTERVAL_YEAR: {
        result.interval_value.interval_sign = return_val->interval_sign;
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.year_month.year =
            return_val->intval.year_month.year;
        break;
      }
      case SQL_C_INTERVAL_MONTH: {
        result.interval_value.interval_sign = return_val->interval_sign;
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.year_month.month =
            return_val->intval.year_month.month;
        break;
      }
      case SQL_C_INTERVAL_YEAR_TO_MONTH: {
        result.interval_value.interval_sign = return_val->interval_sign;
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.year_month.year =
            return_val->intval.year_month.year;
        result.interval_value.intval.year_month.month =
            return_val->intval.year_month.month;
        break;
      }
      case SQL_C_INTERVAL_DAY: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.day =
            return_val->intval.day_second.day;
        break;
      }
      case SQL_C_INTERVAL_HOUR: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.hour =
            return_val->intval.day_second.hour;
        break;
      }
      case SQL_C_INTERVAL_MINUTE: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.minute =
            return_val->intval.day_second.minute;
        break;
      }
      case SQL_C_INTERVAL_SECOND: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.second =
            return_val->intval.day_second.second;
        break;
      }
      case SQL_C_INTERVAL_DAY_TO_HOUR: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.day =
            return_val->intval.day_second.day;
        result.interval_value.intval.day_second.hour =
            return_val->intval.day_second.hour;
        break;
      }
      case SQL_C_INTERVAL_DAY_TO_MINUTE: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.day =
            return_val->intval.day_second.day;
        result.interval_value.intval.day_second.hour =
            return_val->intval.day_second.hour;
        result.interval_value.intval.day_second.minute =
            return_val->intval.day_second.minute;
        break;
      }
      case SQL_C_INTERVAL_DAY_TO_SECOND: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.day =
            return_val->intval.day_second.day;
        result.interval_value.intval.day_second.hour =
            return_val->intval.day_second.hour;
        result.interval_value.intval.day_second.minute =
            return_val->intval.day_second.minute;
        result.interval_value.intval.day_second.second =
            return_val->intval.day_second.second;
        break;
      }
      case SQL_C_INTERVAL_HOUR_TO_MINUTE: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.hour =
            return_val->intval.day_second.hour;
        result.interval_value.intval.day_second.minute =
            return_val->intval.day_second.minute;
        break;
      }
      case SQL_C_INTERVAL_HOUR_TO_SECOND: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.hour =
            return_val->intval.day_second.hour;
        result.interval_value.intval.day_second.minute =
            return_val->intval.day_second.minute;
        result.interval_value.intval.day_second.second =
            return_val->intval.day_second.second;
        break;
      }
      case SQL_C_INTERVAL_MINUTE_TO_SECOND: {
        result.interval_value.interval_type = return_val->interval_type;
        result.interval_value.intval.day_second.minute =
            return_val->intval.day_second.minute;
        result.interval_value.intval.day_second.second =
            return_val->intval.day_second.second;
        break;
      }
      default:
        break;
    }
    results.emplace_back(result);
  }
  return results;
}

std::vector<IntervalArthemeticTestStruct> FetchIntervalArtheConvertResults(
    std::shared_ptr<ODBCHandles> const& conn, std::string const& query) {
  SQLRETURN status;
  SQLCHAR data[kBufferLength];
  SQLLEN strlen_or_ind;
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  std::vector<IntervalArthemeticTestStruct> results;
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn);

  for (auto const& expected : kConversionFromSinglePrecisionIntervalData) {
    status = SQLBindCol(conn->hstmt, 1, expected.target_c_type, data,
                        kBufferLength, &strlen_or_ind);
    CheckError(status, "SQLBindCol", conn);
    status = SQLFetch(conn->hstmt);
    CheckError(status, "SQLFetch", conn);

    IntervalArthemeticTestStruct result;
    result.status = expected.target_c_type;
    result.value = {};
    result.status = expected.status;

    if (status == SQL_NO_DATA) {
      break;
    }

    result.status = status;
    switch (expected.target_c_type) {
      case SQL_C_STINYINT: {
        result.value.int_t = *reinterpret_cast<int8_t*>(data);
        break;
      }
      case SQL_C_UTINYINT: {
        result.value.unit_t = *reinterpret_cast<uint8_t*>(data);
        break;
      }
      case SQL_C_SSHORT: {
        result.value.sql_smallint = *reinterpret_cast<SQLSMALLINT*>(data);
        break;
      }
      case SQL_C_USHORT: {
        result.value.sql_usmallint = *reinterpret_cast<SQLUSMALLINT*>(data);
        break;
      }
      case SQL_C_ULONG: {
        result.value.sql_uninteger = *reinterpret_cast<SQLUINTEGER*>(data);
        break;
      }
      case SQL_C_SBIGINT: {
        result.value.sql_bigint = *reinterpret_cast<SQLBIGINT*>(data);
        break;
      }
      case SQL_C_NUMERIC: {
        SQL_NUMERIC_STRUCT num = *reinterpret_cast<SQL_NUMERIC_STRUCT*>(data);
        result.value.numeric = num;
        break;
      }
      default:
        break;
    }
    results.emplace_back(result);
  }
  return results;
}
}  // namespace google::cloud::odbc_tests
