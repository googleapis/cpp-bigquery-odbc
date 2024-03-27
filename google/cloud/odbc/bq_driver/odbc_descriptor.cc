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

#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::HeaderRecord;
using google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

static std::map<DescriptorType, std::set<int>> const kAllowedFieldsToSet = {
    {DescriptorType::kApplication,
     {SQL_DESC_ARRAY_SIZE, SQL_DESC_ARRAY_STATUS_PTR, SQL_DESC_BIND_OFFSET_PTR,
      SQL_DESC_BIND_TYPE, SQL_DESC_COUNT, SQL_DESC_CONCISE_TYPE,
      SQL_DESC_DATA_PTR, SQL_DESC_DATETIME_INTERVAL_CODE,
      SQL_DESC_DATETIME_INTERVAL_PRECISION, SQL_DESC_INDICATOR_PTR,
      SQL_DESC_LENGTH, SQL_DESC_NUM_PREC_RADIX, SQL_DESC_OCTET_LENGTH,
      SQL_DESC_OCTET_LENGTH_PTR, SQL_DESC_PRECISION, SQL_DESC_SCALE,
      SQL_DESC_TYPE}},
    {DescriptorType::kIRD,
     {SQL_DESC_ARRAY_STATUS_PTR, SQL_DESC_ROWS_PROCESSED_PTR}},
    {DescriptorType::kIPD,
     {SQL_DESC_ARRAY_STATUS_PTR, SQL_DESC_COUNT, SQL_DESC_ROWS_PROCESSED_PTR,
      SQL_DESC_CONCISE_TYPE, SQL_DESC_DATETIME_INTERVAL_CODE,
      SQL_DESC_DATETIME_INTERVAL_PRECISION, SQL_DESC_LENGTH, SQL_DESC_NAME,
      SQL_DESC_NUM_PREC_RADIX, SQL_DESC_OCTET_LENGTH, SQL_DESC_PARAMETER_TYPE,
      SQL_DESC_PRECISION, SQL_DESC_SCALE, SQL_DESC_TYPE, SQL_DESC_UNNAMED}}};

SQLRETURN SetCount(DescriptorHandle* handle, std::size_t desc_int_value) {
  auto new_val = static_cast<SQLSMALLINT>(desc_int_value);
  if (new_val < 0) {
    handle->GetDiagnostics().AddStatusRecord(
        StatusRecord{SQLStates::k_07009(), "Invalid descriptor index"});
    return SQL_ERROR;
  }
  int old_val = handle->GetHeaderRecord().count;
  // Unbind everything, which is greater than new SQL_DESC_COUNT
  if (new_val < old_val) {
    for (int i = new_val + 1; i <= old_val; i++) {
      // Skipping positive/negative results for speed
      handle->UnbindDescriptorRecord(i);
    }
  }
  handle->GetHeaderRecord().count = new_val;
  return SQL_SUCCESS;
}

SQLRETURN SetName(DescriptorHandle* handle, DescriptorRecord& descriptor_record,
                  SQLPOINTER desc_value, SQLINTEGER buffer_len) {
  if (buffer_len > SQL_MAX_IDENTIFIER_LEN) {
    handle->GetDiagnostics().AddStatusRecord(
        {SQLStates::k_22001(), "String data, right truncated"});
    return SQL_ERROR;
  }
  if (buffer_len < 0 && buffer_len != SQL_NTS) {
    handle->GetDiagnostics().AddStatusRecord(
        {SQLStates::k_HY090(), "Invalid buffer length"});
    return SQL_ERROR;
  }
  if (desc_value) {
    descriptor_record.SetName(reinterpret_cast<char*>(desc_value), buffer_len);
  } else {
    // nullptr equals to empty string
    descriptor_record.SetName("", 0);
  }
  return SQL_SUCCESS;
}

SQLRETURN SetNumPrecRadix(DescriptorHandle* handle,
                          DescriptorRecord& descriptor_record,
                          std::size_t desc_int_value) {
  auto value = static_cast<SQLINTEGER>(desc_int_value);
  if (value != 0 && value != 2 && value != 10) {
    handle->GetDiagnostics().AddStatusRecord(StatusRecord{
        SQLStates::k_HY092(), "Invalid attribute/option identifier"});
    return SQL_ERROR;
  }
  descriptor_record.num_prec_radix = value;
  return SQL_SUCCESS;
}

SQLRETURN SetParameterType(DescriptorHandle* handle,
                           DescriptorRecord& descriptor_record,
                           std::size_t desc_int_value) {
  auto value = static_cast<SQLSMALLINT>(desc_int_value);
  if (value != SQL_PARAM_INPUT && value != SQL_PARAM_INPUT_OUTPUT &&
      value != SQL_PARAM_OUTPUT) {
    handle->GetDiagnostics().AddStatusRecord(
        StatusRecord{SQLStates::k_HY105(), "Invalid parameter type"});
    return SQL_ERROR;
  }
  descriptor_record.parameter_type = value;
  return SQL_SUCCESS;
}

SQLRETURN SetUnnamed(DescriptorHandle* handle,
                     DescriptorRecord& descriptor_record,
                     std::size_t desc_int_value) {
  auto value = static_cast<SQLSMALLINT>(desc_int_value);
  if (value != SQL_UNNAMED) {
    handle->GetDiagnostics().AddStatusRecord(StatusRecord{
        SQLStates::k_HY091(), "Invalid descriptor field identifier"});
    return SQL_ERROR;
  }
  descriptor_record.unnamed = value;
  return SQL_SUCCESS;
}

SQLRETURN SQLSetDescFieldInternal(SQLHDESC descriptor_handle,
                                  SQLSMALLINT rec_number,
                                  SQLSMALLINT field_identifier,
                                  SQLPOINTER desc_value,
                                  SQLINTEGER desc_value_buffer_len) {
  StatusRecordOr<DescriptorHandle*> handle_result =
      ValidateDescriptorHandle(descriptor_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOptsConsole),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  DescriptorHandle* handle = *handle_result;

  std::set<int> set = kAllowedFieldsToSet.at(handle->GetType());
  if (set.find(field_identifier) == set.end()) {
    handle->GetDiagnostics().AddStatusRecord(
        {SQLStates::k_HY091(), "Invalid descriptor field identifier"});
    return SQL_ERROR;
  }

  auto desc_int_value = reinterpret_cast<size_t>(desc_value);

  // HeaderRecord fields
  HeaderRecord& header_record = handle->GetHeaderRecord();
  switch (field_identifier) {
    case SQL_DESC_ARRAY_SIZE:
      header_record.array_size = static_cast<SQLULEN>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_ARRAY_STATUS_PTR:
      header_record.array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(desc_value);
      return SQL_SUCCESS;
    case SQL_DESC_BIND_OFFSET_PTR:
      header_record.bind_offset_ptr = reinterpret_cast<SQLLEN*>(desc_value);
      return SQL_SUCCESS;
    case SQL_DESC_BIND_TYPE:
      header_record.bind_type = static_cast<SQLINTEGER>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_COUNT:
      return SetCount(handle, desc_int_value);
    case SQL_DESC_ROWS_PROCESSED_PTR:
      header_record.rows_processed_ptr = reinterpret_cast<SQLULEN*>(desc_value);
      return SQL_SUCCESS;
  }

  if (rec_number < 0) {
    handle->GetDiagnostics().AddStatusRecord(
        StatusRecord{SQLStates::k_07009(), "Invalid descriptor index"});
    return SQL_ERROR;
  }

  if (!handle->HasDescriptorRecord(rec_number)) {
    DescriptorRecord new_descriptor_record;
    handle->BindNewDescriptorRecord(rec_number, new_descriptor_record);
  }

  // DescriptorRecord fields
  DescriptorRecord& descriptor_record = handle->GetDescriptorRecord(rec_number);
  switch (field_identifier) {
    case SQL_DESC_CONCISE_TYPE:
      // TODO(331355556) set SQL_DESC_TYPE and SQL_DESC_DATETIME_INTERVAL_CODE
      descriptor_record.concise_type = static_cast<SQLSMALLINT>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_DATA_PTR:
      descriptor_record.data_ptr = desc_value;
      // TODO(331356705) run consistency check
      return SQL_SUCCESS;
    case SQL_DESC_DATETIME_INTERVAL_CODE:
      // TODO(331355556) set SQL_DESC_TYPE and SQL_DESC_CONCISE_TYPE
      descriptor_record.concise_type = static_cast<SQLSMALLINT>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_DATETIME_INTERVAL_PRECISION:
      descriptor_record.datetime_interval_precision =
          static_cast<SQLINTEGER>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_INDICATOR_PTR:
      descriptor_record.indicator_ptr = reinterpret_cast<SQLLEN*>(desc_value);
      return SQL_SUCCESS;
    case SQL_DESC_LENGTH:
      descriptor_record.length = static_cast<SQLULEN>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_NAME:
      return SetName(handle, descriptor_record, desc_value,
                     desc_value_buffer_len);
    case SQL_DESC_NUM_PREC_RADIX:
      return SetNumPrecRadix(handle, descriptor_record, desc_int_value);
    case SQL_DESC_OCTET_LENGTH:
      descriptor_record.octet_length = static_cast<SQLLEN>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_OCTET_LENGTH_PTR:
      descriptor_record.octet_length_ptr =
          reinterpret_cast<SQLLEN*>(desc_value);
      return SQL_SUCCESS;
    case SQL_DESC_PARAMETER_TYPE:
      return SetParameterType(handle, descriptor_record, desc_int_value);
    case SQL_DESC_PRECISION:
      descriptor_record.precision = static_cast<SQLSMALLINT>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_SCALE:
      descriptor_record.scale = static_cast<SQLSMALLINT>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_TYPE:
      // TODO(331355556) set SQL_DESC_DATETIME_INTERVAL_CODE and
      // SQL_DESC_CONCISE_TYPE
      descriptor_record.type = static_cast<SQLSMALLINT>(desc_int_value);
      return SQL_SUCCESS;
    case SQL_DESC_UNNAMED:
      return SetUnnamed(handle, descriptor_record, desc_int_value);
  }

  handle->GetDiagnostics().AddStatusRecord(StatusRecord{
      SQLStates::k_HY091(), "Invalid descriptor field identifier"});
  return SQL_ERROR;
}

}  // namespace google::cloud::odbc_bq_driver
