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
#include "google/cloud/odbc/bq_driver/internal/odbc_transactions.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::AddressToPointer;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::FinishTransactionIfNeeded;
using google::cloud::odbc_bq_driver_internal::HandleType;
using google::cloud::odbc_bq_driver_internal::IntValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

StatusRecord SetConnectionAttributes(ConnectionHandle* conn_handle,
                                     StatementHandle* stmt_handle) {
  SQLULEN metadata_id = 0;
  StatusRecord status =
      conn_handle->GetAttribute(SQL_ATTR_METADATA_ID, &metadata_id, 0, nullptr);
  if (!status.ok()) {
    return status;
  }
  SQLULEN async_enable = 0;
  status = conn_handle->GetAttribute(SQL_ATTR_ASYNC_ENABLE, &async_enable, 0,
                                     nullptr);
  if (!status.ok()) {
    return status;
  }

  status = stmt_handle->SetAttribute(SQL_ATTR_METADATA_ID, metadata_id);
  if (!status.ok()) {
    return status;
  }
  return stmt_handle->SetAttribute(SQL_ATTR_ASYNC_ENABLE, async_enable);
}

void AssociateDescriptorHandle(StatementHandle* stmt_handle,
                               DescriptorType type) {
  stmt_handle->GetDescriptorHandle(type)
      .GetAssociatedStatementHandles()
      .emplace(stmt_handle, type);
}

SQLRETURN SQLAllocStmtHandle(SQLHDBC in_handle, SQLHANDLE* out_conn_handle) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(in_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  ConnectionHandle* conn_handle = *handle_result;

  DescriptorHandle ard(DescriptorType::kARD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ird(DescriptorType::kIRD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);
  auto* stmt_handle = new StatementHandle(conn_handle, {ard, apd, ird, ipd});

  StatusRecord status_record =
      SetConnectionAttributes(*handle_result, stmt_handle);
  if (!status_record.ok()) {
    return LogAndReturnCode(*(*handle_result), status_record);
  }
  conn_handle->GetStatementHandles().insert(stmt_handle);
  AssociateDescriptorHandle(stmt_handle, DescriptorType::kARD);
  AssociateDescriptorHandle(stmt_handle, DescriptorType::kAPD);
  AssociateDescriptorHandle(stmt_handle, DescriptorType::kIRD);
  AssociateDescriptorHandle(stmt_handle, DescriptorType::kIPD);

  std::string cursor_name =
      "SQL_CUR" + std::to_string(reinterpret_cast<std::uintptr_t>(stmt_handle));
  stmt_handle->SetCursorName(cursor_name);

  *out_conn_handle = stmt_handle;
  return SQL_SUCCESS;
}

SQLRETURN SetDescriptorHandle(StatementHandle* handle, int attribute,
                              SQLPOINTER value) {
  if (attribute == SQL_ATTR_IMP_PARAM_DESC ||
      attribute == SQL_ATTR_IMP_ROW_DESC) {
    StatusRecord status_record{SQLStates::k_HY017(),
                               "Invalid try to set implementation descriptor"};
    return LogAndReturnCode(*handle, status_record);
  }
  auto* desc_handle =
      reinterpret_cast<odbc_bq_driver_internal::DescriptorHandle*>(value);
  // nullptr is a valid value here
  if (desc_handle && desc_handle->kType != HandleType::kDescHandle) {
    StatusRecord status_record{
        SQLStates::k_HY024(),
        "Invalid attribute value (invalid descriptor handle)"};
    return LogAndReturnCode(*handle, status_record);
  }

  StatusRecord status = StatusRecord::Ok();
  switch (attribute) {
    case SQL_ATTR_APP_ROW_DESC:
      status = handle->SetDescriptorHandle(DescriptorType::kARD, desc_handle);
      break;
    case SQL_ATTR_APP_PARAM_DESC:
      status = handle->SetDescriptorHandle(DescriptorType::kAPD, desc_handle);
      break;
  }
  return LogAndReturnCode(*handle, status);
}

SQLRETURN SQLSetStmtAttrInternal(SQLHSTMT statement_handle,
                                 SQLINTEGER attribute, SQLPOINTER value,
                                 SQLINTEGER /*value_string_len*/) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  if (attribute == SQL_ATTR_CONCURRENCY || attribute == SQL_ATTR_CURSOR_TYPE ||
      attribute == SQL_ATTR_SIMULATE_CURSOR ||
      attribute == SQL_ATTR_USE_BOOKMARKS) {
    if (handle.GetStmtState() == StmtStates::kStatementPrepared) {
      StatusRecord status_record{
          SQLStates::k_HY011(),
          "Attribute cannot be set now - statement was prepared"};
      return LogAndReturnCode(handle, status_record);
    }
  }
  if (attribute == SQL_ATTR_CONCURRENCY || attribute == SQL_ATTR_CURSOR_TYPE ||
      attribute == SQL_ATTR_SIMULATE_CURSOR ||
      attribute == SQL_ATTR_USE_BOOKMARKS) {
    if (handle.IsCursorOpen()) {
      StatusRecord status_record{SQLStates::k_24000(),
                                 "Invalid cursor state - cursor is open"};
      return LogAndReturnCode(handle, status_record);
    }
  }

  if (attribute == SQL_ATTR_APP_ROW_DESC ||
      attribute == SQL_ATTR_APP_PARAM_DESC ||
      attribute == SQL_ATTR_IMP_ROW_DESC ||
      attribute == SQL_ATTR_IMP_PARAM_DESC) {
    return SetDescriptorHandle(&handle, attribute, value);
  }

  auto int_value = reinterpret_cast<size_t>(value);
  switch (attribute) {
    case SQL_ATTR_PARAM_BIND_OFFSET_PTR: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().bind_offset_ptr = reinterpret_cast<SQLLEN*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAM_BIND_TYPE: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().bind_type = static_cast<SQLINTEGER>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAM_OPERATION_PTR: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAM_STATUS_PTR: {
      DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
      ipd.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAMS_PROCESSED_PTR: {
      DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
      ipd.GetHeaderRecord().rows_processed_ptr =
          reinterpret_cast<SQLULEN*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_PARAMSET_SIZE: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      apd.GetHeaderRecord().array_size = static_cast<SQLULEN>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_ARRAY_SIZE: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      ard.GetHeaderRecord().array_size = static_cast<SQLULEN>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_BIND_OFFSET_PTR: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      ard.GetHeaderRecord().bind_offset_ptr = reinterpret_cast<SQLLEN*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_BIND_TYPE: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      ard.GetHeaderRecord().bind_type = static_cast<SQLINTEGER>(int_value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_OPERATION_PTR: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      ard.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROW_STATUS_PTR: {
      DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
      ird.GetHeaderRecord().array_status_ptr =
          reinterpret_cast<SQLUSMALLINT*>(value);
      return SQL_SUCCESS;
    }
    case SQL_ATTR_ROWS_FETCHED_PTR: {
      DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
      ird.GetHeaderRecord().rows_processed_ptr =
          reinterpret_cast<SQLULEN*>(value);
      return SQL_SUCCESS;
    }
  }

  StatusRecord status_record =
      handle.SetAttribute(attribute, reinterpret_cast<SQLULEN>(value));
  return LogAndReturnCode(handle, status_record);
}

SQLRETURN SQLGetStmtAttrInternal(SQLHSTMT statement_handle,
                                 SQLINTEGER attribute, SQLPOINTER value,
                                 SQLINTEGER /*value_buffer_len*/,
                                 SQLINTEGER* value_string_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);
  if (attribute == SQL_ATTR_ROW_NUMBER) {
    if (!handle.IsCursorOpen()) {
      StatusRecord status_record{SQLStates::k_24000(),
                                 "Invalid cursor state - cursor is not open"};
      return LogAndReturnCode(handle, status_record);
    }
    if (handle.GetResultSet().cursor == -1) {
      StatusRecord status_record{
          SQLStates::k_24000(),
          "Invalid cursor state - cursor is positioned before the start"};
      return LogAndReturnCode(handle, status_record);
    }
    if (handle.GetResultSet().cursor >= handle.GetResultSet().rows.size()) {
      StatusRecord status_record{
          SQLStates::k_24000(),
          "Invalid cursor state - cursor is positioned after the end"};
      return LogAndReturnCode(handle, status_record);
    }
  }

  switch (attribute) {
    case SQL_ATTR_APP_ROW_DESC: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      return AddressToPointer(&ard, value, value_string_len);
    }
    case SQL_ATTR_APP_PARAM_DESC: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      return AddressToPointer(&apd, value, value_string_len);
    }
    case SQL_ATTR_IMP_ROW_DESC: {
      DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
      return AddressToPointer(&ird, value, value_string_len);
    }
    case SQL_ATTR_IMP_PARAM_DESC: {
      DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
      return AddressToPointer(&ipd, value, value_string_len);
    }
  }

  switch (attribute) {
    case SQL_ATTR_ROW_NUMBER: {
      int row_number = handle.GetResultSet().cursor + 1;
      return IntValueToOutputBufferResponse(row_number, value,
                                            value_string_len);
    }
    case SQL_ATTR_PARAM_BIND_OFFSET_PTR: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      return AddressToPointer(apd.GetHeaderRecord().bind_offset_ptr, value,
                              value_string_len);
    }
    case SQL_ATTR_PARAM_BIND_TYPE: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      return IntValueToOutputBufferResponse(apd.GetHeaderRecord().bind_type,
                                            value, value_string_len);
    }
    case SQL_ATTR_PARAM_OPERATION_PTR: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      return AddressToPointer(apd.GetHeaderRecord().array_status_ptr, value,
                              value_string_len);
    }
    case SQL_ATTR_PARAM_STATUS_PTR: {
      DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
      return AddressToPointer(ipd.GetHeaderRecord().array_status_ptr, value,
                              value_string_len);
    }
    case SQL_ATTR_PARAMS_PROCESSED_PTR: {
      DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
      return AddressToPointer(ipd.GetHeaderRecord().rows_processed_ptr, value,
                              value_string_len);
    }
    case SQL_ATTR_PARAMSET_SIZE: {
      DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
      return IntValueToOutputBufferResponse(apd.GetHeaderRecord().array_size,
                                            value, value_string_len);
    }
    case SQL_ATTR_ROW_ARRAY_SIZE: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      return IntValueToOutputBufferResponse(ard.GetHeaderRecord().array_size,
                                            value, value_string_len);
    }
    case SQL_ATTR_ROW_BIND_OFFSET_PTR: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      return AddressToPointer(ard.GetHeaderRecord().bind_offset_ptr, value,
                              value_string_len);
    }
    case SQL_ATTR_ROW_BIND_TYPE: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      return IntValueToOutputBufferResponse(ard.GetHeaderRecord().bind_type,
                                            value, value_string_len);
    }
    case SQL_ATTR_ROW_OPERATION_PTR: {
      DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
      return AddressToPointer(ard.GetHeaderRecord().array_status_ptr, value,
                              value_string_len);
    }
    case SQL_ATTR_ROW_STATUS_PTR: {
      DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
      return AddressToPointer(ird.GetHeaderRecord().array_status_ptr, value,
                              value_string_len);
    }
    case SQL_ATTR_ROWS_FETCHED_PTR: {
      DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
      return AddressToPointer(ird.GetHeaderRecord().rows_processed_ptr, value,
                              value_string_len);
    }
  }

  StatusRecordOr<SQLULEN> status = handle.GetAttribute(attribute);
  if (!status) {
    return LogAndReturnCode(handle, status);
  }
  return IntValueToOutputBufferResponse(*status, value, value_string_len);
}

SQLRETURN SQLEndTranInternal(SQLSMALLINT handle_type, SQLHANDLE handle,
                             SQLSMALLINT completion_type) {
  if (handle_type == SQL_HANDLE_DBC) {
    StatusRecordOr<ConnectionHandle*> handle_result =
        ValidateConnectionHandle(handle);
    if (!handle_result) {
      TracePrintInternal(*(*kTraceOption),
                         handle_result.GetStatusRecord().message);
      return handle_result.GetCalculatedReturnCode();
    }
    ConnectionHandle& conn_handle = *(*handle_result);

    StatusRecord status_record =
        FinishTransactionIfNeeded(conn_handle, completion_type);
    return LogAndReturnCode(conn_handle, status_record);
  }
  if (handle_type == SQL_HANDLE_ENV) {
    StatusRecordOr<EnvironmentHandle*> handle_result =
        ValidateEnvironmentHandle(handle);
    if (!handle_result) {
      TracePrintInternal(*(*kTraceOption),
                         handle_result.GetStatusRecord().message);
      return handle_result.GetCalculatedReturnCode();
    }
    EnvironmentHandle& env_handle = *(*handle_result);

    for (auto* conn_handle : env_handle.GetConnectionHandles()) {
      StatusRecord status_record =
          FinishTransactionIfNeeded(*conn_handle, completion_type);
      if (!status_record.ok()) {
        return LogAndReturnCode(*conn_handle, status_record);
      }
    }
  } else {
    TracePrintInternal(*(*kTraceOption), "HandleType is undefined");
    return SQL_INVALID_HANDLE;
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLFreeStmtInternal(SQLHSTMT statement_handle, SQLUSMALLINT option) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  switch (option) {
    case SQL_CLOSE:
      stmt_handle.CloseCursor();
      return SQL_SUCCESS;
    case SQL_UNBIND: {
      DescriptorHandle& ard =
          stmt_handle.GetDescriptorHandle(DescriptorType::kARD);
      StatusRecord status = ard.UnbindAllDescriptorRecordsFrom(0);
      return LogAndReturnCode(stmt_handle, status);
    }
    case SQL_RESET_PARAMS: {
      DescriptorHandle& apd =
          stmt_handle.GetDescriptorHandle(DescriptorType::kAPD);
      StatusRecord status = apd.UnbindAllDescriptorRecordsFrom(0);
      return LogAndReturnCode(stmt_handle, status);
    }
  }
  StatusRecord status{SQLStates::k_HY092(), "Option type out of range"};
  return LogAndReturnCode(stmt_handle, status);
}

SQLRETURN SQLCancelInternal(SQLHSTMT statement_handle) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  // Make sure there is a previous operation to cancel. This includes
  // async operations as well
  if (stmt_handle.GetStmtState() == StmtStates::kStatementPrepared ||
      stmt_handle.GetStmtState() == StmtStates::kStatementAsyncPrepare ||
      stmt_handle.GetStmtState() == StmtStates::kStatementAsyncExecute ||
      stmt_handle.GetStmtState() == StmtStates::kStatementStillExecuting ||
      stmt_handle.GetStmtState() == StmtStates::kNeedsData) {
    stmt_handle.EnableCancellation();
  }

  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
