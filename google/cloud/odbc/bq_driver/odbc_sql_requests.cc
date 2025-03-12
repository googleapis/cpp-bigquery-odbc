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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <chrono>
#include <future>

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using google::cloud::odbc_bq_driver::ToCharStr;
using google::cloud::odbc_bq_driver_internal::CancelBQJob;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::ConstructBasicPostQueryRequest;
using google::cloud::odbc_bq_driver_internal::ConstructPositionalQueryParams;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSResults;
using google::cloud::odbc_bq_driver_internal::FetchBQData;
using google::cloud::odbc_bq_driver_internal::IntValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::StringValueToOutputBufferResponse;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

namespace {

// Check if previous prepare operation is still executing, if so do the
// following: 1) If the operation wasn't canceled then
//    a) Complete the execution of the prepare future.
//    b) Return the result from future.
// 2) If the operation was canceled then return the cancel state.
SQLRETURN HandleAsyncPrepare(StatementHandle& handle_ref) {
  // Just a precautionary check so we don't rely on the caller.
  if (handle_ref.GetStmtState() != StmtStates::kStatementAsyncPrepare) {
    // Nothing to do.
    return SQL_SUCCESS;
  }
  if (!handle_ref.IsOperationCanceled()) {
    std::optional<std::future<StatusRecord>> future_query =
        handle_ref.GetPossibleFuturePrepareQuery();
    if (future_query.has_value()) {
      std::future_status fut_status =
          future_query.value().wait_for(std::chrono::seconds(0));
      if (fut_status == std::future_status::ready) {
        // Will block till prepare future is executed.
        auto status = future_query.value().get();
        if (!status.ok()) {
          // Reset the state to not prepared so it can be executed again.
          handle_ref.SetStmtState(StmtStates::kStatementNotPrepared);
        }
        // Once the prepare future is executed, reset it regardless of status so
        // we don't try to run it again.
        handle_ref.SetNullFuturePrepareQuery();
        return LogAndReturnCode(handle_ref, status);
      }
      // return that we are still executing
      return SQL_STILL_EXECUTING;
    }
    // If for any reason we don't have the future and we have async
    // execution enabled then this is an error. We also reset the statement
    // prepare state.
    handle_ref.SetStmtState(StmtStates::kStatementNotPrepared);
    auto status_record =
        StatusRecord{SQLStates::k_HY000(),
                     "Internal error: cannot prepare query asynchronously"};
    return LogAndReturnCode(handle_ref, status_record);
  }
  // User has requested cancellation of an ongoing prepare operation.
  // We return the Cancelled error state for this request.
  // We disable cancellation and reset the statement prepare state so
  // subsequent prepare requests can be processed.
  handle_ref.DisableCancellation();
  handle_ref.SetStmtState(StmtStates::kStatementNotPrepared);
  // For current prepare request, return operation canceled.
  auto status_record = StatusRecord{SQLStates::k_HY008(), "Operation canceled"};
  return LogAndReturnCode(handle_ref, status_record);
}

// Handles async execution assuming that the operation wasn't cancelled.
// Note that we are handling both SQL_ASYNC_ENABLE_ON and SQL_ASYNC_ENABLE_OFF
// because SQL_ATTR_ASYNC_ENABLE can be updated after the first call to
// SQLExecDirect
SQLRETURN HandleAsyncExecDirect(StatementHandle& stmt_handle,
                                SQLULEN async_enable) {
  // If for any reason we don't have the future here, but the caller had it,
  // this is an error. We reset the statement state to be
  // `kStatementNotPrepared`, so subsequent execute operations can be processed
  // again.
  if (!stmt_handle.GetPossibleFutureExecDirectQuery().has_value()) {
    stmt_handle.SetStmtState(StmtStates::kStatementNotPrepared);
    auto status_record =
        StatusRecord{SQLStates::k_HY000(),
                     "Internal error: cannot execute query asynchronously"};
    return LogAndReturnCode(stmt_handle, status_record);
  }
  // If future was already processed...
  if (!stmt_handle.GetPossibleFutureExecDirectQuery().value().valid()) {
    return SQL_SUCCESS;
  }
  StatusRecord execute_status;
  if (async_enable == SQL_ASYNC_ENABLE_ON) {
    std::future_status fut_status =
        stmt_handle.GetPossibleFutureExecDirectQuery().value().wait_for(
            std::chrono::seconds(0));
    if (fut_status != std::future_status::ready) {
      // return that we are still executing
      return SQL_STILL_EXECUTING;
    }
    // Will block till ExecDirect future is executed.
    execute_status =
        stmt_handle.GetPossibleFutureExecDirectQuery().value().get();
  } else {
    // SQL_ASYNC_ENABLE_OFF here implies that it was set after ExecDirect was
    // called with SQL_ASYNC_ENABLE_ON the first time.
    // We have to wait synchronously for the future to finish..
    execute_status =
        stmt_handle.GetPossibleFutureExecDirectQuery().value().get();
  }
  if (!execute_status.ok()) {
    // Reset the state to not prepared so it can be executed again.
    stmt_handle.SetStmtState(StmtStates::kStatementNotPrepared);
  }
  return LogAndReturnCode(stmt_handle, execute_status);
}

// Check if previous execute operation is still executing, if so do the
// following: 1) If the operation wasn't canceled then
//    a) Complete the execution of the execute future.
//    b) Return the result from future.
// 2) If the operation was canceled then return the cancel state.
SQLRETURN HandleAsyncExecute(StatementHandle& handle_ref) {
  // Just a precautionary validation so we don't rely on the callers
  // good behavior.
  if (handle_ref.GetStmtState() != StmtStates::kStatementAsyncExecute) {
    // Nothing to do.
    return SQL_SUCCESS;
  }
  if (!handle_ref.IsOperationCanceled()) {
    std::optional<std::future<StatusRecord>> future_query =
        handle_ref.GetPossibleFutureExecuteQuery();
    if (future_query.has_value()) {
      std::future_status fut_status =
          future_query.value().wait_for(std::chrono::seconds(0));
      if (fut_status == std::future_status::ready) {
        // Will block till prepare future is executed.
        auto status = future_query.value().get();
        if (!status.ok()) {
          // Reset the state to prepared so it can be executed again in future
          // requests synchronously or asynchronously.
          handle_ref.SetStmtState(StmtStates::kStatementPrepared);
        }
        // Once the execute future is executed, reset it regardless of status so
        // we don't try to execute it again.
        handle_ref.SetNullFutureExecuteQuery();
        return LogAndReturnCode(handle_ref, status);
      }
      // return that we are still executing
      return SQL_STILL_EXECUTING;
    }
    // If for any reason we don't have the future and we have async
    // execution enabled then this is an error. We also reset the statement
    // execute state to be prepared so subsequent execute operations
    // can be processed again.
    handle_ref.SetStmtState(StmtStates::kStatementPrepared);
    auto status_record =
        StatusRecord{SQLStates::k_HY000(),
                     "Internal error: cannot execute query asynchronously"};
    return LogAndReturnCode(handle_ref, status_record);
  }
  // User has requested cancellation of an ongoing execute operation.
  // We return the Cancelled error state for this request.
  // We disable cancellation and reset the statement execute state so
  // subsequent execute requests can be processed.
  handle_ref.DisableCancellation();
  handle_ref.SetStmtState(StmtStates::kStatementPrepared);
  // For current execute request, return operation canceled.
  auto status_record = StatusRecord{SQLStates::k_HY008(), "Operation canceled"};
  return LogAndReturnCode(handle_ref, status_record);
}

// @brief This function synchronously processes current execute requests
// assuming PrepareQuery was called
// @param stmt_handle The statement handle
// @param failure_state The state which should be set if there is any failure.
StatusRecord ActuallyProcessExecute(StatementHandle& stmt_handle,
                                    StmtStates failure_state) {
  stmt_handle.SetStmtState(StmtStates::kStatementStillExecuting);

  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  StatusRecordOr<SQLULEN> query_timeout_status =
      stmt_handle.GetAttribute(SQL_ATTR_QUERY_TIMEOUT);
  if (!query_timeout_status) {
    return query_timeout_status.GetStatusRecord();
  }
  int query_timeout = *query_timeout_status;
  std::string query_str = stmt_handle.GetQueryString();

  PostQueryRequest post_request =
      ConstructBasicPostQueryRequest(conn_handle, query_str, query_timeout);

  std::vector<QueryParameter> basic_query_params =
      stmt_handle.GetQueryParameters();
  DescriptorHandle& apd = stmt_handle.GetDescriptorHandle(DescriptorType::kAPD);
  DescriptorHandle& ipd = stmt_handle.GetDescriptorHandle(DescriptorType::kIPD);

  std::vector<QueryParameter>& query_params = stmt_handle.GetQueryParameters();
  if (!query_params.empty()) {
    StatusRecord status =
        ConstructPositionalQueryParams(apd, ipd, query_params);
    if (!status.ok()) {
      return status;
    }
    QueryRequest query_request = post_request.query_request();
    query_request.set_query_parameters(query_params);
    post_request.set_query_request(query_request);
  }

  auto ds_status_record_or = FetchBQData(conn_handle, post_request);
  if (!ds_status_record_or) {
    stmt_handle.SetStmtState(failure_state);
    return ds_status_record_or.GetStatusRecord();
  }

  stmt_handle.SetDSResults(*ds_status_record_or);

  // Process the DSResults and convert to ResultSet.
  StatusRecordOr<ResultSet> rs_status_record_or =
      ProcessQueryResults(*ds_status_record_or);
  if (!rs_status_record_or) {
    stmt_handle.SetStmtState(failure_state);
    return rs_status_record_or.GetStatusRecord();
  }

  if (stmt_handle.GetPreparedJob().has_value()) {
    Job prepared_job = stmt_handle.GetPreparedJob().value();
    std::string statement_type =
        prepared_job.statistics.job_query_stats.statement_type;
    if (statement_type != "SELECT") {
      stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
    } else {
      // Store the resultset in statement handle.
      stmt_handle.SetResultSet(*rs_status_record_or);
      stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
    }
  } else {
    return StatusRecord{SQLStates::k_HY000(),
                        "Internal state error when executing query"};
  }

  return StatusRecord::Ok();
}

// This function synchronously processes current SQLExecDirect requests.
StatusRecord ActuallyProcessExecDirect(StatementHandle& stmt_handle) {
  stmt_handle.SetStmtState(StmtStates::kStatementStillExecuting);

  std::string query_str = stmt_handle.GetQueryString();
  // We need to call `PrepareQuery` because:
  // 1) We need to get `statement_type` during `ActuallyProcessExecute`
  // through
  //  `Job::statistics.job_query_stats.statement_type`. This is not possible
  //  through `PostQueryResults`
  // 2) For positional params, we need to get `QueryParameter`s before
  //  SQLExecDirect is called.
  StatusRecord prepare_status = stmt_handle.PrepareQuery(query_str);
  if (!prepare_status.ok()) {
    return prepare_status;
  }
  return ActuallyProcessExecute(stmt_handle, StmtStates::kStatementNotPrepared);
}

}  // namespace

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
    return LogAndReturnCode(*handle, status_record);
  }

  if (buffer_length < 0) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "Invalid buffer length"};
    return LogAndReturnCode(*handle, status_record);
  }

  // Before proceeding with further processing,
  // change the statement states if we have a data-at-execution
  // parameter. Here we only change the statement states so we can execute
  // any cancel operation properly. This state change should
  // ideally be done by SQLParamData but its not
  // implemented yet. The implementation of those functions would take care of
  // any changes to SQLBindParameter based on these states.
  //
  // TODO(b/308656307, b/308655631): Move this to SQLParamData and SQLPutData
  // when they are implemented and replace the statement here with calls to
  // SQLParamData and SQLPutData where applicable.
  if (parameter_value_ptr) {
    auto param_value = reinterpret_cast<size_t>(parameter_value_ptr);
    auto data_at_exec = static_cast<SQLINTEGER>(param_value);
    if (data_at_exec == SQL_DATA_AT_EXEC) {
      handle->SetStmtState(StmtStates::kNeedsParams);
    }
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
    return LogAndReturnCode(*handle, status_record);
  }

  status_record = temp_apd_record.SetConciseType(value_type, apd.GetType());
  if (!status_record.ok()) {
    return LogAndReturnCode(*handle, status_record);
  }

  status_record = temp_ipd_record.SetConciseType(parameter_type, ipd.GetType());
  if (!status_record.ok()) {
    return LogAndReturnCode(*handle, status_record);
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

SQLRETURN SQLDescribeParamInternal(SQLHSTMT statement_handle,
                                   SQLUSMALLINT parameter_number,
                                   SQLSMALLINT* data_type_ptr,
                                   SQLULEN* parameter_size_ptr,
                                   SQLSMALLINT* decimal_digits_ptr,
                                   SQLSMALLINT* nullable_ptr) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  if (handle.GetStmtState() == StmtStates::kStatementNotPrepared) {
    StatusRecord status_record = {
        SQLStates::k_HY010(),
        "Function sequence error - statement is not prepared"};
    return LogAndReturnCode(handle, status_record);
  }

  if (parameter_number < 1) {
    StatusRecord status_record = {SQLStates::k_07009(),
                                  "Invalid descriptor index"};
    handle.GetDiagnostics().AddStatusRecord(status_record);
    return status_record.CalculateReturnCode();
  }

  DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
  if (!ipd.HasDescriptorRecord(parameter_number)) {
    StatusRecord status_record = {
        SQLStates::k_07009(),
        "Invalid descriptor index - no parameter for such value"};
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorRecord& desc_record = ipd.GetDescriptorRecord(parameter_number);

  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      desc_record.concise_type, data_type_ptr, nullptr);
  switch (desc_record.concise_type) {
    case SQL_NUMERIC:
    case SQL_DECIMAL:
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    case SQL_BIGINT:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.precision, parameter_size_ptr, nullptr);
      break;
    default:
      IntValueToOutputBufferResponse<SQLULEN, SQLSMALLINT>(
          desc_record.length, parameter_size_ptr, nullptr);
  }
  switch (desc_record.concise_type) {
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
    case SQL_INTERVAL_SECOND:
    case SQL_INTERVAL_DAY_TO_SECOND:
    case SQL_INTERVAL_HOUR_TO_SECOND:
    case SQL_INTERVAL_MINUTE_TO_SECOND:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.precision, decimal_digits_ptr, nullptr);
      break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.scale, decimal_digits_ptr, nullptr);
      break;
    default:
      *decimal_digits_ptr = 0;
  }
  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      desc_record.nullable, nullable_ptr, nullptr);

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

  if (handle.GetStmtState() == StmtStates::kStatementNotPrepared) {
    StatusRecord status_record = {
        SQLStates::k_HY010(),
        "Function sequence error - statement is not prepared"};
    return LogAndReturnCode(handle, status_record);
  }

  return IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      handle.GetParamCount(), param_count, nullptr);
}

SQLRETURN SQLPrepareInternal(SQLHSTMT statement_handle,
                             SQLCHAR* in_statement_text,
                             SQLINTEGER in_text_length) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  StatementHandle& handle_ref = *(*handle_result);

  if ((in_text_length < 1) && (in_text_length != SQL_NTS)) {
    StatusRecord status_record = {SQLStates::k_HY090(), "Invalid query length"};
    return LogAndReturnCode(handle_ref, status_record);
  }

  std::string query_str = ToCharStr(in_statement_text);
  if (query_str.empty()) {
    auto status_record =
        StatusRecord{SQLStates::k_HY000(), "Query text is null or empty"};
    return LogAndReturnCode(handle_ref, status_record);
  }

  StatusRecordOr<SQLULEN> async_enable_status =
      handle_ref.GetAttribute(SQL_ATTR_ASYNC_ENABLE);
  if (!async_enable_status) {
    return LogAndReturnCode(handle_ref, async_enable_status.GetStatusRecord());
  }

  if (handle_ref.GetStmtState() == StmtStates::kStatementAsyncPrepare) {
    return HandleAsyncPrepare(handle_ref);
  }

  // Check if we have canceled a prepare operation that is completed,
  // If so do the following for any future prepare requests:
  //   1) Disable Cancellation
  //   2) Put the statement state to be not prepared.
  // For the the current prepare request
  //   1) Return success without preparing the query because user has
  // requested cancellation.
  // For more details please see the cancel design:
  // http://goto.google.com/odbc-sql-cancel-design
  if (handle_ref.IsOperationCanceled() &&
      handle_ref.GetStmtState() == StmtStates::kStatementPrepared) {
    // For any future prepare requests.
    handle_ref.DisableCancellation();
    handle_ref.SetStmtState(StmtStates::kStatementNotPrepared);
    // For current prepare request.
    return SQL_SUCCESS;
  }

  // Make this an asynchronous operation if the user has requested it
  // to be async.
  if (*async_enable_status == SQL_ASYNC_ENABLE_ON) {
    std::future<StatusRecord> fut_prepare_query = std::async(
        std::launch::async,
        [&handle_ref](std::string const& query_str) {
          StatusRecord status = handle_ref.PrepareQuery(query_str);
          handle_ref.SetStmtState(StmtStates::kStatementPrepared);
          // The states set through `SetStmtState` can be updated.
          // SetStatementPrepared() persists the info that the stmt was
          // prepared.
          handle_ref.SetStatementPrepared();
          return status;
        },
        query_str);
    // Store the fut_prepare_query in statement handle.
    handle_ref.SetFuturePrepareQuery(std::move(fut_prepare_query));
    // Set statement state to indicate its an async prepare request.
    handle_ref.SetStmtState(StmtStates::kStatementAsyncPrepare);
    return SQL_STILL_EXECUTING;
  }

  StatusRecord status = handle_ref.PrepareQuery(query_str);
  handle_ref.SetStmtState(StmtStates::kStatementPrepared);
  // The states set through `SetStmtState` can be updated.
  // SetStatementPrepared() persists the info that the stmt was prepared.
  handle_ref.SetStatementPrepared();
  return LogAndReturnCode(handle_ref, status);
}

SQLRETURN SQLExecuteInternal(SQLHSTMT statement_handle) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  // Check if prepare is still executing from a previous async call,
  // if so first complete that.
  StatusRecordOr<SQLULEN> async_enable_status =
      stmt_handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE);
  if (!async_enable_status) {
    return LogAndReturnCode(stmt_handle, async_enable_status.GetStatusRecord());
  }

  // Handle any asynchronous prepare request from previous operation.
  if (stmt_handle.GetStmtState() == StmtStates::kStatementAsyncPrepare) {
    SQLRETURN status = HandleAsyncPrepare(stmt_handle);
    // Only proceed to execute if prepare successfully executed.
    if (!SQL_SUCCEEDED(status)) {
      return status;
    }
  }

  // At this point, statement should be prepared regardless of sync or async
  // nature.
  if (stmt_handle.GetStmtState() == StmtStates::kStatementNotPrepared) {
    StatusRecord status_record = {
        SQLStates::k_HY010(),
        "Function sequence error - statement is not prepared"};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  // Now handle any asynhronous execute requests from previous operations.
  if (stmt_handle.GetStmtState() == StmtStates::kStatementAsyncExecute) {
    return HandleAsyncExecute(stmt_handle);
  }

  // At this point we are handling new  request for execute. It could sync or
  // async based on statement attribute SQL_ATTR_SYNC_ENABLE.
  if (!stmt_handle.IsOperationCanceled() &&
      stmt_handle.GetStmtState() == StmtStates::kStatementStillExecuting) {
    StatusRecord status_record = {
        SQLStates::k_HY010(),
        "Function sequence error - statement is still executing"};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  if (stmt_handle.GetStmtState() == StmtStates::kStatementExecutedWithoutRs ||
      stmt_handle.GetStmtState() == StmtStates::kStatementExecutedWithRs) {
    StatusRecord status_record = {
        SQLStates::k_HY010(),
        "Function sequence error - statement has already executed"};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  // Check if we have previously canceled an onngoing execute operation request,
  // If so do the following for any future execute requests:
  //   1) Disable Cancellation
  //   2) Put the statement state to be in prepared state so any future requests
  //   can be executed.
  // For the the current execute request
  //   1) Return success without executing the query because user has
  //      requested cancellation.
  // For more details please see the cancel design:
  // http://goto.google.com/odbc-sql-cancel-design
  if (stmt_handle.IsOperationCanceled() &&
      stmt_handle.GetStmtState() == StmtStates::kStatementStillExecuting) {
    // For any future execute requests.
    stmt_handle.DisableCancellation();
    stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
    // For current execution, return without executing the request since
    // user has cancelled it. Additionally, check if there is any jobs we need
    // to cancel on the server.
    if (stmt_handle.GetPreparedJob().has_value()) {
      std::string job_status =
          stmt_handle.GetPreparedJob().value().status.state;
      // We can only cancel running or pending jobs on the server.
      StatusRecordOr<Job> server_cancel_status = StatusRecord::Ok();
      if (job_status != "DONE") {
        // 1) We have a ongoing job to cancel.
        std::string prepared_job_id =
            stmt_handle.GetPreparedJob().value().job_reference.job_id;
        ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
        // 2) Cancel the BQ Job on the server.
        server_cancel_status = CancelBQJob(conn_handle, prepared_job_id);
      }
      // 3) Since we canceled the job set the prepared job to null. This is
      // done regardless of whether cancel is complete or not. User has
      // requested cancellation of this job. This should never be run again.
      stmt_handle.SetNullPreparedJob();
      return LogAndReturnCode(stmt_handle,
                              server_cancel_status.GetStatusRecord());
    }
    // Nothing to do if there is no associated Job.
    return SQL_SUCCESS;
  }

  // Make this an asynchronous operation if the user has requested it
  // to be async.
  if (*async_enable_status == SQL_ASYNC_ENABLE_ON) {
    std::future<StatusRecord> fut_execute_query =
        std::async(std::launch::async, [&stmt_handle]() {
          return ActuallyProcessExecute(stmt_handle,
                                        StmtStates::kStatementPrepared);
        });
    // Store the fut_execute_query in statement handle.
    stmt_handle.SetFutureExecuteQuery(std::move(fut_execute_query));
    // Set statement state to be still executing as per the spec.
    stmt_handle.SetStmtState(StmtStates::kStatementAsyncExecute);
    return SQL_STILL_EXECUTING;
  }

  StatusRecord execute_status =
      ActuallyProcessExecute(stmt_handle, StmtStates::kStatementPrepared);
  if (!execute_status.ok()) {
    stmt_handle.SetStmtState(StmtStates::kNeedsParams);
    return SQL_NEED_DATA;
  }
  return LogAndReturnCode(stmt_handle, execute_status);
}

SQLRETURN SQLExecDirectInternal(SQLHSTMT statement_handle,
                                SQLCHAR* in_statement_text,
                                SQLINTEGER in_text_length) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  if ((in_text_length < 1) && (in_text_length != SQL_NTS)) {
    StatusRecord status_record = {SQLStates::k_HY090(), "Invalid query length"};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  // We need to throw SQL_ERROR if a statement was already executed with a
  // result set.
  // Note: Following cases for SQLExecDirect call are allowed by the
  // existing driver:
  // 1) StmtStates::kStatementPrepared: A statement was prepared.
  // 2) StmtStates::kStatementExecutedWithoutRs: Previous executions
  // didn't return a result set
  if (stmt_handle.GetStmtState() == StmtStates::kStatementExecutedWithRs) {
    auto status_record =
        StatusRecord{SQLStates::k_24000(), "Invalid cursor state."};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  std::string query_str = ToCharStr(in_statement_text);
  if (query_str.empty()) {
    auto status_record =
        StatusRecord{SQLStates::k_HY000(), "Query text is null or empty"};
    return LogAndReturnCode(stmt_handle, status_record);
  }
  stmt_handle.SetQueryString(query_str);

  // Boolean to tell us if a ExecDirect Query was to be processed async and it
  // wasn't finished last time. We are not using a `StmtStates` here because
  // `StmtStates::kStatementStillExecuting` will conflict with it.
  bool was_async_requested =
      stmt_handle.GetPossibleFutureExecDirectQuery().has_value();
  // `.valid()` method tells us whether the state was valid or not.
  // It is possible that `.get` was called on the `std::future` which
  // turned the state invalid.
  // In this case, the query has finished and we return success.
  if (was_async_requested &&
      stmt_handle.GetPossibleFutureExecDirectQuery().value().valid()) {
    return SQL_SUCCESS;
  }

  // *****************************************************************
  // STEP 1: Handle cancellation
  // *****************************************************************

  // Check if we have canceled a ExecDirect operation that is completed,
  // If so do the following for any future ExecDirect requests:
  //   1) Disable Cancellation
  //   2) Put the statement state to be not prepared.
  // For the the current ExecDirect request
  //  1) If the query was supposed to be async last time, CancelBQJob
  //  2) Otherwise, return success without executing the query because user has
  //  requested cancellation.
  if (stmt_handle.IsOperationCanceled()) {
    // For any future ExecDirect requests.
    stmt_handle.DisableCancellation();
    stmt_handle.SetStmtState(StmtStates::kStatementNotPrepared);
    if (was_async_requested) {
      stmt_handle.SetNullFutureExecDirectQuery();
      DSResults& ds_results = stmt_handle.GetDSResults();
      // For current ExecDirect request, return without executing the request
      // since user has cancelled it. Additionally, check if there is any job
      // we need to cancel on the server.
      if (ds_results.job_ref.has_value()) {
        ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
        // 2) Cancel the BQ Job on the server.
        StatusRecordOr<Job> server_cancel_status =
            CancelBQJob(conn_handle, ds_results.job_ref.value().job_id);
        // 3) Since we canceled the job set the prepared job to null. This is
        // done regardless of whether cancel is complete or not. User has
        // requested cancellation of this job. This should never be run again.
        ds_results.job_ref = std::nullopt;
        // We don't need to return SQL_ERROR if it failed.
        // Log the error in cancellation.
        if (!server_cancel_status) {
          TracePrintInternal(*(*kTraceOption),
                             server_cancel_status.GetStatusRecord().message);
        }
      }
      // For current ExecDirect request, return operation canceled.
      auto status_record =
          StatusRecord{SQLStates::k_HY008(), "Operation canceled"};
      return LogAndReturnCode(stmt_handle, status_record);
    }
    // For current ExecDirect request.
    return SQL_SUCCESS;
  }

  StatusRecordOr<SQLULEN> async_enable_status =
      stmt_handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE);
  if (!async_enable_status) {
    return LogAndReturnCode(stmt_handle, async_enable_status.GetStatusRecord());
  }

  // *****************************************************************
  // STEP 2: Handle still executing std::future
  // *****************************************************************

  if (was_async_requested) {
    SQLRETURN status = HandleAsyncExecDirect(stmt_handle, *async_enable_status);
    if (status != SQL_STILL_EXECUTING) {
      // Once the ExecDirect future is executed, reset it regardless of status
      // so we don't try to run it again.
      stmt_handle.SetNullFutureExecDirectQuery();
    }
    return status;
  }

  // *****************************************************************
  // STEP 3: Handle creation of `std::future` if `SQL_ASYNC_ENABLE_ON`
  // *****************************************************************

  // Make this an asynchronous operation if the user has requested it
  // to be async.
  if (*async_enable_status == SQL_ASYNC_ENABLE_ON) {
    std::future<StatusRecord> fut_exec_direct_query = std::async(
        std::launch::async,
        [&stmt_handle]() { return ActuallyProcessExecDirect(stmt_handle); });
    // Store the fut_execute_query in statement handle.
    stmt_handle.SetFutureExecDirectQuery(std::move(fut_exec_direct_query));
    // Set statement state to be still executing as per the spec.
    stmt_handle.SetStmtState(StmtStates::kStatementStillExecuting);
    return SQL_STILL_EXECUTING;
  }

  // *****************************************************************
  // STEP 4: Synchronous execution
  // *****************************************************************
  StatusRecord execute_status = ActuallyProcessExecDirect(stmt_handle);
  return LogAndReturnCode(stmt_handle, execute_status);
}

SQLRETURN SQLSetCursorNameInternal(SQLHSTMT statement_handle,
                                   SQLCHAR const* cursor_name,
                                   SQLSMALLINT name_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  std::string name = ToCharStr(cursor_name);

  if (absl::StartsWith(name, "SQLCUR") || absl::StartsWith(name, "SQL_CUR")) {
    StatusRecord status_record = {SQLStates::k_34000(), "Invalid cursor name"};
    return LogAndReturnCode(stmt_handle, status_record);
  }
  if (name_len < 0 && name_len != SQL_NTS) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "Invalid string length"};
    return LogAndReturnCode(stmt_handle, status_record);
  }
  if (stmt_handle.GetStmtState() != StmtStates::kStatementNotPrepared &&
      stmt_handle.GetStmtState() != StmtStates::kStatementPrepared) {
    StatusRecord status_record = {SQLStates::k_24000(), "Invalid cursor state"};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  stmt_handle.SetCursorName(name);

  return SQL_SUCCESS;
}

SQLRETURN SQLGetCursorNameInternal(SQLHSTMT statement_handle,
                                   SQLCHAR* cursor_name, SQLSMALLINT buffer_len,
                                   SQLSMALLINT* name_string_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(*(*kTraceOption),
                       handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  StatusRecord status = StringValueToOutputBufferResponse(
      stmt_handle.GetCursorName().c_str(), cursor_name, buffer_len,
      name_string_len);
  return LogAndReturnCode(stmt_handle, status);
}

SQLRETURN SQLParamDataInternal(SQLHSTMT statement_handle,
  SQLPOINTER* param_or_target_value) {
StatusRecordOr<StatementHandle*> handle_result =
ValidateStatementHandle(statement_handle);
if (!handle_result) {
TracePrintInternal(*(*kTraceOption),
handle_result.GetStatusRecord().message);
return handle_result.GetCalculatedReturnCode();
}

StatementHandle* handle = *handle_result;

if (handle->GetPreparedJob().has_value()) {
Job prepared_job = handle->GetPreparedJob().value();
std::string stmt_type =
prepared_job.statistics.job_query_stats.statement_type;
if (stmt_type == "UPDATE" || stmt_type == "DELETE") {
return SQL_NO_DATA;
}
}

DescriptorHandle& apd = handle->GetDescriptorHandle(DescriptorType::kAPD);
DescriptorHandle& ipd = handle->GetDescriptorHandle(DescriptorType::kIPD);

auto param_num = handle->GetCurrentParameterIndex();

if (param_num < 0 || param_num > handle->GetParamCount()) {
return LogAndReturnCode(*handle,
{SQLStates::k_HY000(), "Parameter out of bounds"});
}

while (param_num < handle->GetParamCount()) {
param_num++;
auto param = apd.GetDescriptorRecord(param_num);
if (param.indicator_ptr != nullptr ||
*(param.indicator_ptr) == SQL_DATA_AT_EXEC ||
*(param.indicator_ptr) == SQL_LEN_DATA_AT_EXEC(0)) {
  if(param_or_target_value!=nullptr)
    *param_or_target_value = param.data_ptr;
handle->SetCurrentParamIndex(param_num);
handle->SetStmtState(StmtStates::kNeedsPutData);
return SQL_NEED_DATA;
}
}

ConnectionHandle& conn_handle = *(handle->GetConnectionHandle());
StatusRecordOr<SQLULEN> query_timeout_status =
handle->GetAttribute(SQL_ATTR_QUERY_TIMEOUT);
if (!query_timeout_status) {
return LogAndReturnCode(*handle, query_timeout_status);
}
int query_timeout = *query_timeout_status;
std::string query_str = handle->GetQueryString();

PostQueryRequest post_request =
ConstructBasicPostQueryRequest(conn_handle, query_str, query_timeout);

std::vector<QueryParameter>& query_params = handle->GetQueryParameters();
if (!query_params.empty()) {
StatusRecord status =
ConstructPositionalQueryParams(apd, ipd, query_params);
if (!status.ok()) {
return LogAndReturnCode(*handle, status);
}
QueryRequest query_request = post_request.query_request();
query_request.set_query_parameters(query_params);
post_request.set_query_request(query_request);
}

if (!conn_handle.IsConnected()) {
return LogAndReturnCode(
*handle, StatusRecord{SQLStates::k_08S01(),
 "Connection to the data source is broken"});
}

auto bq_client = conn_handle.GetClient();
if (!bq_client) {
return LogAndReturnCode(
*handle,
StatusRecord{SQLStates::k_HY000(),
"Invalid or null BQ Client within the connection handle"});
}
// For now , we use default options.
// We can set timeout here as needed later.
Options options;
auto pq_status = bq_client->PostQuery(post_request, options);
if (!pq_status) {
return LogAndReturnCode(*handle, pq_status);
}

handle->SetStmtState(StmtStates::kStatementPrepared);
return SQL_SUCCESS;
}

SQLRETURN SQLPutDataInternal(SQLHSTMT statement_handle, SQLPOINTER data,
                             SQLLEN str_len_or_ind_ptr) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);
  // Ensure statement is in kNeedsPutData state
  if (stmt_handle.GetStmtState() != StmtStates::kNeedsPutData) {
    return LogAndReturnCode(
        stmt_handle, {SQLStates::k_HY010(),
                      "Function sequence error: Incorrect statement state."});
  }

  // Get the current parameter index (set by SQLParamData)
  SQLUSMALLINT param_index = stmt_handle.GetCurrentParameterIndex();
  if (param_index < 1 || param_index > stmt_handle.GetParamCount()) {
    return LogAndReturnCode(
        stmt_handle,
        {SQLStates::k_HY000(), "No parameter currently expecting data."});
  }

  DescriptorHandle& apd = stmt_handle.GetDescriptorHandle(DescriptorType::kAPD);
  DescriptorHandle& ipd = stmt_handle.GetDescriptorHandle(DescriptorType::kIPD);

  if (!apd.HasDescriptorRecord(param_index) &&
      !ipd.HasDescriptorRecord(param_index)) {
    return LogAndReturnCode(
        stmt_handle, {SQLStates::k_07002(),
                      "Descriptor record does not exist for parameter."});
  }

  DescriptorRecord& apd_rec = apd.GetDescriptorRecord(param_index);
  DescriptorRecord& ipd_rec = ipd.GetDescriptorRecord(param_index);

  // Ensure parameter was marked for deferred execution
  if (apd_rec.indicator_ptr == nullptr &&
      *(apd_rec.indicator_ptr) != SQL_DATA_AT_EXEC &&
      *(apd_rec.indicator_ptr) != SQL_LEN_DATA_AT_EXEC(0)) {
    return LogAndReturnCode(
        stmt_handle, {SQLStates::k_HY010(),
                      "Invalid function sequence: Parameter not deferred."});
  }

  // Check for invalid data pointer when length greater than zero
  if ((data == nullptr && str_len_or_ind_ptr > 0) ||
       str_len_or_ind_ptr < SQL_NULL_DATA) {
    return LogAndReturnCode(
        stmt_handle, {SQLStates::k_HY009(), "Invalid string length or indicator value."});
  }

  // Handle NULL data
  if (str_len_or_ind_ptr == 0 || data == nullptr || 
    str_len_or_ind_ptr == SQL_NULL_DATA) {
    apd_rec.data_ptr = nullptr;
    apd_rec.octet_length = 0;
    return SQL_SUCCESS;
  } 


  // Check if total size exceeds column limit
  //if ((apd_rec.octet_length + str_len_or_ind_ptr) >= ipd_rec.length) {
  //  return LogAndReturnCode(stmt_handle, {SQLStates::k_HY090(),
  //                                        "Data size exceeds column limit."});
  //}

  // Allocate a temporary buffer
  SQLPOINTER temp_buffer = nullptr;
  if (apd_rec.data_ptr == nullptr) {
    // First chunk of data, allocate fresh buffer
    temp_buffer = malloc(str_len_or_ind_ptr);
    if (temp_buffer == nullptr) {
      return LogAndReturnCode(
          stmt_handle, {SQLStates::k_HY001(), "Memory allocation error."});
    }

    memcpy(temp_buffer, data, str_len_or_ind_ptr);
  } else {
    // Additional chunks: allocate new buffer large enough to hold old + new
    // data
    size_t new_size = apd_rec.octet_length + str_len_or_ind_ptr;
    temp_buffer = malloc(new_size);
    if (temp_buffer == nullptr) {
      return LogAndReturnCode(
          stmt_handle, {SQLStates::k_HY001(), "Memory allocation error."});
    }

    // Copy existing data
    memcpy(temp_buffer, apd_rec.data_ptr, apd_rec.octet_length);

    // Append new data
    memcpy(static_cast<char*>(temp_buffer) + apd_rec.octet_length, data,
            str_len_or_ind_ptr);
  }

  // Update descriptor only after successful allocation
  apd_rec.data_ptr = temp_buffer;
  apd_rec.octet_length += str_len_or_ind_ptr;
  apd_rec.octet_length_ptr = &apd_rec.octet_length;
  *apd_rec.indicator_ptr = SQL_NTS;

  // Update descriptor record
  apd.BindNewDescriptorRecord(param_index, apd_rec);
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
