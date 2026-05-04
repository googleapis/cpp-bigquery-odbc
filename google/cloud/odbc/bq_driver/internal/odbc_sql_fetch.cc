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
  LOG(INFO) << "WriteToApplicationBuffer:: Start (bq_data_type="
            << static_cast<int>(bq_data_type)
            << ", target_c_type=" << app_desc_rec.concise_type
            << ", bind_offset=" << bind_offset
            << ", bind_offset_ind=" << bind_offset_ind
            << ", data_ptr=" << static_cast<void*>(app_desc_rec.data_ptr)
            << ", octet_length=" << app_desc_rec.octet_length
            << ", ds_val.size()=" << ds_val.size() << ")";
  try {
    SQLSMALLINT target_c_type = app_desc_rec.concise_type;
    SQLPOINTER app_buffer = app_desc_rec.data_ptr;
    SQLLEN app_buffer_len = app_desc_rec.octet_length;
    SQLLEN* indicator_ptr = app_desc_rec.indicator_ptr;
    SQLLEN* octet_length_ptr = app_desc_rec.octet_length_ptr;

    if (app_buffer == nullptr) {
      LOG(ERROR) << "WriteToApplicationBuffer:: data_ptr is NULL; column not "
                    "bound? Skipping.";
      return StatusRecord::Ok();
    }

    app_buffer = reinterpret_cast<char*>(app_buffer) + bind_offset;
    if (indicator_ptr) {
      indicator_ptr = reinterpret_cast<SQLLEN*>(
          reinterpret_cast<char*>(indicator_ptr) + bind_offset_ind);
    }
    if (octet_length_ptr) {
      octet_length_ptr = reinterpret_cast<SQLLEN*>(
          reinterpret_cast<char*>(octet_length_ptr) + bind_offset_ind);
    }
    LOG(INFO) << "WriteToApplicationBuffer:: pointers offset; "
                 "app_buffer (post-offset)=" << app_buffer
              << ", indicator_ptr=" << static_cast<void*>(indicator_ptr)
              << ", octet_length_ptr=" << static_cast<void*>(octet_length_ptr);

    if (IsDSValueNull(ds_val)) {
      LOG(INFO) << "WriteToApplicationBuffer:: ds_val is NULL";
      if (indicator_ptr == nullptr) {
        LOG(ERROR) << "WriteToApplicationBuffer:: Indicator variable required "
                      "but not supplied for NULL data.";
        return {SQLStates::k_22002(),
                "Indicator variable required but not supplied"};
      }
      *indicator_ptr = SQL_NULL_DATA;
      LOG(INFO) << "WriteToApplicationBuffer:: wrote SQL_NULL_DATA to "
                   "indicator; returning";
      return StatusRecord::Ok();
    }
    // We need to reset the indicator_ptr once it has been set to SQL_NULL_DATA
    // for DSNullValues.
    if (indicator_ptr) {
      LOG(INFO) << "WriteToApplicationBuffer:: writing ds_val.size()="
                << ds_val.size() << " to indicator_ptr";
      *indicator_ptr = ds_val.size();
    }

    DataBuffer data = {target_c_type, app_buffer, app_buffer_len,
                       octet_length_ptr};
    LOG(INFO) << "WriteToApplicationBuffer:: dispatching by bq_data_type="
              << static_cast<int>(bq_data_type);
    StatusRecord status_record;
    switch (bq_data_type) {
      case BQDataType::kInt64:
        status_record = ConvertFromArithmeticDSValue<SQLBIGINT>(ds_val, data);
        break;
      case BQDataType::kFloat64:
        status_record = ConvertFromArithmeticDSValue<SQLDOUBLE>(ds_val, data);
        break;
      case BQDataType::kString:
        status_record = ConvertFromStringDSValue(ds_val, data);
        break;
      case BQDataType::kDate:
        status_record = ConvertFromDateDSValue(ds_val, data);
        break;
      case BQDataType::kTime:
        status_record = ConvertFromTimeDSValue(ds_val, data);
        break;
      case BQDataType::kJson:
        status_record = ConvertFromJsonDSValue(ds_val, data);
        break;
      case BQDataType::kStruct:
        status_record = ConvertFromStructDSValue(ds_val, data);
        break;
      case BQDataType::kArray:
        status_record = ConvertFromArrayDSValue(ds_val, data);
        break;
      case BQDataType::kTimeStamp:
        status_record = ConvertFromTimestampDSValue(ds_val, data);
        break;
      case BQDataType::kDatetime:
        status_record = ConvertFromDatetimeDSValue(ds_val, data);
        break;
      case BQDataType::kInterval:
        status_record = ConvertFromIntervalDSValue(ds_val, data);
        break;
      case BQDataType::kBool:
        status_record = ConvertFromBooleanDSValue(ds_val, data);
        break;
      case BQDataType::kGeography:
        status_record = ConvertFromGeographyDSValue(ds_val, data);
        break;
      case BQDataType::kBytes:
        status_record = ConvertFromBytesDSValue(ds_val, data);
        break;
      case BQDataType::kRange:
        status_record = ConvertFromRangeDSValue(ds_val, data);
        break;
      case BQDataType::kBigNumeric:
      case BQDataType::kNumeric:
        status_record = ConvertFromNumericDSValue(ds_val, data);
        break;
      default:
        LOG(ERROR) << "WriteToApplicationBuffer:: Data type not supported: "
                   << bq_data_type;
        return {SQLStates::k_HYC00(), "Data type not supported"};
    }
    LOG(INFO) << "WriteToApplicationBuffer:: end ok="
              << static_cast<int>(status_record.ok())
              << ", message='" << status_record.message << "'";
    return status_record;
  } catch (std::exception const& e) {
    LOG(ERROR) << "WriteToApplicationBuffer:: std::exception caught: what='"
               << e.what() << "'";
    return StatusRecord{
        SQLStates::k_HY000(),
        std::string("exception in WriteToApplicationBuffer: ") + e.what()};
  } catch (...) {
    LOG(ERROR) << "WriteToApplicationBuffer:: unknown exception caught";
    return StatusRecord{SQLStates::k_HY000(),
                        "Unknown exception in WriteToApplicationBuffer"};
  }
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
  LOG(INFO) << "WriteDSRow:: Start (row_num=" << row_num
            << ", schema cols=" << schema.size()
            << ", row vals=" << ds_row.size() << ")";
  try {
    SQLLEN* bind_offset_ptr = ard.GetHeaderRecord().bind_offset_ptr;
    SQLLEN bind_offset = 0;
    if (bind_offset_ptr) {
      bind_offset = *bind_offset_ptr;
    }
    LOG(INFO) << "WriteDSRow:: bind_offset_ptr="
              << static_cast<void*>(bind_offset_ptr)
              << ", bind_offset=" << bind_offset;

    for (ColumnSchema const& col_schema : schema) {
      int col_index = col_schema.col_index;
      LOG(INFO) << "WriteDSRow:: col_index=" << col_index
                << ", row size=" << ds_row.size();
      if (col_index < 0 ||
          static_cast<size_t>(col_index) >= ds_row.size()) {
        LOG(ERROR) << "WriteDSRow:: col_index out of range; skipping";
        continue;
      }
      DSValue const& ds_val = ds_row[col_index];
      // Column is not bound.
      if (!ard.HasDescriptorRecord(col_index + 1)) {
        LOG(INFO) << "WriteDSRow:: col_index=" << col_index
                  << " not bound; skipping";
        continue;
      }
      LOG(INFO) << "WriteDSRow:: col_index=" << col_index
                << " is bound; getting descriptor";
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
      LOG(INFO) << "WriteDSRow:: col=" << col_index
                << " elem_size=" << elem_size
                << " bind_type=" << bind_type
                << " row_offset=" << row_offset
                << " row_offset_ind=" << row_offset_ind;

      BQDataType bq_data_type = col_schema.col_type;
      if (col_schema.is_mode_repeated) {
        bq_data_type = BQDataType::kArray;
      }

      LOG(INFO) << "WriteDSRow:: col=" << col_index
                << " calling WriteToApplicationBuffer";
      StatusRecord status_record = WriteToApplicationBuffer(
          ds_val, bq_data_type, col_desc, bind_offset + row_offset,
          bind_offset + row_offset_ind);
      LOG(INFO) << "WriteDSRow:: col=" << col_index
                << " WriteToApplicationBuffer returned ok="
                << static_cast<int>(status_record.ok());
      if (!status_record.ok()) {
        LOG(ERROR) << "WriteDSRow::WriteToApplicationBuffer:: "
                   << status_record.message;
        return status_record;
      }
    }
    LOG(INFO) << "WriteDSRow:: end";
    return StatusRecord::Ok();
  } catch (std::exception const& e) {
    LOG(ERROR) << "WriteDSRow:: std::exception caught: what='" << e.what()
               << "'";
    return StatusRecord{SQLStates::k_HY000(),
                        std::string("exception in WriteDSRow: ") + e.what()};
  } catch (...) {
    LOG(ERROR) << "WriteDSRow:: unknown exception caught";
    return StatusRecord{SQLStates::k_HY000(),
                        "Unknown exception in WriteDSRow"};
  }
}

StatusRecord WriteRowset(ResultSet const& result_set, int const rowset_size,
                         DescriptorHandle& ard, DescriptorHandle& ird) {
  LOG(INFO) << "WriteRowset:: Start (rowset_size=" << rowset_size
            << ", cursor=" << result_set.cursor
            << ", rows.size()=" << result_set.rows.size() << ")";
  try {
    if (rowset_size <= 0) {
      LOG(ERROR) << "WriteRowset:: rowset_size should not be <= 0";
      StatusRecord status_record = {SQLStates::k_HY000(),
                                    "rowset_size should not be <= 0"};
      return status_record;
    }
    int cursor = result_set.cursor;
    int row_counter = 0;
    SQLUSMALLINT* row_status_ptr = ird.GetHeaderRecord().array_status_ptr;
    LOG(INFO) << "WriteRowset:: row_status_ptr="
              << static_cast<void*>(row_status_ptr);
    // We write 'rowset_size' rows from result_set.rows starting at the index
    // 'cursor'
    for (int i = cursor; i < cursor + rowset_size && i < result_set.rows.size();
         i++, row_counter++) {
      LOG(INFO) << "WriteRowset:: writing row " << i << " (offset="
                << (i - cursor) << ")";
      StatusRecord status_record = WriteDSRow(
          result_set.rows[i], result_set.row_schema, ard, i - cursor);
      LOG(INFO) << "WriteRowset:: WriteDSRow row " << i << " returned ok="
                << static_cast<int>(status_record.ok());
      if (!status_record.ok()) {
        LOG(ERROR) << "WriteRowset::WriteDSRow:: " << status_record.message;
        return status_record;
      }

      if (row_status_ptr) {
        row_status_ptr[i - cursor] = SQL_ROW_SUCCESS;
      }

      result_set.cursor = i;
    }
    LOG(INFO) << "WriteRowset:: wrote " << row_counter << " rows";

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
    LOG(INFO) << "WriteRowset:: end";
    return StatusRecord::Ok();
  } catch (std::exception const& e) {
    LOG(ERROR) << "WriteRowset:: std::exception caught: what='" << e.what()
               << "'";
    return StatusRecord{SQLStates::k_HY000(),
                        std::string("exception in WriteRowset: ") + e.what()};
  } catch (...) {
    LOG(ERROR) << "WriteRowset:: unknown exception caught";
    return StatusRecord{SQLStates::k_HY000(),
                        "Unknown exception in WriteRowset"};
  }
}

StatusRecord FetchNextResultSet(StatementHandle& stmt_handle) {
  // We need to return only the top `SQL_ATTR_MAX_ROWS` number of rows
  auto max_rows_status = stmt_handle.GetAttribute(SQL_ATTR_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "FetchNextResultSet:: "
               << max_rows_status.GetStatusRecord().message;
    return max_rows_status.GetStatusRecord();
  }
  SQLULEN max_rows = *max_rows_status;
  ResultSet& result_set = stmt_handle.GetResultSet();
  int num_rows_fetched_yet = result_set.num_rows_fetched_yet;
  int num_rows_to_be_fetched = max_rows - num_rows_fetched_yet;
  if (max_rows > 0 && num_rows_to_be_fetched <= 0) {
    LOG(INFO) << "FetchNextResultSet:: SQL_ATTR_MAX_ROWS limit reached.";
    return StatusRecord(
        {SQLStates::k_SQL_NO_DATA(), "SQL_ATTR_MAX_ROWS limit reached."});
  }
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
  if (stmt_handle.WasHtapiEnabled()) {
    StatusRecord read_status = ReadNextResultsFromStream(stmt_handle);
    if (!read_status.ok()) {
      LOG(ERROR) << "ReadNextResultsFromStream:: " << read_status.message;
      return read_status;
    }
  } else {
    StatusRecord read_status = FetchNextPageResultSet(stmt_handle);
    if (!read_status.ok()) {
      LOG(ERROR) << "FetchNextPageResultSet:: " << read_status.message;
      return read_status;
    }
  }
#else

  StatusRecord read_status = FetchNextPageResultSet(stmt_handle);
  if (!read_status.ok()) {
    LOG(ERROR) << "FetchNextPageResultSet:: " << read_status.message;
    return read_status;
  }
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
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
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
