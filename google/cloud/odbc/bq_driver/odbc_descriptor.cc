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
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
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
      SQL_DESC_CONCISE_TYPE, SQL_DESC_DATA_PTR, SQL_DESC_DATETIME_INTERVAL_CODE,
      SQL_DESC_DATETIME_INTERVAL_PRECISION, SQL_DESC_LENGTH, SQL_DESC_NAME,
      SQL_DESC_NUM_PREC_RADIX, SQL_DESC_OCTET_LENGTH, SQL_DESC_PARAMETER_TYPE,
      SQL_DESC_PRECISION, SQL_DESC_SCALE, SQL_DESC_TYPE, SQL_DESC_UNNAMED}}};

SQLRETURN SQLAllocDescHandle(SQLHANDLE in_handle, SQLHANDLE* out_desc_handle) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(in_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOptsConsole),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  // TODO(b/308645203) Associate Descriptor Handle with a Connection Handle

  auto* desc_handle = new DescriptorHandle();
  desc_handle->GetHeaderRecord().alloc_type = SQL_DESC_ALLOC_USER;
  auto* wrapped_handle =
      new HandleWrapped(HandleType::kStatementHandle, desc_handle);
  *out_desc_handle = wrapped_handle;
  return SQL_SUCCESS;
}

SQLRETURN SetCount(DescriptorHandle* handle, std::size_t desc_int_value) {
  auto new_val = static_cast<SQLSMALLINT>(desc_int_value);
  if (new_val >= handle->GetHeaderRecord().count) {
    handle->GetHeaderRecord().count = new_val;
    return SQL_SUCCESS;
  }
  StatusRecord status_record = handle->UnbindAllDescriptorRecordsFrom(
      static_cast<SQLSMALLINT>(desc_int_value));
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

SQLRETURN SetName(DescriptorHandle* handle, DescriptorRecord& descriptor_record,
                  SQLPOINTER desc_value, SQLINTEGER buffer_len) {
  if (buffer_len > SQL_MAX_IDENTIFIER_LEN) {
    StatusRecord status_record{SQLStates::k_22001(),
                               "String data, right truncated"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }
  if (buffer_len < 0 && buffer_len != SQL_NTS) {
    StatusRecord status_record{SQLStates::k_HY090(), "Invalid buffer length"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
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
  StatusRecord status_record = descriptor_record.SetNumPrecRadix(value);
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

SQLRETURN SetParameterType(DescriptorHandle* handle,
                           DescriptorRecord& descriptor_record,
                           std::size_t desc_int_value) {
  auto value = static_cast<SQLSMALLINT>(desc_int_value);
  StatusRecord status_record = descriptor_record.SetParameterType(value);
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

SQLRETURN SetUnnamed(DescriptorHandle* handle,
                     DescriptorRecord& descriptor_record,
                     std::size_t desc_int_value) {
  auto value = static_cast<SQLSMALLINT>(desc_int_value);
  StatusRecord status_record = descriptor_record.SetUnnamed(value);
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

SQLRETURN SetType(DescriptorHandle* handle, DescriptorRecord& descriptor_record,
                  std::size_t desc_int_value) {
  StatusRecord status_record = descriptor_record.SetType(
      static_cast<SQLSMALLINT>(desc_int_value), handle->GetType());
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

SQLRETURN SetConciseType(DescriptorHandle* handle,
                         DescriptorRecord& descriptor_record,
                         std::size_t desc_int_value) {
  StatusRecord status_record = descriptor_record.SetConciseType(
      static_cast<SQLSMALLINT>(desc_int_value));
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

SQLRETURN SetDataPointer(DescriptorHandle* handle,
                         DescriptorRecord& descriptor_record,
                         SQLPOINTER data_ptr) {
  StatusRecord status_record =
      descriptor_record.SetDataPointer(data_ptr, handle->GetType());
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
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
    StatusRecord status_record{SQLStates::k_HY091(),
                               "Invalid descriptor field identifier"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
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
    StatusRecord status_record{SQLStates::k_07009(),
                               "Invalid descriptor index"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  if (!handle->HasDescriptorRecord(rec_number)) {
    DescriptorRecord new_descriptor_record;
    handle->BindNewDescriptorRecord(rec_number, new_descriptor_record);
  }

  // DescriptorRecord fields
  DescriptorRecord& descriptor_record = handle->GetDescriptorRecord(rec_number);
  switch (field_identifier) {
    case SQL_DESC_CONCISE_TYPE:
      return SetConciseType(handle, descriptor_record, desc_int_value);
    case SQL_DESC_DATA_PTR:
      return SetDataPointer(handle, descriptor_record, desc_value);
    case SQL_DESC_DATETIME_INTERVAL_CODE:
      descriptor_record.datetime_interval_code =
          static_cast<SQLSMALLINT>(desc_int_value);
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
      return SetType(handle, descriptor_record, desc_int_value);
    case SQL_DESC_UNNAMED:
      return SetUnnamed(handle, descriptor_record, desc_int_value);
  }

  StatusRecord status_record{SQLStates::k_HY091(),
                             "Invalid descriptor field identifier"};
  handle->GetDiagnostics().AddStatusRecord(status_record);
  return status_record.CalculateReturnCode();
}

}  // namespace google::cloud::odbc_bq_driver
