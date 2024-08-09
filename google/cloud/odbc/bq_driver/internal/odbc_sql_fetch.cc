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

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

StatusRecord WriteToApplicationBuffer(DSValue const& ds_val,
                                      BQDataType bq_data_type,
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
  switch (bq_data_type) {
    case BQDataType::kInt64:
      return ConvertFromArithmeticDSValue<SQLBIGINT>(ds_val, data);
    case BQDataType::kFloat64:
      return ConvertFromArithmeticDSValue<SQLDOUBLE>(ds_val, data);
    case BQDataType::kString:
      return ConvertFromStringDSValue(ds_val, data);
  }
  return {SQLStates::k_HYC00(), "Data type not supported"};
}

StatusRecord WriteDSRow(DSRow const& ds_row, RowSchema const& schema,
                        DescriptorHandle& ard) {
  for (ColumnSchema const& col_schema : schema) {
    int col_index = col_schema.col_index;
    DSValue const& ds_val = ds_row[col_index];
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

StatusRecord WriteRowset(ResultSet const& result_set, int rowset_size,
                         DescriptorHandle& ard) {
  if (rowset_size <= 0) {
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "rowset_size should not be <= 0"};
    return status_record;
  }
  int cursor = result_set.cursor;
  // We write 'rowset_size' rows from result_set.rows starting at the index
  // 'cursor'
  for (int i = cursor; i < cursor + rowset_size && i < result_set.rows.size();
       i++) {
    StatusRecord status_record =
        WriteDSRow(result_set.rows[i], result_set.row_schema, ard);
    if (!status_record.ok()) {
      return status_record;
    }
    result_set.cursor = i;
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
