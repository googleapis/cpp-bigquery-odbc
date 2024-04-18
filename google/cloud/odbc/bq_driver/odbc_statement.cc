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

#include "google/cloud/odbc/bq_driver/odbc_statement.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::HandleType;
using google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

SQLRETURN SQLAllocStmtHandle(SQLHDBC in_handle, SQLHANDLE* out_conn_handle) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(in_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOptsConsole),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  auto* stmt_handle = new StatementHandle();
  *out_conn_handle = stmt_handle;
  return SQL_SUCCESS;
}

SQLRETURN SetDescriptorHandle(StatementHandle* handle, int attribute,
                              SQLPOINTER value) {
  if (attribute == SQL_ATTR_IMP_PARAM_DESC ||
      attribute == SQL_ATTR_IMP_ROW_DESC) {
    StatusRecord status_record{SQLStates::k_HY017(),
                               "Invalid try to set implementation descriptor"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }
  StatusRecordOr<DescriptorHandle*> desc_handle =
      CastToHandle<DescriptorHandle>(HandleType::kDescHandle, value);
  if (!desc_handle) {
    StatusRecord status_record{
        SQLStates::k_HY024(),
        "Invalid attribute value (invalid descriptor handle)"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  StatusRecord status = StatusRecord::Ok();
  switch (attribute) {
    case SQL_ATTR_APP_ROW_DESC:
      status = handle->SetDescriptorHandle(DescriptorType::kARD, *desc_handle);
      break;
    case SQL_ATTR_APP_PARAM_DESC:
      status = handle->SetDescriptorHandle(DescriptorType::kAPD, *desc_handle);
      break;
  }
  if (!status.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status);
    return status.CalculateReturnCode();
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLSetStmtAttrInternal(SQLHSTMT statement_handle,
                                 SQLINTEGER attribute, SQLPOINTER value,
                                 SQLINTEGER /*value_string_len*/) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOptsConsole),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  auto* handle = *handle_result;

  if (attribute == SQL_ATTR_CONCURRENCY || attribute == SQL_ATTR_CURSOR_TYPE ||
      attribute == SQL_ATTR_SIMULATE_CURSOR ||
      attribute == SQL_ATTR_USE_BOOKMARKS) {
    // TODO(b/334849872) Check if statement was not prepared
  }
  if (attribute == SQL_ATTR_CONCURRENCY || attribute == SQL_ATTR_CURSOR_TYPE ||
      attribute == SQL_ATTR_SIMULATE_CURSOR ||
      attribute == SQL_ATTR_USE_BOOKMARKS) {
    // TODO(b/334845645) Check if cursor was not open
  }

  // Descriptors
  if (attribute == SQL_ATTR_APP_ROW_DESC ||
      attribute == SQL_ATTR_APP_PARAM_DESC ||
      attribute == SQL_ATTR_IMP_ROW_DESC ||
      attribute == SQL_ATTR_IMP_PARAM_DESC) {
    return SetDescriptorHandle(handle, attribute, value);
  }

  // Descriptor attributes
  auto int_value = reinterpret_cast<size_t>(value);
  switch (attribute) {
    case SQL_ATTR_PARAM_BIND_OFFSET_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().bind_offset_ptr = reinterpret_cast<SQLLEN*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAM_BIND_TYPE: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().bind_type = static_cast<SQLINTEGER>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAM_OPERATION_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAM_STATUS_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kIPD);
      apd.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAMS_PROCESSED_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kIPD);
      apd.GetHeaderRecord().rows_processed_ptr =
          reinterpret_cast<SQLULEN*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAMSET_SIZE: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().array_size = static_cast<SQLULEN>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_ARRAY_SIZE: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kARD);
      apd.GetHeaderRecord().array_size = static_cast<SQLULEN>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_BIND_OFFSET_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kARD);
      apd.GetHeaderRecord().bind_offset_ptr = reinterpret_cast<SQLLEN*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_BIND_TYPE: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kARD);
      apd.GetHeaderRecord().bind_type = static_cast<SQLINTEGER>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_OPERATION_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kARD);
      apd.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_STATUS_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kIRD);
      apd.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROWS_FETCHED_PTR: {
      DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kIRD);
      apd.GetHeaderRecord().rows_processed_ptr =
          reinterpret_cast<SQLULEN*>(value);
      return SQL_SUCCESS;
    }
  }

  // Statement attributes
  StatusRecord status_record =
      handle->SetAttribute(attribute, reinterpret_cast<SQLULEN>(value));
  if (!status_record.ok()) {
    handle->GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

}  // namespace google::cloud::odbc_bq_driver
