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

// We need sorting functions
#include "google/cloud/internal/backoff_policy.h"
#include "google/cloud/internal/getenv.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <locale.h>
#include <map>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

namespace google::cloud::odbc_tests {

using ::google::cloud::internal::ExponentialBackoffPolicy;
using ::google::cloud::internal::GetEnv;
// Column-wise results
using Results = std::map<std::string, std::vector<std::string>>;
// Row-wise results
using RowWiseResults = std::vector<std::map<int, std::string>>;

#ifdef BQ_DRIVER_INTEGRATION_TESTS
bool const kIsBqDriver = true;
#else
bool const kIsBqDriver = false;
#endif

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
  SQLULEN data_size;    // max size of column data
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
  std::string name;
  SQLSMALLINT type;
};

using Schema = std::vector<ColumnMinimal>;

struct StdRow {
  std::string str_field;
  SQLBIGINT int_field;
  SQLDOUBLE float_field;
};

using StdRows = std::vector<StdRow>;

struct StdOdbcRow {
  SQLCHAR str_field[3 * kBufferLength];
  SQLLEN len_status_ind_str;
  SQLINTEGER int_field;
  SQLINTEGER len_status_ind_int;
  SQLDOUBLE float_field;
  SQLINTEGER len_status_ind_float;
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
    {SQL_C_CHAR, {SQL_C_CHAR, SQL_C_CHAR, 0, 1, 1, 0, 1}},
    {SQL_C_BINARY, {SQL_C_BINARY, SQL_C_BINARY, 0, 1, 1, 0, 1}},
    {SQL_C_NUMERIC, {SQL_C_NUMERIC, SQL_C_NUMERIC, 0, 38, 38, 0, 38}},
    {SQL_C_FLOAT, {SQL_C_FLOAT, SQL_C_FLOAT, 0, 24, 24, 0, 24}},
    {SQL_C_DOUBLE, {SQL_C_DOUBLE, SQL_C_DOUBLE, 0, 53, 53, 0, 53}},
    {SQL_C_WCHAR, {SQL_C_WCHAR, SQL_C_WCHAR, 0, 0, 0, 0, 0}},
    {SQL_C_STINYINT, {SQL_C_STINYINT, SQL_C_STINYINT, 0, 0, 0, 0, 0}},
    {SQL_C_SSHORT, {SQL_C_SSHORT, SQL_C_SSHORT, 0, 0, 0, 0, 0}},
    {SQL_C_SLONG, {SQL_C_SLONG, SQL_C_SLONG, 0, 0, 0, 0, 0}},
    {SQL_C_SBIGINT, {SQL_C_SBIGINT, SQL_C_SBIGINT, 0, 0, 0, 0, 0}},
    {SQL_C_BIT, {SQL_C_BIT, SQL_C_BIT, 0, 0, 0, 0, 0}},
    {SQL_C_GUID, {SQL_C_GUID, SQL_C_GUID, 0, 16, 16, 0, 16}},

    {SQL_C_TYPE_DATE,
     {SQL_DATETIME, SQL_C_TYPE_DATE, SQL_CODE_DATE, 0, 0, 0, 0}},
    {SQL_C_TYPE_TIME,
     {SQL_DATETIME, SQL_C_TYPE_TIME, SQL_CODE_TIME, 0, 0, 0, 0}},
    {SQL_C_TYPE_TIMESTAMP,
     {SQL_DATETIME, SQL_C_TYPE_TIMESTAMP, SQL_CODE_TIMESTAMP, 0, 6, 6, 0}},

    {SQL_C_INTERVAL_MONTH,
     {SQL_INTERVAL, SQL_C_INTERVAL_MONTH, SQL_CODE_MONTH, 2, 0, 0, 2}},
    {SQL_C_INTERVAL_YEAR,
     {SQL_INTERVAL, SQL_C_INTERVAL_YEAR, SQL_CODE_YEAR, 2, 0, 0, 2}},
    {SQL_C_INTERVAL_YEAR_TO_MONTH,
     {SQL_INTERVAL, SQL_C_INTERVAL_YEAR_TO_MONTH, SQL_CODE_YEAR_TO_MONTH, 2, 0,
      0, 2}},
    {SQL_C_INTERVAL_DAY,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY, SQL_CODE_DAY, 2, 0, 0, 2}},
    {SQL_C_INTERVAL_HOUR,
     {SQL_INTERVAL, SQL_C_INTERVAL_HOUR, SQL_CODE_HOUR, 2, 0, 0, 2}},
    {SQL_C_INTERVAL_MINUTE,
     {SQL_INTERVAL, SQL_C_INTERVAL_MINUTE, SQL_CODE_MINUTE, 2, 0, 0, 2}},
    {SQL_C_INTERVAL_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_SECOND, SQL_CODE_SECOND, 2, 6, 6, 2}},
    {SQL_C_INTERVAL_DAY_TO_HOUR,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY_TO_HOUR, SQL_CODE_DAY_TO_HOUR, 2, 0, 0,
      2}},
    {SQL_C_INTERVAL_DAY_TO_MINUTE,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY_TO_MINUTE, SQL_CODE_DAY_TO_MINUTE, 2, 0,
      0, 2}},
    {SQL_C_INTERVAL_DAY_TO_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_DAY_TO_SECOND, SQL_CODE_DAY_TO_SECOND, 2, 6,
      6, 2}},
    {SQL_C_INTERVAL_HOUR_TO_MINUTE,
     {SQL_INTERVAL, SQL_C_INTERVAL_HOUR_TO_MINUTE, SQL_CODE_HOUR_TO_MINUTE, 2,
      0, 0, 2}},
    {SQL_C_INTERVAL_HOUR_TO_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_HOUR_TO_SECOND, SQL_CODE_HOUR_TO_SECOND, 2,
      6, 6, 2}},
    {SQL_C_INTERVAL_MINUTE_TO_SECOND,
     {SQL_INTERVAL, SQL_C_INTERVAL_MINUTE_TO_SECOND, SQL_CODE_MINUTE_TO_SECOND,
      2, 6, 6, 2}},
};

inline bool str_comparison(std::string a, std::string b) { return a < b; }

inline SQLSMALLINT NumSqlChar(SQLCHAR* x) {
  return (sizeof(x) / sizeof(SQLCHAR));
}

// Copies a source <string> to a destination <char *>
inline void StrToChar(char* dest, std::string src) {
  strcpy(dest, src.c_str());
}

inline std::string ToBqFieldType(SQLSMALLINT odbc_data_type) {
  switch (odbc_data_type) {
    case SQL_VARCHAR:
      return "STRING";
    case SQL_NUMERIC:
      return "BIGNUMERIC";
    case SQL_BIGINT:
    case SQL_INTEGER:
      return "INT64";
    case SQL_FLOAT:
    case SQL_DOUBLE:
      return "FLOAT64";
    case SQL_DATETIME:
      return "DATETIME";
    default:
      throw std::runtime_error("Invalid odbc data type: " + odbc_data_type);
  }
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
    default:
      throw std::runtime_error("Invalid column data type: " +
                               col_ptr->data_type);
  }
}

class Table {
 public:
  Table(std::string table_name) { table_name_ = table_name; };

  void Create(std::shared_ptr<ODBCHandles> conn,
              std::string schema_str = "(Column INT64)", bool use_ansi = false);

  void CreateWithPrepare(std::shared_ptr<ODBCHandles> conn,
                         std::string schema_str);

  void Drop(std::shared_ptr<ODBCHandles> conn, bool use_ansi = false);

  void DropWithPrepare(std::shared_ptr<ODBCHandles> conn);

  void InsertData(std::shared_ptr<ODBCHandles> conn, StdRows rows,
                  bool use_ansi = false, bool use_sqlprepare = false);

  // This is used to insert strings into a table which only has a string column.
  // If `insert_index` is set to true, an additional column `index` will be
  // populated to order the values
  void InsertStrData(std::shared_ptr<ODBCHandles> conn,
                     std::vector<std::string> rows, bool insert_index = false);

  // This is used to insert 'double' into a table which only has a NUMERIC
  // column. If `insert_index` is set to true, an additional column `index` will
  // be populated to order the values
  void InsertNumericData(std::shared_ptr<ODBCHandles> conn,
                         std::vector<double> rows, bool insert_index = false);

  // This is used to insert 'SQLBIGINT' into a table which only has a INT64
  // column. If `insert_index` is set to true, an additional column `index` will
  // be populated to order the values
  void InsertInt64Data(std::shared_ptr<ODBCHandles> conn,
                       std::vector<SQLBIGINT> rows, bool insert_index = false);

 private:
  std::string table_name_;
};

std::string GetRandomString(int len);

std::string getSchemaStr(Schema schema);

void CreateTableDirect(std::shared_ptr<ODBCHandles> conn,
                       std::string create_table_schema, bool use_ansi = false);

void CreateTableWithPrepare(std::shared_ptr<ODBCHandles> conn,
                            std::string table_name, std::string schema);

void DropTableWithPrepare(std::shared_ptr<ODBCHandles> conn,
                          std::string table_name);

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

void ExecuteStatement(std::shared_ptr<ODBCHandles> conn, char stmt[],
                      bool use_ansi = false);

// Executes the SQLDescribeCol API to initialize the Column struct
void DescribeCol(std::shared_ptr<ODBCHandles> conn,
                 std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index,
                 bool use_ansi = false);

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

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_COMMONS_H
