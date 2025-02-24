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

#include "google/cloud/odbc/bq_driver/internal/odbc_query.h"
#include "google/cloud/odbc/bq_driver/internal/data_translation.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bq_driver_internal::BQDataType;
using google::cloud::odbc_bq_driver_internal::ConvertFromArithmeticDSValue;
using google::cloud::odbc_bq_driver_internal::DataBuffer;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

StatusRecord GetColumnData(DSValue const& ds_val, BQDataType bq_data_type,
                           SQLSMALLINT target_c_type, SQLPOINTER target_value,
                           SQLLEN target_value_buffer_len,
                           SQLLEN* target_value_string_len) {
  StatusRecord status_record;
  if (IsDSValueNull(ds_val)) {
    if (target_value_string_len == nullptr) {
      return {SQLStates::k_22002(),
              "Indicator variable required but not supplied"};
    }
    *target_value_string_len = SQL_NULL_DATA;
    return StatusRecord::Ok();
  }
  // We need to reset the target_value_string_len once it has been set to
  // SQL_NULL_DATA for DSNullValues.
  if (target_value_string_len) {
    *target_value_string_len = ds_val.size();
  }

  DataBuffer data = {target_c_type, target_value, target_value_buffer_len,
                     target_value_string_len};

  // TODO(b/345194139): More data types would be added as they are implemented.
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
    case BQDataType::kTimeStamp:
    case BQDataType::kDatetime:
      status_record = ConvertFromTimestampDSValue(ds_val, data);
      break;
    case BQDataType::kInterval:
      status_record = ConvertFromIntervalDSValue(ds_val, data);
      break;
    case BQDataType::kBytes:
      status_record = ConvertFromBytesDSValue(ds_val,data);
      break;
    case BQDataType::kBool:
      status_record = ConvertFromBooleanDSValue(ds_val, data);
      break;
    default:
      status_record = {SQLStates::k_HYC00(), "Data type not supported"};
  }

  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
