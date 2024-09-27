// Copyright 2023 Google LLC
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

#ifndef _WIN32
#include <iconv.h>
#endif  // _WIN32

#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/status_or.h"

namespace google::cloud::odbc_tests {

using ::google::cloud::internal::ExponentialBackoffPolicy;

std::string GetRandomString(int len) {
  static constexpr char kChars[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  std::string str;
  str.reserve(len);
  for (int i = 0; i < len; i++) {
    str += kChars[rand() % (sizeof(kChars) - 1)];
  }
  return str;
}

std::string getSchemaStr(Schema schema) {
  std::string schema_str = "(";
  for (int i = 0; i < schema.size(); i++) {
    ColumnMinimal col = schema[i];
    schema_str.append(col.name + " " + col.type);
    if (i < schema.size() - 1) {
      schema_str.append(", ");
    }
  }
  schema_str.append(")");
  return schema_str;
}

std::string GetIntervalTypeStr(const SQLINTERVAL type) {
  std::string result;
  switch (type) {
    case SQL_IS_YEAR_TO_MONTH:
      result = "YEAR TO MONTH";
      break;
    case SQL_IS_YEAR:
      result = "YEAR";
      break;
    case SQL_IS_MONTH:
      result = "MONTH";
      break;
    case SQL_IS_DAY:
      result = "DAY";
      break;
    case SQL_IS_HOUR:
      result = "HOUR";
      break;
    case SQL_IS_MINUTE:
      result = "MINUTE";
      break;
    case SQL_IS_SECOND:
      result = "SECOND";
      break;
    case SQL_IS_DAY_TO_HOUR:
      result = "DAY TO HOUR";
      break;
    case SQL_IS_DAY_TO_MINUTE:
      result = "DAY TO MINUTE";
      break;
    case SQL_IS_DAY_TO_SECOND:
      result = "DAY TO SECOND";
      break;
    case SQL_IS_HOUR_TO_MINUTE:
      result = "HOUR TO MINUTE";
      break;
    case SQL_IS_HOUR_TO_SECOND:
      result = "HOUR TO SECOND";
      break;
    case SQL_IS_MINUTE_TO_SECOND:
      result = "MINUTE TO SECOND";
      break;
    default:
      throw std::runtime_error("Invalid interval type: " +
                               std::to_string(type));
  }
  return result;
}

std::string FormatIntervalString(const SQL_INTERVAL_STRUCT interval) {
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

std::string SQLNumericToString(const SQL_NUMERIC_STRUCT& numeric) {
  unsigned long long value = 0;

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

SQLRETURN GetCancelErrorDetails(std::string const& api, SQLHANDLE handle,
                                std::string& error_details) {
  if (handle == nullptr) {
    return -1;
  }
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[15];
  char error_str[kBufferLength];
  SQLINTEGER native_error = 0;
  SQLRETURN status;
  int rec_num = 0;
  int num_recs = 0;

  status = SQLGetDiagField(SQL_HANDLE_STMT, handle, 0, SQL_DIAG_NUMBER,
                           &num_recs, 0, 0);
  if (!SQL_SUCCEEDED(status)) {
    return status;
  }
  while (handle && num_recs--) {
    status = SQLGetDiagRec(SQL_HANDLE_STMT, handle, ++rec_num, sqlstate,
                           &native_error, buf, kBufferLength, NULL);
    if (status == SQL_NO_DATA) {
      continue;
    }
    if (!SQL_SUCCEEDED(status)) {
      return status;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num,
            api.c_str(), buf, (long)native_error, sqlstate);
    error_details.append(error_str);
  }
  return status;
}

void GetErrorDetails(std::string const& api, SQLHANDLE handle,
                     SQLSMALLINT handle_type, bool use_ansi) {
  if (handle == nullptr) {
    return;
  }
  SQLCHAR buf[kBufferLength];
  SQLCHAR sqlstate[15];
  char error_str[kBufferLength];
  SQLINTEGER native_error = 0;
  SQLRETURN status;
  int rec_num = 0;
  int num_recs = 0;

  status =
      SQLGetDiagField(handle_type, handle, 0, SQL_DIAG_NUMBER, &num_recs, 0, 0);
  if (!SQL_SUCCEEDED(status)) {
    FAIL() << "SQLGetDiagField(" << handle_type
           << ") failed with status: " << status;
    return;
  }
  while (handle && num_recs--) {
    if (use_ansi) {
      status = SQLGetDiagRecA(handle_type, handle, ++rec_num, sqlstate,
                              &native_error, buf, kBufferLength, NULL);
    } else {
      status = SQLGetDiagRec(handle_type, handle, ++rec_num, sqlstate,
                             &native_error, buf, kBufferLength, NULL);
    }
    if (status == SQL_NO_DATA) {
      continue;
    }
    if (!SQL_SUCCEEDED(status)) {
      FAIL() << "SQLGetDiagRec(" << handle_type
             << ") failed with status: " << status;
      break;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num,
            api.c_str(), buf, (long)native_error, sqlstate);
    FAIL() << error_str;
  }
}

void GetErrorDetails(std::string const& api, std::shared_ptr<ODBCHandles> conn,
                     bool use_ansi = false) {
  GetErrorDetails(api, conn->ard, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->ird, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->apd, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->ipd, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->hstmt, SQL_HANDLE_STMT, use_ansi);
  GetErrorDetails(api, conn->hdbc, SQL_HANDLE_DBC, use_ansi);
  GetErrorDetails(api, conn->henv, SQL_HANDLE_ENV, use_ansi);
}

inline void CheckError(SQLRETURN status, std::string const api,
                       std::shared_ptr<ODBCHandles> conn, bool use_ansi) {
  if (!SQL_SUCCEEDED(status)) {
    if (use_ansi) {
      std::string ansi_api = "ANSI-";
      ansi_api.append(api);
      GetErrorDetails(ansi_api, conn, use_ansi);
    } else {
      GetErrorDetails(api, conn, use_ansi);
    }
    throw std::runtime_error(api +
                             " failed with status: " + std::to_string(status));
  }
}

void Table::Create(std::shared_ptr<ODBCHandles> conn, std::string schema_str,
                   bool use_ansi) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt,
            "CREATE OR REPLACE TABLE " + table_name_ + " " + schema_str);
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void Table::CreateWithPrepare(std::shared_ptr<ODBCHandles> conn,
                              std::string schema_str) {
  CreateTableWithPrepare(conn, table_name_, schema_str);
}

void Table::Drop(std::shared_ptr<ODBCHandles> conn, bool use_ansi) {
  char drop_table_stmt[kBufferLength];
  StrToChar(drop_table_stmt, "DROP TABLE IF EXISTS " + table_name_);
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)drop_table_stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)drop_table_stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void Table::DropWithPrepare(std::shared_ptr<ODBCHandles> conn) {
  DropTableWithPrepare(conn, table_name_);
}

// TODO(#11): Generic implementation of InsertIntoTable function from
// testing/commons.*
void Table::InsertData(std::shared_ptr<ODBCHandles> conn, StdRows rows,
                       bool use_ansi, bool use_sqlprepare) {
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    auto row = rows[i];
    std::string row_str = "( ";

    auto str_field = row.str_field;
    if (!str_field.empty()) {
      row_str.append("'" + str_field + "', ");
    } else {
      row_str.append("NULL, ");
    }

    auto int_field = row.int_field;
    if (int_field != NULL) {
      row_str.append(std::to_string(int_field) + ", ");
    } else {
      row_str.append("NULL, ");
    }

    auto float_field = row.float_field;
    if (float_field != NULL) {
      row_str.append(std::to_string(float_field));
    } else {
      row_str.append("NULL");
    }

    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status;
  if (use_sqlprepare) {
    if (use_ansi) {
      status = SQLPrepareA(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                           insert_stmt.size());
    } else {
      status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                          insert_stmt.size());
    }
    CheckError(status, "SQLPrepareA", conn, use_ansi);
    status = SQLExecute(conn->hstmt);
    CheckError(status, "SQLExecute", conn, use_ansi);
  } else {
    if (use_ansi) {
      status =
          SQLExecDirectA(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
    } else {
      status =
          SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
    }
    CheckError(status, "SQLExecDirect", conn, use_ansi);
  }
}

void Table::InsertStrData(std::shared_ptr<ODBCHandles> conn,
                          std::vector<std::string> rows, bool insert_index) {
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    std::string str_field = rows[i];
    std::string row_str = "( ";
    if (insert_index) {
      row_str.append(std::to_string(i) + ", ");
    }
    if (!str_field.empty()) {
      row_str.append("'" + str_field + "'");
    } else {
      row_str.append("NULL, ");
    }

    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void Table::InsertNumericData(std::shared_ptr<ODBCHandles> conn,
                              std::vector<double> rows, bool insert_index) {
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    double numeric_field = rows[i];
    std::string row_str = "( ";
    if (insert_index) {
      row_str.append(std::to_string(i) + ", ");
    }
    row_str.append(std::to_string(numeric_field));

    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void Table::InsertInt64Data(std::shared_ptr<ODBCHandles> conn,
                            std::vector<SQLBIGINT> rows, bool insert_index) {
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    SQLBIGINT numeric_field = rows[i];
    std::string row_str = "( ";
    if (insert_index) {
      row_str.append(std::to_string(i) + ", ");
    }
    row_str.append(std::to_string(numeric_field));

    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void Table::InsertTimestampData(std::shared_ptr<ODBCHandles> conn,
                                std::vector<SQL_TIMESTAMP_STRUCT> rows,
                                bool insert_index) {
  if (rows.empty()) {
    return;
  }
  std::ostringstream insert_stmt;
  insert_stmt << "INSERT INTO " << table_name_ << " VALUES ";

  for (size_t i = 0; i < rows.size(); ++i) {
    auto const& row = rows[i];
    insert_stmt << "(";

    if (insert_index) {
      insert_stmt << i << ", ";
    }

    // Insert the timestamp
    if (row.year != 0) {
      insert_stmt << "'" << row.year << "-" << (row.month < 10 ? "0" : "")
                  << row.month << "-" << (row.day < 10 ? "0" : "") << row.day
                  << " " << (row.hour < 10 ? "0" : "") << row.hour << ":"
                  << (row.minute < 10 ? "0" : "") << row.minute << ":"
                  << (row.second < 10 ? "0" : "") << row.second << "."
                  << row.fraction << "'";
    } else {
      insert_stmt << "NULL";
    }

    insert_stmt << ")";

    if (i != rows.size() - 1) {
      insert_stmt << ", ";
    }
  }

  std::string insert_stmt_str = insert_stmt.str();
  SQLRETURN status;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt_str.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}
std::string FormatDate(const SQL_DATE_STRUCT& date) {
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", date.year, date.month,
           date.day);
  return buffer;
}

void Table::InsertDateData(std::shared_ptr<ODBCHandles> conn,
                           std::vector<SQL_DATE_STRUCT> rows,
                           bool insert_index) {
  if (rows.empty()) {
    return;
  }

  std::ostringstream insert_stmt;
  insert_stmt << "INSERT INTO " << table_name_ << " VALUES ";

  for (size_t i = 0; i < rows.size(); ++i) {
    auto const& row = rows[i];
    insert_stmt << "(";

    if (insert_index) {
      insert_stmt << i << ", ";
    }

    // Insert the date
    if (row.year != 0) {
      insert_stmt << "'" << row.year << "-" << (row.month < 10 ? "0" : "")
                  << row.month << "-" << (row.day < 10 ? "0" : "") << row.day
                  << "'";
    } else {
      insert_stmt << "NULL";
    }

    insert_stmt << ")";

    if (i != rows.size() - 1) {
      insert_stmt << ", ";
    }
  }

  std::string insert_stmt_str = insert_stmt.str();
  SQLRETURN status;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt_str.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

std::string FormatTimetoString(const SQL_TIME_STRUCT& time) {
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", time.hour, time.minute,
           time.second);
  return buffer;
}

void Table::InsertTimeData(std::shared_ptr<ODBCHandles> conn,
                           std::vector<SQL_TIME_STRUCT> rows,
                           bool insert_index) {
  if (rows.empty()) {
    return;
  }
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }
  for (size_t i = 0; i < num_rows; ++i) {
    SQL_TIME_STRUCT time_data = rows[i];
    std::string row_str = "( ";
    if (insert_index) {
      row_str.append(std::to_string(i) + ", ");
    }

    // Insert the time
    row_str.append("\"");
    if ((time_data.hour >= 0) && (time_data.hour <= 24)) {
      row_str.append(std::to_string(time_data.hour) + ":");
    } else {
      row_str.append(":");
    }
    if ((time_data.minute >= 0) && (time_data.minute <= 59)) {
      row_str.append(std::to_string(time_data.minute) + ":");
    } else {
      row_str.append(":");
    }
    if ((time_data.second >= 0) && (time_data.second <= 59)) {
      row_str.append(std::to_string(time_data.second));
    } else {
      row_str.append("");
    }
    row_str.append("\"");
    row_str.append(" )");

    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }
  SQLRETURN status;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                      insert_stmt.size());

  CheckError(status, "SQLPrepareA", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertIntervalData(std::shared_ptr<ODBCHandles> conn,
                               std::vector<SQL_INTERVAL_STRUCT> rows) {
  if (rows.empty()) {
    return;
  }

  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }
  for (int i = 0; i < num_rows; i++) {
    auto row = rows[i];
    std::string row_str = "( ";
    row_str.append(std::to_string(i + 1) + ", ");
    if (row.interval_type != NULL) {
      auto kIntervalTypeToStr = GetIntervalTypeStr(row.interval_type);
      std::string interval_str;
      switch (row.interval_type) {
        case SQL_IS_YEAR:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.year_month.year) + " " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_MONTH:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.year_month.month) + " " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_DAY:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.day_second.day) + " " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_HOUR:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.day_second.hour) + " " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_MINUTE:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.day_second.minute) + " " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_SECOND:
          interval_str =
              "INTERVAL " + std::to_string(row.intval.day_second.second);
          if (row.intval.day_second.fraction != 0) {
            interval_str +=
                "." + std::to_string(row.intval.day_second.fraction);
          }

          interval_str += " " + kIntervalTypeToStr;
          break;
        case SQL_IS_YEAR_TO_MONTH:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.year_month.year) + "-" +
                         std::to_string(row.intval.year_month.month) + "' " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_DAY_TO_HOUR:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.day) + " " +
                         std::to_string(row.intval.day_second.hour) + "' " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_DAY_TO_MINUTE:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.day) + " " +
                         std::to_string(row.intval.day_second.hour) + ":" +
                         std::to_string(row.intval.day_second.minute) + "' " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_DAY_TO_SECOND:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.day) + " " +
                         std::to_string(row.intval.day_second.hour) + ":" +
                         std::to_string(row.intval.day_second.minute) + ":" +
                         std::to_string(row.intval.day_second.second);
          if (row.intval.day_second.fraction != 0) {
            interval_str +=
                "." + std::to_string(row.intval.day_second.fraction);
          }
          interval_str += "' " + kIntervalTypeToStr;
          break;
        case SQL_IS_HOUR_TO_MINUTE:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.hour) + ":" +
                         std::to_string(row.intval.day_second.minute) + "' " +
                         kIntervalTypeToStr;
          break;
        case SQL_IS_HOUR_TO_SECOND:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.hour) + ":" +
                         std::to_string(row.intval.day_second.minute) + ":" +
                         std::to_string(row.intval.day_second.second);
          if (row.intval.day_second.fraction != 0) {
            interval_str +=
                "." + std::to_string(row.intval.day_second.fraction);
          }
          interval_str += "' " + kIntervalTypeToStr;
          break;
        case SQL_IS_MINUTE_TO_SECOND:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.minute) + ":" +
                         std::to_string(row.intval.day_second.second);
          if (row.intval.day_second.fraction != 0) {
            interval_str +=
                "." + std::to_string(row.intval.day_second.fraction);
          }
          interval_str += "' " + kIntervalTypeToStr;
          break;
        default:
          throw std::runtime_error("Invalid INTERVAL value: " + interval_str);
          break;
      }
      row_str.append(interval_str);
    } else {
      row_str.append("NULL");
    }
    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }
  insert_stmt.append(";");

  SQLRETURN status;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                      insert_stmt.size());
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertJsonData(std::shared_ptr<ODBCHandles> conn,
                           std::vector<nlohmann::json> rows,
                           bool insert_index) {
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    nlohmann::json json_field = rows[i];
    std::string row_str = "( ";
    if (insert_index) {
      row_str.append(std::to_string(i) + ", ");
    }

    if (json_field != NULL) {
      row_str.append("JSON '");
      row_str.append(to_string(json_field));
      row_str.append("'");
    }

    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status;

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)insert_stmt.c_str(),
                      insert_stmt.size());

  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void CreateTableDirect(std::shared_ptr<ODBCHandles> conn,
                       std::string create_table_schema, bool use_ansi) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt, create_table_schema);

  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)create_table_stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void CreateTableWithPrepare(std::shared_ptr<ODBCHandles> conn,
                            std::string table_name, std::string schema) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt,
            "CREATE OR REPLACE TABLE " + table_name + " " + schema);

  SQLRETURN status;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)create_table_stmt,
                      strlen(create_table_stmt));
  CheckError(status, "SQLPrepare", conn, false);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn, false);
}

void DropTableWithPrepare(std::shared_ptr<ODBCHandles> conn,
                          std::string table_name) {
  char drop_table_stmt[kBufferLength];
  StrToChar(drop_table_stmt, "DROP TABLE IF EXISTS " + table_name);
  SQLRETURN status;
  status = SQLPrepare(conn->hstmt, (SQLCHAR*)drop_table_stmt,
                      strlen(drop_table_stmt));
  CheckError(status, "SQLPrepare", conn, false);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn, false);
}

void ExecuteStatement(std::shared_ptr<ODBCHandles> conn, char stmt[],
                      bool use_ansi) {
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(conn->hstmt, (SQLCHAR*)stmt, SQL_NTS);
  } else {
    status = SQLExecDirect(conn->hstmt, (SQLCHAR*)stmt, SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void DescribeCol(std::shared_ptr<ODBCHandles> conn,
                 std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index,
                 bool use_ansi) {
  SQLRETURN status;
  if (use_ansi) {
    status = SQLDescribeColA(conn->hstmt, col_index, col_ptr->name,
                             kBufferLength, &col_ptr->name_len,
                             &col_ptr->data_type, &col_ptr->data_size,
                             &col_ptr->decimal_digits, &col_ptr->nullable);

  } else {
    status = SQLDescribeCol(conn->hstmt, col_index, col_ptr->name,
                            kBufferLength, &col_ptr->name_len,
                            &col_ptr->data_type, &col_ptr->data_size,
                            &col_ptr->decimal_digits, &col_ptr->nullable);
  }

  CheckError(status, "SQLDescribeCol", conn, use_ansi);
}

void BindCol(std::shared_ptr<ODBCHandles> conn, std::shared_ptr<Column> col_ptr,
             SQLUSMALLINT col_index) {
  if (col_ptr->data_len_ptr == nullptr) {
    col_ptr->data_len_ptr = &col_ptr->data_len;
  }
  auto status =
      SQLBindCol(conn->hstmt, col_index, col_ptr->data_type, col_ptr->data,
                 col_ptr->data_size, col_ptr->data_len_ptr);

  CheckError(status, "SQLBindCol", conn);
}

void BindColManually(std::shared_ptr<ODBCHandles> conn,
                     std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index,
                     bool use_ansi) {
  SQLHDESC ard_handle;  // Application row descriptor
  SQLRETURN status;
  if (use_ansi) {
    status = SQLGetStmtAttrA(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &ard_handle, 0,
                             NULL);

  } else {
    status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &ard_handle, 0,
                            NULL);
  }
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn, use_ansi);

  // Get the highest record
  SQLSMALLINT record_count;
  if (use_ansi) {
    status = SQLGetDescFieldA(ard_handle, 0, SQL_DESC_COUNT, &record_count,
                              SQL_IS_SMALLINT, NULL);

  } else {
    status = SQLGetDescField(ard_handle, 0, SQL_DESC_COUNT, &record_count,
                             SQL_IS_SMALLINT, NULL);
  }
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn, use_ansi);

  // Update the highest record
  if (col_index > record_count) {
    status = SQLSetDescField(ard_handle, 0, SQL_DESC_COUNT,
                             (SQLPOINTER)col_index, SQL_IS_INTEGER);
    CheckError(status, "SQLGetStmtAttr(SQL_DESC_COUNT)", conn);
  }

  // Assign column attributes

  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_TYPE,
                           (SQLPOINTER)col_ptr->data_type, SQL_IS_SMALLINT);
  CheckError(status, "SQLSetDescField(SQL_DESC_TYPE)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_CONCISE_TYPE,
                           (SQLPOINTER)col_ptr->data_type, SQL_IS_SMALLINT);
  CheckError(status, "SQLSetDescField(SQL_DESC_CONCISE_TYPE)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_LENGTH,
                           &col_ptr->data_size, SQL_IS_UINTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_LENGTH)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_OCTET_LENGTH,
                           &col_ptr->data_size, SQL_IS_UINTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_OCTET_LENGTH)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_DATA_PTR,
                           col_ptr->data, SQL_NTS);
  CheckError(status, "SQLSetDescField(SQL_DESC_OCTET_LENGTH)", conn);
  if (col_ptr->data_len_ptr == nullptr) {
    col_ptr->data_len_ptr = &col_ptr->data_len;
  }
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_INDICATOR_PTR,
                           col_ptr->data_len_ptr, SQL_IS_INTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_INDICATOR_PTR)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_OCTET_LENGTH_PTR,
                           col_ptr->data_len_ptr, SQL_IS_INTEGER);
  CheckError(status, "SQLSetDescField(SQL_DESC_OCTET_LENGTH_PTR)", conn);
}

void BindStdColumns(std::shared_ptr<ODBCHandles> conn,
                    TestingDataBuffer* columns) {
  SQLRETURN status;

  columns[0].target_type = SQL_C_CHAR;
  status = SQLBindCol(conn->hstmt, (SQLUSMALLINT)1, columns[0].target_type,
                      columns[0].target_value, columns[0].buffer_length,
                      &(columns[0].str_len));
  CheckError(status, "SQLBindCol", conn);

  columns[1].target_type = SQL_C_SBIGINT;
  status = SQLBindCol(conn->hstmt, (SQLUSMALLINT)2, columns[1].target_type,
                      columns[1].target_value, columns[1].buffer_length,
                      &(columns[1].str_len));
  CheckError(status, "SQLBindCol", conn);

  columns[2].target_type = SQL_C_DOUBLE;
  status = SQLBindCol(conn->hstmt, (SQLUSMALLINT)3, columns[2].target_type,
                      columns[2].target_value, columns[2].buffer_length,
                      &(columns[2].str_len));
  CheckError(status, "SQLBindCol", conn);
}

std::string FormatTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp) {
  std::ostringstream ts;
  ts << std::setfill('0') << std::setw(4) << timestamp.year << "-"
     << std::setfill('0') << std::setw(2) << timestamp.month << "-"
     << std::setfill('0') << std::setw(2) << timestamp.day << " "
     << std::setfill('0') << std::setw(2) << timestamp.hour << ":"
     << std::setfill('0') << std::setw(2) << timestamp.minute << ":"
     << std::setfill('0') << std::setw(2) << timestamp.second << "."
     << std::setfill('0') << std::left << std::setw(6) << timestamp.fraction;

  return ts.str();
}

std::string FormatBinaryTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp) {
  std::ostringstream ts;
  ts << std::setfill('0') << std::setw(4) << timestamp.year << "-"
     << std::setfill('0') << std::setw(2) << timestamp.month << "-"
     << std::setfill('0') << std::setw(2) << timestamp.day << " "
     << std::setfill('0') << std::setw(2) << timestamp.hour << ":"
     << std::setfill('0') << std::setw(2) << timestamp.minute << ":"
     << std::setfill('0') << std::setw(2) << timestamp.second << "."
     << std::setfill('0') << std::left << std::setw(9) << timestamp.fraction;

  return ts.str();
}

std::string Utf16ToUtf8(std::wstring const& utf_16_str) {
  if (utf_16_str.empty()) {
    throw std::runtime_error(" utf16 string is empty/Null");
  }
#ifdef _WIN32
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int utf8Length = WideCharToMultiByte(CP_UTF8, 0, utf_16_str.c_str(), -1, NULL,
                                       0, NULL, NULL);
  if (utf8Length == 0) {
    throw std::runtime_error(
        "Error determining buffer size while converting wstring to string");
  }
  std::string utf8Str(utf8Length, 0);
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int result = WideCharToMultiByte(CP_UTF8, 0, utf_16_str.c_str(), -1,
                                   &utf8Str[0], utf8Length, NULL, NULL);
  if (result == 0) {
    throw std::runtime_error("Error while converting wstring to string");
  }
  return utf8Str;
#else
  iconv_t cd = iconv_open("UTF-8", "WCHAR_T");
  int errorno = -1;
  int* errorptr = &errorno;
  if (cd == reinterpret_cast<iconv_t>(errorptr)) {
    throw std::runtime_error(
        "iconv_open failed while converting wstring to string: " +
        std::string(strerror(errno)));
  }

  std::vector<char> inbuf(
      reinterpret_cast<char const*>(utf_16_str.data()),
      reinterpret_cast<char const*>(utf_16_str.data() + utf_16_str.length()));
  size_t inbytesleft = inbuf.size();
  size_t outbytesleft = inbytesleft * 4;  // Allocate more space for utf8 output

  std::string utf8str(outbytesleft, '\0');
  char* inptr = inbuf.data();
  char* outptr = const_cast<char*>(utf8str.data());

  size_t res = iconv(cd, &inptr, &inbytesleft, &outptr, &outbytesleft);
  if (res == static_cast<size_t>(-1)) {
    iconv_close(cd);
    throw std::runtime_error(
        "iconv16 failed while converting wstring to string " +
        std::string(strerror(errno)));
  }

  iconv_close(cd);
  utf8str.resize(outptr - utf8str.data());
  return utf8str;
#endif
}

std::wstring Utf8ToUtf16(std::string const& utf_8_str) {
  if (utf_8_str.empty()) {
    throw std::runtime_error("utf_8_str string isempty/Null");
  }
#ifdef _WIN32
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
  int utf16Length =
      MultiByteToWideChar(CP_UTF8, 0, utf_8_str.c_str(), -1, NULL, 0);
  if (utf16Length == 0) {
    throw std::runtime_error(
        "Error determining buffer size while converting string to wstring");
  }
  std::wstring utf16Str(utf16Length, 0);
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
  int result = MultiByteToWideChar(CP_UTF8, 0, utf_8_str.c_str(), -1,
                                   &utf16Str[0], utf16Length);
  if (result == 0) {
    throw std::runtime_error("Error while converting string to wstring");
  }
  return utf16Str;
#else

  iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
  int errorno = -1;
  int* errorptr = &errorno;
  if (cd == reinterpret_cast<iconv_t>(errorptr)) {
    throw std::runtime_error(
        "iconv_open failed while converting string to wstring " +
        std::string(strerror(errno)));
  }

  // Use string length for input byte count
  size_t inbytesleft = utf_8_str.length();
  // Allocate more space for the output buffer
  size_t outbytesleft = inbytesleft * sizeof(wchar_t);
  std::wstring utf16str(outbytesleft + sizeof(wchar_t), L'\0');

  char* inbuf = const_cast<char*>(utf_8_str.data());
  char* outbuf = reinterpret_cast<char*>(const_cast<wchar_t*>(utf16str.data()));

  size_t res = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
  if (res == static_cast<size_t>(-1)) {
    iconv_close(cd);
    throw std::runtime_error(
        "iconv8 failed while converting string to wstring " +
        std::string(strerror(errno)));
  }

  iconv_close(cd);

  // Resize the output string to the actual converted size
  utf16str.resize((outbuf - reinterpret_cast<char*>(
                                const_cast<wchar_t*>(utf16str.data()))) /
                  sizeof(wchar_t));

  return utf16str;
#endif
}

std::string ConvertSQLWCHARToString(SQLWCHAR* in_str, SQLINTEGER in_str_len) {
  if (((in_str != nullptr) && (in_str[0] == '\0'))) {
    throw std::runtime_error("in_str string is empty/Null");
  }
  std::wstring stmt_txt_wstr;
  std::wstring wstr(reinterpret_cast<wchar_t const*>(in_str));
  if (in_str_len == SQL_NTS || in_str_len == NULL) {
    in_str_len = wstr.size() * sizeof(SQLWCHAR);
  }
  stmt_txt_wstr.reserve(in_str_len);
  for (SQLINTEGER i = 0; i < in_str_len; ++i) {
    stmt_txt_wstr.push_back(static_cast<wchar_t>(in_str[i]));
  }
  return Utf16ToUtf8(stmt_txt_wstr);
}

SQLRETURN GetConvertedJsonData(std::shared_ptr<ODBCHandles> conn,
                               std::string query, SQLSMALLINT target_c_type,
                               SQLLEN* strlen_or_ind, SQLPOINTER* data) {
  SQLRETURN status;
  // SQLPOINTER data[kBufferLength];
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query.c_str());

  status = SQLPrepare(conn->hstmt, (SQLCHAR*)read_stmt, SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
  status = SQLBindCol(conn->hstmt, 1, target_c_type, data, kBufferLength,
                      strlen_or_ind);
  CheckError(status, "SQLBindCol", conn);
  status = SQLFetch(conn->hstmt);
  if (SQL_SUCCEEDED(status)) {
    CheckError(status, "SQLFetch", conn);
  }
  SQLFreeStmt(conn->hstmt, SQL_CLOSE);
  return status;
}

}  // namespace google::cloud::odbc_tests
