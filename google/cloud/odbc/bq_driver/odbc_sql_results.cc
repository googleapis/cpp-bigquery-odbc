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

#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fetch.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "odbc_sql_results.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::CreateDSRowFromTypeInfo;
using google::cloud::odbc_bq_driver_internal::CreateTypeInfoRowSchema;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSRow;
using google::cloud::odbc_bq_driver_internal::DSValue;
using google::cloud::odbc_bq_driver_internal::IntValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::kSqlToBqDataTypes;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::RowSchema;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::StringValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::ToSqlPointer;
using google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using google::cloud::odbc_bq_driver_internal::WriteRowset;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

SQLRETURN SQLBindColInternal(SQLHSTMT statement_handle,
                             SQLUSMALLINT column_number,
                             SQLSMALLINT target_c_type, SQLPOINTER target_value,
                             SQLLEN target_value_buffer_len,
                             SQLLEN* target_value_str_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;

  DescriptorHandle& ard = handle->GetDescriptorHandle(DescriptorType::kARD);

  // ----- Validations ---------

  if (column_number < 0) {
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "ColumnNumber should not < 0"};
    return LogAndReturnCode(*handle, status_record);
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle->GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    return LogAndReturnCode(*handle, use_bookmarks_status);
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {SQLStates::k_07006(),
                                  "ColumnNumber should not be 0"};
    return LogAndReturnCode(*handle, status_record);
  }

  // Unbinding column
  if (target_value == nullptr) {
    // Here we don't care about the status record returned by
    // UnbindDescriptorRecord because SQL_SUCCESS should be returned in case of
    // unbound column number too.
    // (https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindcol-function?view=sql-server-ver16#unbinding-columns)
    ard.UnbindDescriptorRecord(column_number);
    return SQL_SUCCESS;
  }

  StatusRecord status_record;
  if (target_value_buffer_len < 0) {
    status_record = {SQLStates::k_HY090(), "BufferLength should not < 0"};
    return LogAndReturnCode(*handle, status_record);
  }

  bool no_desc_bound_previously = !ard.HasDescriptorRecord(column_number);

  // Setting DESC_CONCISE_TYPE will also set  DESC_TYPE and
  // DESC_DATETIME_INTERVAL_CODE
  status_record = SetDescField(&ard, column_number, SQL_DESC_CONCISE_TYPE,
                               ToSqlPointer<SQLSMALLINT>(target_c_type), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  // ----- Set fields for target_value_buffer_len, target_value ------

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH,
                   ToSqlPointer<SQLLEN>(target_value_buffer_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_DATA_PTR, target_value, 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  // ----- Set fields for target_value_str_len ------

  status_record = SetDescField(&ard, column_number, SQL_DESC_INDICATOR_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  status_record = SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLFetchInternal(SQLHSTMT statement_handle) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  if (handle.GetStmtState() == StmtStates::kStatementExecutedWithoutRs) {
    return SQL_NO_DATA;
  }

  if (handle.GetStmtState() != StmtStates::kStatementExecutedWithRs) {
    StatusRecord status_record = {SQLStates::k_HY010(),
                                  "No statement has been executed"};
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);

  ResultSet const& result_set = handle.GetResultSet();
  result_set.cursor++;
  if (result_set.cursor >= result_set.rows.size()) {
    return SQL_NO_DATA;
  }

  int rowset_size = ard.GetHeaderRecord().array_size;
  if (!rowset_size) {
    rowset_size = 1;
  }
  StatusRecord status_record = WriteRowset(result_set, rowset_size, ard);
  return LogAndReturnCode(handle, status_record);
}

SQLRETURN SQLNumResultColsInternal(SQLHSTMT statement_handle,
                                   SQLSMALLINT* column_count_ptr) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;
  StatusRecord status_record = StatusRecord::Ok();
  if (column_count_ptr == nullptr) {
    status_record = {SQLStates::k_HY001(),
                     "Parameter 'column_count_ptr' cannot be null"};
    return LogAndReturnCode(*handle, status_record);
  }

  auto stmt_state = handle->GetStmtState();
  switch (stmt_state) {
    case StmtStates::kStatementPrepared:
    case StmtStates::kStatementExecutedWithRs:
      break;
    case StmtStates::kStatementExecutedWithoutRs:
      status_record = {SQLStates::k_01000(), "Statement Executed without Data"};
      break;
    case StmtStates::kStatementStillExecuting:
      status_record = {SQLStates::k_HY010(), "Statement is still executing"};
      break;
    case StmtStates::kNeedsPutData:
      status_record = {SQLStates::k_HY010(),
                       "Statement needs Data to be executed"};
      break;
    default:
      status_record = {SQLStates::k_HY010(), "No statement has been executed"};
      break;
  }
  if (!status_record.ok()) {
    return LogAndReturnCode(*handle, status_record);
  }
  DescriptorHandle ird = handle->GetDescriptorHandle(DescriptorType::kIRD);
  if (ird.GetHeaderRecord().count < 0) {
    status_record = {SQLStates::k_07006(),
                     "ColumnCount should not be less than 0"};
    return LogAndReturnCode(*handle, status_record);
  }
  *column_count_ptr = ird.GetHeaderRecord().count;
  return status_record.CalculateReturnCode();
}

SQLRETURN SQLGetTypeInfoInternal(SQLHSTMT stmt_handle, SQLSMALLINT data_type) {
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  StatementHandle& handle = *(*handle_result);

  ResultSet result_set;
  if (data_type == SQL_ALL_TYPES) {
    for (auto [sql_data_type, bq_data_type_info] : kSqlToBqDataTypes) {
      for (auto [bq_data_type, type_info] : bq_data_type_info) {
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
      }
    }
  } else {
    if (kSqlToBqDataTypes.count(data_type)) {
      for (auto [bq_data_type, type_info] : kSqlToBqDataTypes.at(data_type)) {
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
      }
    }
  }

  if (!result_set.rows.empty()) {
    CreateTypeInfoRowSchema(result_set);
    handle.SetResultSet(result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }

  return SQL_SUCCESS;
}

SQLRETURN SQLDescribeColInternal(
    SQLHSTMT statement_handle, SQLUSMALLINT column_number, SQLCHAR* column_name,
    SQLSMALLINT column_name_buffer_len, SQLSMALLINT* column_name_le,
    SQLSMALLINT* column_sql_data_type, SQLULEN* column_size,
    SQLSMALLINT* decimal_digits, SQLSMALLINT* column_nullable) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  if (handle.GetStmtState() == StmtStates::kStatementNotPrepared) {
    StatusRecord status_record = {
        SQLStates::k_HY010(),
        "Function sequence error - statement is not prepared"};
    return LogAndReturnCode(handle, status_record);
  }

  if (column_number < 0) {
    StatusRecord status_record = {
        SQLStates::k_HY000(),
        "Invalid ColumnNumber parameter - should not be < 0"};
    return LogAndReturnCode(handle, status_record);
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle.GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    return LogAndReturnCode(handle, use_bookmarks_status);
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {
        SQLStates::k_07006(),
        "Invalid column number value for bookmark attribute - should not be 0"};
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  if (!ird.HasDescriptorRecord(column_number)) {
    StatusRecord status_record = {
        SQLStates::k_07009(),
        "Invalid descriptor index - no column for such value"};
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorRecord& desc_record = ird.GetDescriptorRecord(column_number);

  StatusRecord status_record =
      StringValueToOutputBufferResponse(desc_record.name.c_str(), column_name,
                                        column_name_buffer_len, column_name_le);
  if (!status_record.ok()) {
    return LogAndReturnCode(handle, status_record);
  }

  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      desc_record.concise_type, column_sql_data_type, nullptr);

  switch (desc_record.concise_type) {
    case SQL_NUMERIC:
    case SQL_DECIMAL:
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    case SQL_BIGINT:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.precision, column_size, nullptr);
      break;
    default:
      IntValueToOutputBufferResponse<SQLULEN, SQLSMALLINT>(
          desc_record.length, column_size, nullptr);
  }

  switch (desc_record.concise_type) {
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
    case SQL_INTERVAL_SECOND:
    case SQL_INTERVAL_DAY_TO_SECOND:
    case SQL_INTERVAL_HOUR_TO_SECOND:
    case SQL_INTERVAL_MINUTE_TO_SECOND:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.precision, decimal_digits, nullptr);
      break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.scale, decimal_digits, nullptr);
      break;
    default:
      *decimal_digits = 0;
  }
  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      desc_record.nullable, column_nullable, nullptr);

  return SQL_SUCCESS;
}

SQLRETURN SQLColAttributeInternal(SQLHSTMT statement_handle,
                                  SQLUSMALLINT column_number,
                                  SQLUSMALLINT field_identifier,
                                  SQLPOINTER char_attr,
                                  SQLSMALLINT char_attr_buffer_len,
                                  SQLSMALLINT* char_attr_string_len,
                                  SQLLEN* numeric_attribute) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);

  StatusRecordOr<SQLRETURN> result;
  switch (field_identifier) {
    case SQL_DESC_BASE_COLUMN_NAME:
    case SQL_DESC_BASE_TABLE_NAME:
    case SQL_DESC_CATALOG_NAME:
    case SQL_DESC_LABEL:
    case SQL_DESC_LITERAL_PREFIX:
    case SQL_DESC_LITERAL_SUFFIX:
    case SQL_DESC_LOCAL_TYPE_NAME:
    case SQL_DESC_NAME:
    case SQL_DESC_SCHEMA_NAME:
    case SQL_DESC_TABLE_NAME:
    case SQL_DESC_TYPE_NAME:
      result =
          GetDescField(&ird, static_cast<SQLSMALLINT>(column_number),
                       static_cast<SQLSMALLINT>(field_identifier), char_attr,
                       static_cast<SQLINTEGER>(char_attr_buffer_len),
                       reinterpret_cast<SQLINTEGER*>(char_attr_string_len));
      break;
    default:
      result = GetDescField(&ird, static_cast<SQLSMALLINT>(column_number),
                            static_cast<SQLSMALLINT>(field_identifier),
                            numeric_attribute, 0, nullptr);
  }
  return LogAndReturnCode(stmt_handle, result);
}

SQLRETURN SQLCloseCursorInternal(SQLHSTMT statement_handle) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  if (!stmt_handle.IsCursorOpen()) {
    StatusRecord status_record = {
        SQLStates::k_24000(), "Invalid cursor state - cursor was not opened"};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  stmt_handle.CloseCursor();

  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
