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
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {
using google::cloud::bigquery_v2_minimal_internal::TableReference;
using ::google::cloud::odbc_internal::SQLStates;

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
  kNeedsPutData
};

class ConnectionHandle;

class StatementHandle : public Handle {
 public:
  // This constructor is used only for tests
  explicit StatementHandle(ConnectionHandle* conn_handle = nullptr)
      : conn_handle_(conn_handle) {};
  explicit StatementHandle(ConnectionHandle* conn_handle,
                           Descriptors const& descriptors)
      : conn_handle_(conn_handle),
        descriptors_(std::move(descriptors)),
        attributes_(kDefaultAttributes) {};

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
      google::cloud::bigquery_v2_minimal_internal::TableSchema const& schema,
      TableReference const& table_fields);

  static odbc_internal::StatusRecord PopulateIpd(
      DescriptorHandle& handle,
      google::cloud::bigquery_v2_minimal_internal::JobStatistics const&
          job_statistics);

  odbc_internal::StatusRecord PrepareQuery(std::string const& query);
  HandleType kType = HandleType::kStmtHandle;

  inline ConnectionHandle* GetConnectionHandle() { return conn_handle_; };

  inline void SetCursorName(std::string& cursor_name) {
    cursor_name_ = cursor_name;
  };

  [[nodiscard]] inline std::string GetCursorName() { return cursor_name_; };

  inline void SetStmtState(StmtStates stmt_state) { stmt_state_ = stmt_state; }

  [[nodiscard]] inline StmtStates GetStmtState() const { return stmt_state_; }

  [[nodiscard]] inline SQLSMALLINT GetParamCount() const {
    return query_parameters_.size();
  }

  // Setters and Getters related to unprocessed results from the client
  // interface
  DSResults& GetDSResults() { return ds_results_; }

  inline void SetDSResults(DSResults const& ds_results) {
    ds_results_ = ds_results;
  }

  // Getter for the iterator
  inline std::optional<StreamRange<
      ::google::cloud::bigquery::storage::v1::ReadRowsResponse>::iterator>&
  GetReadRowsIterator() {
    return read_rows_iterator_;
  }

  // Setter for the iterator
  inline void SetReadRowsIterator(
      StreamRange<::google::cloud::bigquery::storage::v1::ReadRowsResponse>::
          iterator it) {
    read_rows_iterator_ = std::move(it);
  }

  // Clearer for the iterator
  inline void ClearReadRowsIterator() { read_rows_iterator_.reset(); }

  // Returns true if read_rows_stream_ is NOT null (i.e., has a value).
  inline bool IsHtapiEnabled() const { return read_rows_stream_.has_value(); }

  void SetReadRowsStream(
      StreamRange<::google::cloud::bigquery::storage::v1::ReadRowsResponse>
          stream_range) {
    read_rows_stream_ = std::move(stream_range);
  }

  std::optional<
      StreamRange<::google::cloud::bigquery::storage::v1::ReadRowsResponse>>&
  GetReadRowsStream() {
    return read_rows_stream_;
  }

  // Clearer: Provides the mechanism to "clear" the stream.
  inline void ClearReadRowsStream() { read_rows_stream_.reset(); }

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
  inline void SetArrowSchema(std::shared_ptr<arrow::Schema> arrow_schema) {
    result_set_.arrow_schema = arrow_schema;
  }

  inline std::shared_ptr<arrow::Schema> GetArrowSchema() {
    return result_set_.arrow_schema;
  }
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

  inline void SetResultSet(ResultSet const& result_set) {
    result_set_ = result_set;
  }

  // Getter for the results processed by the Driver
  [[nodiscard]] inline ResultSet const& GetResultSet() const {
    return result_set_;
  }
  // non-const Getter for the results processed by the Driver
  [[nodiscard]] inline ResultSet& GetResultSet() { return result_set_; }

  inline bool IsCursorOpen() const {
    return stmt_state_ == StmtStates::kStatementExecutedWithRs;
  }

  void CloseCursor();

  [[nodiscard]] inline std::vector<
      google::cloud::bigquery_v2_minimal_internal::QueryParameter>&
  GetQueryParameters() {
    return query_parameters_;
  }

  inline void SetQueryParameters(
      std::vector<
          google::cloud::bigquery_v2_minimal_internal::QueryParameter> const&
          query_parameters) {
    query_parameters_ = query_parameters;
  }

  inline void SetQueryString(std::string& query_str) {
    query_str_ = query_str;
  };

  [[nodiscard]] inline std::string GetQueryString() const { return query_str_; }

  [[nodiscard]] inline std::optional<
      ::google::cloud::bigquery_v2_minimal_internal::Job>
  GetPreparedJob() {
    return prepared_job_;
  }

  void SetPreparedJob(
      ::google::cloud::bigquery_v2_minimal_internal::Job const& job) {
    prepared_job_ = job;
  }

  void SetNullPreparedJob() { prepared_job_ = std::nullopt; }

  // `StmtStates` will get updated when SQLExecute is called.
  // `is_statement_prepared_` is supposed to persist throughout the life of a
  // query. This is used to differentiate (SQLPrepare + SQLExecute) vs
  // SQLExecDirect.
  void SetStatementPrepared() { is_statement_prepared_ = true; }

  inline bool StatementPrepared() const { return is_statement_prepared_; }

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

  std::optional<std::future<StatusRecord>> GetPossibleFutureExecDirectQuery() {
    return std::move(future_exec_direct_query_);
  }

  std::optional<std::future<StatusRecord>> GetPossibleFutureMoreResults() {
    return std::move(future_more_results_query_);
  }

  void SetFuturePrepareQuery(std::future<StatusRecord> fut_prepare_query) {
    future_prepare_query_ = std::move(fut_prepare_query);
  }
  void SetNullFuturePrepareQuery() { future_prepare_query_ = std::nullopt; }

  void SetFutureExecuteQuery(std::future<StatusRecord> fut_execute_query) {
    future_execute_query_ = std::move(fut_execute_query);
  }
  void SetNullFutureExecuteQuery() { future_execute_query_ = std::nullopt; }

  void SetFutureExecDirectQuery(
      std::future<StatusRecord> fut_exec_direct_query) {
    future_exec_direct_query_ = std::move(fut_exec_direct_query);
  }
  void SetNullFutureExecDirectQuery() {
    future_exec_direct_query_ = std::nullopt;
  }

  void SetFutureMoreResultsQuery(
      std::future<StatusRecord> fut_more_results_query) {
    future_more_results_query_ = std::move(fut_more_results_query);
  }

  void SetNullFutureMoreResultsQuery() {
    future_more_results_query_ = std::nullopt;
  }

  bool HasJobData() const { return !job_data_.empty(); }

  void SetJobData(std::string const& job_id,
                  std::string const& statement_type) {
    // Store the job ID and statement type as a pair in the vector
    job_data_.emplace_back(job_id, statement_type);
  }

  // Gets next the job ID and statement type as a pair in the vector
  StatusRecordOr<std::pair<std::string, std::string>> GetNextJobData() const {
    if (!job_data_.empty()) {
      return job_data_.back();
    }
    return StatusRecord{SQLStates::k_HY000(), "No job data available"};
  }

  // Deletes next the job ID and statement type as a pair in the vector
  void DeleteNextJobData() {
    if (!job_data_.empty()) {
      job_data_.pop_back();
    }
  }

  SQLUSMALLINT GetCurrentParamIndex() const { return current_param_index_; }

  inline void SetCurrentParamIndex(SQLUSMALLINT param_index) {
    current_param_index_ = param_index;
  }

  inline void SetIsPartialPutdataCalled(bool partial_putdata_called) {
    is_partial_putdata_called_ = partial_putdata_called;
  }

  bool GetIsPartialPutdataCalled() const { return is_partial_putdata_called_; }

 protected:
  StmtStates stmt_state_ = StmtStates::kStatementNotPrepared;
  ResultSet result_set_;
  std::string query_str_;
  SQLUSMALLINT current_param_index_ = 0;
  // Needed for handling partial put data calls
  bool is_partial_putdata_called_ = false;

 private:
  std::shared_ptr<Query> query_;
  Descriptors descriptors_;
  std::map<int, SQLULEN> attributes_;
  ConnectionHandle* conn_handle_{nullptr};
  std::string cursor_name_;
  DSResults ds_results_;

  std::optional<StreamRange<
      ::google::cloud::bigquery::storage::v1::ReadRowsResponse>::iterator>
      read_rows_iterator_;
  std::optional<
      StreamRange<::google::cloud::bigquery::storage::v1::ReadRowsResponse>>
      read_rows_stream_;
  mutable std::mutex statement_handle_mutex_;
  ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest
      post_query_request_;
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
  // Needed for cancellation and re-execution of asynchronous ExecDirect
  // requests.
  std::optional<std::future<StatusRecord>> future_exec_direct_query_ =
      std::nullopt;
  // Needed for cancellation and re-execution of asynchronous more results
  // requests.
  std::optional<std::future<StatusRecord>> future_more_results_query_;
  // vector of pair of jobs Ids and respective statement types.
  std::vector<std::pair<std::string, std::string>> job_data_;
  bool is_statement_prepared_ = false;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_HANDLE_H
