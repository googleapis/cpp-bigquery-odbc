// Copyright 2024 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fetch.h"
#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

StatusRecord WriteToApplicationBuffer(DSValue const& ds_val,
                                      BQDataType bq_data_type,
                                      DescriptorRecord& app_desc_rec,
                                      SQLLEN bind_offset,
                                      SQLLEN bind_offset_ind) {
  SQLSMALLINT target_c_type = app_desc_rec.concise_type;
  SQLPOINTER app_buffer = app_desc_rec.data_ptr;
  SQLLEN app_buffer_len = app_desc_rec.octet_length;
  SQLLEN* indicator_ptr = app_desc_rec.indicator_ptr;
  SQLLEN* octet_length_ptr = app_desc_rec.octet_length_ptr;

  app_buffer = reinterpret_cast<char*>(app_buffer) + bind_offset;
  if (indicator_ptr) {
    indicator_ptr = reinterpret_cast<SQLLEN*>(
        reinterpret_cast<char*>(indicator_ptr) + bind_offset_ind);
  }
  if (octet_length_ptr) {
    octet_length_ptr = reinterpret_cast<SQLLEN*>(
        reinterpret_cast<char*>(octet_length_ptr) + bind_offset_ind);
  }

  if (IsDSValueNull(ds_val)) {
    LOG(ERROR) << "WriteToApplicationBuffer:: Indicator variable required but "
                  "not supplied for NULL data.";
    if (indicator_ptr == nullptr) {
      return {SQLStates::k_22002(),
              "Indicator variable required but not supplied"};
    }
    *indicator_ptr = SQL_NULL_DATA;
    return StatusRecord::Ok();
  }
  // We need to reset the indicator_ptr once it has been set to SQL_NULL_DATA
  // for DSNullValues.
  if (indicator_ptr) {
    *indicator_ptr = ds_val.size();
  }

  DataBuffer data = {target_c_type, app_buffer, app_buffer_len,
                     octet_length_ptr};
  StatusRecord status_record;
  switch (bq_data_type) {
    case BQDataType::kInt64:
      return ConvertFromArithmeticDSValue<SQLBIGINT>(ds_val, data);
    case BQDataType::kFloat64:
      return ConvertFromArithmeticDSValue<SQLDOUBLE>(ds_val, data);
    case BQDataType::kString:
      return ConvertFromStringDSValue(ds_val, data);
    case BQDataType::kDate:
      return ConvertFromDateDSValue(ds_val, data);
    case BQDataType::kTime:
      return ConvertFromTimeDSValue(ds_val, data);
    case BQDataType::kJson:
      return ConvertFromJsonDSValue(ds_val, data);
    case BQDataType::kStruct:
      return ConvertFromStructDSValue(ds_val, data);
    case BQDataType::kArray:
      return ConvertFromArrayDSValue(ds_val, data);
    case BQDataType::kTimeStamp:
      return ConvertFromTimestampDSValue(ds_val, data);
    case BQDataType::kDatetime:
      return ConvertFromDatetimeDSValue(ds_val, data);
    case BQDataType::kInterval:
      return ConvertFromIntervalDSValue(ds_val, data);
    case BQDataType::kBool:
      return ConvertFromBooleanDSValue(ds_val, data);
    case BQDataType::kGeography:
      return ConvertFromGeographyDSValue(ds_val, data);
    case BQDataType::kBytes:
      return ConvertFromBytesDSValue(ds_val, data);
    case BQDataType::kRange:
      return ConvertFromRangeDSValue(ds_val, data);
    case BQDataType::kBigNumeric:
    case BQDataType::kNumeric:
      return ConvertFromNumericDSValue(ds_val, data);
  }
  LOG(ERROR) << "WriteToApplicationBuffer:: Data type not supported: "
             << bq_data_type;
  return {SQLStates::k_HYC00(), "Data type not supported"};
}

// This is according to the spec:
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindcol-function?view=sql-server-ver16#buffer-addresses
SQLLEN GetElemSize(DescriptorRecord& app_desc_rec) {
  SQLSMALLINT target_c_type = app_desc_rec.concise_type;
  SQLLEN app_buffer_len = app_desc_rec.octet_length;
  switch (target_c_type) {
    case SQL_C_CHAR:
    case SQL_C_WCHAR:
    case SQL_C_BINARY:
      return app_buffer_len;
    case SQL_C_SSHORT:
      return sizeof(SQLSMALLINT);
    case SQL_C_USHORT:
      return sizeof(SQLUSMALLINT);
    case SQL_C_SLONG:
      return sizeof(SQLINTEGER);
    case SQL_C_ULONG:
      return sizeof(SQLUINTEGER);
    case SQL_C_FLOAT:
      return sizeof(SQLREAL);
    case SQL_C_DOUBLE:
      return sizeof(SQLDOUBLE);
    case SQL_C_BIT:
      return sizeof(SQLCHAR);
    case SQL_C_STINYINT:
      return sizeof(SQLSCHAR);
    case SQL_C_UTINYINT:
      return sizeof(SQLCHAR);
    case SQL_C_SBIGINT:
      return sizeof(SQLBIGINT);
    case SQL_C_UBIGINT:
      return sizeof(SQLUBIGINT);
    case SQL_C_NUMERIC:
      return sizeof(SQL_NUMERIC_STRUCT);
    case SQL_C_TYPE_DATE:
      return sizeof(SQL_DATE_STRUCT);
    case SQL_C_TYPE_TIME:
      return sizeof(SQL_TIME_STRUCT);
    case SQL_C_TYPE_TIMESTAMP:
      return sizeof(SQL_TIMESTAMP_STRUCT);
    default:
      return 0;
  }
}

StatusRecord WriteDSRow(DSRow const& ds_row, RowSchema const& schema,
                        DescriptorHandle& ard, int row_num) {
  SQLLEN* bind_offset_ptr = ard.GetHeaderRecord().bind_offset_ptr;
  SQLLEN bind_offset = 0;
  if (bind_offset_ptr) {
    bind_offset = *bind_offset_ptr;
  }

  for (ColumnSchema const& col_schema : schema) {
    int col_index = col_schema.col_index;
    DSValue const& ds_val = ds_row[col_index];
    // Column is not bound.
    if (!ard.HasDescriptorRecord(col_index + 1)) {
      continue;
    }
    DescriptorRecord& col_desc = ard.GetDescriptorRecord(col_index + 1);

    SQLLEN elem_size, elem_size_ind;
    SQLINTEGER bind_type = ard.GetHeaderRecord().bind_type;
    if (bind_type == SQL_BIND_BY_COLUMN) {
      elem_size = GetElemSize(col_desc);
      elem_size_ind = sizeof(SQLLEN);
    } else {
      elem_size = bind_type;
      elem_size_ind = bind_type;
    }
    SQLLEN row_offset = row_num * elem_size;
    SQLLEN row_offset_ind = row_num * elem_size_ind;

    BQDataType bq_data_type = col_schema.col_type;
    if (col_schema.is_mode_repeated) {
      bq_data_type = BQDataType::kArray;
    }

    StatusRecord status_record = WriteToApplicationBuffer(
        ds_val, bq_data_type, col_desc, bind_offset + row_offset,
        bind_offset + row_offset_ind);
    if (!status_record.ok()) {
      LOG(ERROR) << "WriteDSRow::WriteToApplicationBuffer:: "
                 << status_record.message;
      return status_record;
    }
  }
  return StatusRecord::Ok();
}

StatusRecord WriteRowset(ResultSet const& result_set, int const rowset_size,
                         DescriptorHandle& ard, DescriptorHandle& ird) {
  if (rowset_size <= 0) {
    LOG(ERROR) << "WriteRowset:: rowset_size should not be <= 0";
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "rowset_size should not be <= 0"};
    return status_record;
  }
  int cursor = result_set.cursor;
  int row_counter = 0;
  SQLUSMALLINT* row_status_ptr = ird.GetHeaderRecord().array_status_ptr;
  // We write 'rowset_size' rows from result_set.rows starting at the index
  // 'cursor'
  for (int i = cursor; i < cursor + rowset_size && i < result_set.rows.size();
       i++, row_counter++) {
    StatusRecord status_record =
        WriteDSRow(result_set.rows[i], result_set.row_schema, ard, i - cursor);
    if (!status_record.ok()) {
      LOG(ERROR) << "WriteRowset::WriteDSRow:: " << status_record.message;
      return status_record;
    }

    if (row_status_ptr) {
      row_status_ptr[i - cursor] = SQL_ROW_SUCCESS;
    }

    result_set.cursor = i;
  }

  // Mark unused rows
  if (row_status_ptr) {
    for (int i = row_counter; i < rowset_size; i++) {
      row_status_ptr[i] = SQL_ROW_NOROW;
    }
  }

  SQLULEN* rows_processed_ptr = ird.GetHeaderRecord().rows_processed_ptr;
  if (rows_processed_ptr) {
    *rows_processed_ptr = row_counter;
  }

  return StatusRecord::Ok();
}

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
StatusRecord FetchNextResultSet(StatementHandle& stmt_handle) {
  // In case of non-HTAPI execution there is no pagination, so we have to return
  // `SQL_NO_DATA`
  if (!stmt_handle.GetConnectionHandle()->GetDsn().allow_htapi) {
    return StatusRecord(
        {SQLStates::k_SQL_NO_DATA(), "No more data to return."});
  }
  // We need to return only the top `SQL_ATTR_MAX_ROWS` number of rows
  auto max_rows_status = stmt_handle.GetAttribute(SQL_ATTR_MAX_ROWS);
  if (!max_rows_status) {
    return max_rows_status.GetStatusRecord();
  }
  SQLULEN max_rows = *max_rows_status;
  int num_rows_fetched_yet = stmt_handle.GetResultSet().num_rows_fetched_yet;
  int num_rows_to_be_fetched = max_rows - num_rows_fetched_yet;
  if (max_rows > 0 && num_rows_to_be_fetched <= 0) {
    LOG(INFO) << "FetchNextResultSet:: SQL_ATTR_MAX_ROWS limit reached.";
    return StatusRecord(
        {SQLStates::k_SQL_NO_DATA(), "SQL_ATTR_MAX_ROWS limit reached."});
  }

  // We clear the existing rows so they can be replaced by the new batch.
  stmt_handle.GetResultSet().rows.clear();
  StatusRecordOr<ResultSet> read_status =
      ReadNextResultsFromStream(stmt_handle);
  if (!read_status) {
    stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
    return read_status.GetStatusRecord();
  }
  DSResults results;
  results.data_source_results = *read_status;
  stmt_handle.SetDSResults(results);
  auto rs_status_record_or = ProcessQueryResults(results);
  if (!rs_status_record_or) {
    stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
    LOG(ERROR) << "FetchNextResultSet:: "
               << rs_status_record_or.GetStatusRecord().message;
    return rs_status_record_or.GetStatusRecord();
  }

  ResultSet& result_set = *rs_status_record_or;
  auto& rs_rows = result_set.rows;
  if (rs_rows.empty()) {
    LOG(INFO) << "FetchNextResultSet:: Empty result set fetched.";
    return StatusRecord(
        {SQLStates::k_SQL_NO_DATA(), "Empty result set fetched."});
  }
  if (max_rows > 0 && num_rows_to_be_fetched < rs_rows.size()) {
    rs_rows.erase(rs_rows.begin() + num_rows_to_be_fetched, rs_rows.end());
  }
  stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  stmt_handle.SetResultSet(result_set);
  return StatusRecord::Ok();
}
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

}  // namespace google::cloud::odbc_bq_driver_internal
