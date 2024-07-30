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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_SQL_REQUESTS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_SQL_REQUESTS_H

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// operations on ODBC SQL request operations:
//
// SQLPrepareInternal
// SQLBindParameterInternal
// SQLDescribeParamInternal
// SQLGetCursorNameInternal
// SQLSetCursorNameInternal
// SQLExecuteInternal
// SQLExecDirectInternal
// SQLNativeSqlInternal
// SQLNumParamsInternal
// SQLParamDataInternal
// SQLPutDataInternal
// SQLBulkOperationsInternal
///////////////////////////////////////////////////////////

#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver {

SQLRETURN SQLBindParameterInternal(
    SQLHSTMT statement_handle, SQLUSMALLINT parameter_number,
    SQLSMALLINT input_output_type, SQLSMALLINT value_type,
    SQLSMALLINT parameter_type, SQLULEN column_size, SQLSMALLINT decimal_digits,
    SQLPOINTER parameter_value_ptr, SQLLEN buffer_length,
    SQLLEN* str_len_or_ind_ptr);

SQLRETURN SQLDescribeParamInternal(SQLHSTMT statement_handle,
                                   SQLUSMALLINT parameter_number,
                                   SQLSMALLINT* data_type_ptr,
                                   SQLULEN* parameter_size_ptr,
                                   SQLSMALLINT* decimal_digits_ptr,
                                   SQLSMALLINT* nullable_ptr);

SQLRETURN SQLNumParamsInternal(SQLHSTMT statement_handle,
                               SQLSMALLINT* param_count);

SQLRETURN SQLPrepareInternal(SQLHSTMT statement_handle,
                             SQLCHAR* in_statement_text,
                             SQLINTEGER in_text_length);

SQLRETURN SQLExecuteInternal(SQLHSTMT statement_handle);

SQLRETURN SQLSetCursorNameInternal(SQLHSTMT statement_handle,
                                   SQLCHAR const* cursor_name,
                                   SQLSMALLINT name_len);

SQLRETURN SQLGetCursorNameInternal(SQLHSTMT statement_handle,
                                   SQLCHAR* cursor_name, SQLSMALLINT buffer_len,
                                   SQLSMALLINT* name_string_len);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_SQL_REQUESTS_H
