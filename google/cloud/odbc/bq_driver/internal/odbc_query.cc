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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

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
      LOG(ERROR) << "GetColumnData:: Indicator variable required but not "
                    "supplied for NULL data.";
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
      return ConvertFromArithmeticDSValue<SQLBIGINT>(ds_val, data);
    case BQDataType::kBigNumeric:
    case BQDataType::kNumeric:
      return ConvertFromNumericDSValue(ds_val, data);
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
    case BQDataType::kTimeStamp:
      return ConvertFromTimestampDSValue(ds_val, data);
    case BQDataType::kDatetime:
      return ConvertFromDatetimeDSValue(ds_val, data);
    case BQDataType::kInterval:
      return ConvertFromIntervalDSValue(ds_val, data);
    case BQDataType::kGeography:
      return ConvertFromGeographyDSValue(ds_val, data);
    case BQDataType::kBytes:
      return ConvertFromBytesDSValue(ds_val, data);
    case BQDataType::kArray:
      return ConvertFromArrayDSValue(ds_val, data);
    case BQDataType::kBool:
      return ConvertFromBooleanDSValue(ds_val, data);
    case BQDataType::kRange:
      return ConvertFromRangeDSValue(ds_val, data);
    default:
      LOG(ERROR) << "GetColumnData:: Data type is not supported: "
                 << bq_data_type;
      return {SQLStates::k_HYC00(), "Data type is not supported"};
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
