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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_COMMONS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_COMMONS_H

#include <iodbcext.h>
#include <locale.h>
#include <sql.h>
#include <sqlext.h>
#include <sqlucode.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <map>
// We need sorting functions 
#include <algorithm>

using namespace std;

namespace google {
namespace cloud {
namespace bigquery_odbc {

using Results = map<string, vector<string>>;

constexpr SQLSMALLINT kBufferLength = 512;

const string kDatasetName = "ODBCTESTDATASET";

struct ConnectionHandle {
  HENV henv;
  HDBC hdbc;
  HSTMT hstmt;
  bool connected;
  SQLCHAR outdsn[4096];
};

struct Column {
  SQLCHAR name[kBufferLength]; // Column name
  SQLSMALLINT name_len;
  SQLSMALLINT data_type;
  SQLCHAR * data; // Returned column data
  SQLCHAR * result_set; // Returned column data for a result set
  SQLULEN data_size; // max size of column data
  SQLLEN data_len; // size of data returned
  shared_ptr<SQLLEN[]> row_data_len; // row-wise size of returned data while fetching result sets
  SQLSMALLINT decimal_digits;
  SQLSMALLINT nullable;
};

struct ColumnMinimal {
  string name;
  SQLSMALLINT type;
};

using Schema = vector<ColumnMinimal>;

struct StdRow {
  string str_field;
  int int_field;
  float float_field;
};

using StdRows = vector<StdRow>;

inline bool str_comparison (string a, string b) { return a < b;}

inline SQLSMALLINT NumSqlChar(SQLCHAR * x) {
  return (sizeof(x) / sizeof(SQLCHAR));
}

// Copies a source <std::string> to a destination <char *>
inline void StrToChar(char * dest, string src) {
  strcpy(dest, src.c_str());
}

// Updates col_ptr->data_type to the C datatype macro to have consistency while reading results
void SqlToCdataTypes(shared_ptr<Column> col_ptr);

// If there was an error, gets description from SQLGetDiagRec and throws an error
void GetErrorDetails(const string api, shared_ptr<ConnectionHandle> conn);

inline string ToBqFieldType(SQLSMALLINT odbcType) {
  switch (odbcType) {
    case SQL_VARCHAR:
      return "STRING";
    case SQL_NUMERIC:
      return "NUMERIC";
    case SQL_INTEGER:
      return "INT64";
    case SQL_FLOAT:
    case SQL_DOUBLE:
      return "FLOAT64";
    case SQL_DATETIME:
      return "DATETIME";
    default:
      throw std::runtime_error("Invalid odbc data type: " + odbcType);
  }
}

void SqlToCdataTypes(shared_ptr<Column> col_ptr);

string getSchemaStr(Schema schema);

inline void CheckError(SQLRETURN status, const string api, shared_ptr<ConnectionHandle> conn);

void CreateTable(shared_ptr<ConnectionHandle> conn, string table_name, string schema);

void DropTable(shared_ptr<ConnectionHandle> conn, string table_name);

void ExecuteStatement(shared_ptr<ConnectionHandle> conn, char stmt[]);

void InsertIntoTable(shared_ptr<ConnectionHandle> conn, string table_name, StdRows rows);

// Executes the SQLDescribeCol API to initialize the Column struct
void DescribeCol(shared_ptr<ConnectionHandle> conn, shared_ptr<Column> col_ptr, SQLUSMALLINT col_index);

// Executes the BindCol API to bind the Column struct data buffers to the statement handle
void BindCol(shared_ptr<ConnectionHandle> conn, shared_ptr<Column> col_ptr, SQLUSMALLINT col_index);

}  // namespace bigquery_odbc
}  // namespace cloud
}  // namespace google

#endif  //CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_COMMONS_H
