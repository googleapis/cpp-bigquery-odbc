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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_TRACE_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_TRACE_H

#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/status_or.h"
#include <map>
#include <string>

/////////////////////////////////////////////////////////////
// Defines the functions related to tracing entry and exit
// of all ODBC APIs. Tracing includes tracing of parameters,
// API names and return codes.
/////////////////////////////////////////////////////////////

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::TraceOptions;

void TraceFunctionEntry_SQLAllocHandle(SQLSMALLINT handle_type,
                                       SQLHANDLE input_handle,
                                       SQLHANDLE* output_handle,
                                       TraceOptions& opts);
void TraceFunctionExit_SQLAllocHandle(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLDriverConnect(
    SQLHDBC connection_handle, SQLHWND window_handle,
    SQLCHAR* in_connection_str, SQLSMALLINT in_connection_str_len,
    SQLCHAR* out_conn_str, SQLSMALLINT out_conn_str_buf_len,
    SQLSMALLINT* out_conn_str_len, SQLUSMALLINT driver_completion,
    TraceOptions& opts);
void TraceFunctionExit_SQLDriverConnect(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLDriverConnectW(
    SQLHDBC connection_handle, SQLHWND window_handle,
    SQLWCHAR* in_connection_str, SQLSMALLINT in_connection_str_len,
    SQLWCHAR* out_conn_str, SQLSMALLINT out_conn_str_buf_len,
    SQLSMALLINT* out_conn_str_len, SQLUSMALLINT driver_completion,
    TraceOptions& opts);
void TraceFunctionExit_SQLDriverConnectW(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLBrowseConnect(SQLHDBC connection_handle,
                                         SQLCHAR* in_conn_str,
                                         SQLSMALLINT in_conn_str_len,
                                         SQLCHAR* out_conn_str,
                                         SQLSMALLINT out_conn_str_buf_len,
                                         SQLSMALLINT* out_conn_str_len,
                                         TraceOptions& opts);
void TraceFunctionExit_SQLBrowseConnect(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLBrowseConnectW(SQLHDBC connection_handle,
                                          SQLWCHAR* in_conn_str,
                                          SQLSMALLINT in_conn_str_len,
                                          SQLWCHAR* out_conn_str,
                                          SQLSMALLINT out_conn_str_buf_len,
                                          SQLSMALLINT* out_conn_str_len,
                                          TraceOptions& opts);
void TraceFunctionExit_SQLBrowseConnectW(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLConnect(
    SQLHDBC connection_handle, SQLCHAR* server_name,
    SQLSMALLINT server_name_len, SQLCHAR* user_name, SQLSMALLINT user_name_len,
    const SQLCHAR* auth_str, SQLSMALLINT auth_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLConnect(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLConnectW(
    SQLHDBC connection_handle, SQLWCHAR* server_name,
    SQLSMALLINT server_name_len, SQLWCHAR* user_name, SQLSMALLINT user_name_len,
    const SQLWCHAR* auth_str, SQLSMALLINT auth_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLConnectW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetInfo(SQLHDBC connection_handle,
                                   SQLUSMALLINT info_type,
                                   SQLPOINTER info_value,
                                   SQLSMALLINT info_value_buf_len,
                                   SQLSMALLINT* info_value_str_len,
                                   TraceOptions& opts);
void TraceFunctionExit_SQLGetInfo(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetInfoW(SQLHDBC connection_handle,
                                    SQLUSMALLINT info_type,
                                    SQLPOINTER info_value,
                                    SQLSMALLINT info_value_buf_len,
                                    SQLSMALLINT* info_value_str_len,
                                    TraceOptions& opts);
void TraceFunctionExit_SQLGetInfoW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetFunctions(SQLHDBC connection_handle,
                                        SQLUSMALLINT fn_id,
                                        SQLUSMALLINT* supported_fn,
                                        TraceOptions& opts);
void TraceFunctionExit_SQLGetFunctions(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetTypeInfo(SQLHSTMT statement_handle,
                                       SQLSMALLINT data_type,
                                       TraceOptions& opts);
void TraceFunctionExit_SQLGetTypeInfo(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetConnectAttr(SQLHDBC connection_handle,
                                          SQLINTEGER attr, SQLPOINTER value,
                                          SQLINTEGER value_str_len,
                                          TraceOptions& opts);
void TraceFunctionExit_SQLSetConnectAttr(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLSetConnectAttrW(SQLHDBC connection_handle,
                                           SQLINTEGER attr, SQLPOINTER value,
                                           SQLINTEGER value_str_len,
                                           TraceOptions& opts);
void TraceFunctionExit_SQLSetConnectAttrW(SQLRETURN ret_code,
                                          TraceOptions& opts);

void TraceFunctionEntry_SQLGetConnectAttr(SQLHDBC connection_handle,
                                          SQLINTEGER attr, SQLPOINTER value,
                                          SQLINTEGER value_buf_len,
                                          SQLINTEGER* value_str_len,
                                          TraceOptions& opts);
void TraceFunctionExit_SQLGetConnectAttr(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLGetConnectAttrW(SQLHDBC connection_handle,
                                           SQLINTEGER attr, SQLPOINTER value,
                                           SQLINTEGER value_buf_len,
                                           SQLINTEGER* value_str_len,
                                           TraceOptions& opts);
void TraceFunctionExit_SQLGetConnectAttrW(SQLRETURN ret_code,
                                          TraceOptions& opts);

void TraceFunctionEntry_SQLSetStmtAttr(SQLHSTMT statement_handle,
                                       SQLINTEGER attr, SQLPOINTER value,
                                       SQLINTEGER value_str_len,
                                       TraceOptions& opts);
void TraceFunctionExit_SQLSetStmtAttr(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetStmtAttrW(SQLHSTMT statement_handle,
                                        SQLINTEGER attr, SQLPOINTER value,
                                        SQLINTEGER value_str_len,
                                        TraceOptions& opts);
void TraceFunctionExit_SQLSetStmtAttrW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetStmtAttr(SQLHSTMT statement_handle,
                                       SQLINTEGER attr, SQLPOINTER value,
                                       SQLINTEGER value_buf_len,
                                       SQLINTEGER* value_str_len,
                                       TraceOptions& opts);
void TraceFunctionExit_SQLGetStmtAttr(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetStmtAttrW(SQLHSTMT statement_handle,
                                        SQLINTEGER attr, SQLPOINTER value,
                                        SQLINTEGER value_buf_len,
                                        SQLINTEGER* value_str_len,
                                        TraceOptions& opts);
void TraceFunctionExit_SQLGetStmtAttrW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetEnvAttr(SQLHENV env_handle, SQLINTEGER attr,
                                      SQLPOINTER value,
                                      SQLINTEGER value_str_len,
                                      TraceOptions& opts);
void TraceFunctionExit_SQLSetEnvAttr(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetEnvAttr(SQLHENV env_handle, SQLINTEGER attr,
                                      SQLPOINTER value,
                                      SQLINTEGER value_buf_len,
                                      SQLINTEGER* value_str_len,
                                      TraceOptions& opts);
void TraceFunctionExit_SQLGetEnvAttr(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDescField(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_id,
    SQLPOINTER out_desc_val, SQLINTEGER out_desc_val_buf_len,
    SQLINTEGER* out_desc_val_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLGetDescField(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDescFieldW(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_id,
    SQLPOINTER out_desc_val, SQLINTEGER out_desc_val_buf_len,
    SQLINTEGER* out_desc_val_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLGetDescFieldW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDescRec(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLCHAR* name,
    SQLSMALLINT name_buf_len, SQLSMALLINT* name_str_len, SQLSMALLINT* desc_type,
    SQLSMALLINT* desc_sub_type, SQLLEN* desc_oct_len, SQLSMALLINT* desc_prec,
    SQLSMALLINT* desc_sc, SQLSMALLINT* nullable, TraceOptions& opts);
void TraceFunctionExit_SQLGetDescRec(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDescRecW(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLWCHAR* name,
    SQLSMALLINT name_buf_len, SQLSMALLINT* name_str_len, SQLSMALLINT* desc_type,
    SQLSMALLINT* desc_sub_type, SQLLEN* desc_oct_len, SQLSMALLINT* desc_prec,
    SQLSMALLINT* desc_sc, SQLSMALLINT* nullable, TraceOptions& opts);
void TraceFunctionExit_SQLGetDescRecW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetDescField(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_identifier,
    SQLPOINTER desc_val, SQLINTEGER desc_val_buf_len, TraceOptions& opts);
void TraceFunctionExit_SQLSetDescField(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetDescFieldW(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT field_identifier,
    SQLPOINTER desc_val, SQLINTEGER desc_val_buf_len, TraceOptions& opts);
void TraceFunctionExit_SQLSetDescFieldW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetDescRec(
    SQLHDESC desc_handle, SQLSMALLINT rec_no, SQLSMALLINT desc_type,
    SQLSMALLINT desc_sub_type, SQLLEN desc_oct_len, SQLSMALLINT desc_prec,
    SQLSMALLINT desc_sc, SQLPOINTER desc_data, SQLLEN* desc_oct_len_ptr,
    SQLLEN* desc_ind, TraceOptions& opts);
void TraceFunctionExit_SQLSetDescRec(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLCopyDesc(SQLHDESC src_desc_handle,
                                    SQLHDESC target_desc_handle,
                                    TraceOptions& opts);
void TraceFunctionExit_SQLCopyDesc(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLPrepare(SQLHSTMT statement_handle, SQLCHAR* stmt_txt,
                                   SQLINTEGER stmt_txt_len, TraceOptions& opts);
void TraceFunctionExit_SQLPrepare(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLPrepareW(SQLHSTMT statement_handle,
                                    SQLWCHAR* stmt_txt, SQLINTEGER stmt_txt_len,
                                    TraceOptions& opts);
void TraceFunctionExit_SQLPrepareW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLBindParameter(
    SQLHSTMT statement_handle, SQLUSMALLINT param_num, SQLSMALLINT param_type,
    SQLSMALLINT param_c_type, SQLSMALLINT param_sql_type, SQLULEN param_col_sz,
    SQLSMALLINT param_scale, SQLPOINTER param_data_val,
    SQLLEN param_data_val_buf_len, SQLLEN* param_data_val_str_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLBindParameter(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetCursorName(SQLHSTMT statement_handle,
                                         SQLCHAR* cur_name,
                                         SQLSMALLINT cur_name_buf_len,
                                         SQLSMALLINT* cur_name_str_len,
                                         TraceOptions& opts);
void TraceFunctionExit_SQLGetCursorName(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetCursorNameW(SQLHSTMT statement_handle,
                                          SQLWCHAR* cur_name,
                                          SQLSMALLINT cur_name_buf_len,
                                          SQLSMALLINT* cur_name_str_len,
                                          TraceOptions& opts);
void TraceFunctionExit_SQLGetCursorNameW(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLSetCursorName(SQLHSTMT statement_handle,
                                         SQLCHAR* cur_name,
                                         SQLSMALLINT cur_name_len,
                                         TraceOptions& opts);
void TraceFunctionExit_SQLSetCursorName(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetCursorNameW(SQLHSTMT statement_handle,
                                          SQLWCHAR* cur_name,
                                          SQLSMALLINT cur_name_len,
                                          TraceOptions& opts);
void TraceFunctionExit_SQLSetCursorNameW(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLExecute(SQLHSTMT statement_handle,
                                   TraceOptions& opts);
void TraceFunctionExit_SQLExecute(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLExecDirect(SQLHSTMT statement_handle,
                                      SQLCHAR* stmt_txt,
                                      SQLINTEGER stmt_txt_len,
                                      TraceOptions& opts);
void TraceFunctionExit_SQLExecDirect(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLExecDirectW(SQLHSTMT statement_handle,
                                       SQLWCHAR* stmt_txt,
                                       SQLINTEGER stmt_txt_len,
                                       TraceOptions& opts);
void TraceFunctionExit_SQLExecDirectW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLNativeSql(
    SQLHDBC connection_handle, SQLCHAR* in_stmt_txt, SQLINTEGER in_stmt_txt_len,
    SQLCHAR* out_stmt_txt, SQLINTEGER out_stmt_txt_buf_len,
    SQLINTEGER* out_stmt_txt_len, TraceOptions& opts);
void TraceFunctionExit_SQLNativeSql(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLNativeSqlW(SQLHDBC connection_handle,
                                      SQLWCHAR* in_stmt_txt,
                                      SQLINTEGER in_stmt_txt_len,
                                      SQLWCHAR* out_stmt_txt,
                                      SQLINTEGER out_stmt_txt_buf_len,
                                      SQLINTEGER* out_stmt_txt_len,
                                      TraceOptions& opts);
void TraceFunctionExit_SQLNativeSqlW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLNumParams(SQLHSTMT statement_handle,
                                     SQLSMALLINT* param_count,
                                     TraceOptions& opts);
void TraceFunctionExit_SQLNumParams(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLParamData(SQLHSTMT statement_handle,
                                     SQLPOINTER* param_or_tgt_val,
                                     TraceOptions& opts);
void TraceFunctionExit_SQLParamData(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLPutData(SQLHSTMT statement_handle,
                                   SQLPOINTER param_data, SQLLEN param_data_len,
                                   TraceOptions& opts);
void TraceFunctionExit_SQLPutData(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLDescribeParam(
    SQLHSTMT statement_handle, SQLUSMALLINT param_num,
    SQLSMALLINT* param_sql_type, SQLULEN* param_sz, SQLSMALLINT* param_scale,
    SQLSMALLINT* param_nullable, TraceOptions& opts);
void TraceFunctionExit_SQLDescribeParam(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetData(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLSMALLINT target_c_type,
    SQLPOINTER target_val, SQLLEN target_val_buf_len,
    SQLLEN* target_val_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLGetData(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLNumResultCols(SQLHSTMT statement_handle,
                                         SQLSMALLINT* col_count,
                                         TraceOptions& opts);
void TraceFunctionExit_SQLNumResultCols(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLFetch(SQLHSTMT statement_handle, TraceOptions& opts);
void TraceFunctionExit_SQLFetch(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLExtendedFetch(SQLHSTMT statement_handle,
                                         SQLUSMALLINT fetch_orientation,
                                         SQLLEN fetch_offset,
                                         SQLULEN* row_count,
                                         SQLUSMALLINT* row_status_arr,
                                         TraceOptions& opts);
void TraceFunctionExit_SQLExtendedFetch(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLColAttribute(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts);
void TraceFunctionExit_SQLColAttribute(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLColAttributeW(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts);
void TraceFunctionExit_SQLColAttributeW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLColAttributes(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts);
void TraceFunctionExit_SQLColAttributes(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLColAttributesW(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num,
    SQLUSMALLINT field_identifier, SQLPOINTER char_attr,
    SQLSMALLINT char_attr_buf_len, SQLSMALLINT* char_attr_str_len,
    SQLLEN* numeric_attr, TraceOptions& opts);
void TraceFunctionExit_SQLColAttributesW(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLDescribeCol(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLCHAR* col_name,
    SQLSMALLINT col_name_buf_len, SQLSMALLINT* col_name_len,
    SQLSMALLINT* col_sql_data_type, SQLULEN* col_sz, SQLSMALLINT* dec_digits,
    SQLSMALLINT* col_nullable, TraceOptions& opts);
void TraceFunctionExit_SQLDescribeCol(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLDescribeColW(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLWCHAR* col_name,
    SQLSMALLINT col_name_buf_len, SQLSMALLINT* col_name_len,
    SQLSMALLINT* col_sql_data_type, SQLULEN* col_sz, SQLSMALLINT* dec_digits,
    SQLSMALLINT* col_nullable, TraceOptions& opts);
void TraceFunctionExit_SQLDescribeColW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLBindCol(
    SQLHSTMT statement_handle, SQLUSMALLINT col_num, SQLSMALLINT target_c_type,
    SQLPOINTER target_val, SQLLEN target_val_buf_len,
    SQLLEN* target_val_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLBindCol(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLRowCount(SQLHSTMT statement_handle,
                                    SQLLEN* row_count, TraceOptions& opts);
void TraceFunctionExit_SQLRowCount(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLFetchScroll(SQLHSTMT statement_handle,
                                       SQLSMALLINT fetch_orientation,
                                       SQLLEN fetch_offset, TraceOptions& opts);
void TraceFunctionExit_SQLFetchScroll(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLMoreResults(SQLHSTMT statement_handle,
                                       TraceOptions& opts);
void TraceFunctionExit_SQLMoreResults(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDiagField(
    SQLSMALLINT handle_type, SQLHANDLE handle, SQLSMALLINT rec_no,
    SQLSMALLINT diag_id, SQLPOINTER diag_info, SQLSMALLINT diag_info_buf_len,
    SQLSMALLINT* diag_info_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLGetDiagField(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDiagFieldW(
    SQLSMALLINT handle_type, SQLHANDLE handle, SQLSMALLINT rec_no,
    SQLSMALLINT diag_id, SQLPOINTER diag_info, SQLSMALLINT diag_info_buf_len,
    SQLSMALLINT* diag_info_str_len, TraceOptions& opts);
void TraceFunctionExit_SQLGetDiagFieldW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDiagRec(SQLSMALLINT handle_type, SQLHANDLE handle,
                                      SQLSMALLINT rec_no, SQLCHAR* sql_state,
                                      SQLINTEGER* native_err, SQLCHAR* msg_txt,
                                      SQLSMALLINT msg_txt_buf_len,
                                      SQLSMALLINT* msg_txt_len,
                                      TraceOptions& opts);
void TraceFunctionExit_SQLGetDiagRec(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLGetDiagRecW(
    SQLSMALLINT handle_type, SQLHANDLE handle, SQLSMALLINT rec_no,
    SQLWCHAR* sql_state, SQLINTEGER* native_err, SQLWCHAR* msg_txt,
    SQLSMALLINT msg_txt_buf_len, SQLSMALLINT* msg_txt_len, TraceOptions& opts);
void TraceFunctionExit_SQLGetDiagRecW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLColumns(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLColumns(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLColumnsW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLWCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLColumnsW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLTables(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLCHAR* table_type, SQLSMALLINT table_type_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLTables(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLTablesW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLWCHAR* table_type,
    SQLSMALLINT table_type_len, TraceOptions& opts);
void TraceFunctionExit_SQLTablesW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLPrimaryKeys(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLPrimaryKeys(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLPrimaryKeysW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLPrimaryKeysW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLProcedureColumns(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* proc_name, SQLSMALLINT proc_name_len,
    SQLCHAR* col_name, SQLSMALLINT col_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLProcedureColumns(SQLRETURN ret_code,
                                           TraceOptions& opts);

void TraceFunctionEntry_SQLProcedureColumnsW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* proc_name, SQLSMALLINT proc_name_len,
    SQLWCHAR* col_name, SQLSMALLINT col_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLProcedureColumnsW(SQLRETURN ret_code,
                                            TraceOptions& opts);

void TraceFunctionEntry_SQLProcedures(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* proc_name, SQLSMALLINT proc_name_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLProcedures(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLProceduresW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* proc_name, SQLSMALLINT proc_name_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLProceduresW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSpecialColumns(
    SQLHSTMT statement_handle, SQLUSMALLINT id_type, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT min_rowid_scope,
    SQLUSMALLINT col_nullable, TraceOptions& opts);
void TraceFunctionExit_SQLSpecialColumns(SQLRETURN ret_code,
                                         TraceOptions& opts);

void TraceFunctionEntry_SQLSpecialColumnsW(
    SQLHSTMT statement_handle, SQLUSMALLINT id_type, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT min_rowid_scope,
    SQLUSMALLINT col_nullable, TraceOptions& opts);
void TraceFunctionExit_SQLSpecialColumnsW(SQLRETURN ret_code,
                                          TraceOptions& opts);

void TraceFunctionEntry_SQLStatistics(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT index_type, SQLUSMALLINT reserved,
    TraceOptions& opts);
void TraceFunctionExit_SQLStatistics(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLStatisticsW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLUSMALLINT index_type, SQLUSMALLINT reserved,
    TraceOptions& opts);
void TraceFunctionExit_SQLStatisticsW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLTablePrivileges(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLTablePrivileges(SQLRETURN ret_code,
                                          TraceOptions& opts);

void TraceFunctionEntry_SQLTablePrivilegesW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLTablePrivilegesW(SQLRETURN ret_code,
                                           TraceOptions& opts);

void TraceFunctionEntry_SQLForeignKeys(
    SQLHSTMT statement_handle, SQLCHAR* pk_catalog_name,
    SQLSMALLINT pk_catalog_name_len, SQLCHAR* pk_schema_name,
    SQLSMALLINT pk_schema_name_len, SQLCHAR* pk_table_name,
    SQLSMALLINT pk_table_name_len, SQLCHAR* fk_catalog_name,
    SQLSMALLINT fk_catalog_name_len, SQLCHAR* fk_schema_name,
    SQLSMALLINT fk_schema_name_len, SQLCHAR* fk_table_name,
    SQLSMALLINT fk_table_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLForeignKeys(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLForeignKeysW(
    SQLHSTMT statement_handle, SQLWCHAR* pk_catalog_name,
    SQLSMALLINT pk_catalog_name_len, SQLWCHAR* pk_schema_name,
    SQLSMALLINT pk_schema_name_len, SQLWCHAR* pk_table_name,
    SQLSMALLINT pk_table_name_len, SQLWCHAR* fk_catalog_name,
    SQLSMALLINT fk_catalog_name_len, SQLWCHAR* fk_schema_name,
    SQLSMALLINT fk_schema_name_len, SQLWCHAR* fk_table_name,
    SQLSMALLINT fk_table_name_len, TraceOptions& opts);
void TraceFunctionExit_SQLForeignKeysW(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLColumnPrivileges(
    SQLHSTMT statement_handle, SQLCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLCHAR* table_name,
    SQLSMALLINT table_name_len, SQLCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLColumnPrivileges(SQLRETURN ret_code,
                                           TraceOptions& opts);

void TraceFunctionEntry_SQLColumnPrivilegesW(
    SQLHSTMT statement_handle, SQLWCHAR* catalog_name,
    SQLSMALLINT catalog_name_len, SQLWCHAR* schema_name,
    SQLSMALLINT schema_name_len, SQLWCHAR* table_name,
    SQLSMALLINT table_name_len, SQLWCHAR* col_name, SQLSMALLINT col_name_len,
    TraceOptions& opts);
void TraceFunctionExit_SQLColumnPrivilegesW(SQLRETURN ret_code,
                                            TraceOptions& opts);

void TraceFunctionEntry_SQLFreeStmt(SQLHSTMT statement_handle,
                                    SQLUSMALLINT option, TraceOptions& opts);
void TraceFunctionExit_SQLFreeStmt(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLEndTran(SQLSMALLINT handle_type, SQLHANDLE handle,
                                   SQLSMALLINT completion_type,
                                   TraceOptions& opts);
void TraceFunctionExit_SQLEndTran(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLCancel(SQLHSTMT statement_handle,
                                  TraceOptions& opts);
void TraceFunctionExit_SQLCancel(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLCloseCursor(SQLHSTMT statement_handle,
                                       TraceOptions& opts);
void TraceFunctionExit_SQLCloseCursor(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLDisconnect(SQLHDBC connection_handle,
                                      TraceOptions& opts);
void TraceFunctionExit_SQLDisconnect(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLFreeHandle(SQLSMALLINT handle_type, SQLHANDLE handle,
                                      TraceOptions& opts);
void TraceFunctionExit_SQLFreeHandle(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLCancelHandle(SQLSMALLINT handle_type,
                                        SQLHANDLE handle, TraceOptions& opts);
void TraceFunctionExit_SQLCancelHandle(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLSetPos(SQLHSTMT statement_handle,
                                  SQLSETPOSIROW row_number, SQLUSMALLINT op,
                                  SQLUSMALLINT lock_type, TraceOptions& opts);
void TraceFunctionExit_SQLSetPos(SQLRETURN ret_code, TraceOptions& opts);

void TraceFunctionEntry_SQLBulkOperations(SQLHSTMT statement_handle,
                                          SQLSMALLINT op, TraceOptions& opts);
void TraceFunctionExit_SQLBulkOperations(SQLRETURN ret_code,
                                         TraceOptions& opts);

#ifdef _WIN32
void TraceFunctionEntry_ConfigDSN(HWND hwndParent, WORD fRequest,
                                  LPCSTR lpszDriver, LPCSTR lpszAttributes,
                                  TraceOptions& opts);

void TraceFunctionExit_ConfigDSN(SQLRETURN ret_code, TraceOptions& opts);
#endif  // _WIN32
}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_TRACE_H
