// Copyright 2025 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/data_translation_inv.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DoubleStrToInt;
using google::cloud::odbc_internal::SQLStates;

StatusRecord ConstructPositionalQueryParams(DescriptorHandle& apd, DescriptorHandle& ipd, std::vector<QueryParameter>& basic_query_params) {
  for (int param_ind = 0; param_ind < basic_query_params.size(); param_ind++) {
    if (!apd.HasDescriptorRecord(param_ind + 1)) {
      return StatusRecord{SQLStates::k_07002(), "Expected descriptor record does not exist during query execution."};
    }
    DescriptorRecord& apd_rec = apd.GetDescriptorRecord(param_ind + 1);
    SQLSMALLINT c_type = apd_rec.concise_type;
    SQLPOINTER app_buffer = apd_rec.data_ptr;
    SQLLEN app_buffer_len = apd_rec.octet_length;
    SQLLEN* octet_length_ptr = apd_rec.octet_length_ptr;
    SQLLEN* indicator_ptr = apd_rec.indicator_ptr;
    // SQL_NULL_DATA implies the application wants to use empty data.
    if (indicator_ptr != nullptr && *indicator_ptr == SQL_NULL_DATA) {
      continue;
    }
    if(app_buffer == nullptr) {
      return StatusRecord{SQLStates::k_HY009(), "The bound param buffer was null"};
    }
    
    DataBuffer data = {c_type, app_buffer, app_buffer_len,
                     octet_length_ptr};

    DescriptorRecord& ipd_rec = ipd.GetDescriptorRecord(param_ind + 1);
    if (!ipd.HasDescriptorRecord(param_ind + 1)) {
      return StatusRecord{SQLStates::k_07002(), "Expected descriptor record does not exist during query execution."};
    }
    SQLSMALLINT sql_type = ipd_rec.concise_type;
    StatusRecordOr<std::string> conv_status = ConvertFromBuffer(data, sql_type);
    if (!conv_status) {
      return conv_status.GetStatusRecord();
    }
    std::string& value_str = *conv_status;
    // "INT64" is a special case where a string like "23.000" will not be
    // accepted by the BQ Server. For ex, this may occur when translating from
    // SQL_C_CHAR->SQL_DOUBLE.
    if (basic_query_params[param_ind].parameter_type.type == "INT64") {
      StatusRecord status = DoubleStrToInt(value_str);
      if(!status.ok()) {
        return status;
      }
    }
    basic_query_params[param_ind].parameter_value.value = value_str;
  }
  return StatusRecord::Ok();
}

}
