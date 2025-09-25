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
using ms = std::chrono::milliseconds;

#ifdef __APPLE__
std::string const kFromCode = "UTF-32LE";
#else
std::string const kFromCode = "WCHAR_T";
#endif

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
  uint64_t value = 0;
  int byte_count = std::min<int>(numeric.precision, sizeof(numeric.val));

  for (int i = byte_count - 1; i >= 0; --i) {
    value = (value << 8) + static_cast<unsigned char>(numeric.val[i]);
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

SQL_NUMERIC_STRUCT ConvertStringToNumeric(std::string const& numeric_str) {
  SQL_NUMERIC_STRUCT numeric_struct = {};
  std::string num_str;
  bool is_negative = false;
  size_t decimal_pos = std::string::npos;

  // Parse sign and find decimal point
  size_t start = numeric_str.find_first_not_of(" \t");
  if (start != std::string::npos && numeric_str[start] == '-') {
    is_negative = true;
    start++;
  }

  // Extract all digits (both integral and fractional)
  for (size_t i = start; i < numeric_str.size(); i++) {
    if (isdigit(numeric_str[i])) {
      num_str += numeric_str[i];
    } else if (numeric_str[i] == '.' && decimal_pos == std::string::npos) {
      decimal_pos = num_str.length();
    }
  }

  // Handle cases where decimal point was at end or not found
  if (decimal_pos == std::string::npos) {
    decimal_pos = num_str.length();
  }

  // Remove leading zeros except if it's the only digit before decimal
  if (num_str.length() > 1) {
    size_t first_non_zero = num_str.find_first_not_of('0');
    if (first_non_zero != std::string::npos && first_non_zero < decimal_pos) {
      num_str.erase(0, first_non_zero);
      decimal_pos -= first_non_zero;
    } else if (first_non_zero == std::string::npos) {
      num_str = "0";
      decimal_pos = 1;
    }
  }

  // Calculate precision and scale
  numeric_struct.precision = static_cast<SQLCHAR>(num_str.length());
  numeric_struct.scale = static_cast<SQLSCHAR>(num_str.length() - decimal_pos);
  numeric_struct.sign = is_negative ? 0 : 1;

  // Convert to binary (little-endian)
  memset(numeric_struct.val, 0, SQL_MAX_NUMERIC_LEN);
  std::string bigint_str = num_str;  // Full number without decimal
  uint64_t value = 0;

  try {
    value = std::stoull(bigint_str);
  } catch (...) {
    throw std::runtime_error("Numeric value out of range");
  }

  // Store in little-endian format
  for (size_t i = 0; i < sizeof(value) && i < SQL_MAX_NUMERIC_LEN; i++) {
    numeric_struct.val[i] = static_cast<SQLCHAR>((value >> (i * 8)) & 0xFF);
  }

  return numeric_struct;
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
                           &num_recs, 0, nullptr);
  if (!SQL_SUCCEEDED(status)) {
    return status;
  }
  while (handle && num_recs--) {
    status = SQLGetDiagRec(SQL_HANDLE_STMT, handle, ++rec_num, sqlstate,
                           &native_error, buf, kBufferLength, nullptr);
    if (status == SQL_NO_DATA) {
      continue;
    }
    if (!SQL_SUCCEEDED(status)) {
      return status;
    }
    sprintf(error_str, "ERROR:: %d: %s = %s (%ld) SQLSTATE=%s\n", rec_num,
            api.c_str(), buf, static_cast<long>(native_error), sqlstate);
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

  status = SQLGetDiagField(handle_type, handle, 0, SQL_DIAG_NUMBER, &num_recs,
                           0, nullptr);
  if (!SQL_SUCCEEDED(status)) {
    FAIL() << "SQLGetDiagField(" << handle_type
           << ") failed with status: " << status;
    return;
  }
  while (handle && num_recs--) {
    if (use_ansi) {
      status = SQLGetDiagRecA(handle_type, handle, ++rec_num, sqlstate,
                              &native_error, buf, kBufferLength, nullptr);
    } else {
      status = SQLGetDiagRec(handle_type, handle, ++rec_num, sqlstate,
                             &native_error, buf, kBufferLength, nullptr);
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
            api.c_str(), buf, static_cast<long>(native_error), sqlstate);
    FAIL() << error_str;
  }
}

void GetErrorDetails(std::string const& api,
                     std::shared_ptr<ODBCHandles> const& conn,
                     bool use_ansi = false) {
  GetErrorDetails(api, conn->ard, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->ird, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->apd, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->ipd, SQL_HANDLE_DESC, use_ansi);
  GetErrorDetails(api, conn->hstmt, SQL_HANDLE_STMT, use_ansi);
  GetErrorDetails(api, conn->hdbc, SQL_HANDLE_DBC, use_ansi);
  GetErrorDetails(api, conn->henv, SQL_HANDLE_ENV, use_ansi);
}

inline void CheckError(SQLRETURN status, std::string const& api,
                       std::shared_ptr<ODBCHandles> const& conn,
                       bool use_ansi) {
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

std::string GetInsertionString(std::string const& table_name, StdRows rows) {
  std::string insert_stmt = "INSERT INTO " + table_name + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return "";
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
  return insert_stmt;
}

std::string GetAllTypeInsertionString(std::string const& table_name,
                                      StdAllTypesRows const& rows) {
  std::ostringstream row_str;
  row_str << "INSERT INTO " << table_name << " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return "";
  }

  for (int i = 0; i < num_rows; i++) {
    auto row = rows[i];
    row_str << "( ";

    row_str << ToBQInsertionStr(row.str_field) << ", ";
    row_str << ToBQInsertionStr(row.int_field) << ", ";
    row_str << ToBQInsertionStr(row.float_field) << ", ";
    row_str << ToBQInsertionStr(row.timestamp) << ", ";
    row_str << ToBQInsertionStr(row.date) << ", ";
    row_str << ToBQInsertionStr(row.time) << ", ";
    row_str << ToBQInsertionStr(row.json_field);

    row_str << ")";
    if (i != (num_rows - 1)) {
      row_str << ", ";
    }
  }
  std::string insert_stmt = row_str.str();
  return insert_stmt;
}

void Table::Create(std::shared_ptr<ODBCHandles> const& conn,
                   std::string const& schema_str, bool use_ansi) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt,
            "CREATE OR REPLACE TABLE " + table_name_ + " " + schema_str);
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(create_table_stmt), SQL_NTS);
  } else {
    status = SQLExecDirect(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(create_table_stmt), SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void Table::CreateW(std::shared_ptr<ODBCHandles> const& conn,
                    std::wstring const& schema_str) {
  std::wstring query =
      L"CREATE OR REPLACE TABLE " + wtable_name_ + L" " + schema_str;
  std::vector<SQLWCHAR> sql_wstr(query.begin(), query.end());
  sql_wstr.emplace_back(L'\0');
  SQLRETURN status = SQLExecDirectW(conn->hstmt, sql_wstr.data(), SQL_NTS);
  CheckError(status, "SQLExecDirectW", conn);
}

void Table::CreateWithPrepare(std::shared_ptr<ODBCHandles> const& conn,
                              std::string const& schema_str) {
  CreateTableWithPrepare(std::move(conn), table_name_, std::move(schema_str));
}

void Table::Drop(std::shared_ptr<ODBCHandles> const& conn, bool use_ansi) {
  char drop_table_stmt[kBufferLength];
  StrToChar(drop_table_stmt, "DROP TABLE IF EXISTS " + table_name_);
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(drop_table_stmt), SQL_NTS);
  } else {
    status = SQLExecDirect(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(drop_table_stmt), SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void Table::DropW(std::shared_ptr<ODBCHandles> const& conn) {
  std::wstring query = L"DROP TABLE IF EXISTS " + wtable_name_;
  std::vector<SQLWCHAR> sql_wstr(query.begin(), query.end());
  sql_wstr.emplace_back(L'\0');
  SQLRETURN status = SQLExecDirectW(conn->hstmt, sql_wstr.data(), SQL_NTS);
  CheckError(status, "SQLExecDirectW", conn);
}

void Table::DropWithPrepare(std::shared_ptr<ODBCHandles> const& conn) {
  DropTableWithPrepare(std::move(conn), table_name_);
}

void Procedure::DropWithPrepare(std::shared_ptr<ODBCHandles> const& conn) {
  DropProcedureWithPrepare(std::move(conn), procedure_name_);
}

void Procedure::Drop(std::shared_ptr<ODBCHandles> const& conn, bool use_ansi) {
  char drop_procedure_stmt[kBufferLength];
  StrToChar(drop_procedure_stmt, "DROP PROCEDURE IF EXISTS " + procedure_name_);
  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(drop_procedure_stmt), SQL_NTS);
  } else {
    status = SQLExecDirect(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(drop_procedure_stmt), SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void DropProcedureWithPrepare(std::shared_ptr<ODBCHandles> const& conn,
                              std::string const& procedure_name) {
  // Allocate a buffer to store the DROP PROCEDURE SQL statement
  char drop_procedure_stmt[kBufferLength];
  // Construct the SQL statement to drop the procedure
  StrToChar(drop_procedure_stmt, "DROP PROCEDURE IF EXISTS " + procedure_name);

  SQLRETURN status;
  // Prepare the SQL statement
  status =
      SQLPrepare(conn->hstmt, reinterpret_cast<SQLCHAR*>(drop_procedure_stmt),
                 strlen(drop_procedure_stmt));
  CheckError(status, "SQLPrepare", conn, false);

  // Execute the prepared statement to drop the procedure
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn, false);
}

// TODO(#11): Generic implementation of InsertIntoTable function from
// testing/commons.*
void Table::InsertData(std::shared_ptr<ODBCHandles> const& conn, StdRows rows,
                       bool use_ansi, bool use_sqlprepare) {
  SQLRETURN status;
  std::string insert_stmt = GetInsertionString(table_name_, std::move(rows));
  if (insert_stmt.empty()) {
    return;
  }
  if (use_sqlprepare) {
    auto* insert_stmt_ptr = const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>(insert_stmt.c_str()));
    if (use_ansi) {
      status = SQLPrepareA(conn->hstmt, insert_stmt_ptr, insert_stmt.size());
    } else {
      status = SQLPrepare(conn->hstmt, insert_stmt_ptr, insert_stmt.size());
    }
    CheckError(status, "SQLPrepareA", conn, use_ansi);
    status = SQLExecute(conn->hstmt);
    CheckError(status, "SQLExecute", conn, use_ansi);
  } else {
    if (use_ansi) {
      status = SQLExecDirectA(
          conn->hstmt,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(insert_stmt.c_str())),
          SQL_NTS);
    } else {
      status = SQLExecDirect(
          conn->hstmt,
          const_cast<SQLCHAR*>(
              reinterpret_cast<const SQLCHAR*>(insert_stmt.c_str())),
          SQL_NTS);
    }
    CheckError(status, "SQLExecDirect", conn, use_ansi);
  }
}

void Table::InsertUnicodeData(std::shared_ptr<ODBCHandles> const& conn,
                              StdUnicodeRows rows) {
  std::wstring wstr_table_name = Utf8ToUtf16(table_name_);
  SQLRETURN status;
  while (!wstr_table_name.empty() && wstr_table_name.back() == L'\0') {
    wstr_table_name.pop_back();
  }
  std::wstring insert_stmt =
      std::wstring(L"INSERT INTO ") + wstr_table_name + L" VALUES";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    auto row = rows[i];
    std::wstring row_str = L"( ";

    auto int_field = row.int_field;
    if (int_field != NULL) {
      row_str.append(std::to_wstring(int_field) + L", ");
    } else {
      row_str += L'\0';
    }

    auto str_field1 = row.str_field1;
    if (!str_field1.empty()) {
      row_str.append(L"'" + str_field1 + L"', ");
    } else {
      row_str += L'\0';
    }

    auto str_field2 = row.str_field2;
    if (!str_field2.empty()) {
      row_str.append(L"'" + str_field2 + L"'");
    } else {
      row_str += L'\0';
    }

    row_str.append(L")");
    if (i != (num_rows - 1)) {
      row_str.append(L", ");
    } else {
      row_str += L'\0';
    }
    insert_stmt.append(row_str);
  }
  std::vector<SQLWCHAR> sql_w_str(insert_stmt.begin(), insert_stmt.end());
  status = SQLPrepareW(conn->hstmt, sql_w_str.data(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertAllData(std::shared_ptr<ODBCHandles> const& conn,
                          StdAllTypesRows const& rows) {
  SQLRETURN status;
  std::string insert_stmt = GetAllTypeInsertionString(table_name_, rows);
  if (insert_stmt.empty()) {
    return;
  }

  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt.c_str())),
                      insert_stmt.size());
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertStrData(std::shared_ptr<ODBCHandles> const& conn,
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

  SQLRETURN status;

  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt.c_str())),
                      SQL_NTS);
  CheckError(status, "SQLPrepare", conn, false);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn, false);
}

void Table::InsertNumericData(std::shared_ptr<ODBCHandles> const& conn,
                              std::vector<std::string> rows,
                              bool insert_index) {
  auto insert_stmt = "INSERT INTO " + table_name_ + " VALUES ";
  int num_rows = rows.size();
  if (!num_rows) {
    return;
  }

  for (int i = 0; i < num_rows; i++) {
    std::string numeric_field = rows[i];
    std::string row_str = "( ";
    if (insert_index) {
      row_str.append(std::to_string(i) + ", ");
    }
    row_str.append(numeric_field);

    row_str.append(")");
    if (i != (num_rows - 1)) {
      row_str.append(", ");
    }
    insert_stmt.append(row_str);
  }

  SQLRETURN status =
      SQLExecDirect(conn->hstmt,
                    const_cast<SQLCHAR*>(
                        reinterpret_cast<const SQLCHAR*>(insert_stmt.c_str())),
                    SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void Table::InsertInt64Data(std::shared_ptr<ODBCHandles> const& conn,
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
      SQLExecDirect(conn->hstmt,
                    const_cast<SQLCHAR*>(
                        reinterpret_cast<const SQLCHAR*>(insert_stmt.c_str())),
                    SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);
}

void Table::InsertTimestampData(std::shared_ptr<ODBCHandles> const& conn,
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

  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt_str.c_str())),
                      SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertRangeTimeStampData(
    std::shared_ptr<ODBCHandles> const& conn,
    std::vector<std::pair<SQL_TIMESTAMP_STRUCT, SQL_TIMESTAMP_STRUCT>> const&
        data,
    bool insert_index, std::string const& datatype) {
  if (data.empty()) {
    return;
  }

  std::ostringstream insert_stmt;
  insert_stmt << "INSERT INTO " << table_name_ << " VALUES ";

  for (size_t i = 0; i < data.size(); ++i) {
    auto const& row = data[i];
    insert_stmt << "(";

    if (insert_index) {
      insert_stmt << i << ", ";
    }

    if (row.first.year != 0 && row.second.year != 0) {
      insert_stmt << "RANGE(" << datatype << " '"
                  << FormatRangeTimeStamp(row.first) << "', " << datatype
                  << " '" << FormatRangeTimeStamp(row.second) << "')";
    } else {
      insert_stmt << "NULL";
    }

    insert_stmt << ")";

    if (i != data.size() - 1) {
      insert_stmt << ", ";
    }
  }

  std::string insert_stmt_str = insert_stmt.str();
  SQLRETURN status = ExecWithPrepare(conn, insert_stmt_str);
  CheckError(status, "ExecWithPrepare", conn);
}

void Table::InsertStructData(std::shared_ptr<ODBCHandles> const& conn,
                             std::vector<StructBasicTestStruct> const& rows,
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

    insert_stmt << "STRUCT(" << row.int_value << ", " << row.double_value
                << ", '" << row.string_value << "', ";

    if (row.int_array) {
      insert_stmt << "ARRAY[";
      for (size_t j = 0; j < row.int_array->size(); ++j) {
        insert_stmt << (*row.int_array)[j];
        if (j != row.int_array->size() - 1) {
          insert_stmt << ", ";
        }
      }
      insert_stmt << "]";
    } else {
      insert_stmt << "NULL";
    }

    insert_stmt << ")";
    insert_stmt << ")";

    if (i != rows.size() - 1) {
      insert_stmt << ", ";
    }
  }

  insert_stmt << ";";

  SQLRETURN status = ExecWithPrepare(conn, insert_stmt.str());
  CheckError(status, "Execute with Prepare", conn);
}

void Table::InsertArrayData(std::shared_ptr<ODBCHandles> const& conn,
                            StdArrayRows array_rows, bool insert_index) {
  if (array_rows.empty()) {
    return;
  }
  std::ostringstream insert_stmt;
  insert_stmt << "INSERT INTO " << table_name_ << " VALUES ";

  for (size_t i = 0; i < array_rows.size(); ++i) {
    auto const& array_row = array_rows[i];
    auto const& int_row = array_row.int_value;
    auto const& double_row = array_row.double_value;
    auto const& string_row = array_row.string_value;
    auto const& struct_row = array_row.struct_value;

    insert_stmt << "(";
    if (insert_index) {
      insert_stmt << i << ", [";
    }
    int col_index = 0;
    for (auto const& var : int_row) {
      insert_stmt << " " << var;
      if (col_index != int_row.size() - 1) {
        insert_stmt << ", ";
      }
      col_index++;
    }

    insert_stmt << "], [";

    col_index = 0;
    for (auto const& var : double_row) {
      insert_stmt << " " << var;
      if (col_index != double_row.size() - 1) {
        insert_stmt << ", ";
      }
      col_index++;
    }

    insert_stmt << "], [";

    col_index = 0;
    for (auto const& var : string_row) {
      insert_stmt << " '" << var << "'";
      if (col_index != string_row.size() - 1) {
        insert_stmt << ", ";
      }
      col_index++;
    }

    insert_stmt << "], [";

    // Preparing insert statement for Array of Structs
    col_index = 0;
    for (auto const& var : struct_row) {
      insert_stmt << " STRUCT(" << var.int_value << ", " << var.double_value
                  << ", '" << var.string_value << "')";
      if (col_index != struct_row.size() - 1) {
        insert_stmt << ", ";
      }
      col_index++;
    }

    insert_stmt << "])";

    if (i != array_rows.size() - 1) {
      insert_stmt << ", ";
    }
  }

  std::string insert_stmt_str = insert_stmt.str();
  SQLRETURN status;

  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt_str.c_str())),
                      SQL_NTS);
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

void Table::InsertDateData(std::shared_ptr<ODBCHandles> const& conn,
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

  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt_str.c_str())),
                      SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertBooleanData(std::shared_ptr<ODBCHandles> const& conn,
                              std::vector<uint8_t> rows, bool insert_index) {
  std::ostringstream insert_stmt;
  insert_stmt << "INSERT INTO " << table_name_ << " VALUES ";

  for (size_t i = 0; i < rows.size(); ++i) {
    auto const& row = rows[i];
    insert_stmt << "(";

    if (insert_index) {
      insert_stmt << i << ", ";
    }

    insert_stmt << (row != 0 ? "TRUE" : "FALSE");
    insert_stmt << ")";

    if (i != rows.size() - 1) {
      insert_stmt << ", ";
    }
  }

  std::string insert_stmt_str = insert_stmt.str();
  SQLRETURN status;
  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt_str.c_str())),
                      SQL_NTS);
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}
void Table::InsertBytesData(std::shared_ptr<ODBCHandles> const& conn,
                            std::vector<std::vector<SQLCHAR>> const& bytes_data,
                            bool insert_index) {
  std::ostringstream insert_stmt;
  insert_stmt << "INSERT INTO " << table_name_ << " VALUES ";

  for (size_t i = 0; i < bytes_data.size(); ++i) {
    auto const& row = bytes_data[i];
    insert_stmt << "(";

    if (insert_index) {
      insert_stmt << i << ", ";
    }

    insert_stmt << "B\"";
    for (auto const& byte : row) {
      insert_stmt << "\\x" << std::hex << std::uppercase << std::setw(2)
                  << std::setfill('0') << static_cast<int>(byte);
    }
    insert_stmt << "\"";
    insert_stmt << ")";

    if (i != bytes_data.size() - 1) {
      insert_stmt << ", ";
    }
  }

  std::string insert_stmt_str = insert_stmt.str();

  SQLRETURN status;
  status = ExecWithPrepare(conn, insert_stmt_str);
  CheckError(status, "Execute With Prepare", conn);
}

std::string FormatTimetoString(const SQL_TIME_STRUCT& time) {
  char buffer[16];
  std::string time_format =
      kIsBqDriver ? "%02d:%02d:%02d" : "%02d:%02d:%02d.000000";
  snprintf(buffer, sizeof(buffer), time_format.c_str(), time.hour, time.minute,
           time.second);
  return std::string(buffer);
}

void Table::InsertTimeData(std::shared_ptr<ODBCHandles> const& conn,
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

  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt.c_str())),
                      insert_stmt.size());

  CheckError(status, "SQLPrepareA", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertIntervalData(std::shared_ptr<ODBCHandles> const& conn,
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
      auto k_interval_type_to_str = GetIntervalTypeStr(row.interval_type);
      std::string interval_str;
      switch (row.interval_type) {
        case SQL_IS_YEAR:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.year_month.year) + " " +
                         k_interval_type_to_str;
          break;
        case SQL_IS_MONTH:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.year_month.month) + " " +
                         k_interval_type_to_str;
          break;
        case SQL_IS_DAY:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.day_second.day) + " " +
                         k_interval_type_to_str;
          break;
        case SQL_IS_HOUR:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.day_second.hour) + " " +
                         k_interval_type_to_str;
          break;
        case SQL_IS_MINUTE:
          interval_str = "INTERVAL " +
                         std::to_string(row.intval.day_second.minute) + " " +
                         k_interval_type_to_str;
          break;
        case SQL_IS_SECOND:
          interval_str =
              "INTERVAL " + std::to_string(row.intval.day_second.second);
          if (row.intval.day_second.fraction != 0) {
            interval_str +=
                "." + std::to_string(row.intval.day_second.fraction);
          }

          interval_str += " " + k_interval_type_to_str;
          break;
        case SQL_IS_YEAR_TO_MONTH:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.year_month.year) + "-" +
                         std::to_string(row.intval.year_month.month) + "' " +
                         k_interval_type_to_str;
          break;
        case SQL_IS_DAY_TO_HOUR:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.day) + " " +
                         std::to_string(row.intval.day_second.hour) + "' " +
                         k_interval_type_to_str;
          break;
        case SQL_IS_DAY_TO_MINUTE:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.day) + " " +
                         std::to_string(row.intval.day_second.hour) + ":" +
                         std::to_string(row.intval.day_second.minute) + "' " +
                         k_interval_type_to_str;
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
          interval_str += "' " + k_interval_type_to_str;
          break;
        case SQL_IS_HOUR_TO_MINUTE:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.hour) + ":" +
                         std::to_string(row.intval.day_second.minute) + "' " +
                         k_interval_type_to_str;
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
          interval_str += "' " + k_interval_type_to_str;
          break;
        case SQL_IS_MINUTE_TO_SECOND:
          interval_str = "INTERVAL '" +
                         std::to_string(row.intval.day_second.minute) + ":" +
                         std::to_string(row.intval.day_second.second);
          if (row.intval.day_second.fraction != 0) {
            interval_str +=
                "." + std::to_string(row.intval.day_second.fraction);
          }
          interval_str += "' " + k_interval_type_to_str;
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
  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt.c_str())),
                      insert_stmt.size());
  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertJsonData(std::shared_ptr<ODBCHandles> const& conn,
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

  status = SQLPrepare(conn->hstmt,
                      const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(
                          insert_stmt.c_str())),
                      insert_stmt.size());

  CheckError(status, "SQLPrepare", conn);
  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);
}

void Table::InsertGeographyData(
    std::shared_ptr<ODBCHandles> const& conn,
    std::vector<std::pair<std::string, std::string>> data, bool insert_index) {
  if (data.empty()) {
    return;
  }
  std::ostringstream insert_stmt;
  insert_stmt << "INSERT INTO " << table_name_ << " VALUES ";

  for (int i = 0; i < data.size(); i++) {
    auto const& elem = data[i];
    insert_stmt << "(";

    if (insert_index) {
      insert_stmt << std::to_string(i + 1) << ", ";
    }
    insert_stmt << elem.first << "('" << elem.second << "')"
                << ")";
    if (i != data.size() - 1) {
      insert_stmt << ",";
    }
  }
  std::string insert_stmt_str = insert_stmt.str();
  SQLRETURN status = ExecWithPrepare(conn, insert_stmt_str);
}

void Table::InsertRangeDateData(
    std::shared_ptr<ODBCHandles> const& conn,
    std::vector<std::pair<SQL_DATE_STRUCT, SQL_DATE_STRUCT>> rows,
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

    // Insert the range
    if (row.first.year != 0 && row.second.year != 0) {
      insert_stmt << "RANGE(DATE '" << FormatDate(row.first) << "', DATE '"
                  << FormatDate(row.second) << "')";
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

  status = ExecWithPrepare(conn, insert_stmt_str);
  CheckError(status, "ExecWithPrepare", conn);
}

void CreateTableDirect(std::shared_ptr<ODBCHandles> const& conn,
                       std::string const& create_table_schema, bool use_ansi) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt, create_table_schema);

  SQLRETURN status;
  if (use_ansi) {
    status = SQLExecDirectA(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(create_table_stmt), SQL_NTS);
  } else {
    status = SQLExecDirect(
        conn->hstmt, reinterpret_cast<SQLCHAR*>(create_table_stmt), SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void CreateTableWithPrepare(std::shared_ptr<ODBCHandles> const& conn,
                            std::string const& table_name,
                            std::string const& schema) {
  char create_table_stmt[kBufferLength];
  StrToChar(create_table_stmt,
            "CREATE OR REPLACE TABLE " + table_name + " " + schema);

  SQLRETURN status;
  status =
      SQLPrepare(conn->hstmt, reinterpret_cast<SQLCHAR*>(create_table_stmt),
                 strlen(create_table_stmt));
  CheckError(status, "SQLPrepare", conn, false);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn, false);
}

void DropTableWithPrepare(std::shared_ptr<ODBCHandles> const& conn,
                          std::string const& table_name) {
  char drop_table_stmt[kBufferLength];
  StrToChar(drop_table_stmt, "DROP TABLE IF EXISTS " + table_name);
  SQLRETURN status;
  status = SQLPrepare(conn->hstmt, reinterpret_cast<SQLCHAR*>(drop_table_stmt),
                      strlen(drop_table_stmt));
  CheckError(status, "SQLPrepare", conn, false);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecDirect", conn, false);
}

void ExecuteStatement(std::shared_ptr<ODBCHandles> const& conn, char stmt[],
                      bool use_ansi) {
  SQLRETURN status;
  if (use_ansi) {
    status =
        SQLExecDirectA(conn->hstmt, reinterpret_cast<SQLCHAR*>(stmt), SQL_NTS);
  } else {
    status =
        SQLExecDirect(conn->hstmt, reinterpret_cast<SQLCHAR*>(stmt), SQL_NTS);
  }
  CheckError(status, "SQLExecDirect", conn, use_ansi);
}

void DescribeCol(std::shared_ptr<ODBCHandles> const& conn,
                 std::shared_ptr<Column> const& col_ptr, SQLUSMALLINT col_index,
                 bool is_async) {
  SQLRETURN status;
  ExponentialBackoffPolicy backoff(ms(10), ms(100), 2);
  if (is_async) {
    status = PollODBC(SQLDescribeCol, backoff, conn->hstmt, col_index,
                      col_ptr->name, kBufferLength, &col_ptr->name_len,
                      &col_ptr->data_type, &col_ptr->data_size,
                      &col_ptr->decimal_digits, &col_ptr->nullable);

  } else {
    status = SQLDescribeCol(conn->hstmt, col_index, col_ptr->name,
                            kBufferLength, &col_ptr->name_len,
                            &col_ptr->data_type, &col_ptr->data_size,
                            &col_ptr->decimal_digits, &col_ptr->nullable);
  }

  CheckError(status, "SQLDescribeCol", conn);
}

void BindCol(std::shared_ptr<ODBCHandles> const& conn,
             std::shared_ptr<Column> const& col_ptr, SQLUSMALLINT col_index) {
  if (col_ptr->data_len_ptr == nullptr) {
    col_ptr->data_len_ptr = &col_ptr->data_len;
  }
  auto status =
      SQLBindCol(conn->hstmt, col_index, col_ptr->data_type, col_ptr->data,
                 col_ptr->data_size, col_ptr->data_len_ptr);

  CheckError(status, "SQLBindCol", conn);
}

void BindColManually(std::shared_ptr<ODBCHandles> const& conn,
                     std::shared_ptr<Column> const& col_ptr,
                     SQLUSMALLINT col_index, bool use_ansi) {
  SQLHDESC ard_handle;  // Application row descriptor
  SQLRETURN status;
  if (use_ansi) {
    status = SQLGetStmtAttrA(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &ard_handle, 0,
                             nullptr);

  } else {
    status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_APP_ROW_DESC, &ard_handle, 0,
                            nullptr);
  }
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_APP_ROW_DESC)", conn, use_ansi);

  // Get the highest record
  SQLSMALLINT record_count;
  if (use_ansi) {
    status = SQLGetDescFieldA(ard_handle, 0, SQL_DESC_COUNT, &record_count,
                              SQL_IS_SMALLINT, nullptr);

  } else {
    status = SQLGetDescField(ard_handle, 0, SQL_DESC_COUNT, &record_count,
                             SQL_IS_SMALLINT, nullptr);
  }
  CheckError(status, "SQLGetDescField(SQL_DESC_COUNT)", conn, use_ansi);

  // Update the highest record
  if (col_index > record_count) {
    status = SQLSetDescField(ard_handle, 0, SQL_DESC_COUNT,
                             ToSqlPointer(col_index), SQL_IS_INTEGER);
    CheckError(status, "SQLGetStmtAttr(SQL_DESC_COUNT)", conn);
  }

  // Assign column attributes

  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_TYPE,
                           ToSqlPointer(col_ptr->data_type), SQL_IS_SMALLINT);
  CheckError(status, "SQLSetDescField(SQL_DESC_TYPE)", conn);
  status = SQLSetDescField(ard_handle, col_index, SQL_DESC_CONCISE_TYPE,
                           ToSqlPointer(col_ptr->data_type), SQL_IS_SMALLINT);
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

void BindStdColumns(std::shared_ptr<ODBCHandles> const& conn,
                    TestingDataBuffer* columns) {
  SQLRETURN status;

  columns[0].target_type = SQL_C_CHAR;
  status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(1),
                      columns[0].target_type, columns[0].target_value,
                      columns[0].buffer_length, &(columns[0].str_len));
  CheckError(status, "SQLBindCol", conn);

  columns[1].target_type = SQL_C_SBIGINT;
  status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(2),
                      columns[1].target_type, columns[1].target_value,
                      columns[1].buffer_length, &(columns[1].str_len));
  CheckError(status, "SQLBindCol", conn);

  columns[2].target_type = SQL_C_DOUBLE;
  status = SQLBindCol(conn->hstmt, static_cast<SQLUSMALLINT>(3),
                      columns[2].target_type, columns[2].target_value,
                      columns[2].buffer_length, &(columns[2].str_len));
  CheckError(status, "SQLBindCol", conn);
}

std::string FormatTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp,
                            bool is_type_datetime) {
  std::ostringstream ts;
  ts << std::setfill('0') << std::setw(4) << timestamp.year << "-"
     << std::setfill('0') << std::setw(2) << timestamp.month << "-"
     << std::setfill('0') << std::setw(2) << timestamp.day;
  ts << ((kIsBqDriver && is_type_datetime) ? "T" : " ");
  ts << std::setfill('0') << std::setw(2) << timestamp.hour << ":"
     << std::setfill('0') << std::setw(2) << timestamp.minute << ":"
     << std::setfill('0') << std::setw(2) << timestamp.second;
  if (kIsBqDriver) {
    if (timestamp.fraction != 0) {
      ts << "." << std::setfill('0') << std::left << std::setw(6)
         << timestamp.fraction;
    }
  } else {
    ts << "." << std::setfill('0') << std::left << std::setw(6)
       << timestamp.fraction;
  }
  return ts.str();
}

std::string FormatBinaryTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp,
                                  bool is_type_datetime) {
  std::ostringstream ts;
  ts << std::setfill('0') << std::setw(4) << timestamp.year << "-"
     << std::setfill('0') << std::setw(2) << timestamp.month << "-"
     << std::setfill('0') << std::setw(2) << timestamp.day;
  ts << ((kIsBqDriver && is_type_datetime) ? "T" : " ");
  ts << std::setfill('0') << std::setw(2) << timestamp.hour << ":"
     << std::setfill('0') << std::setw(2) << timestamp.minute << ":"
     << std::setfill('0') << std::setw(2) << timestamp.second;
  if (kIsBqDriver) {
    if (timestamp.fraction != 0) {
      ts << "." << std::setfill('0') << std::left << std::setw(9)
         << timestamp.fraction;
    }
  } else {
    ts << "." << std::setfill('0') << std::left << std::setw(9)
       << timestamp.fraction;
  }
  return ts.str();
}

std::string FormatRangeTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp,
                                 bool is_type_datetime) {
  std::ostringstream ts;
  ts << std::setfill('0') << std::setw(4) << timestamp.year << "-"
     << std::setfill('0') << std::setw(2) << timestamp.month << "-"
     << std::setfill('0') << std::setw(2) << timestamp.day;
  ts << ((kIsBqDriver && is_type_datetime) ? "T" : " ");
  ts << std::setfill('0') << std::setw(2) << timestamp.hour << ":"
     << std::setfill('0') << std::setw(2) << timestamp.minute << ":"
     << std::setfill('0') << std::setw(2) << timestamp.second;
  if (kIsBqDriver) {
    if (timestamp.fraction != 0) {
      ts << "." << std::setfill('0') << std::setw(6) << timestamp.fraction;
    }
  } else {
    ts << "." << std::setfill('0') << std::setw(6) << timestamp.fraction;
  }

  return ts.str();
}

std::string Utf16ToUtf8(std::wstring const& utf_16_str,
                        unsigned int code_page) {
  if (utf_16_str.empty()) {
    return std::string();
  }
#ifdef _WIN32
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int utf8Length = WideCharToMultiByte(code_page, 0, utf_16_str.c_str(), -1,
                                       NULL, 0, NULL, NULL);
  if (utf8Length == 0) {
    throw std::runtime_error(
        "Error determining buffer size while converting wstring to string");
  }
  if (sizeof(SQLWCHAR) == 2) {
    utf8Length = utf8Length * sizeof(SQLWCHAR);
  }
  std::string utf8Str(utf8Length, 0);
  // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
  int result = WideCharToMultiByte(code_page, 0, utf_16_str.c_str(), -1,
                                   &utf8Str[0], utf8Length, NULL, NULL);
  if (result == 0) {
    throw std::runtime_error("Error while converting wstring to string");
  }
  return utf8Str;
#else
  (void)code_page;
  iconv_t cd = iconv_open("UTF-8", kFromCode.c_str());
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
    return std::wstring();
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
  iconv_t cd = iconv_open(kFromCode.c_str(), "UTF-8");
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
    return std::string();
  }
  if (in_str_len == SQL_NTS || in_str_len == NULL) {
    in_str_len =
        static_cast<SQLINTEGER>(std::char_traits<SQLWCHAR>::length(in_str));
  }

  // Directly create a wide string
  std::wstring wstr(in_str, in_str + in_str_len);

  return Utf16ToUtf8(wstr);
}

std::string ConvertHexToChar(std::string const& hex_str) {
  std::vector<char> chars;
  for (size_t i = 0; i < hex_str.length(); i += 2) {
    std::string hex_pair = hex_str.substr(i, 2);
    char c = static_cast<char>(std::stoi(hex_pair, nullptr, 16));
    chars.push_back(c);
  }
  return std::string(chars.begin(), chars.end());
}

std::wstring ConvertHexToWchar(std::string const& hex_str) {
  std::wstring result;
  if (hex_str.length() % 2 != 0) {
    throw std::invalid_argument("Hex string must have an even length.");
  }
  for (size_t i = 0; i < hex_str.length(); i += 2) {
    std::string hex_byte = hex_str.substr(i, 2);
    unsigned int hex_value;
    std::stringstream(hex_byte) >> std::hex >> hex_value;
    result += static_cast<wchar_t>(hex_value);
  }
  return result;
}

SQLRETURN GetConvertedJsonData(std::shared_ptr<ODBCHandles> const& conn,
                               std::string const& query,
                               SQLSMALLINT target_c_type, SQLLEN* strlen_or_ind,
                               SQLPOINTER* data) {
  SQLRETURN status;
  // SQLPOINTER data[kBufferLength];
  char read_stmt[kBufferLength];
  StrToChar(read_stmt, query);

  status =
      SQLPrepare(conn->hstmt, reinterpret_cast<SQLCHAR*>(read_stmt), SQL_NTS);
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

SQLRETURN ExecWithPrepare(std::shared_ptr<ODBCHandles> const& conn,
                          std::string const& query) {
  SQLRETURN ret = SQLPrepare(
      conn->hstmt, reinterpret_cast<SQLCHAR*>(const_cast<char*>(query.c_str())),
      SQL_NTS);
  if (!SQL_SUCCEEDED(ret)) {
    return ret;
  }
  ret = SQLExecute(conn->hstmt);
  return ret;
}

void CleanupODBCHandles(ODBCHandles& conn, bool need_env_handle_freed) {
  if (conn.ard) {
    SQLFreeHandle(SQL_HANDLE_DESC, conn.ard);
    conn.ard = nullptr;
  }
  if (conn.ird) {
    SQLFreeHandle(SQL_HANDLE_DESC, conn.ird);
    conn.ird = nullptr;
  }
  if (conn.apd) {
    SQLFreeHandle(SQL_HANDLE_DESC, conn.apd);
    conn.apd = nullptr;
  }
  if (conn.ipd) {
    SQLFreeHandle(SQL_HANDLE_DESC, conn.ipd);
    conn.ipd = nullptr;
  }
  if (conn.hstmt) {
    SQLFreeHandle(SQL_HANDLE_STMT, conn.hstmt);
    conn.hstmt = nullptr;
  }
  if (conn.hdbc) {
    SQLDisconnect(conn.hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, conn.hdbc);
    conn.hdbc = nullptr;
  }
  if (need_env_handle_freed) {
    // On Windows, the Driver Manager automatically frees the environment handle
    // after the last connection handle is released
    if (conn.henv) {
      SQLFreeHandle(SQL_HANDLE_ENV, conn.henv);
      conn.henv = nullptr;
    }
  }
}

}  // namespace google::cloud::odbc_tests
