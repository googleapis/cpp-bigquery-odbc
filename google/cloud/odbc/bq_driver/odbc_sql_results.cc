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

#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fetch.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/bq_driver/odbc_sql_requests.h"
#include "odbc_sql_results.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::CreateDSRowFromTypeInfo;
using google::cloud::odbc_bq_driver_internal::CreateTypeInfoRowSchema;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSRow;
using google::cloud::odbc_bq_driver_internal::DSValue;
using google::cloud::odbc_bq_driver_internal::IntValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::kSqlToBqDataTypes;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::RowSchema;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::StringValueToOutputBufferResponse;
using google::cloud::odbc_bq_driver_internal::ToSqlPointer;
using google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using google::cloud::odbc_bq_driver_internal::WriteRowset;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;

namespace{

  SQLRETURN HandleAsyncGetResults(StatementHandle& handle_ref) {
      // Precautionary check for state.
      if (handle_ref.GetStmtState() != StmtStates::kStatementAsyncGetResults) {
          TracePrintInternal(*(*kTraceOption), "Unexpected state in HandleAsyncGetResults:");
          return SQL_SUCCESS;
      }

      // Handle cancellation scenario.
      if (handle_ref.IsOperationCanceled()) {
          TracePrintInternal(*(*kTraceOption), "Operation canceled during async result fetching.");
          handle_ref.DisableCancellation();
          handle_ref.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
          auto status_record = StatusRecord{SQLStates::k_HY008(), "Operation canceled"};
          return LogAndReturnCode(handle_ref, status_record);
      }

      // Check for future results.
      std::optional<std::future<StatusRecord>> future_results = handle_ref.GetPossibleFutureGetResults();
      if (future_results.has_value()) {
          std::future_status fut_status = future_results.value().wait_for(std::chrono::seconds(0));
          if (fut_status == std::future_status::ready) {
              // Future is ready; get the result.
              auto status = future_results.value().get();
              if (!status.ok()) {
                  TracePrintInternal(*(*kTraceOption), "Async result fetch failed: " + status.message);
                  handle_ref.SetStmtState(StmtStates::kStatementExecutedWithRs);
              }
              handle_ref.SetNullFutureGetResultsQuery();  // Reset the future.
              return LogAndReturnCode(handle_ref, status);
          }
          // Future is not ready; indicate ongoing execution.
          return SQL_STILL_EXECUTING;
      }

      // Future is missing; handle as an error.
      TracePrintInternal(*(*kTraceOption), "Missing future object for async result fetching.");
      handle_ref.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
      auto status_record = StatusRecord{SQLStates::k_HY000(), 
                                        "Failed to fetch results asynchronously: missing future"};
      return LogAndReturnCode(handle_ref, status_record);
  }

  StatusRecord ActuallyFetchResults(StatementHandle& stmt_handle) {
      stmt_handle.SetStmtState(StmtStates::kStatementStillExecuting);

      ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
      std::string query_str = stmt_handle.GetQueryString();

      // Split queries for multiple result sets
      std::vector<std::string> queries = google::cloud::odbc_tests::SplitQueries(query_str);
      if (queries.empty()) {
          return StatusRecord{SQLStates::k_HY000(), "No queries to execute"};
      }

      std::vector<ResultSet> all_result_sets;
      for (const auto& query : queries) {
          // Construct the request and fetch data for each query
          PostQueryRequest post_request = ConstructBasicPostQueryRequest(conn_handle, query);
          auto ds_status_record_or = FetchBQData(conn_handle, post_request);

          if (!ds_status_record_or) {
              stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
              return ds_status_record_or.GetStatusRecord();
          }

          // Process query results
          auto rs_status_record_or = ProcessQueryResults(*ds_status_record_or);
          if (!rs_status_record_or) {
              stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
              return rs_status_record_or.GetStatusRecord();
          }

          // Add result set to the list
          all_result_sets.push_back(*rs_status_record_or);
      }

      if (all_result_sets.empty()) {
          stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
          return StatusRecord(SQLStates::k_HY000(), "No result sets received");
      } else {
          // Store all result sets in the statement handle
          stmt_handle.SetAllResultSets(all_result_sets);
          stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
      }

      return StatusRecord::Ok();
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

}


SQLRETURN SQLBindColInternal(SQLHSTMT statement_handle,
                             SQLUSMALLINT column_number,
                             SQLSMALLINT target_c_type, SQLPOINTER target_value,
                             SQLLEN target_value_buffer_len,
                             SQLLEN* target_value_str_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;

  DescriptorHandle& ard = handle->GetDescriptorHandle(DescriptorType::kARD);

  // ----- Validations ---------

  if (column_number < 0) {
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "ColumnNumber should not < 0"};
    return LogAndReturnCode(*handle, status_record);
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle->GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    return LogAndReturnCode(*handle, use_bookmarks_status);
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {SQLStates::k_07006(),
                                  "ColumnNumber should not be 0"};
    return LogAndReturnCode(*handle, status_record);
  }

  // Unbinding column
  if (target_value == nullptr) {
    // Here we don't care about the status record returned by
    // UnbindDescriptorRecord because SQL_SUCCESS should be returned in case of
    // unbound column number too.
    // (https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindcol-function?view=sql-server-ver16#unbinding-columns)
    ard.UnbindDescriptorRecord(column_number);
    return SQL_SUCCESS;
  }

  StatusRecord status_record;
  if (target_value_buffer_len < 0) {
    status_record = {SQLStates::k_HY090(), "BufferLength should not < 0"};
    return LogAndReturnCode(*handle, status_record);
  }

  bool no_desc_bound_previously = !ard.HasDescriptorRecord(column_number);

  // Setting DESC_CONCISE_TYPE will also set  DESC_TYPE and
  // DESC_DATETIME_INTERVAL_CODE
  status_record = SetDescField(&ard, column_number, SQL_DESC_CONCISE_TYPE,
                               ToSqlPointer<SQLSMALLINT>(target_c_type), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  // ----- Set fields for target_value_buffer_len, target_value ------

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH,
                   ToSqlPointer<SQLLEN>(target_value_buffer_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  status_record =
      SetDescField(&ard, column_number, SQL_DESC_DATA_PTR, target_value, 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  // ----- Set fields for target_value_str_len ------

  status_record = SetDescField(&ard, column_number, SQL_DESC_INDICATOR_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }

  status_record = SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH_PTR,
                               ToSqlPointer<SQLLEN*>(target_value_str_len), 0);
  if (!status_record.ok()) {
    no_desc_bound_previously&& ard.UnbindDescriptorRecord(column_number);
    return LogAndReturnCode(*handle, status_record);
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLFetchInternal(SQLHSTMT statement_handle) {
    // Validate the statement handle
    StatusRecordOr<StatementHandle*> handle_result = ValidateStatementHandle(statement_handle);
    if (!handle_result) {
        TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
    }
    StatementHandle& handle = *(*handle_result);

    // Check for valid statement state
    if (handle.GetStmtState() == StmtStates::kStatementExecutedWithoutRs) {
        return SQL_NO_DATA;
    }

    if (handle.GetStmtState() != StmtStates::kStatementExecutedWithRs &&
        handle.GetStmtState() != StmtStates::kStatementAsyncGetResults) {
        StatusRecord status_record = {SQLStates::k_HY010(), "No statement has been executed or fetching is not allowed"};
        return LogAndReturnCode(handle, status_record);
    }

    // Handle async fetch state
    if (handle.GetStmtState() == StmtStates::kStatementAsyncGetResults) {
        SQLRETURN async_result = HandleAsyncGetResults(handle);
        if (async_result == SQL_STILL_EXECUTING) {
            return async_result;
        }
        // Transition state after async fetch completes
        if (async_result != SQL_SUCCESS && async_result != SQL_SUCCESS_WITH_INFO) {
            return async_result;
        }
    }

    // Fetch the next row in the result set
    ResultSet result_set = handle.GetResultSet();
    result_set.cursor++;
    if (result_set.cursor >= result_set.rows.size()) {
        // If rows are exhausted, check for additional result sets
        if (handle.HasMoreResults()) {
            // Transition to the next result set and reset the cursor
            handle.SetStmtState(StmtStates::kStatementExecutedWithRs); // Update state
            result_set = handle.GetNextResultSet(); // Assuming GetNextResultSet handles the transition
            result_set.cursor = 0; // Reset the cursor
            return SQL_SUCCESS;   // Indicate success for transitioning to the new result set
        } else {
            handle.SetStmtState(StmtStates::kStatementResultsConsumed);
            return SQL_NO_DATA;  // Signal that no more rows or result sets are available
        }
    }

    // Determine the rowset size
    DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
    int rowset_size = ard.GetHeaderRecord().array_size;
    if (!rowset_size) {
        rowset_size = 1;
    }

    // Write the rowset to the application buffer
    DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
    StatusRecord status_record = WriteRowset(result_set, rowset_size, ard, ird);
    return LogAndReturnCode(handle, status_record);
}

SQLRETURN SQLNumResultColsInternal(SQLHSTMT statement_handle,
                                   SQLSMALLINT* column_count_ptr) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;
  StatusRecord status_record = StatusRecord::Ok();
  if (column_count_ptr == nullptr) {
    status_record = {SQLStates::k_HY001(),
                     "Parameter 'column_count_ptr' cannot be null"};
    return LogAndReturnCode(*handle, status_record);
  }

  auto stmt_state = handle->GetStmtState();
  switch (stmt_state) {
    case StmtStates::kStatementPrepared:
    case StmtStates::kStatementExecutedWithRs:
      break;
    case StmtStates::kStatementExecutedWithoutRs:
      status_record = {SQLStates::k_01000(), "Statement Executed without Data"};
      break;
    case StmtStates::kStatementStillExecuting:
      status_record = {SQLStates::k_HY010(), "Statement is still executing"};
      break;
    case StmtStates::kNeedsPutData:
      status_record = {SQLStates::k_HY010(),
                       "Statement needs Data to be executed"};
      break;
    default:
      status_record = {SQLStates::k_HY010(), "No statement has been executed"};
      break;
  }
  if (!status_record.ok()) {
    return LogAndReturnCode(*handle, status_record);
  }
  DescriptorHandle ird = handle->GetDescriptorHandle(DescriptorType::kIRD);
  if (ird.GetHeaderRecord().count < 0) {
    status_record = {SQLStates::k_07006(),
                     "ColumnCount should not be less than 0"};
    return LogAndReturnCode(*handle, status_record);
  }
  *column_count_ptr = ird.GetHeaderRecord().count;
  return status_record.CalculateReturnCode();
}

SQLRETURN SQLGetTypeInfoInternal(SQLHSTMT stmt_handle, SQLSMALLINT data_type) {
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  StatementHandle& handle = *(*handle_result);

  ResultSet result_set;
  if (data_type == SQL_ALL_TYPES) {
    for (auto [sql_data_type, bq_data_type_info] : kSqlToBqDataTypes) {
      for (auto [bq_data_type, type_info] : bq_data_type_info) {
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
      }
    }
  } else {
    if (kSqlToBqDataTypes.count(data_type)) {
      for (auto [bq_data_type, type_info] : kSqlToBqDataTypes.at(data_type)) {
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
      }
    }
  }

  if (!result_set.rows.empty()) {
    CreateTypeInfoRowSchema(result_set);
    handle.SetResultSet(result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }

  return SQL_SUCCESS;
}

SQLRETURN SQLDescribeColInternal(
    SQLHSTMT statement_handle, SQLUSMALLINT column_number, SQLCHAR* column_name,
    SQLSMALLINT column_name_buffer_len, SQLSMALLINT* column_name_le,
    SQLSMALLINT* column_sql_data_type, SQLULEN* column_size,
    SQLSMALLINT* decimal_digits, SQLSMALLINT* column_nullable) {
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

  if (column_number < 0) {
    StatusRecord status_record = {
        SQLStates::k_HY000(),
        "Invalid ColumnNumber parameter - should not be < 0"};
    return LogAndReturnCode(handle, status_record);
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle.GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    return LogAndReturnCode(handle, use_bookmarks_status);
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {
        SQLStates::k_07006(),
        "Invalid column number value for bookmark attribute - should not be 0"};
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  if (!ird.HasDescriptorRecord(column_number)) {
    StatusRecord status_record = {
        SQLStates::k_07009(),
        "Invalid descriptor index - no column for such value"};
    return LogAndReturnCode(handle, status_record);
  }

  DescriptorRecord& desc_record = ird.GetDescriptorRecord(column_number);

  StatusRecord status_record =
      StringValueToOutputBufferResponse(desc_record.name.c_str(), column_name,
                                        column_name_buffer_len, column_name_le);
  if (!status_record.ok()) {
    return LogAndReturnCode(handle, status_record);
  }

  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      desc_record.concise_type, column_sql_data_type, nullptr);

  switch (desc_record.concise_type) {
    case SQL_NUMERIC:
    case SQL_DECIMAL:
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    case SQL_BIGINT:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.precision, column_size, nullptr);
      break;
    default:
      IntValueToOutputBufferResponse<SQLULEN, SQLSMALLINT>(
          desc_record.length, column_size, nullptr);
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
          desc_record.precision, decimal_digits, nullptr);
      break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT:
      IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
          desc_record.scale, decimal_digits, nullptr);
      break;
    default:
      *decimal_digits = 0;
  }
  IntValueToOutputBufferResponse<SQLSMALLINT, SQLSMALLINT>(
      desc_record.nullable, column_nullable, nullptr);

  return SQL_SUCCESS;
}

SQLRETURN SQLColAttributeInternal(SQLHSTMT statement_handle,
                                  SQLUSMALLINT column_number,
                                  SQLUSMALLINT field_identifier,
                                  SQLPOINTER char_attr,
                                  SQLSMALLINT char_attr_buffer_len,
                                  SQLSMALLINT* char_attr_string_len,
                                  SQLLEN* numeric_attribute) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);

  StatusRecordOr<SQLRETURN> result;
  switch (field_identifier) {
    case SQL_DESC_BASE_COLUMN_NAME:
    case SQL_DESC_BASE_TABLE_NAME:
    case SQL_DESC_CATALOG_NAME:
    case SQL_DESC_LABEL:
    case SQL_DESC_LITERAL_PREFIX:
    case SQL_DESC_LITERAL_SUFFIX:
    case SQL_DESC_LOCAL_TYPE_NAME:
    case SQL_DESC_NAME:
    case SQL_DESC_SCHEMA_NAME:
    case SQL_DESC_TABLE_NAME:
    case SQL_DESC_TYPE_NAME:
      result =
          GetDescField(&ird, static_cast<SQLSMALLINT>(column_number),
                       static_cast<SQLSMALLINT>(field_identifier), char_attr,
                       static_cast<SQLINTEGER>(char_attr_buffer_len),
                       reinterpret_cast<SQLINTEGER*>(char_attr_string_len));
      break;
    default:
      result = GetDescField(&ird, static_cast<SQLSMALLINT>(column_number),
                            static_cast<SQLSMALLINT>(field_identifier),
                            numeric_attribute, 0, nullptr);
  }
  return LogAndReturnCode(stmt_handle, result);
}

SQLRETURN SQLCloseCursorInternal(SQLHSTMT statement_handle) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& stmt_handle = *(*handle_result);

  if (!stmt_handle.IsCursorOpen()) {
    StatusRecord status_record = {
        SQLStates::k_24000(), "Invalid cursor state - cursor was not opened"};
    return LogAndReturnCode(stmt_handle, status_record);
  }

  stmt_handle.CloseCursor();

  return SQL_SUCCESS;
}

SQLRETURN SQLMoreResultsInternal(SQLHSTMT statement_handle) {
    // Validate the statement handle first
    StatusRecordOr<StatementHandle*> handle_result =
        ValidateStatementHandle(statement_handle);
    if (!handle_result) {
        TracePrintInternal(*(*kTraceOption),
                           handle_result.GetStatusRecord().message);
        return handle_result.GetCalculatedReturnCode();
    }
    StatementHandle& stmt_handle = *(*handle_result);

    // Check for cancelled statement
    if (stmt_handle.IsOperationCanceled()) {
        StatusRecord status_record = {
            SQLStates::k_HY008(),
            "Statement has been cancelled"};
        return LogAndReturnCode(stmt_handle, status_record);
    }

    // Handle asynchronous execution and result fetching if needed
    StatusRecordOr<SQLULEN> async_enable_status =
        stmt_handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE);
    if (!async_enable_status) {
        return LogAndReturnCode(stmt_handle, async_enable_status.GetStatusRecord());
    }

    // Check if the statement is still executing
    if (stmt_handle.GetStmtState() == StmtStates::kStatementStillExecuting) {
        StatusRecord status_record = {
            SQLStates::k_HY010(),
            "Function sequence error - statement is still executing"};
        return LogAndReturnCode(stmt_handle, status_record);
    }

    // Handle asynchronous execution in progress (initial state)
    if (stmt_handle.GetStmtState() == StmtStates::kStatementAsyncExecute) {
        return HandleAsyncExecute(stmt_handle);  // Handle async execution completion
    }

    // Handle asynchronous result fetching if async results are enabled
    if (stmt_handle.GetStmtState() == StmtStates::kStatementAsyncGetResults) {
        SQLRETURN async_result = HandleAsyncGetResults(stmt_handle);
        if (async_result == SQL_STILL_EXECUTING) {
            TracePrintInternal(*(*kTraceOption), "Statement still executing asynchronously.");
            return async_result;
        }
        // Check if operation completed successfully or was canceled
        if (async_result == SQL_SUCCESS || async_result == SQL_SUCCESS_WITH_INFO) {
            TracePrintInternal(*(*kTraceOption), "Async results successfully fetched.");
        } else {
            TracePrintInternal(*(*kTraceOption), "Async fetch resulted in error or cancellation.");
        }
        return async_result;
    }

    // Handle already executed statement without results (non-SELECT queries)
    if (stmt_handle.GetStmtState() == StmtStates::kStatementExecutedWithoutRs ||
        stmt_handle.GetStmtState() == StmtStates::kStatementResultsConsumed) {
        TracePrintInternal(*(*kTraceOption), "No more results available in the statement.");
        return SQL_NO_DATA;  // No results or result set consumed
    }

    // If no more results, fetch the results synchronously
    StatusRecord fetch_status = ActuallyFetchResults(stmt_handle);
    if (!SQL_SUCCEEDED(fetch_status.CalculateReturnCode())) {
        // If the fetch fails, check if it's due to a no-results condition
        if (fetch_status.CalculateReturnCode() == SQL_ERROR) {
            TracePrintInternal(*(*kTraceOption), "No data available during fetch.");
            return SQL_NO_DATA;  // Explicitly handle no data condition
        }
        TracePrintInternal(*(*kTraceOption), "Error occurred while fetching results: " + fetch_status.message);
        return LogAndReturnCode(stmt_handle, fetch_status);  // Error in fetching results
    }

    // Final check to see if we have more results
    if (stmt_handle.HasMoreResults()) {
        stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithRs);  // Transition to executed state
        TracePrintInternal(*(*kTraceOption), "More results are available.");
        return SQL_SUCCESS_WITH_INFO;  // Indicate that there are more results
    }

    TracePrintInternal(*(*kTraceOption), "No more results available.");
    return SQL_NO_DATA;  // No more results
}

}  // namespace google::cloud::odbc_bq_driver
