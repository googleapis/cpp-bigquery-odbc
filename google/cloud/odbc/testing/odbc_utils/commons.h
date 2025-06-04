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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_COMMONS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_COMMONS_H

#include "google/cloud/odbc/testing/odbc_utils/types.h"
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
// We need sorting functions
#include <algorithm>
#include <codecvt>
#include <fstream>
#include <iomanip>
#include <locale>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

namespace google::cloud::odbc_tests {

using ::google::cloud::internal::ExponentialBackoffPolicy;
using ::google::cloud::internal::GetEnv;
// Column-wise results
using Results = std::map<std::string, std::vector<std::string>>;
using Row = std::map<int, std::string>;
// Row-wise results
using RowWiseResults = std::vector<Row>;

#ifdef BQ_DRIVER_INTEGRATION_TESTS
bool const kIsBqDriver = true;
#else
bool const kIsBqDriver = false;
#endif

bool const kIsUnixODBC =
    google::cloud::internal::GetEnv("UNIXODBC_INSTALLED").value_or("false") ==
    "true";

constexpr SQLSMALLINT kBufferLength = 1024;

std::string const kCatalogName = "bigquery-devtools-drivers";

inline std::string const GetDefaultTablePrefix() {
  return google::cloud::internal::GetEnv("CPP_BIGQUERY_ODBC_TEST_TABLE_PREFIX")
      .value_or("");
}

std::string const kTableNamePrefix = GetDefaultTablePrefix() + "_";
std::string const kDatasetName = "ODBC_TEST_DATASET";
std::string const kDatasetWithTablePrefix =
    kDatasetName + "." + kTableNamePrefix;

// Data Buffer used in demo and integration tests
struct TestingDataBuffer {
  SQLSMALLINT target_type;
  SQLCHAR target_value[512];
  SQLLEN buffer_length = 512;
  SQLLEN str_len;
};

// Stores information about the driver fetched from SQLGetInfo within the
// ODBCHandles. This is populated in the ODBCHandles after calling
// GetDriverInfo.
struct Metadata {
  std::string dsn_name;
  std::string project_id;
  std::string db_odbc_ver;
  std::string driver_name;
  std::string driver_odbc_ver;
  std::string driver_ver;
};

// Stores the various ODBC handles required to create a connection and execute
// statements.
struct ODBCHandles {
  HENV henv;
  HDBC hdbc;
  HSTMT hstmt;
  SQLHDESC ard;  // Application row descriptor
  SQLHDESC ird;  // Implementation row descriptor
  SQLHDESC apd;  // Application parameter descriptor
  SQLHDESC ipd;  // Implementation parameter descriptor
  bool connected;
  SQLCHAR outdsn[4096];
  Metadata metadata;
};

// The fields correspond to the ones set/retrieved by SQLBind/SQLDescribeCol.
struct Column {
  SQLCHAR name[kBufferLength];  // Column name
  SQLSMALLINT name_len;
  SQLSMALLINT data_type;
  SQLPOINTER data;  // Returned column data
  TestingDataBuffer data_buf;
  SQLCHAR* result_set;  // Returned column data for a result set
  std::unique_ptr<SQLCHAR[]> result_set_owner;  // Owns the buffer
  SQLULEN data_size;                            // max size of column data
  // We need to allocate space for data_len_ptr in case the caller doesn't
  // explicitly set that.
  SQLLEN data_len;  // size of data returned
  // Optionally, if the caller sets data_len_ptr, 'BindCol' will use bind this
  // buffer, otherwise it binds the address of 'data_len'
  SQLLEN* data_len_ptr;  // pointer to the buffer of for size of data returned
  std::shared_ptr<SQLLEN[]> row_data_len;  // row-wise size of returned data
                                           // while fetching result sets
  SQLSMALLINT decimal_digits;
  SQLSMALLINT nullable;
};

struct ColumnMinimal {
  std::string name;  // Name of the column
  std::string type;  // The BQ Data Type
};

using Schema = std::vector<ColumnMinimal>;

struct StdRow {
  std::string str_field;
  SQLBIGINT int_field;
  SQLDOUBLE float_field;
};

struct StdUnicodeRow {
  SQLBIGINT int_field;
  std::wstring str_field1;
  std::wstring str_field2;
};

using StdRows = std::vector<StdRow>;

using StdUnicodeRows = std::vector<StdUnicodeRow>;

struct BasicTestStruct {
  // The value that should be returned by SQLGetData if it succeeds
  std::string str_field;
  SQLBIGINT int_field;
  SQLDOUBLE float_field;
  SQL_TIMESTAMP_STRUCT timestamp;
  SQL_DATE_STRUCT date;
  SQL_TIME_STRUCT time;
  nlohmann::json json_field;
};

using StdAllTypesRows = std::vector<BasicTestStruct>;

struct StructBasicTestStruct {
  SQLBIGINT int_value;
  SQLDOUBLE double_value;
  std::string string_value;
  std::optional<std::vector<int>> int_array;
};

struct ArrayBasicTestStruct {
  SQLSMALLINT target_c_type;
  std::vector<SQLBIGINT> int_value;
  std::vector<SQLDOUBLE> double_value;
  std::vector<std::string> string_value;
  std::vector<StructBasicTestStruct> struct_value;
  SQLRETURN status;
};

using StdArrayRows = std::vector<ArrayBasicTestStruct>;

struct StdOdbcRow {
  SQLCHAR str_field[3 * kBufferLength];
  SQLLEN len_status_ind_str;
  SQLINTEGER int_field;
  // We should use SQLLEN instead of SQLINTEGER for length indicators
  SQLLEN len_status_ind_int;
  SQLDOUBLE float_field;
  SQLLEN len_status_ind_float;
};

struct ExpectedDescriptorConfig {
  SQLSMALLINT c_type;
  SQLSMALLINT concise_c_type;
  SQLSMALLINT desc_datetime_interval_code;
  SQLULEN desc_len;
  SQLSMALLINT desc_precision;
  SQLSMALLINT desc_scale;
  SQLINTEGER desc_datetime_precision;
};

static std::map<SQLSMALLINT, ExpectedDescriptorConfig> const kAppDescTestMap = {
    {SQL_C_CHAR, {SQL_C_CHAR, SQL_C_CHAR, 0, 1, 1, 0, 0}},
    {SQL_C_BINARY, {SQL_C_BINARY, SQL_C_BINARY, 0, 1, 1, 0, 0}},
    {SQL_C_NUMERIC, {SQL_C_NUMERIC, SQL_C_NUMERIC, 0, 38, 38, 0, 0}},
    {SQL_C_FLOAT, {SQL_C_FLOAT, SQL_C_FLOAT, 0, 24, 24, 0, 0}},
    {SQL_C_DOUBLE, {SQL_C_DOUBLE, SQL_C_DOUBLE, 0, 53, 53, 0, 0}},
    {SQL_C_WCHAR, {SQL_C_WCHAR, SQL_C_WCHAR, 0, 0, 0, 0, 0}},
    {SQL_C_STINYINT, {SQL_C_STINYINT, SQL_C_STINYINT, 0, 0, 0, 0, 0}},
    {SQL_C_SSHORT, {SQL_C_SSHORT, SQL_C_SSHORT, 0, 0, 0, 0, 0}},
    {SQL_C_SLONG, {SQL_C_SLONG, SQL_C_SLONG, 0, 0, 0, 0, 0}},
    {SQL_C_SBIGINT, {SQL_C_SBIGINT, SQL_C_SBIGINT, 0, 0, 0, 0, 0}},
    {SQL_C_BIT, {SQL_C_BIT, SQL_C_BIT, 0, 0, 0, 0, 0}},
    {SQL_C_GUID, {SQL_C_GUID, SQL_C_GUID, 0, 16, 16, 0, 0}},

    {SQL_C_TYPE_DATE,
     {SQL_DATETIME, SQL_C_TYPE_DATE, SQL_CODE_DATE, 0, 0, 0, 0}},
    {SQL_C_TYPE_TIME,
     {SQL_DATETIME, SQL_C_TYPE_TIME, SQL_CODE_TIME, 0, 0, 0, 0}},
    {SQL_C_TYPE_TIMESTAMP,
     {SQL_DATETIME, SQL_C_TYPE_TIMESTAMP, SQL_CODE_TIMESTAMP, 0, 6, 6, 0}},

    {SQL_C_INTERVAL_MONTH,
     {SQL_INTERVAL, SQL_C_INTERVAL_MONTH, SQL_CODE_MONTH, 0, 0, 0, 2}},
    {SQL_C_INTERVAL_YEAR,
     {SQL_INTERVAL, SQL_C_INTERVAL_YEAR, SQL_CODE_YEAR, 0, 0, 0, 2}},
    {SQL_C_INTERVAL_YEAR_TO_MONTH,
     {SQL_INTERVAL, SQL_C_INTERVAL_YEAR_TO_MONTH, SQL_CODE_YEAR_TO_MONTH, 0, 0,
      0, 2}},
    {SQL_C_INTERVAL_DAY,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY, SQL_CODE_DAY, 0, 0, 0, 2}},
    {SQL_C_INTERVAL_HOUR,
     {SQL_INTERVAL, SQL_C_INTERVAL_HOUR, SQL_CODE_HOUR, 0, 0, 0, 2}},
    {SQL_C_INTERVAL_MINUTE,
     {SQL_INTERVAL, SQL_C_INTERVAL_MINUTE, SQL_CODE_MINUTE, 0, 0, 0, 2}},
    {SQL_C_INTERVAL_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_SECOND, SQL_CODE_SECOND, 0, 6, 6, 2}},
    {SQL_C_INTERVAL_DAY_TO_HOUR,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY_TO_HOUR, SQL_CODE_DAY_TO_HOUR, 0, 0, 0,
      2}},
    {SQL_C_INTERVAL_DAY_TO_MINUTE,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY_TO_MINUTE, SQL_CODE_DAY_TO_MINUTE, 0, 0,
      0, 2}},
    {SQL_C_INTERVAL_DAY_TO_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY_TO_SECOND, SQL_CODE_DAY_TO_SECOND, 0, 6,
      6, 2}},
    {SQL_C_INTERVAL_HOUR_TO_MINUTE,
     {SQL_INTERVAL, SQL_C_INTERVAL_HOUR_TO_MINUTE, SQL_CODE_HOUR_TO_MINUTE, 0,
      0, 0, 2}},
    {SQL_C_INTERVAL_HOUR_TO_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_HOUR_TO_SECOND, SQL_CODE_HOUR_TO_SECOND, 0,
      6, 6, 2}},
    {SQL_C_INTERVAL_MINUTE_TO_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_MINUTE_TO_SECOND, SQL_CODE_MINUTE_TO_SECOND,
      0, 6, 6, 2}},
};

static Schema const kStdSchema = {
    {"Str2", "STRING"},
    {"Int2", "INT64"},
    {"Float2", "FLOAT64"},
};

static Schema const kFullSchema = {
    {"IntField", "INT64"},
    {"BoolField", "BOOL"},
    {"DateField", "DATE"},
    {"FloatField", "FLOAT64"},
    {"TimeField", "TIME"},
    {"TimeStampField", "TIMESTAMP"},
    {"DatetimeField", "DATETIME"},
    {"BytesField", "BYTES"},
    {"Bytes7Field", "BYTES(7)"},
    {"StrField", "STRING"},
    {"Str7Field", "STRING(7)"},
    {"ArrayField", "ARRAY<INT64>"},
    {"StructField", "STRUCT<x STRING(10)>"},
    {"IntervalField", "INTERVAL"},
    {"JsonField", "JSON"},
    {"GeoField", "GEOGRAPHY"},
    {"NumericField", "NUMERIC"},
    {"BignumericField", "BIGNUMERIC"},
    /*
    TODO(b/353804301): See if we should add range columns here:
    example:
      {"RangeDate", "RANGE<DATE>"},
      {"RangeDateTime", "RANGE<DATETIME>"},
      {"RangeTimestamp", "RANGE<TIMESTAMP>"},
    */
};

// Struct to store test data for date range validation
struct RangeDateStruct {
  // Target C type
  SQLSMALLINT target_c_type;
  // Range of date values (start and end) where result value is stored
  std::pair<SQL_DATE_STRUCT, SQL_DATE_STRUCT> value;
  // The status that should be returned for this C Type
  SQLRETURN status;
};

struct RangeTimeStampStruct {
  // Target C type
  SQLSMALLINT target_c_type;
  // Range of timestamp values (start and end) where result value is stored
  std::pair<SQL_TIMESTAMP_STRUCT, SQL_TIMESTAMP_STRUCT> value;
  // The status that should be returned for this C Type
  SQLRETURN status;
};

inline bool str_comparison(std::string a, std::string b) { return a < b; }

inline bool isNumeric(std::string const& str) {
  try {
    std::stod(str);
    return true;
  } catch (std::exception const& e) {
    return false;
  }
}

inline SQLSMALLINT NumSqlChar(SQLCHAR* x) {
  return (sizeof(x) / sizeof(SQLCHAR));
}

// Copies a source <string> to a destination <char *>
inline void StrToChar(char* dest, std::string src) {
  strcpy(dest, src.c_str());
}

inline std::wstring ToWStr(std::string const& str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.from_bytes(str);
}

inline std::string WStrToStr(std::wstring const& wstr) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(wstr);
}

inline SQL_INTERVAL_STRUCT MakeYearMonthInterval(SQLINTERVAL type,
                                                 SQLUINTEGER year,
                                                 SQLUINTEGER month) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_type = type;
  interval.interval_sign = 0;
  interval.intval.year_month.year = year;
  interval.intval.year_month.month = month;
  return interval;
}

inline SQL_INTERVAL_STRUCT MakeDaySecondInterval(
    SQLINTERVAL type, SQLUINTEGER day, SQLUINTEGER hour, SQLUINTEGER minute,
    SQLUINTEGER second, SQLUINTEGER fraction) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_type = type;
  interval.interval_sign = 0;
  interval.intval.day_second.day = day;
  interval.intval.day_second.hour = hour;
  interval.intval.day_second.minute = minute;
  interval.intval.day_second.second = second;
  interval.intval.day_second.fraction = fraction;
  return interval;
}

// Updates col_ptr->data_type to the C datatype macro to have consistency while
// reading results
inline void SqlToCdataTypes(std::shared_ptr<Column> col_ptr) {
  switch (col_ptr->data_type) {
    case SQL_BIGINT:
      col_ptr->data_type = SQL_C_SBIGINT;
      break;
    case SQL_INTEGER:
      col_ptr->data_type = SQL_C_SLONG;
      break;
    case SQL_DOUBLE:
      col_ptr->data_type = SQL_C_DOUBLE;
      break;
    case SQL_FLOAT:
      col_ptr->data_type = SQL_C_FLOAT;
      break;
    case SQL_VARCHAR:
    case SQL_CHAR:
      col_ptr->data_type = SQL_C_CHAR;
      break;
    case SQL_TYPE_TIMESTAMP:
      col_ptr->data_type = SQL_C_TYPE_TIMESTAMP;
      break;
    case SQL_TYPE_TIME:
      col_ptr->data_type = SQL_C_TYPE_TIME;
      break;
    case SQL_TYPE_DATE:
      col_ptr->data_type = SQL_C_TYPE_DATE;
      break;
    case SQL_INTERVAL_YEAR:
      col_ptr->data_type = SQL_C_INTERVAL_YEAR;
      break;
    case SQL_INTERVAL_MONTH:
      col_ptr->data_type = SQL_C_INTERVAL_MONTH;
      break;
    case SQL_INTERVAL_DAY:
      col_ptr->data_type = SQL_C_INTERVAL_DAY;
      break;
    case SQL_INTERVAL_HOUR:
      col_ptr->data_type = SQL_C_INTERVAL_HOUR;
      break;
    case SQL_INTERVAL_MINUTE:
      col_ptr->data_type = SQL_C_INTERVAL_MINUTE;
      break;
    case SQL_INTERVAL_SECOND:
      col_ptr->data_type = SQL_C_INTERVAL_SECOND;
      break;
    case SQL_INTERVAL_YEAR_TO_MONTH:
      col_ptr->data_type = SQL_C_INTERVAL_YEAR_TO_MONTH;
      break;
    case SQL_INTERVAL_DAY_TO_HOUR:
      col_ptr->data_type = SQL_C_INTERVAL_DAY_TO_HOUR;
      break;
    case SQL_INTERVAL_DAY_TO_MINUTE:
      col_ptr->data_type = SQL_C_INTERVAL_DAY_TO_MINUTE;
      break;
    case SQL_INTERVAL_DAY_TO_SECOND:
      col_ptr->data_type = SQL_C_INTERVAL_DAY_TO_SECOND;
      break;
    case SQL_INTERVAL_HOUR_TO_MINUTE:
      col_ptr->data_type = SQL_C_INTERVAL_HOUR_TO_MINUTE;
      break;
    case SQL_INTERVAL_HOUR_TO_SECOND:
      col_ptr->data_type = SQL_C_INTERVAL_HOUR_TO_SECOND;
      break;
    case SQL_INTERVAL_MINUTE_TO_SECOND:
      col_ptr->data_type = SQL_C_INTERVAL_MINUTE_TO_SECOND;
      break;
    default:
      throw std::runtime_error("Invalid column data type: " +
                               col_ptr->data_type);
  }
}

std::string GetInsertionString(std::string table_name, StdRows rows);

std::string GetAllTypeInsertionString(std::string const& table_name,
                                      StdAllTypesRows const& rows);

class Table {
 public:
  Table() = default;
  Table(std::string table_name) {
    table_name_ = table_name;
    wtable_name_ = ToWStr(table_name_);
  };

  Table(std::wstring wtable_name) {
    table_name_ = WStrToStr(wtable_name);
    wtable_name_ = wtable_name;
  };

  void Create(std::shared_ptr<ODBCHandles> conn,
              std::string schema_str = "(Column INT64)", bool use_ansi = false);

  // Uses SQLExecDirectW
  void CreateW(std::shared_ptr<ODBCHandles> conn, std::wstring schema_str);

  void CreateWithPrepare(std::shared_ptr<ODBCHandles> conn,
                         std::string schema_str);

  void Drop(std::shared_ptr<ODBCHandles> conn, bool use_ansi = false);

  // Uses SQLExecDirectW
  void DropW(std::shared_ptr<ODBCHandles> conn);

  RowWiseResults Fetch(std::shared_ptr<ODBCHandles> conn,
                       std::string query = "");

  void DropWithPrepare(std::shared_ptr<ODBCHandles> conn);

  void InsertData(std::shared_ptr<ODBCHandles> conn, StdRows rows,
                  bool use_ansi = false, bool use_sqlprepare = false);

  void InsertUnicodeData(std::shared_ptr<ODBCHandles> conn,
                         StdUnicodeRows rows);

  void InsertAllData(std::shared_ptr<ODBCHandles> conn,
                     StdAllTypesRows const& rows);

  // This is used to insert strings into a table which only has a string column.
  // If `insert_index` is set to true, an additional column `index` will be
  // populated to order the values
  void InsertStrData(std::shared_ptr<ODBCHandles> conn,
                     std::vector<std::string> rows, bool insert_index = false);

  // This is used to insert 'double' into a table which only has a NUMERIC
  // column. If `insert_index` is set to true, an additional column `index` will
  // be populated to order the values
  void InsertNumericData(std::shared_ptr<ODBCHandles> conn,
                         std::vector<std::string> rows,
                         bool insert_index = false);

  // This is used to insert 'SQLBIGINT' into a table which only has a INT64
  // column. If `insert_index` is set to true, an additional column `index` will
  // be populated to order the values
  void InsertInt64Data(std::shared_ptr<ODBCHandles> conn,
                       std::vector<SQLBIGINT> rows, bool insert_index = false);

  void InsertTimestampData(std::shared_ptr<ODBCHandles> conn,
                           std::vector<SQL_TIMESTAMP_STRUCT> rows,
                           bool insert_index);

  void InsertArrayData(std::shared_ptr<ODBCHandles> conn,
                       StdArrayRows array_rows, bool insert_index);

  void InsertDateData(std::shared_ptr<ODBCHandles> conn,
                      std::vector<SQL_DATE_STRUCT> rows, bool insert_index);

  void InsertTimeData(std::shared_ptr<ODBCHandles> conn,
                      std::vector<SQL_TIME_STRUCT> rows, bool insert_index);

  void InsertIntervalData(std::shared_ptr<ODBCHandles> conn,
                          std::vector<SQL_INTERVAL_STRUCT> rows);

  // This is used to insert json darainto a table which only has a string
  // column.
  void InsertJsonData(std::shared_ptr<ODBCHandles> conn,
                      std::vector<nlohmann::json> rows,
                      bool insert_index = false);

  void InsertGeographyData(
      std::shared_ptr<ODBCHandles> conn,
      std::vector<std::pair<std::string, std::string>> data, bool insert_index);

  void InsertBooleanData(std::shared_ptr<ODBCHandles> conn,
                         std::vector<uint8_t> rows, bool insert_index);

  void InsertBytesData(std::shared_ptr<ODBCHandles> conn,
                       std::vector<std::vector<SQLCHAR>> const& bytes_data,
                       bool use_prepared_stmt);

  void InsertStructData(std::shared_ptr<ODBCHandles> conn,
                        std::vector<StructBasicTestStruct> const& rows,
                        bool insert_index);

  void InsertRangeTimeStampData(
      std::shared_ptr<ODBCHandles> conn,
      std::vector<std::pair<SQL_TIMESTAMP_STRUCT, SQL_TIMESTAMP_STRUCT>> const&
          data,
      bool insert_index, std::string datatype);

  void InsertRangeDateData(
      std::shared_ptr<ODBCHandles> conn,
      std::vector<std::pair<SQL_DATE_STRUCT, SQL_DATE_STRUCT>> rows,
      bool insert_index);

 private:
  std::string table_name_;
  std::wstring wtable_name_;
};

class Procedure {
 public:
  Procedure() = default;
  Procedure(std::string procedure_name) {
    procedure_name_ = procedure_name;
    wprocedure_name_ = ToWStr(procedure_name_);
  };

  Procedure(std::wstring wprocedure_name) {
    procedure_name_ = WStrToStr(wprocedure_name);
    wprocedure_name_ = wprocedure_name;
  };
  void Drop(std::shared_ptr<ODBCHandles> conn, bool use_ansi = false);

  void DropWithPrepare(std::shared_ptr<ODBCHandles> conn);

 private:
  std::string procedure_name_;
  std::wstring wprocedure_name_;
};

std::string GetRandomString(int len);

std::string getSchemaStr(Schema schema);

std::string FormatDate(const SQL_DATE_STRUCT& date);

std::string FormatTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp);

std::string FormatBinaryTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp);

std::string FormatTimetoString(const SQL_TIME_STRUCT& time);

std::string FormatTimetoString(const SQL_TIME_STRUCT& time);

std::string FormatRangeTimeStamp(const SQL_TIMESTAMP_STRUCT& timestamp);

std::string GetIntervalTypeStr(const SQLINTERVAL type);

std::string FormatIntervalString(const SQL_INTERVAL_STRUCT interval);

std::string SQLNumericToString(const SQL_NUMERIC_STRUCT& numeric);

SQL_NUMERIC_STRUCT ConvertStringToNumeric(std::string const& numeric_str);

void CreateTableDirect(std::shared_ptr<ODBCHandles> conn,
                       std::string create_table_schema, bool use_ansi = false);

void CreateTableWithPrepare(std::shared_ptr<ODBCHandles> conn,
                            std::string table_name, std::string schema);

void DropTableWithPrepare(std::shared_ptr<ODBCHandles> conn,
                          std::string table_name);

void DropProcedureWithPrepare(std::shared_ptr<ODBCHandles> conn,
                              std::string procedure_name);

// If SQL_ASYNC_ENABLE_ON, this function can be used to run a ODBC API till the
// status is not SQL_STILL_EXECUTING
template <typename Func, typename... Args>
SQLRETURN PollODBC(Func odbc_api, ExponentialBackoffPolicy& backoff,
                   Args&&... args) {
  SQLRETURN status;
  while (1) {
    status = odbc_api(std::forward<Args>(args)...);
    if (status == SQL_STILL_EXECUTING) {
      std::this_thread::sleep_for(backoff.OnCompletion());
      continue;
    }
    return status;
  }
}

// If there was an error, gets description from SQLGetDiagRec and throws an
// error
inline void CheckError(SQLRETURN status, std::string const api,
                       std::shared_ptr<ODBCHandles> conn,
                       bool use_ansi = false);

void GetErrorDetails(std::string const& api, SQLHANDLE handle,
                     SQLSMALLINT handle_type, bool use_ansi = false);

// Used for getting api error details after executing a SQLCancel operation.
SQLRETURN GetCancelErrorDetails(std::string const& api, SQLHANDLE handle,
                                std::string& error_details);

void ExecuteStatement(std::shared_ptr<ODBCHandles> conn, char stmt[],
                      bool use_ansi = false);

// Executes the SQLDescribeCol API to initialize the Column struct
void DescribeCol(std::shared_ptr<ODBCHandles> conn,
                 std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index,
                 bool is_async = false);

// Executes the BindCol API to bind the Column struct data buffers to the
// statement handle
void BindCol(std::shared_ptr<ODBCHandles> conn, std::shared_ptr<Column> col_ptr,
             SQLUSMALLINT col_index);

// The logic (internal implementation) of SQLBindCol
// Works only with String type (also with null strings)
void BindColManually(std::shared_ptr<ODBCHandles> conn,
                     std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index,
                     bool use_ansi = false);

// Binds buffers TestingDataBuffer for StdRow type of data
void BindStdColumns(std::shared_ptr<ODBCHandles> conn,
                    TestingDataBuffer* columns);

std::string Utf16ToUtf8(std::wstring const& utf_16_str,
                        unsigned int code_page = 65001 /* UTF-8 */);

std::wstring Utf8ToUtf16(std::string const& utf_8_str);

std::string ConvertSQLWCHARToString(SQLWCHAR* in_str, SQLINTEGER in_str_len);

std::string ConvertHexToChar(std::string const& hex_str);

std::wstring ConvertHexToWchar(std::string const& hex_str);

SQLRETURN GetConvertedJsonData(std::shared_ptr<ODBCHandles> conn,
                               std::string query, SQLSMALLINT target_c_type,
                               SQLLEN* strlen_or_ind, SQLPOINTER* data);

SQLRETURN ExecWithPrepare(std::shared_ptr<ODBCHandles> conn,
                          std::string const& query);
}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_COMMONS_H
