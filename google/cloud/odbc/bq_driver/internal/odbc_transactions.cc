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

#include "google/cloud/odbc/bq_driver/internal/odbc_transactions.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using google::cloud::odbc_bq_driver_internal::ConstructBasicPostQueryRequest;
using google::cloud::odbc_bq_driver_internal::FetchBQData;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

StatusRecord BeginTransactionIfNeeded(ConnectionHandle& conn_handle) {
  if (conn_handle.IsTransactionActive()) {
    return StatusRecord::Ok();
  }
  SQLUINTEGER auto_commit = 0;
  auto attribute_status =
      conn_handle.GetAttribute(SQL_ATTR_AUTOCOMMIT, &auto_commit, 0, nullptr);
  if (!attribute_status.ok()) {
    LOG(ERROR) << "BeginTransactionIfNeeded::GetAttribute:: "
               << attribute_status.message;
    return attribute_status;
  }
  if (auto_commit == SQL_AUTOCOMMIT_ON) {
    return StatusRecord::Ok();
  }

  std::string query = "BEGIN TRANSACTION;";
  PostQueryRequest post_request =
      ConstructBasicPostQueryRequest(conn_handle, query);

  auto ds_status_record_or = FetchBQData(conn_handle, post_request);
  if (!ds_status_record_or) {
    LOG(ERROR) << "BeginTransactionIfNeeded::FetchBQData:: "
               << ds_status_record_or.GetStatusRecord().message;
    return ds_status_record_or.GetStatusRecord();
  }
  conn_handle.SetTransactionActive(true);
  return StatusRecord::Ok();
}

StatusRecord FinishTransactionIfNeeded(ConnectionHandle& conn_handle,
                                       SQLSMALLINT completion_type) {
  if (!conn_handle.IsTransactionActive()) {
    return StatusRecord::Ok();
  }
  if (completion_type != SQL_COMMIT && completion_type != SQL_ROLLBACK) {
    LOG(ERROR) << "FinishTransactionIfNeeded::Invalid completion type: "
               << completion_type;
    return {SQLStates::k_HY012(), "Invalid transaction operation code"};
  }
  std::string query = completion_type == SQL_COMMIT ? "COMMIT TRANSACTION;"
                                                    : "ROLLBACK TRANSACTION;";
  PostQueryRequest post_request =
      ConstructBasicPostQueryRequest(conn_handle, query);

  auto ds_status_record_or = FetchBQData(conn_handle, post_request);
  if (!ds_status_record_or) {
    LOG(ERROR) << "FinishTransactionIfNeeded::FetchBQData:: "
               << ds_status_record_or.GetStatusRecord().message;
    return ds_status_record_or.GetStatusRecord();
  }
  conn_handle.SetTransactionActive(false);
  for (auto* stmt_handle : conn_handle.GetStatementHandles()) {
    stmt_handle->CloseCursor();
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
