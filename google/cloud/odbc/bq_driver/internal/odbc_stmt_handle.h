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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_HANDLE_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_query.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_attr.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/minimal/internal/common_v2_resources.h"
#include <map>
#include <memory>

namespace google::cloud::odbc_bq_driver_internal {

// These are the states statement handle maintains for validations in ODBC APIs
// using it.
enum class StmtStates {
  kStatementNotPrepared,
  kStatementPrepared,
  kStatementAsyncPrepare,
  kStatementAsyncExecute,
  kStatementStillExecuting,
  kStatementExecutedWithoutRs,
  kStatementExecutedWithRs,
  kNeedsParams,
  kNeedsPutData,
};

class ConnectionHandle;

class StatementHandle : public Handle {
 public:
  // This constructor is used only for tests
  explicit StatementHandle(ConnectionHandle* conn_handle = nullptr)
      : conn_handle_(conn_handle){};
  explicit StatementHandle(ConnectionHandle* conn_handle,
                           Descriptors const& descriptors)
      : conn_handle_(conn_handle),
        descriptors_(std::move(descriptors)),
        attributes_(kDefaultAttributes){};

  ~StatementHandle() = default;

  StatementHandle(StatementHandle const& statementHandle);
  StatementHandle& operator=(StatementHandle const& statementHandle);
  StatementHandle(StatementHandle&& statementHandle) noexcept;
  StatementHandle& operator=(StatementHandle&& statementHandle) noexcept;

  [[nodiscard]] DescriptorHandle& GetDescriptorHandle(
      DescriptorType type) const;

  odbc_internal::StatusRecord SetDescriptorHandle(
      DescriptorType type, DescriptorHandle* descriptor_handle);

  odbc_internal::StatusRecord SetAttribute(int attribute, SQLULEN value);
  odbc_internal::StatusRecordOr<SQLULEN> GetAttribute(int attribute);

  static odbc_internal::StatusRecord PopulateIrd(
      DescriptorHandle& descriptor_handle,
      google::cloud::bigquery_v2_minimal_internal::TableSchema const& schema);

  static odbc_internal::StatusRecord PopulateIpd(
      DescriptorHandle& handle,
      google::cloud::bigquery_v2_minimal_internal::JobStatistics const&
          job_statistics);

  odbc_internal::StatusRecord PrepareQuery(const SQLCHAR* query_text);
  HandleType kType = HandleType::kStmtHandle;

  inline ConnectionHandle* GetConnectionHandle() { return conn_handle_; };

  inline void SetCursorName(std::string& cursor_name) {
    cursor_name_ = cursor_name;
  };

  [[nodiscard]] inline std::string GetCursorName() { return cursor_name_; };

  inline void SetStmtState(StmtStates stmt_state) { stmt_state_ = stmt_state; }

  [[nodiscard]] inline StmtStates GetStmtState() const { return stmt_state_; }

  inline void SetResultSet(ResultSet const& result_set) {
    result_set_ = result_set;
  }

  [[nodiscard]] inline SQLSMALLINT GetParamCount() const {
    return query_parameters_.size();
  }

  [[nodiscard]] inline ResultSet const& GetResultSet() const {
    return result_set_;
  }

  inline bool IsCursorOpen() const {
    return stmt_state_ == StmtStates::kStatementExecutedWithRs;
  }

  void CloseCursor();

  [[nodiscard]] inline std::vector<
      google::cloud::bigquery_v2_minimal_internal::QueryParameter> const&
  GetQueryParameters() const {
    return query_parameters_;
  }

  inline void SetQueryParameters(
      std::vector<
          google::cloud::bigquery_v2_minimal_internal::QueryParameter> const&
          query_parameters) {
    query_parameters_ = query_parameters;
  }

  [[nodiscard]] inline std::string GetQueryString() const { return query_str_; }

  [[nodiscard]] inline std::optional<
      ::google::cloud::bigquery_v2_minimal_internal::Job>
  GetPreparedJob() {
    return prepared_job_;
  }

  void SetNullPreparedJob() { prepared_job_ = std::nullopt; }

  inline static bool WasStatementPrepared() {
    // TODO(b/358002035) Implement this function
    return true;
  }

  // Setters and Getters related to canceling an operation.
  inline bool IsOperationCanceled() const { return operation_canceled_; }

  inline void EnableCancellation() { operation_canceled_ = true; }

  inline void DisableCancellation() { operation_canceled_ = false; }

  std::mutex& GetMutex() const { return statement_handle_mutex_; }

  std::optional<std::future<StatusRecord>> GetPossibleFuturePrepareQuery() {
    return std::move(future_prepare_query_);
  }

  std::optional<std::future<StatusRecord>> GetPossibleFutureExecuteQuery() {
    return std::move(future_execute_query_);
  }

  void SetFuturePrepareQuery(std::future<StatusRecord> fut_prepare_query) {
    future_prepare_query_ = std::move(fut_prepare_query);
  }
  void SetNullFuturePrepareQuery() { future_prepare_query_ = std::nullopt; }

  void SetFutureExecuteQuery(std::future<StatusRecord> fut_execute_query) {
    future_execute_query_ = std::move(fut_execute_query);
  }
  void SetNullFutureExecuteQuery() { future_execute_query_ = std::nullopt; }

 protected:
  StmtStates stmt_state_ = StmtStates::kStatementNotPrepared;
  ResultSet result_set_;
  std::string query_str_;

 private:
  std::shared_ptr<Query> query_;
  Descriptors descriptors_;
  std::map<int, SQLULEN> attributes_;
  ConnectionHandle* conn_handle_{nullptr};
  std::string cursor_name_;
  mutable std::mutex statement_handle_mutex_;
  std::vector<google::cloud::bigquery_v2_minimal_internal::QueryParameter>
      query_parameters_;
  std::optional<google::cloud::bigquery_v2_minimal_internal::Job> prepared_job_;
  odbc_internal::StatusRecord PopulateResultSet(
      google::cloud::bigquery_v2_minimal_internal::TableSchema const& schema);
  bool operation_canceled_{false};
  // Needed for cancellation and re-execution of asynchronous prepare requests.
  std::optional<std::future<StatusRecord>> future_prepare_query_;
  // Needed for cancellation and re-execution of asynchronous execute requests.
  std::optional<std::future<StatusRecord>> future_execute_query_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_HANDLE_H
