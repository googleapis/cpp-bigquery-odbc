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

#include "google/cloud/odbc/bq_driver/odbc_sql_requests.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::IntValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

SQLSMALLINT GetLengthForSeconds(SQLSMALLINT parameter_type,
                                SQLSMALLINT decimal_digits) {
  switch (parameter_type) {
    case SQL_TYPE_TIME:
    case SQL_INTERVAL_HOUR_TO_SECOND:
      return (decimal_digits == 0) ? 8 : 9 + decimal_digits;
    case SQL_TYPE_TIMESTAMP:
      return (decimal_digits == 0) ? 19 : 20 + decimal_digits;
    case SQL_INTERVAL_SECOND:
      return (decimal_digits == 0) ? 2 : 3 + decimal_digits;
    case SQL_INTERVAL_DAY_TO_SECOND:
      return (decimal_digits == 0) ? 11 : 12 + decimal_digits;
    case SQL_INTERVAL_MINUTE_TO_SECOND:
      return (decimal_digits == 0) ? 5 : 6 + decimal_digits;
    default:
      return 0;
  }
}

SQLRETURN SQLBindParameterInternal(
    SQLHSTMT statement_handle, SQLUSMALLINT parameter_number,
    SQLSMALLINT input_output_type, SQLSMALLINT value_type,
    SQLSMALLINT parameter_type, SQLULEN column_size, SQLSMALLINT decimal_digits,
    SQLPOINTER parameter_value_ptr, SQLLEN buffer_length,
    SQLLEN* str_len_or_ind_ptr) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;

  if (parameter_number < 1) {
    StatusRecord status_record = {SQLStates::k_07009(),
                                  "Invalid descriptor index"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }
  if (buffer_length < 0) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "Invalid buffer length"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kAPD);
  DescriptorHandle& ipd = handle->GetDescriptorHandle(DescriptorType::kIPD);

  // Using temporary descriptor records to guard against partially update
  // descriptor record
  DescriptorRecord temp_apd_record;
  if (apd.HasDescriptorRecord(parameter_number)) {
    temp_apd_record = apd.GetDescriptorRecord(parameter_number);
  }
  DescriptorRecord temp_ipd_record;
  if (apd.HasDescriptorRecord(parameter_number)) {
    temp_ipd_record = ipd.GetDescriptorRecord(parameter_number);
  }

  StatusRecord status_record =
      temp_ipd_record.SetParameterType(input_output_type);
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  status_record = temp_apd_record.SetConciseType(value_type, apd.GetType());
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  status_record = temp_ipd_record.SetConciseType(parameter_type, ipd.GetType());
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  if (parameter_type == SQL_CHAR || parameter_type == SQL_VARCHAR ||
      parameter_type == SQL_LONGVARCHAR || parameter_type == SQL_BINARY ||
      parameter_type == SQL_VARBINARY || parameter_type == SQL_LONGVARBINARY ||
      parameter_type == SQL_DECIMAL || parameter_type == SQL_NUMERIC ||
      parameter_type == SQL_WCHAR || parameter_type == SQL_WVARCHAR ||
      parameter_type == SQL_WLONGVARCHAR) {
    temp_ipd_record.precision = column_size;
    temp_ipd_record.datetime_interval_precision = column_size;
    temp_ipd_record.length = column_size;
  }
  if (parameter_type == SQL_TYPE_TIME || parameter_type == SQL_TYPE_TIMESTAMP ||
      parameter_type == SQL_INTERVAL_SECOND ||
      parameter_type == SQL_INTERVAL_DAY_TO_SECOND ||
      parameter_type == SQL_INTERVAL_HOUR_TO_SECOND ||
      parameter_type == SQL_INTERVAL_MINUTE_TO_SECOND) {
    temp_ipd_record.precision = decimal_digits;
    temp_ipd_record.scale = decimal_digits;
    temp_ipd_record.length =
        GetLengthForSeconds(parameter_type, decimal_digits);
  } else if (parameter_type == SQL_NUMERIC || parameter_type == SQL_DECIMAL) {
    temp_ipd_record.scale = decimal_digits;
  }
  temp_apd_record.data_ptr = parameter_value_ptr;
  temp_apd_record.octet_length = buffer_length;
  temp_apd_record.octet_length_ptr = str_len_or_ind_ptr;
  temp_apd_record.indicator_ptr = str_len_or_ind_ptr;

  apd.BindNewDescriptorRecord(parameter_number, temp_apd_record);
  ipd.BindNewDescriptorRecord(parameter_number, temp_ipd_record);

  return SQL_SUCCESS;
}

SQLRETURN SQLNumParamsInternal(SQLHSTMT statement_handle,
                               SQLSMALLINT* param_count) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  // TODO(b/340440354) Check if statement handle is in 'prepared' state

  return IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      handle.GetParamCount(), param_count, nullptr);
}

SQLRETURN SQLPrepareInternal(SQLHSTMT statement_handle, SQLCHAR * in_statement_text, SQLINTEGER in_text_length)
{
    StatusRecordOr<StatementHandle*> handle_result =
    ValidateStatementHandle(statement_handle);
    if (!handle_result) {
        TracePrintInternal(*(*kTraceOption),
        handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
  }
  
  StatementHandle& handle_ref = *(*handle_result);

  if (in_text_length < 1) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "Invalid query length"};
    handle_ref.GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }
  
  StatusRecord status = handle_ref.PrepareQuery(in_statement_text);

  handle_ref.GetDiagnostics().AddStatusRecord(status);
  
  if (!status.ok()) {
    return SQL_ERROR;
  }
  return SQL_SUCCESS;
}
}  // namespace google::cloud::odbc_bq_driver
