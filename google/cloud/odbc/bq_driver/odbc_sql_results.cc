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
#include "google/cloud/odbc/bq_driver/internal/odbc_query.h"
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

using google::cloud::odbc_bq_driver_internal::BQDataType;
using google::cloud::odbc_bq_driver_internal::CheckTargetType;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::CreateDSRowFromTypeInfo;
using google::cloud::odbc_bq_driver_internal::CreateTypeInfoRowSchema;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSRow;
using google::cloud::odbc_bq_driver_internal::DSValue;
using google::cloud::odbc_bq_driver_internal::GetColumnData;
using google::cloud::odbc_bq_driver_internal::IntValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::kSqlToBqDataTypes;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::RowSchema;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::StringValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::ToSqlPointer;
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
    LOG(ERROR) << "SQLBindCol::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;

  DescriptorHandle& ard = handle->GetDescriptorHandle(DescriptorType::kARD);

  // ----- Validations ---------

  if (column_number < 0) {
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "ColumnNumber should not < 0"};
    LOG(ERROR) << "SQLBindCol:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle->GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    LOG(ERROR) << "SQLBindCol::GetAttribute:: "
               << use_bookmarks_status.GetStatusRecord().message;
    return LogAndReturnCode(*handle, use_bookmarks_status);
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {SQLStates::k_07006(),
                                  "ColumnNumber should not be 0"};
    LOG(ERROR) << "SQLBindCol:: " << status_record.message;
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
    LOG(ERROR) << "SQLBindCol:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  bool no_desc_bound_previously = !ard.HasDescriptorRecord(column_number);

  // Setting DESC_CONCISE_TYPE will also set  DESC_TYPE and
  // DESC_DATETIME_INTERVAL_CODE
  status_record = SetDescField(&ard, column_number, SQL_DESC_CONCISE_TYPE,
                               ToSqlPointer<SQLSMALLINT>(target_c_type), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    LOG(ERROR) << "SQLBindCol::SetDescField:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  // ----- Set fields for target_value_buffer_len, target_value ------

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH,
                   ToSqlPointer<SQLLEN>(target_value_buffer_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    LOG(ERROR) << "SQLBindCol::SetDescField:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_DATA_PTR, target_value, 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    LOG(ERROR) << "SQLBindCol::SetDescField:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  // ----- Set fields for target_value_str_len ------

  status_record = SetDescField(&ard, column_number, SQL_DESC_INDICATOR_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    LOG(ERROR) << "SQLBindCol::SetDescField:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  status_record = SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    LOG(ERROR) << "SQLBindCol::SetDescField:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLFetchInternal(SQLHSTMT statement_handle) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLFetch::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  if (handle.GetStmtState() == StmtStates::kStatementExecutedWithoutRs) {
    auto status_record =
        StatusRecord{SQLStates::k_24000(), "Invalid cursor state."};
    LOG(ERROR) << "SQLFetch:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  if (handle.GetStmtState() != StmtStates::kStatementExecutedWithRs) {
    StatusRecord status_record = {SQLStates::k_HY010(),
                                  "No statement has been executed"};
    LOG(ERROR) << "SQLFetch:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);

  ResultSet const& result_set = handle.GetResultSet();
  result_set.cursor++;
  result_set.translated_data.row_offset = 0;
  // result_set.translated_data.data.clear();
  // result_set.translated_data.data.shrink_to_fit();
  if (result_set.cursor >= result_set.rows.size()) {
    LOG(INFO) << "SQLFetch:: cursor is greater then result set size";
    return SQL_NO_DATA;
  }

  int rowset_size = ard.GetHeaderRecord().array_size;
  if (!rowset_size) {
    rowset_size = 1;
  }
  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  StatusRecord status_record = WriteRowset(result_set, rowset_size, ard, ird);
  return LogAndReturnCode(handle, status_record);
}

SQLRETURN SQLFetchScrollInternal(SQLHSTMT statement_handle,
                                 SQLSMALLINT fetch_orientation,
                                 SQLLEN /*fetch_offset*/) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  StatusRecord status_record;
  if (!handle_result) {
    LOG(ERROR) << "SQLFetechScroll::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  if (handle.GetStmtState() == StmtStates::kStatementExecutedWithoutRs) {
    status_record = StatusRecord{SQLStates::k_24000(), "Invalid cursor state."};
    LOG(ERROR) << "SQLFetechScroll:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  if (handle.GetStmtState() != StmtStates::kStatementExecutedWithRs) {
    status_record = {SQLStates::k_HY010(), "No statement has been executed"};
    LOG(ERROR) << "SQLFetechScroll:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  // Validate FetchOrientation
  if (fetch_orientation != SQL_FETCH_NEXT &&
      fetch_orientation != SQL_FETCH_PRIOR &&
      fetch_orientation != SQL_FETCH_FIRST &&
      fetch_orientation != SQL_FETCH_LAST &&
      fetch_orientation != SQL_FETCH_ABSOLUTE &&
      fetch_orientation != SQL_FETCH_RELATIVE &&
      fetch_orientation != SQL_FETCH_BOOKMARK) {
    status_record = {SQLStates::k_HY106(), "Fetch type out of range"};
    LOG(ERROR) << "SQLFetechScroll::fetch_orientation:: "
               << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);

  ResultSet const& result_set = handle.GetResultSet();
  result_set.translated_data.row_offset = 0;

  // Compute new row position based on fetch type
  switch (fetch_orientation) {
    case SQL_FETCH_NEXT:
      if (result_set.cursor + 1 < result_set.rows.size())
        result_set.cursor++;
      else
        return SQL_NO_DATA;
      break;
    case SQL_FETCH_PRIOR:
    case SQL_FETCH_FIRST:
    case SQL_FETCH_LAST:
    case SQL_FETCH_ABSOLUTE:
    case SQL_FETCH_RELATIVE:
    case SQL_FETCH_BOOKMARK:
      status_record = {SQLStates::k_HY106(),
                       "Fetch type not supported, as fetch orientation is not "
                       "compatible with current settings."};
      LOG(ERROR) << "SQLFetechScroll::fetch_orientation:: "
                 << status_record.message;
      return LogAndReturnCode(handle, status_record);
  }
  int rowset_size = ard.GetHeaderRecord().array_size;
  if (!rowset_size) {
    rowset_size = 1;
  }
  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  status_record = WriteRowset(result_set, rowset_size, ard, ird);
  return LogAndReturnCode(handle, status_record);
}

SQLRETURN SQLNumResultColsInternal(SQLHSTMT statement_handle,
                                   SQLSMALLINT* column_count_ptr) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLNumResultCols::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;
  StatusRecord status_record = StatusRecord::Ok();
  if (column_count_ptr == nullptr) {
    status_record = {SQLStates::k_HY001(),
                     "Parameter 'column_count_ptr' cannot be null"};
    LOG(ERROR) << "SQLNumResultCols:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  *column_count_ptr = 0;
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
    LOG(ERROR) << "SQLNumResultCols::StmtState:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }
  DescriptorHandle ird = handle->GetDescriptorHandle(DescriptorType::kIRD);
  if (ird.GetHeaderRecord().count < 0) {
    status_record = {SQLStates::k_07006(),
                     "ColumnCount should not be less than 0"};
    LOG(ERROR) << "SQLNumResultCols::GetHeaderRecord:: "
               << status_record.message;
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
    LOG(ERROR) << "SQLGetTypeInfo::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }

  StatementHandle& handle = *(*handle_result);

  ResultSet result_set;
  SQLULEN row_count = 0;
  auto max_rows_status = handle.GetAttribute(SQL_ATTR_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "SQLGetTypeInfo::GetAttribute:: "
               << max_rows_status.GetStatusRecord().message;
    return max_rows_status.GetCalculatedReturnCode();
  }
  SQLULEN max_rows = *max_rows_status;

  if (data_type == SQL_ALL_TYPES) {
    for (auto [sql_data_type, bq_data_type_info] : kSqlToBqDataTypes) {
      for (auto [bq_data_type, type_info] : bq_data_type_info) {
        if (max_rows != 0 && row_count >= max_rows) break;
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
        ++row_count;
      }
      if (max_rows != 0 && row_count >= max_rows) break;
    }
  } else {
    if (kSqlToBqDataTypes.count(data_type)) {
      for (auto [bq_data_type, type_info] : kSqlToBqDataTypes.at(data_type)) {
        if (max_rows != 0 && row_count >= max_rows) break;
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
        ++row_count;
      }
    }
  }

  CreateTypeInfoRowSchema(result_set);
  handle.SetResultSet(result_set);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);

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
    LOG(ERROR) << "SQLDescribeCol::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  if (handle.GetStmtState() == StmtStates::kStatementNotPrepared) {
    StatusRecord status_record = {
        SQLStates::k_HY010(),
        "Function sequence error - statement is not prepared"};
    LOG(ERROR) << "SQLDescribeCol:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  if (column_number < 0) {
    StatusRecord status_record = {
        SQLStates::k_HY000(),
        "Invalid ColumnNumber parameter - should not be < 0"};
    LOG(ERROR) << "SQLDescribeCol:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle.GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    LOG(ERROR) << "SQLDescribeCol::GetAttribute:: "
               << use_bookmarks_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, use_bookmarks_status);
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {
        SQLStates::k_07006(),
        "Invalid column number value for bookmark attribute - should not be 0"};
    LOG(ERROR) << "SQLDescribeCol:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  if (!ird.HasDescriptorRecord(column_number)) {
    StatusRecord status_record = {
        SQLStates::k_07009(),
        "Invalid descriptor index - no column for such value"};
    LOG(ERROR) << "SQLDescribeCol:: " << status_record.message;
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorRecord& desc_record = ird.GetDescriptorRecord(column_number);

  StatusRecord status_record =
      StringValueToOutputBufferResponse(desc_record.name.c_str(), column_name,
                                        column_name_buffer_len, column_name_le);
  if (!status_record.ok()) {
    LOG(ERROR) << "SQLDescribeCol::StringValueToOutputBufferResponse:: "
               << status_record.message;
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
    LOG(ERROR) << "SQLColAttribute::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
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
                       reinterpret_cast<SQLSMALLINT*>(char_attr_string_len));
      break;
    default:
      // SQLColAttribute expects some descriptor fields to return values as
      // SQLLEN, but their default type is SQLSMALLINT. This datatype mismatch
      // leads to truncation or incorrect (garbage) values during conversion.
      result = GetDescField(&ird, static_cast<SQLSMALLINT>(column_number),
                            static_cast<SQLSMALLINT>(field_identifier),
                            numeric_attribute, 0, nullptr, true);
  }
  return LogAndReturnCode(stmt_handle, result);
}

SQLRETURN SQLCloseCursorInternal(SQLHSTMT statement_handle) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLCloseCursor::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  if (!stmt_handle.IsCursorOpen()) {
    StatusRecord status_record = {
        SQLStates::k_24000(), "Invalid cursor state - cursor was not opened"};
    LOG(ERROR) << "SQLCloseCursor::Cursor:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  stmt_handle.CloseCursor();

  return SQL_SUCCESS;
}

SQLRETURN SQLRowCountInternal(SQLHSTMT statement_handle, SQLLEN* row_count) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLRowCount::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);
  StatusRecord status_record = StatusRecord::Ok();
  if (row_count == nullptr) {
    status_record = {SQLStates::k_HY001(),
                     "Parameter 'row_count' cannot be null"};
    LOG(ERROR) << "SQLRowCount::RowCount " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }
  auto stmt_state = stmt_handle.GetStmtState();
  switch (stmt_state) {
    case StmtStates::kStatementNotPrepared:
      status_record = {SQLStates::k_HY001(), "Statement is not prepared"};
      break;
    case StmtStates::kStatementAsyncExecute:
    case StmtStates::kStatementAsyncPrepare:
    case StmtStates::kStatementStillExecuting:
      status_record = {SQLStates::k_HY010(), "Function sequence error"};
      break;
    case StmtStates::kNeedsPutData:
      status_record = {SQLStates::k_HY010(),
                       "Statement needs Data to be executed"};
      break;
    default:
      break;
  }
  if (!status_record.ok()) {
    LOG(ERROR) << "SQLRowCount::StmtState:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }
  auto prepared_job = stmt_handle.GetPreparedJob();
  if (!prepared_job) {
    status_record = {SQLStates::k_HY001(), "Prepared job is not available"};
    LOG(ERROR) << "SQLRowCount::GetPreparedJob:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }
  std::string operation =
      prepared_job->statistics.job_query_stats.statement_type;
  std::string sub_operation_type;
  if (stmt_handle.HasJobData()) {
    auto job_status = stmt_handle.GetNextJobData();
    if (!job_status.Ok()) {
      LOG(ERROR) << "SQLRowCount::GetNextJobData:: " << status_record.message;
      return LogAndReturnCode(stmt_handle, job_status.GetStatusRecord());
    }
    sub_operation_type = job_status.GetValue().second;
  }

  if (operation == "INSERT" || sub_operation_type == "INSERT" ||
      operation == "UPDATE" || sub_operation_type == "UPDATE" ||
      operation == "DELETE" || sub_operation_type == "DELETE") {
    *row_count = stmt_handle.GetDSResults().num_dml_affected_rows;
  } else {
    *row_count = -1;
  }

  return status_record.CalculateReturnCode();
}

SQLRETURN SQLGetDataInternal(SQLHSTMT statement_handle,
                             SQLUSMALLINT column_number,
                             SQLSMALLINT target_c_type, SQLPOINTER target_value,
                             SQLLEN target_value_buffer_len,
                             SQLLEN* target_value_string_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLGetData::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  StatusRecord status_record;
  if (stmt_handle.GetStmtState() == StmtStates::kStatementNotPrepared) {
    status_record = {SQLStates::k_HY007(),
                     "Associated statement is not prepared"};
    LOG(ERROR) << "SQLGetData::StmtState:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }
  if (column_number < 0) {
    status_record = {SQLStates::k_HY000(),
                     "Invalid ColumnNumber parameter - should not be < 0"};
    LOG(ERROR) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      stmt_handle.GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    LOG(ERROR) << "SQLGetData::GetAttribute:: "
               << use_bookmarks_status.GetStatusRecord().message;
    return LogAndReturnCode(stmt_handle, use_bookmarks_status);
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    status_record = {SQLStates::k_07009(), "Invalid descriptor index"};
    LOG(ERROR) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  if (target_value == nullptr) {
    status_record = {SQLStates::k_HY009(), "Invalid use of null pointer"};
    LOG(ERROR) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  if (target_value_buffer_len < 0) {
    status_record = {SQLStates::k_HY090(), "Invalid string or buffer length"};
    LOG(ERROR) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  if (!CheckTargetType(target_c_type)) {
    status_record = {SQLStates::k_HY003(), "Program type out of range"};
    LOG(ERROR) << "SQLGetData::CheckTargetType:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  if (column_number > stmt_handle.GetResultSet().row_schema.size()) {
    status_record = {SQLStates::k_07009(), "Invalid Column In Result Set"};
    LOG(ERROR) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  ResultSet const& result_set = stmt_handle.GetResultSet();
  int cursor = result_set.cursor;
  int row_size = result_set.rows.size();
  if (cursor >= row_size) {
    LOG(INFO) << "SQLGetData:: Cursor is greater then row size";
    return SQL_NO_DATA;
  }

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);
  if (target_c_type == SQL_ARD_TYPE) {
    GetDescField(&ard, column_number, SQL_DESC_CONCISE_TYPE, &target_c_type, 0,
                 nullptr);
  }

  DSRow const& ds_row = result_set.rows[cursor];
  RowSchema const& schema = result_set.row_schema;
  BQDataType bq_data_type;
  for (auto const& col_schema : schema) {
    if (col_schema.col_index == column_number - 1) {
      if (col_schema.is_mode_repeated) {
        bq_data_type = BQDataType::kArray;
      } else {
        bq_data_type = col_schema.col_type;
      }
    }
  }
  DSValue const& ds_val = ds_row[column_number - 1];

  // Updating result_set.translated_data.last_column_index with column_number
  // and row_offset_ to 0 when last fetched column number and column_number
  // passed here are different
  if (result_set.translated_data.last_column_index != column_number) {
    result_set.translated_data.row_offset = 0;
    result_set.translated_data.last_column_index = column_number;
  }
  result_set.translated_data.last_column_index = column_number;

  SQLLEN offset = result_set.translated_data.row_offset;

  // Translating complete data in case of less buffer length when SQLGetData
  // called for the first time
  //  and storing it in result_set.translated_data.data in case of
  // variable length data type i.e. string and binary followed by copying into
  // target_value in parts of buffer length.
  if (offset == 0) {
    if ((ds_val.size() > target_value_buffer_len) &&
        (bq_data_type == BQDataType::kString ||
         bq_data_type == BQDataType::kBytes ||
         bq_data_type == BQDataType::kJson ||
         bq_data_type == BQDataType::kStruct ||
         bq_data_type == BQDataType::kArray)) {
      result_set.translated_data.last_target_c_type = target_c_type;
      SQLLEN ds_val_bytes = (target_c_type == SQL_C_WCHAR)
                                            ? ds_val.size() 
                                            * sizeof(SQLWCHAR) : ds_val.size();
     
      auto ds_val_null_bytes  = (target_c_type == SQL_C_WCHAR) 
                                                ? sizeof(SQLWCHAR)
                                                 : 1;

      SQLLEN ds_val_total_bytes = ds_val_bytes + ds_val_null_bytes;
      if (result_set.translated_data.data.capacity() < ds_val_total_bytes) {
        result_set.translated_data.data.reserve(ds_val_total_bytes);
      }
      result_set.translated_data.data.resize(ds_val_total_bytes);

      SQLLEN target_value_len = 0;

      status_record = GetColumnData(ds_val, bq_data_type, target_c_type,
                                    result_set.translated_data.data.data(),
                                    ds_val_total_bytes, &target_value_len);
      if (target_value_string_len) {
        *target_value_string_len = target_value_len;
      }

      // Ensure resizing does not shrink below the required buffer size
      if (target_value_len < result_set.translated_data.data.capacity()) {
        result_set.translated_data.data.resize(target_value_len);
      }
      std::memset(target_value, '\0', target_value_buffer_len);
    } else {
      status_record =
          GetColumnData(ds_val, bq_data_type, target_c_type, target_value,
                        target_value_buffer_len, target_value_string_len);
      return LogAndReturnCode(stmt_handle, status_record);
    }
  }

  // Check if the target data type has changed from the last fetch.
  // If the target data type is different from the last fetched type, return an
  // error because changing data types in a multipart fetch operation is not
  // supported.
  if (result_set.translated_data.last_target_c_type != target_c_type) {
    status_record = {
        SQLStates::k_HY000(),
        "Changing types between multipart SQLGetData() calls is not supported"};
    LOG(ERROR) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  // Validating if data size is more then buffersize, SQLGetData will return
  // partial Data
   SQLLEN data_size = result_set.translated_data.data.size();
  SQLLEN remaining_bytes = data_size - offset;
  
  if(target_c_type == SQL_C_BINARY){
    if(remaining_bytes >= target_value_buffer_len){
        std::memcpy(target_value, result_set.translated_data.data.data() + offset,
                  target_value_buffer_len);

    result_set.translated_data.row_offset = offset + target_value_buffer_len;
    if (target_value_string_len) {
      *target_value_string_len = target_value_buffer_len;
    }
    status_record = {SQLStates::k_01004(), "Binary data, right truncated"};
    LOG(WARNING) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
    }
  }
  else if (target_c_type == SQL_C_WCHAR){
         SQLLEN max_buffer_chars = (target_value_buffer_len/ sizeof(SQLWCHAR));
        SQLLEN remaining_chars = (data_size -1) - (offset/sizeof(SQLWCHAR));
        if(remaining_chars > max_buffer_chars){
           SQLLEN max_chars = max_buffer_chars - 1; 
          SQLLEN copy_chars =  max_chars;

          std::memcpy(target_value, result_set.translated_data.data.data() + offset,
                    copy_chars * sizeof(SQLWCHAR));
          ((SQLWCHAR*)target_value)[copy_chars] = L'\0';
          result_set.translated_data.row_offset =  offset + (copy_chars * sizeof(SQLWCHAR));
  
           if (target_value_string_len) {
              *target_value_string_len = copy_chars;
           }
      status_record =
          StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
      LOG(WARNING) << "SQLGetData:: " << status_record.message;
      LOG(INFO) << "SQLGetData:: " << status_record.message;
      return LogAndReturnCode(stmt_handle, status_record);
        }
  }else{
    if(remaining_bytes >= target_value_buffer_len){
      std::memcpy(target_value, result_set.translated_data.data.data() + offset,
                  target_value_buffer_len - 1);
      result_set.translated_data.row_offset =
          offset + target_value_buffer_len - 1;

        }
    if (target_value_string_len) {
          *target_value_string_len = target_value_buffer_len;
        }
    status_record = {SQLStates::k_01004(), "String data, right truncated"};
    LOG(WARNING) << "SQLGetData:: " << status_record.message;
    return LogAndReturnCode(stmt_handle, status_record);
  }

  if (offset != 0) {
    if (target_c_type == SQL_C_BINARY) {
      std::memcpy(target_value, result_set.translated_data.data.data() + offset,
                  result_set.translated_data.data.size() - offset);
    }else if (target_c_type == SQL_C_WCHAR) {
      std::memcpy(target_value, result_set.translated_data.data.data() + offset,
                  (result_set.translated_data.data.size() * sizeof(SQLWCHAR)) - offset + 1);
    }
     else {
      std::memcpy(target_value, result_set.translated_data.data.data() + offset,
                  result_set.translated_data.data.size() - offset + 1);
    }
    return LogAndReturnCode(stmt_handle, status_record);
  }
}

SQLRETURN SQLNativeSqlInternal(SQLHDBC connection_handle,
                               SQLCHAR* in_statement_text,
                               SQLINTEGER in_statement_text_len,
                               SQLCHAR* out_statement_text,
                               SQLINTEGER out_statement_text_buffer_len,
                               SQLINTEGER* out_statement_text_len) {
  // Validate the connection handle
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLNativeSql::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  ConnectionHandle& conn_handle = *(*handle_result);

  // Validate input SQL statement
  if (!in_statement_text) {
    LOG(ERROR) << "SQLNativeSql:: Invalid use of null pointer";
    return LogAndReturnCode(
        conn_handle, {SQLStates::k_HY009(), "Invalid use of null pointer"});
  }

  if (in_statement_text_len < 0 && in_statement_text_len != SQL_NTS) {
    LOG(ERROR) << "SQLNativeSql:: Invalid string or buffer length";
    return LogAndReturnCode(
        conn_handle, {SQLStates::k_HY090(), "Invalid string or buffer length"});
  }

  // Convert input SQL statement to a std::string
  std::string input_sql(
      reinterpret_cast<char const*>(in_statement_text),
      (in_statement_text_len == SQL_NTS)
          ? std::strlen(reinterpret_cast<char const*>(in_statement_text))
          : static_cast<size_t>(in_statement_text_len));

  if (input_sql.empty()) {
    LOG(ERROR) << "SQLNativeSql:: Empty SQL statement";
    return LogAndReturnCode(conn_handle,
                            {SQLStates::k_HY090(), "Empty SQL statement"});
  }

  // Output is same as input for BigQuery
  std::string const& output_sql = input_sql;
  auto output_length = static_cast<SQLINTEGER>(output_sql.size());

  if (out_statement_text_buffer_len == 0) {
    if (out_statement_text_len) {
      *out_statement_text_len = output_length;
    }
    if (out_statement_text) {
      out_statement_text[out_statement_text_buffer_len] = '\0';
    }
    LOG(ERROR) << "SQLNativeSql:: String data, right truncated";
    return LogAndReturnCode(
        conn_handle, {SQLStates::k_01004(), "String data, right truncated"});
  }

  // Use helper for output buffer handling
  auto status_record = StringValueToOutputBufferResponse<SQLINTEGER>(
      output_sql.c_str(), out_statement_text, out_statement_text_buffer_len,
      out_statement_text_len);

  // Output length is same as input length
  if (out_statement_text_len) {
    *out_statement_text_len = output_length;
  }

  if (!status_record.ok()) {
    LOG(ERROR) << "SQLNativeSql::StringValueToOutputBufferResponse:: "
               << status_record.message;
    return LogAndReturnCode(conn_handle, status_record);
  }

  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
