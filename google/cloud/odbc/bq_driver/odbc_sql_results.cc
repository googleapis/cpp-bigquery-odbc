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
#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::BQDataType;
using google::cloud::odbc_bq_driver_internal::ColumnSchema;
using google::cloud::odbc_bq_driver_internal::ConvertFromArithmeticDSValue;
using google::cloud::odbc_bq_driver_internal::ConvertFromStringDSValue;
using google::cloud::odbc_bq_driver_internal::DataBuffer;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSRow;
using google::cloud::odbc_bq_driver_internal::DSValue;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::RowSchema;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::ToSqlPointer;
using google::cloud::odbc_bq_driver_internal::TracePrintInternal;
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
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle->GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    handle->GetDiagnostics().AddStatusRecord(
        use_bookmarks_status.GetStatusRecord());
    return use_bookmarks_status.GetCalculatedReturnCode();
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {SQLStates::k_07006(),
                                  "ColumnNumber should not be 0"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
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
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  // Setting DESC_CONCISE_TYPE will also set  DESC_TYPE and
  // DESC_DATETIME_INTERVAL_CODE
  status_record = SetDescField(&ard, column_number, SQL_DESC_CONCISE_TYPE,
                               ToSqlPointer<SQLSMALLINT>(target_c_type), 0);
  if (!status_record.ok()) {
    ard.UnbindDescriptorRecord(column_number);
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  // ----- Set fields for target_value_buffer_len, target_value ------

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH,
                   ToSqlPointer<SQLLEN>(target_value_buffer_len), 0);
  if (!status_record.ok()) {
    ard.UnbindDescriptorRecord(column_number);
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_DATA_PTR, target_value, 0);
  if (!status_record.ok()) {
    ard.UnbindDescriptorRecord(column_number);
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  // ----- Set fields for target_value_str_len ------

  status_record = SetDescField(&ard, column_number, SQL_DESC_INDICATOR_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    ard.UnbindDescriptorRecord(column_number);
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  status_record = SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    ard.UnbindDescriptorRecord(column_number);
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }
  return SQL_SUCCESS;
}

// NOLINTBEGIN(misc-unused-parameters)

StatusRecord WriteToApplicationBuffer(DSValue& ds_val, BQDataType bq_data_type,
                                      DescriptorRecord& app_desc_rec) {
  if (ds_val.empty()) {
    return StatusRecord::Ok();
  }
  SQLSMALLINT target_c_type = app_desc_rec.concise_type;
  SQLPOINTER app_buffer = app_desc_rec.data_ptr;
  SQLLEN app_buffer_len = app_desc_rec.octet_length;
  SQLLEN* indicator_ptr = app_desc_rec.indicator_ptr;
  SQLLEN* octet_length_ptr = app_desc_rec.octet_length_ptr;
  DataBuffer data = {target_c_type, app_buffer, app_buffer_len,
                     octet_length_ptr};

  StatusRecord status_record;
  if (bq_data_type == BQDataType::kInt64) {
    return ConvertFromArithmeticDSValue<SQLBIGINT>(ds_val, data);
  }
  if (bq_data_type == BQDataType::kFloat64) {
    return ConvertFromArithmeticDSValue<SQLDOUBLE>(ds_val, data);
  }
  if (bq_data_type == BQDataType::kString) {
    using google::cloud::odbc_bq_driver_internal::DSValueToString;
    std::string tmp_str;
    DSValueToString(ds_val, tmp_str);
    return ConvertFromStringDSValue(ds_val, data);
  }
  return {SQLStates::k_HYC00(), "Optional feature not implemented"};
}

StatusRecord WriteDSRow(DSRow& ds_row, RowSchema& schema,
                        DescriptorHandle& ard) {
  for (ColumnSchema& col_schema : schema) {
    int col_index = col_schema.col_index;
    DSValue& ds_val = ds_row[col_index];
    // Column is not bound.
    if (!ard.HasDescriptorRecord(col_index + 1)) {
      continue;
    }
    DescriptorRecord& col_desc = ard.GetDescriptorRecord(col_index + 1);
    StatusRecord status_record =
        WriteToApplicationBuffer(ds_val, col_schema.col_type, col_desc);
    if (!status_record.ok()) {
      return status_record;
    }
  }
  return StatusRecord::Ok();
}

StatusRecord WriteRowset(ResultSet& result_set, int rowset_size,
                         DescriptorHandle& ard) {
  int cursor = result_set.cursor;
  for (int i = cursor; i < cursor + rowset_size && i < result_set.rows.size();
       i++, result_set.cursor = i) {
    StatusRecord status_record =
        WriteDSRow(result_set.rows[i], result_set.row_schema, ard);
    if (!status_record.ok()) {
      return status_record;
    }
  }
  return StatusRecord::Ok();
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
    handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);

  ResultSet& result_set = handle.GetResultSet();
  if (result_set.cursor >= result_set.rows.size()) {
    return SQL_NO_DATA;
  }

  int rowset_size = ard.GetHeaderRecord().array_size;
  if (!rowset_size) {
    rowset_size = 1;
  }
  StatusRecord status_record = WriteRowset(result_set, rowset_size, ard);
  if (!status_record.ok()) {
    handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  return status_record.CalculateReturnCode();
}

// NOLINTEND(misc-unused-parameters)

}  // namespace google::cloud::odbc_bq_driver
