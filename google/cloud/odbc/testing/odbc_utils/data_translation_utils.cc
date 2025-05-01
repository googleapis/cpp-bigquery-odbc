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
}  // namespace google::cloud::odbc_tests
