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
#include <gtest/gtest.h>
#include <algorithm>
#include <locale.h>
#include <map>
#include <memory>
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>

namespace google::cloud::odbc_tests {

using ::google::cloud::internal::ExponentialBackoffPolicy;
using Results = std::map<std::string, std::vector<std::string>>;

constexpr SQLSMALLINT kBufferLength = 1024;

std::string const kDatasetName = "ODBC_TEST_DATASET";

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
  SQLCHAR* data;        // Returned column data
  SQLCHAR* result_set;  // Returned column data for a result set
  SQLULEN data_size;    // max size of column data
  SQLLEN data_len;      // size of data returned
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
  int int_field;
  float float_field;
};

using StdRows = std::vector<StdRow>;

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
    case SQL_INTEGER:
      col_ptr->data_type = SQL_C_LONG;
      break;
    case SQL_DOUBLE:
      col_ptr->data_type = SQL_C_DOUBLE;
    case SQL_FLOAT:
      col_ptr->data_type = SQL_C_FLOAT;
      break;
    case SQL_VARCHAR:
    case SQL_C_CHAR:
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

  void Create(std::shared_ptr<ODBCHandles> conn, std::string schema_str);

  void Drop(std::shared_ptr<ODBCHandles> conn);

  void Insert(std::shared_ptr<ODBCHandles> conn, StdRows rows);

 private:
  std::string table_name_;
};

std::string GetRandomString(int len);

std::string getSchemaStr(Schema schema);

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
                       std::shared_ptr<ODBCHandles> conn);

void ExecuteStatement(std::shared_ptr<ODBCHandles> conn, char stmt[]);

// Executes the SQLDescribeCol API to initialize the Column struct
void DescribeCol(std::shared_ptr<ODBCHandles> conn,
                 std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index);

// Executes the BindCol API to bind the Column struct data buffers to the
// statement handle
void BindCol(std::shared_ptr<ODBCHandles> conn, std::shared_ptr<Column> col_ptr,
             SQLUSMALLINT col_index);

// The logic (internal implementation) of SQLBindCol
// Works only with String type (also with null strings)
void BindColManually(std::shared_ptr<ODBCHandles> conn,
                     std::shared_ptr<Column> col_ptr, SQLUSMALLINT col_index);

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_COMMONS_H
