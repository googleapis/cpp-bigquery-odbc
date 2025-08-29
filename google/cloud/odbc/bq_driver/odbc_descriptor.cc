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
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include <algorithm>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::AddressToPointer;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::HeaderRecord;
using google::cloud::odbc_bq_driver_internal::IntValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::StringValueToOutputBufferResponse;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

static std::map<DescriptorType, std::vector<int>> const kAllowedFieldsToSet = {
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

static std::map<DescriptorType, std::vector<int>> const kAllowedFieldsToGet = {
    {DescriptorType::kApplication,
     {SQL_DESC_ALLOC_TYPE, SQL_DESC_ARRAY_SIZE, SQL_DESC_ARRAY_STATUS_PTR,
      SQL_DESC_BIND_OFFSET_PTR, SQL_DESC_BIND_TYPE, SQL_DESC_COUNT,
      SQL_DESC_CONCISE_TYPE, SQL_DESC_DATA_PTR, SQL_DESC_DATETIME_INTERVAL_CODE,
      SQL_DESC_DATETIME_INTERVAL_PRECISION, SQL_DESC_INDICATOR_PTR,
      SQL_DESC_LENGTH, SQL_DESC_NUM_PREC_RADIX, SQL_DESC_OCTET_LENGTH,
      SQL_DESC_OCTET_LENGTH_PTR, SQL_DESC_PRECISION, SQL_DESC_SCALE,
      SQL_DESC_TYPE}},
    {DescriptorType::kIRD,
     {SQL_DESC_ALLOC_TYPE,
      SQL_DESC_ARRAY_STATUS_PTR,
      SQL_DESC_COUNT,
      SQL_DESC_ROWS_PROCESSED_PTR,
      SQL_DESC_AUTO_UNIQUE_VALUE,
      SQL_DESC_BASE_COLUMN_NAME,
      SQL_DESC_BASE_TABLE_NAME,
      SQL_DESC_CASE_SENSITIVE,
      SQL_DESC_CATALOG_NAME,
      SQL_DESC_CONCISE_TYPE,
      SQL_DESC_DATETIME_INTERVAL_CODE,
      SQL_DESC_DATETIME_INTERVAL_PRECISION,
      SQL_DESC_DISPLAY_SIZE,
      SQL_DESC_FIXED_PREC_SCALE,
      SQL_DESC_LABEL,
      SQL_DESC_LENGTH,
      SQL_DESC_LITERAL_PREFIX,
      SQL_DESC_LITERAL_SUFFIX,
      SQL_DESC_LOCAL_TYPE_NAME,
      SQL_DESC_NAME,
      SQL_DESC_NULLABLE,
      SQL_DESC_NUM_PREC_RADIX,
      SQL_DESC_OCTET_LENGTH,
      SQL_DESC_PRECISION,
      SQL_DESC_SCALE,
      SQL_DESC_SCHEMA_NAME,
      SQL_DESC_SEARCHABLE,
      SQL_DESC_TABLE_NAME,
      SQL_DESC_TYPE,
      SQL_DESC_TYPE_NAME,
      SQL_DESC_UNNAMED,
      SQL_DESC_UNSIGNED,
      SQL_DESC_UPDATABLE}},
    {DescriptorType::kIPD,
     {SQL_DESC_ALLOC_TYPE,
      SQL_DESC_ARRAY_STATUS_PTR,
      SQL_DESC_COUNT,
      SQL_DESC_ROWS_PROCESSED_PTR,
      SQL_DESC_CASE_SENSITIVE,
      SQL_DESC_CONCISE_TYPE,
      SQL_DESC_DATETIME_INTERVAL_CODE,
      SQL_DESC_DATETIME_INTERVAL_PRECISION,
      SQL_DESC_FIXED_PREC_SCALE,
      SQL_DESC_LENGTH,
      SQL_DESC_LOCAL_TYPE_NAME,
      SQL_DESC_NAME,
      SQL_DESC_NULLABLE,
      SQL_DESC_NUM_PREC_RADIX,
      SQL_DESC_OCTET_LENGTH,
      SQL_DESC_PARAMETER_TYPE,
      SQL_DESC_PRECISION,
      SQL_DESC_SCALE,
      SQL_DESC_TYPE,
      SQL_DESC_TYPE_NAME,
      SQL_DESC_UNNAMED,
      SQL_DESC_UNSIGNED}}};

DescriptorType Convert(DescriptorType type) {
  if (type == DescriptorType::kIRD || type == DescriptorType::kIPD) {
    return type;
  }
  return DescriptorType::kApplication;
}

SQLRETURN SQLAllocDescHandle(SQLHANDLE in_handle, SQLHANDLE* out_desc_handle) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(in_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLAllocDescHandle::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  ConnectionHandle* conn_handle = *handle_result;

  auto* desc_handle =
      new DescriptorHandle(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);
  conn_handle->GetDescriptorHandles().insert(desc_handle);
  desc_handle->SetConnectionHandle(conn_handle);

  *out_desc_handle = desc_handle;
  return SQL_SUCCESS;
}

StatusRecord SetCount(DescriptorHandle* handle, std::size_t desc_int_value) {
  auto new_val = static_cast<SQLSMALLINT>(desc_int_value);
  if (new_val >= handle->GetHeaderRecord().count) {
    handle->GetHeaderRecord().count = new_val;
    return StatusRecord::Ok();
  }
  return handle->UnbindAllDescriptorRecordsFrom(
      static_cast<SQLSMALLINT>(desc_int_value));
}

StatusRecord SetName(DescriptorRecord& descriptor_record, SQLPOINTER desc_value,
                     SQLINTEGER buffer_len) {
  if (buffer_len > SQL_MAX_IDENTIFIER_LEN) {
    LOG(ERROR) << "SetName:: String data, right truncated";
    return StatusRecord{SQLStates::k_22001(), "String data, right truncated"};
  }
  if (buffer_len < 0 && buffer_len != SQL_NTS) {
    LOG(ERROR) << "SetName:: Invalid buffer length";
    return StatusRecord{SQLStates::k_HY090(), "Invalid buffer length"};
  }
  if (desc_value) {
    descriptor_record.SetName(reinterpret_cast<char*>(desc_value), buffer_len);
  } else {
    // nullptr equals to empty string
    descriptor_record.SetName("", 0);
  }
  return StatusRecord::Ok();
}

StatusRecord SetDescField(DescriptorHandle* handle, SQLSMALLINT rec_number,
                          SQLSMALLINT field_identifier, SQLPOINTER desc_value,
                          SQLINTEGER desc_value_buffer_len) {
  std::vector<int> vec = kAllowedFieldsToSet.at(Convert(handle->GetType()));
  if (std::find(vec.begin(), vec.end(), field_identifier) == vec.end()) {
    LOG(ERROR) << "SetDescField:: Invalid descriptor field identifier ";
    return StatusRecord{SQLStates::k_HY091(),
                        "Invalid descriptor field identifier"};
  }

  auto desc_int_value = reinterpret_cast<size_t>(desc_value);

  // HeaderRecord fields
  HeaderRecord& header_record = handle->GetHeaderRecord();
  switch (field_identifier) {
    case SQL_DESC_ARRAY_SIZE:
      header_record.array_size = static_cast<SQLULEN>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_ARRAY_STATUS_PTR:
      header_record.array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(desc_value);
      return StatusRecord::Ok();
    case SQL_DESC_BIND_OFFSET_PTR:
      header_record.bind_offset_ptr = reinterpret_cast<SQLLEN*>(desc_value);
      return StatusRecord::Ok();
    case SQL_DESC_BIND_TYPE:
      header_record.bind_type = static_cast<SQLINTEGER>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_COUNT:
      return SetCount(handle, desc_int_value);
    case SQL_DESC_ROWS_PROCESSED_PTR:
      header_record.rows_processed_ptr = reinterpret_cast<SQLULEN*>(desc_value);
      return StatusRecord::Ok();
  }

  if (rec_number < 0) {
    LOG(ERROR) << "SetDescField:: Invalid descriptor index ";
    return StatusRecord{SQLStates::k_07009(), "Invalid descriptor index"};
  }

  if (!handle->HasDescriptorRecord(rec_number)) {
    LOG(INFO) << "SetDescField:: Descriptor record created";
    DescriptorRecord new_descriptor_record;
    handle->BindNewDescriptorRecord(rec_number, new_descriptor_record);
  }

  // DescriptorRecord fields
  DescriptorRecord& descriptor_record = handle->GetDescriptorRecord(rec_number);
  switch (field_identifier) {
    case SQL_DESC_CONCISE_TYPE:
      return descriptor_record.SetConciseType(
          static_cast<SQLSMALLINT>(desc_int_value), handle->GetType());
    case SQL_DESC_DATA_PTR:
      return descriptor_record.SetDataPointer(desc_value, handle->GetType());
    case SQL_DESC_DATETIME_INTERVAL_CODE:
      descriptor_record.datetime_interval_code =
          static_cast<SQLSMALLINT>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_DATETIME_INTERVAL_PRECISION:
      descriptor_record.datetime_interval_precision =
          static_cast<SQLINTEGER>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_INDICATOR_PTR:
      descriptor_record.indicator_ptr = reinterpret_cast<SQLLEN*>(desc_value);
      return StatusRecord::Ok();
    case SQL_DESC_LENGTH:
      descriptor_record.length = static_cast<SQLULEN>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_NAME:
      return SetName(descriptor_record, desc_value, desc_value_buffer_len);
    case SQL_DESC_NUM_PREC_RADIX:
      return descriptor_record.SetNumPrecRadix(
          static_cast<SQLINTEGER>(desc_int_value));
    case SQL_DESC_OCTET_LENGTH:
      descriptor_record.octet_length = static_cast<SQLLEN>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_OCTET_LENGTH_PTR:
      descriptor_record.octet_length_ptr =
          reinterpret_cast<SQLLEN*>(desc_value);
      return StatusRecord::Ok();
    case SQL_DESC_PARAMETER_TYPE:
      return descriptor_record.SetParameterType(
          static_cast<SQLSMALLINT>(desc_int_value));
    case SQL_DESC_PRECISION:
      descriptor_record.precision = static_cast<SQLSMALLINT>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_SCALE:
      descriptor_record.scale = static_cast<SQLSMALLINT>(desc_int_value);
      return StatusRecord::Ok();
    case SQL_DESC_TYPE:
      return descriptor_record.SetType(static_cast<SQLSMALLINT>(desc_int_value),
                                       handle->GetType());
    case SQL_DESC_UNNAMED:
      return descriptor_record.SetUnnamed(
          static_cast<SQLSMALLINT>(desc_int_value));
  }
  LOG(ERROR) << "SetDescField:: Invalid descriptor field identifier ";
  return StatusRecord{SQLStates::k_HY091(),
                      "Invalid descriptor field identifier"};
}

SQLRETURN SQLSetDescFieldInternal(SQLHDESC descriptor_handle,
                                  SQLSMALLINT rec_number,
                                  SQLSMALLINT field_identifier,
                                  SQLPOINTER desc_value,
                                  SQLINTEGER desc_value_buffer_len) {
  StatusRecordOr<DescriptorHandle*> handle_result =
      ValidateDescriptorHandle(descriptor_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLSetDescField:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatusRecord status_record =
      SetDescField(*handle_result, rec_number, field_identifier, desc_value,
                   desc_value_buffer_len);
  return LogAndReturnCode(*(*handle_result), status_record);
}

StatusRecordOr<SQLRETURN> GetDescField(DescriptorHandle* handle,
                                       SQLSMALLINT rec_number,
                                       SQLSMALLINT field_identifier,
                                       SQLPOINTER out_value,
                                       SQLINTEGER value_buffer_len,
                                       SQLSMALLINT* value_string_len,
                                       bool is_type_sqllen) {
  std::vector<int> vec = kAllowedFieldsToGet.at(Convert(handle->GetType()));
  if (std::find(vec.begin(), vec.end(), field_identifier) == vec.end()) {
    LOG(ERROR) << "GetDescField:: Invalid descriptor field identifier ";
    return StatusRecord{SQLStates::k_HY091(),
                        "Invalid descriptor field identifier"};
  }

  // HeaderRecord fields
  HeaderRecord& header_record = handle->GetHeaderRecord();
  switch (field_identifier) {
    case SQL_DESC_ALLOC_TYPE:
      IntValueToOutputBufferResponse(header_record.GetAllocType(), out_value,
                                     value_string_len);
      return SQL_SUCCESS;
    case SQL_DESC_ARRAY_SIZE:
      IntValueToOutputBufferResponse(header_record.array_size, out_value,
                                     value_string_len);
      return SQL_SUCCESS;
    case SQL_DESC_ARRAY_STATUS_PTR: {
      AddressToPointer(header_record.array_status_ptr, out_value,
                       value_string_len);
      return SQL_SUCCESS;
    }
    case SQL_DESC_BIND_OFFSET_PTR:
      AddressToPointer(header_record.bind_offset_ptr, out_value,
                       value_string_len);
      return SQL_SUCCESS;
    case SQL_DESC_BIND_TYPE:
      IntValueToOutputBufferResponse(header_record.bind_type, out_value,
                                     value_string_len);
      return SQL_SUCCESS;
    case SQL_DESC_COUNT:
      IntValueToOutputBufferResponse(header_record.count, out_value,
                                     value_string_len);
      return SQL_SUCCESS;
    case SQL_DESC_ROWS_PROCESSED_PTR:
      AddressToPointer(header_record.rows_processed_ptr, out_value,
                       value_string_len);
      return SQL_SUCCESS;
  }

  if (rec_number < 0) {
    LOG(ERROR) << "GetDescField:: Invalid descriptor index (negative) ";
    return StatusRecord{SQLStates::k_07009(),
                        "Invalid descriptor index (negative)"};
  }
  if (rec_number > header_record.count) {
    StatusRecord status_record{
        SQLStates::k_07009(),
        "Invalid descriptor index (greater than SQL_DESC_COUNT)"};
    LOG(ERROR) << "GetDescField:: " << status_record.message;
    return StatusRecordOr<SQLRETURN>{status_record, SQL_NO_DATA};
  }

  if (handle->GetType() == DescriptorType::kIRD &&
      !handle->GetAssociatedStatementHandles().empty()) {
    // For IPD and IRD there can be only one associated statement handle
    auto* stmt_handle = handle->GetAssociatedStatementHandles().begin()->first;
    if (stmt_handle->GetStmtState() == StmtStates::kStatementNotPrepared) {
      LOG(ERROR) << "GetDescField:: Associated statement is not prepared ";
      return StatusRecord{SQLStates::k_HY007(),
                          "Associated statement is not prepared"};
    }
  }

  // DescriptorRecord fields
  DescriptorRecord default_descriptor_record;
  // Use default values if RecNumber is less than SQL_DESC_COUNT and there is no
  // row for that RecNumber
  DescriptorRecord& descriptor_record =
      (handle->HasDescriptorRecord(rec_number))
          ? handle->GetDescriptorRecord(rec_number)
          : default_descriptor_record;
  StatusRecord result = StatusRecord::Ok();
  switch (field_identifier) {
    case SQL_DESC_AUTO_UNIQUE_VALUE:
      IntValueToOutputBufferResponse(descriptor_record.auto_unique_value,
                                     out_value, value_string_len);
      break;
    case SQL_DESC_BASE_COLUMN_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.base_column_name.c_str(), out_value,
          value_buffer_len, value_string_len);
      break;
    case SQL_DESC_BASE_TABLE_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.base_table_name.c_str(), out_value,
          value_buffer_len, value_string_len);
      break;
    case SQL_DESC_CASE_SENSITIVE:
      IntValueToOutputBufferResponse(descriptor_record.case_sensitive,
                                     out_value, value_string_len);
      break;
    case SQL_DESC_CATALOG_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.catalog_name.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_CONCISE_TYPE:
      // Cast descriptor_record.type to T datatype, to prevent truncation
      // or garbage value when the field is smaller (e.g., SQLSMALLINT).
      if (is_type_sqllen) {
        SQLLEN cast_val = static_cast<SQLLEN>(descriptor_record.concise_type);
        IntValueToOutputBufferResponse(cast_val, out_value, value_string_len);
      } else {
        IntValueToOutputBufferResponse(descriptor_record.concise_type,
                                       out_value, value_string_len);
      }
      break;
    case SQL_DESC_DATA_PTR:
      AddressToPointer(descriptor_record.data_ptr, out_value, value_string_len);
      break;
    case SQL_DESC_DATETIME_INTERVAL_CODE:
      IntValueToOutputBufferResponse(descriptor_record.datetime_interval_code,
                                     out_value, value_string_len);
      break;
    case SQL_DESC_DATETIME_INTERVAL_PRECISION:
      IntValueToOutputBufferResponse(
          descriptor_record.datetime_interval_precision, out_value,
          value_string_len);
      break;
    case SQL_DESC_DISPLAY_SIZE:
      IntValueToOutputBufferResponse(descriptor_record.display_size, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_FIXED_PREC_SCALE:
      IntValueToOutputBufferResponse(descriptor_record.fixed_prec_scale,
                                     out_value, value_string_len);
      break;
    case SQL_DESC_INDICATOR_PTR:
      AddressToPointer(descriptor_record.indicator_ptr, out_value,
                       value_string_len);
      break;
    case SQL_DESC_LABEL:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.label.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_LENGTH:
      IntValueToOutputBufferResponse(descriptor_record.length, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_LITERAL_PREFIX:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.literal_prefix.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_LITERAL_SUFFIX:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.literal_suffix.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_LOCAL_TYPE_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.local_type_name.c_str(), out_value,
          value_buffer_len, value_string_len);
      break;
    case SQL_DESC_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.name.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_NULLABLE:
      IntValueToOutputBufferResponse(descriptor_record.nullable, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_NUM_PREC_RADIX:
      IntValueToOutputBufferResponse(descriptor_record.num_prec_radix,
                                     out_value, value_string_len);
      break;
    case SQL_DESC_OCTET_LENGTH:
      IntValueToOutputBufferResponse(descriptor_record.octet_length, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_OCTET_LENGTH_PTR:
      AddressToPointer(descriptor_record.octet_length_ptr, out_value,
                       value_string_len);
      break;
    case SQL_DESC_PARAMETER_TYPE:
      IntValueToOutputBufferResponse(descriptor_record.parameter_type,
                                     out_value, value_string_len);
      break;
    case SQL_DESC_PRECISION:
      IntValueToOutputBufferResponse(descriptor_record.precision, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_ROWVER:
      IntValueToOutputBufferResponse(descriptor_record.rowver, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_SCALE:
      IntValueToOutputBufferResponse(descriptor_record.scale, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_SCHEMA_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.schema_name.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_SEARCHABLE:
      IntValueToOutputBufferResponse(descriptor_record.searchable, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_TABLE_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.table_name.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_TYPE:
      // Cast descriptor_record.type to T datatype, to prevent truncation
      // or garbage value when the field is smaller (e.g., SQLSMALLINT).
      if (is_type_sqllen) {
        SQLLEN cast_val = static_cast<SQLLEN>(descriptor_record.type);
        IntValueToOutputBufferResponse(cast_val, out_value, value_string_len);
      } else {
        IntValueToOutputBufferResponse(descriptor_record.type, out_value,
                                       value_string_len);
      }
      break;
    case SQL_DESC_TYPE_NAME:
      result = StringValueToOutputBufferResponse<SQLSMALLINT>(
          descriptor_record.type_name.c_str(), out_value, value_buffer_len,
          value_string_len);
      break;
    case SQL_DESC_UNNAMED:
      IntValueToOutputBufferResponse(descriptor_record.unnamed, out_value,
                                     value_string_len);
      break;
    case SQL_DESC_UNSIGNED:
      IntValueToOutputBufferResponse(descriptor_record.sql_desc_unsigned,
                                     out_value, value_string_len);
      break;
    case SQL_DESC_UPDATABLE:
      IntValueToOutputBufferResponse(descriptor_record.updatable, out_value,
                                     value_string_len);
      break;
    default:
      result = StatusRecord{SQLStates::k_HY091(),
                            "Invalid descriptor field identifier"};
      LOG(ERROR) << "GetDescField:: " << result.message;
  }
  if (!result.ok()) {
    return result;
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLGetDescFieldInternal(SQLHDESC descriptor_handle,
                                  SQLSMALLINT rec_number,
                                  SQLSMALLINT field_identifier,
                                  SQLPOINTER out_value,
                                  SQLINTEGER value_buffer_len,
                                  SQLINTEGER* value_string_len) {
  StatusRecordOr<DescriptorHandle*> handle_result =
      ValidateDescriptorHandle(descriptor_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLGetDescField::ValidateDescriptorHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }

  StatusRecordOr<SQLRETURN> status_record_or = GetDescField(
      *handle_result, rec_number, field_identifier, out_value, value_buffer_len,
      reinterpret_cast<SQLSMALLINT*>(value_string_len));
  return LogAndReturnCode(*(*handle_result), status_record_or);
}

SQLRETURN SetDescRec(DescriptorHandle* handle, SQLSMALLINT rec_number,
                     SQLSMALLINT type, SQLSMALLINT sub_type, SQLLEN length,
                     SQLSMALLINT precision, SQLSMALLINT scale,
                     SQLPOINTER data_ptr, SQLLEN* string_length_ptr,
                     SQLLEN* indicator_ptr) {
  if (rec_number < 0) {
    StatusRecord status_record{SQLStates::k_07009(),
                               "Invalid descriptor index"};
    LOG(ERROR) << "SetDescRec:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  if (!handle->HasDescriptorRecord(rec_number)) {
    DescriptorRecord new_descriptor_record;
    handle->BindNewDescriptorRecord(rec_number, new_descriptor_record);
  }
  DescriptorRecord& descriptor_record = handle->GetDescriptorRecord(rec_number);

  DescriptorRecord temp_desc = descriptor_record;
  temp_desc.datetime_interval_code = sub_type;
  StatusRecord status_record = temp_desc.SetType(type, handle->GetType());
  if (!status_record.ok()) {
    LOG(ERROR) << "SetDescRec:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }
  temp_desc.octet_length = length;
  temp_desc.precision = precision;
  temp_desc.scale = scale;
  temp_desc.data_ptr = data_ptr;
  temp_desc.octet_length_ptr = string_length_ptr;
  temp_desc.indicator_ptr = indicator_ptr;

  status_record = temp_desc.ConsistencyCheck();
  if (!status_record.ok()) {
    LOG(ERROR) << "SetDescRec:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  descriptor_record = temp_desc;
  return SQL_SUCCESS;
}

SQLRETURN SQLSetDescRecInternal(SQLHDESC descriptor_handle,
                                SQLSMALLINT rec_number, SQLSMALLINT type,
                                SQLSMALLINT sub_type, SQLLEN length,
                                SQLSMALLINT precision, SQLSMALLINT scale,
                                SQLPOINTER data_ptr, SQLLEN* string_length_ptr,
                                SQLLEN* indicator_ptr) {
  StatusRecordOr<DescriptorHandle*> handle_result =
      ValidateDescriptorHandle(descriptor_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLSetDescRec::ValidateDescriptorHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  return SetDescRec(*handle_result, rec_number, type, sub_type, length,
                    precision, scale, data_ptr, string_length_ptr,
                    indicator_ptr);
}

SQLRETURN GetDescRec(DescriptorHandle* handle, SQLSMALLINT rec_number,
                     SQLCHAR* name, SQLSMALLINT buffer_length,
                     SQLSMALLINT* string_length_ptr, SQLSMALLINT* type_ptr,
                     SQLSMALLINT* sub_type_ptr, SQLLEN* length_ptr,
                     SQLSMALLINT* precision_ptr, SQLSMALLINT* scale_ptr,
                     SQLSMALLINT* nullable_ptr) {
  if (rec_number < 0) {
    StatusRecord status_record{SQLStates::k_07009(),
                               "Invalid descriptor index (negative)"};
    LOG(ERROR) << "GetDescRec:: Invalid descriptor index (negative)";
    return LogAndReturnCode(*handle, status_record);
  }
  if (rec_number > handle->GetHeaderRecord().count) {
    StatusRecord status_record{
        SQLStates::k_07009(),
        "Invalid descriptor index (greater than SQL_DESC_COUNT)"};
    LOG(ERROR) << "GetDescRec:: " << status_record.message;
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return SQL_NO_DATA;
  }

  DescriptorRecord default_descriptor_record;
  // Use default values if RecNumber is less than SQL_DESC_COUNT and there is no
  // row for that RecNumber
  DescriptorRecord& descriptor_record =
      (handle->HasDescriptorRecord(rec_number))
          ? handle->GetDescriptorRecord(rec_number)
          : default_descriptor_record;

  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      descriptor_record.type, type_ptr, nullptr);
  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      descriptor_record.datetime_interval_code, sub_type_ptr, nullptr);
  IntValueToOutputBufferResponse<SQLLEN, SQLLEN>(descriptor_record.octet_length,
                                                 length_ptr, nullptr);
  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      descriptor_record.precision, precision_ptr, nullptr);
  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      descriptor_record.scale, scale_ptr, nullptr);
  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      descriptor_record.nullable, nullable_ptr, nullptr);
  StatusRecord status_record = StringValueToOutputBufferResponse(
      descriptor_record.name.c_str(), name, buffer_length, string_length_ptr);
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }

  return status_record.CalculateReturnCode();
}

SQLRETURN SQLGetDescRecInternal(
    SQLHDESC descriptor_handle, SQLSMALLINT rec_number, SQLCHAR* name,
    SQLSMALLINT buffer_length, SQLSMALLINT* string_length_ptr,
    SQLSMALLINT* type_ptr, SQLSMALLINT* sub_type_ptr, SQLLEN* length_ptr,
    SQLSMALLINT* precision_ptr, SQLSMALLINT* scale_ptr,
    SQLSMALLINT* nullable_ptr) {
  StatusRecordOr<DescriptorHandle*> handle_result =
      ValidateDescriptorHandle(descriptor_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLGetDescRec::ValidateDescriptorHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  return GetDescRec(*handle_result, rec_number, name, buffer_length,
                    string_length_ptr, type_ptr, sub_type_ptr, length_ptr,
                    precision_ptr, scale_ptr, nullable_ptr);
}

SQLRETURN SQLCopyDescInternal(SQLHDESC source_desc_handle,
                              SQLHDESC target_desc_handle) {
  StatusRecordOr<DescriptorHandle*> handle_result =
      ValidateDescriptorHandle(source_desc_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLCopyDesc::ValidateDescriptorHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  DescriptorHandle* src_handle = *handle_result;
  handle_result = ValidateDescriptorHandle(target_desc_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLCopyDesc::ValidateDescriptorHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  DescriptorHandle* target_handle = *handle_result;

  if (target_handle->GetType() == DescriptorType::kIRD) {
    StatusRecord status_record{
        SQLStates::k_HY016(), "Cannot modify an implementation row descriptor"};
    LOG(ERROR) << "SQLCopyDesc:: " << status_record.message;
    return LogAndReturnCode(*target_handle, status_record);
  }
  if (src_handle->GetType() == DescriptorType::kIRD) {
    // TODO(332469364) Check if statement handle is in 'prepared' or 'executed'
    // state (HY007)
  }

  target_handle->GetHeaderRecord().CopyHeaderRecordsFrom(
      src_handle->GetHeaderRecord());

  StatusRecord status_record =
      target_handle->SetDescriptorRecords(src_handle->GetDescriptorRecords());
  if (!status_record.ok()) {
    LOG(ERROR) << "SQLCopyDesc::SetDescriptorRecords:: "
               << status_record.message;
  }
  return LogAndReturnCode(*target_handle, status_record);
}

}  // namespace google::cloud::odbc_bq_driver
