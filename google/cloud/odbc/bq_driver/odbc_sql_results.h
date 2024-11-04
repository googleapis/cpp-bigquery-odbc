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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_SQL_RESULTS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_SQL_RESULTS_H

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// operations on ODBC SQL results:
//
// SQLGetDataInternal
// SQLNumResultColsInternal
// SQLFetchInternal
// SQLFetchScrollInternal
// SQLExtendedFetchInternal
// SQLColAttributeInternal
// SQLDescribeColInternal
// SQLBindColInternal
// SQLRowCountInternal
// SQLMoreResultsInternal
// SQLCloseCursorInternal
// SQLSetPosInternal
///////////////////////////////////////////////////////////

#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver {

// Implements the semantics for SQLBindCol ODBC API
// as per the ODBC 3.8 spec and the design doc.
//
// For details on the implementation semantics please refer to
// the following:
//
// Design Doc: http://goto.google.com/bq-odbc-sql-get-type-info-design
// ODBC Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindcol-function?view=sql-server-ver16
SQLRETURN SQLBindColInternal(SQLHSTMT statement_handle,
                             SQLUSMALLINT column_number,
                             SQLSMALLINT target_c_type, SQLPOINTER target_value,
                             SQLLEN target_value_buffer_len,
                             SQLLEN* target_value_str_len);

// Implements the semantics for SQLFetch ODBC API
// as per the ODBC 3.8 spec and the design doc.
//
// For details on the implementation semantics please refer to
// the following:
//
// Design Doc: http://goto.google.com/odbc-sqlfetch-design
// ODBC Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfetch-function?view=sql-server-ver16
SQLRETURN SQLFetchInternal(SQLHSTMT statement_handle);

// Implements the semantics for SQLFetch ODBC API
// as per the ODBC 3.8 spec and the design doc.
//
// For details on the implementation semantics please refer to
// the following:
//
// Design Doc: http://goto.google.com/bq-odbc-sql-get-type-info-design
// ODBC Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlfetch-function?view=sql-server-ver16
SQLRETURN SQLGetTypeInfoInternal(SQLHSTMT stmt_handle, SQLSMALLINT data_type);

SQLRETURN SQLNumResultColsInternal(SQLHSTMT statement_handle,
                                   SQLSMALLINT* column_count_ptr);

// Implements the semantics for SQLDescribeCol ODBC API
// as per the ODBC 3.8 spec and the design doc.
//
// For details on the implementation semantics please refer to
// the following:
//
// ODBC Spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqldescribecol-function?view=sql-server-ver16
SQLRETURN SQLDescribeColInternal(
    SQLHSTMT statement_handle, SQLUSMALLINT column_number, SQLCHAR* column_name,
    SQLSMALLINT column_name_buffer_len, SQLSMALLINT* column_name_le,
    SQLSMALLINT* column_sql_data_type, SQLULEN* column_size,
    SQLSMALLINT* decimal_digits, SQLSMALLINT* column_nullable);

SQLRETURN SQLColAttributeInternal(SQLHSTMT statement_handle,
                                  SQLUSMALLINT column_number,
                                  SQLUSMALLINT field_identifier,
                                  SQLPOINTER char_attr,
                                  SQLSMALLINT char_attr_buffer_len,
                                  SQLSMALLINT* char_attr_string_len,
                                  SQLLEN* numeric_attribute);

SQLRETURN SQLCloseCursorInternal(SQLHSTMT statement_handle);

SQLRETURN SQLRowCountInternal(SQLHSTMT statement_handle, SQLLEN* row_count);

// Implements the semantics for SQLFetch ODBC API
// as per the ODBC 3.8 spec and the design doc.
//
// For details on the implementation semantics please refer to
// the following:
//
// Design Doc: http://goto.google.com/odbc-sqlfetch-design
// ODBC Spec:
//https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdata-function?view=sql-server-ver16
SQLRETURN SQLGetDataInternal(SQLHSTMT statement_handle,
                             SQLUSMALLINT column_number, SQLSMALLINT target_c_type,
                             SQLPOINTER target_value,
                             SQLLEN target_value_buffer_len,
                             SQLLEN* target_value_string_len);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_SQL_RESULTS_H
