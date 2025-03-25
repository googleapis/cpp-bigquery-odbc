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

#include "google/cloud/odbc/bq_driver/odbc_sql_requests.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::bigquery_v2_minimal_internal::Job;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::
    CreateDescRecordWithRandomValues;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateStmtHandleWithState;
using ::testing::HasSubstr;

TEST(SQLBindParameterInternal, Fail_InvalidHandle) {
  DescriptorHandle desc_handle;
  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_CHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  SQLRETURN status = SQLBindParameterInternal(
      &desc_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLBindParameterInternal, Fail_ParameterNumberIsZero) {
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLUSMALLINT param_number = 0;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_CHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  SQLRETURN status = SQLBindParameterInternal(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_FALSE(stmt_handle.GetStmtState() == StmtStates::kNeedsParams);
}

TEST(SQLBindParameterInternal, Fail_BufferLengthIzNegative) {
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_CHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = -40;
  SQLLEN str_len = 50;

  SQLRETURN status = SQLBindParameterInternal(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_FALSE(stmt_handle.GetStmtState() == StmtStates::kNeedsParams);
}

TEST(SQLBindParameterInternal, DataAtExecutionParameters) {
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = SQL_CHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 0;
  SQLLEN buff_len = col_size;
  SQLLEN str_len = SQL_LEN_DATA_AT_EXEC(buff_len);

  SQLBindParameterInternal(&stmt_handle, param_number, in_out_type, value_type,
                           param_type, col_size, decimal_digits,
                           (SQLPOINTER)SQL_DATA_AT_EXEC, buff_len, &str_len);

  EXPECT_TRUE(stmt_handle.GetStmtState() == StmtStates::kNeedsParams);
}

TEST(SQLBindParameterInternal,
     FailToSetInvalidType_SQL_DESC_COUNT_IsNotUpdated_APD) {
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = 11111;
  SQLSMALLINT param_type = SQL_CHAR;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  SQLRETURN status = SQLBindParameterInternal(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ(0, stmt_handle.GetDescriptorHandle(DescriptorType::kAPD)
                   .GetHeaderRecord()
                   .count);
  EXPECT_FALSE(stmt_handle.GetStmtState() == StmtStates::kNeedsParams);
}

TEST(SQLBindParameterInternal,
     FailToSetInvalidType_SQL_DESC_COUNT_IsNotUpdated_IPD) {
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLUSMALLINT param_number = 1;
  SQLSMALLINT in_out_type = SQL_PARAM_INPUT;
  SQLSMALLINT value_type = SQL_C_CHAR;
  SQLSMALLINT param_type = 11111;
  SQLULEN col_size = 10;
  SQLSMALLINT decimal_digits = 20;
  SQLINTEGER param_val = 30;
  SQLLEN buff_len = 40;
  SQLLEN str_len = 50;

  SQLRETURN status = SQLBindParameterInternal(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ(0, stmt_handle.GetDescriptorHandle(DescriptorType::kIPD)
                   .GetHeaderRecord()
                   .count);
  EXPECT_FALSE(stmt_handle.GetStmtState() == StmtStates::kNeedsParams);
}

void AssertDescribeParamResults(SQLRETURN status,
                                DescriptorRecord const& record,
                                SQLSMALLINT data_type, SQLULEN param_size,
                                SQLSMALLINT decimal_digits,
                                SQLSMALLINT nullable) {
  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(record.concise_type, data_type);
  switch (data_type) {
    case SQL_NUMERIC:
    case SQL_DECIMAL:
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    case SQL_BIGINT:
      EXPECT_EQ(record.precision, param_size);
      break;
    default:
      EXPECT_EQ(record.length, param_size);
  }
  switch (data_type) {
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
    case SQL_INTERVAL_SECOND:
    case SQL_INTERVAL_DAY_TO_SECOND:
    case SQL_INTERVAL_HOUR_TO_SECOND:
    case SQL_INTERVAL_MINUTE_TO_SECOND:
      EXPECT_EQ(record.precision, decimal_digits);
      break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT:
      EXPECT_EQ(record.scale, decimal_digits);
      break;
    default:
      EXPECT_EQ(0, decimal_digits);
  }
  EXPECT_EQ(record.nullable, nullable);
}

TEST(SQLDescribeParam, Fail_InvalidHandle) {
  SQLSMALLINT data_type = 0;
  SQLULEN param_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;

  SQLRETURN status = SQLDescribeParamInternal(
      nullptr, 1, &data_type, &param_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLDescribeParam, Fail_ParameterNumberIsZero) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  SQLSMALLINT data_type = 0;
  SQLULEN param_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;

  SQLRETURN status = SQLDescribeParamInternal(
      &stmt_handle, 0, &data_type, &param_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeParam, Fail_InvalidParameterNumber) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  SQLSMALLINT data_type = 0;
  SQLULEN param_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;

  SQLRETURN status = SQLDescribeParamInternal(
      &stmt_handle, 10, &data_type, &param_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeParam, Fail_StatementIsNotPrepared) {
  StatementHandle stmt_handle = CreateStatementHandle();
  DescriptorRecord record;
  DescriptorHandle& ipd = stmt_handle.GetDescriptorHandle(DescriptorType::kIPD);
  SQLUSMALLINT param_number = 1;
  ipd.BindNewDescriptorRecord(param_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN param_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLRETURN status =
      SQLDescribeParamInternal(&stmt_handle, param_number, &data_type,
                               &param_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY010(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeParam, Describe_SQL_NUMERIC) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_NUMERIC);
  DescriptorHandle& ipd = stmt_handle.GetDescriptorHandle(DescriptorType::kIPD);
  SQLUSMALLINT param_number = 1;
  ipd.BindNewDescriptorRecord(param_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN param_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLRETURN status =
      SQLDescribeParamInternal(&stmt_handle, param_number, &data_type,
                               &param_size, &decimal_digits, &nullable);

  AssertDescribeParamResults(status, record, data_type, param_size,
                             decimal_digits, nullable);
}

TEST(SQLDescribeParam, Describe_SQL_CHAR) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_CHAR);
  DescriptorHandle& ipd = stmt_handle.GetDescriptorHandle(DescriptorType::kIPD);
  SQLUSMALLINT param_number = 1;
  ipd.BindNewDescriptorRecord(param_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN param_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLRETURN status =
      SQLDescribeParamInternal(&stmt_handle, param_number, &data_type,
                               &param_size, &decimal_digits, &nullable);

  AssertDescribeParamResults(status, record, data_type, param_size,
                             decimal_digits, nullable);
}

TEST(SQLDescribeParam, Describe_SQL_DATE) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_TYPE_DATE);
  DescriptorHandle& ipd = stmt_handle.GetDescriptorHandle(DescriptorType::kIPD);
  SQLUSMALLINT param_number = 1;
  ipd.BindNewDescriptorRecord(param_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN param_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLRETURN status =
      SQLDescribeParamInternal(&stmt_handle, param_number, &data_type,
                               &param_size, &decimal_digits, &nullable);

  AssertDescribeParamResults(status, record, data_type, param_size,
                             decimal_digits, nullable);
}

TEST(SQLNumParamsInternal, Fails_InvalidHandle) {
  SQLSMALLINT num_param = 0;

  SQLRETURN status = SQLNumParamsInternal(nullptr, &num_param);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLNumParamsInternal, Fail_StatementIsNotPrepared) {
  StatementHandle handle = CreateStatementHandle();
  SQLSMALLINT num_param = 0;

  SQLRETURN status = SQLNumParamsInternal(&handle, &num_param);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY010(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLNumParamsInternal, ReturnsParamCount) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  ::google::cloud::bigquery_v2_minimal_internal::QueryParameter
      query_parameter = {"min_age", {"INTEGER"}, {"30"}};
  handle.SetQueryParameters({query_parameter});
  SQLSMALLINT num_param = 0;

  SQLRETURN status = SQLNumParamsInternal(&handle, &num_param);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(1, num_param);
}

TEST(SQLPrepareInternal, Fail_InvalidHandle) {
  StatementHandle* stmt_handle = nullptr;
  std::string queryStr = "Select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = queryStr.length();

  SQLRETURN status = SQLPrepareInternal(stmt_handle, query, len);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLPrepareInternal, InvalidQueryLength) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementNotPrepared);
  std::string queryStr = "select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = 0;

  SQLRETURN status = SQLPrepareInternal(&handle, query, len);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY090(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Invalid query length"));
}

TEST(SQLPrepareInternal, NullQueryText) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementNotPrepared);

  SQLRETURN status = SQLPrepareInternal(&handle, nullptr, SQL_NTS);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Query text is null or empty"));
}

TEST(SQLPrepareInternal, EmptyQueryText) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementNotPrepared);
  std::string queryStr = "";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();

  SQLRETURN status = SQLPrepareInternal(&handle, query, SQL_NTS);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Query text is null or empty"));
}

TEST(SQLPrepareInternal, DisableCancellation_PreviouslyCompletedOperation) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  handle.EnableCancellation();
  std::string queryStr = "Select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = queryStr.length();

  SQLRETURN status = SQLPrepareInternal(&handle, query, len);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);
}

TEST(SQLPrepareInternal, PreviouslyOngoingAsyncOperation_Canceled) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementAsyncPrepare);
  handle.EnableCancellation();
  std::string queryStr = "Select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = queryStr.length();

  SQLRETURN status = SQLPrepareInternal(&handle, query, len);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY008(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Operation canceled"));
}

TEST(SQLPrepareInternal, PreviouslyOngoingAsyncOperation_NotCanceled) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementAsyncPrepare);
  handle.SetAttribute(SQL_ATTR_ASYNC_ENABLE, SQL_ASYNC_ENABLE_ON);
  handle.DisableCancellation();
  std::string queryStr = "Select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = queryStr.length();

  SQLRETURN status = SQLPrepareInternal(&handle, query, len);

  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("cannot prepare query asynchronously"));
}

TEST(SQLExecuteInternal, PreviouslyOngoingAsyncOperation_Canceled) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementAsyncExecute);
  handle.EnableCancellation();

  SQLRETURN status = SQLExecuteInternal(&handle);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementPrepared);

  EXPECT_EQ(SQLStates::k_HY008(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Operation canceled"));
}

TEST(SQLExecuteInternal, PreviouslyOngoingAsyncOperation_NotCanceled) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementAsyncExecute);
  handle.SetAttribute(SQL_ATTR_ASYNC_ENABLE, SQL_ASYNC_ENABLE_ON);
  handle.DisableCancellation();

  SQLRETURN status = SQLExecuteInternal(&handle);

  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementPrepared);

  EXPECT_EQ(SQLStates::k_HY000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("cannot execute query asynchronously"));
}

TEST(SQLExecuteInternal, Fail_NullHandle) {
  SQLRETURN status = SQLExecuteInternal(nullptr);
  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLExecuteInternal, Fail_InvalidHandle) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  SQLRETURN status = SQLExecuteInternal(&conn_handle);
  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLExecuteInternal, Fail_UnPreparedHandle) {
  StatementHandle stmt_handle = CreateStatementHandle();

  SQLRETURN status = SQLExecuteInternal(&stmt_handle);
  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(1, stmt_handle.GetDiagnostics().GetStatusRecords().size());
  EXPECT_EQ(SQLStates::k_HY010(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Function sequence error - statement is not prepared",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLExecuteInternal, Fail_ExecutionInProgress) {
  StatementHandle stmt_handle = CreateStatementHandle();
  stmt_handle.SetStmtState(StmtStates::kStatementStillExecuting);

  SQLRETURN status = SQLExecuteInternal(&stmt_handle);
  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(1, stmt_handle.GetDiagnostics().GetStatusRecords().size());
  EXPECT_EQ(SQLStates::k_HY010(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Function sequence error - statement is still executing",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLExecuteInternal, CancellationOfOngoingExecuteOperation) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementStillExecuting);
  handle.EnableCancellation();

  SQLRETURN status = SQLExecuteInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementPrepared);
}

TEST(SQLExecDirectInternal, Fail_InvalidHandle) {
  StatementHandle* stmt_handle = nullptr;
  std::string queryStr = "Select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = queryStr.length();

  SQLRETURN status = SQLExecDirectInternal(stmt_handle, query, len);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLExecDirectInternal, InvalidQueryLength) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementNotPrepared);
  std::string queryStr = "select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = 0;

  SQLRETURN status = SQLExecDirectInternal(&handle, query, len);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY090(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Invalid query length"));
}

TEST(SQLExecDirectInternal, NullQueryText) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementNotPrepared);

  SQLRETURN status = SQLExecDirectInternal(&handle, nullptr, SQL_NTS);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Query text is null or empty"));
}

TEST(SQLExecDirectInternal, EmptyQueryText) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementNotPrepared);
  std::string queryStr = "";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();

  SQLRETURN status = SQLExecDirectInternal(&handle, query, SQL_NTS);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Query text is null or empty"));
}

TEST(SQLExecDirectInternal, CancellationBetweenExecutions) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);
  handle.EnableCancellation();
  std::string queryStr = "Select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = queryStr.length();

  SQLRETURN status = SQLExecDirectInternal(&handle, query, len);
  ASSERT_EQ(SQL_ERROR, status);
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementExecutedWithRs);
}

TEST(SQLExecDirectInternal, PreviouslyOngoingAsyncOperation_Canceled) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementStillExecuting);
  std::future<StatusRecord> fut_query =
      std::async(std::launch::async, []() { return StatusRecord::Ok(); });
  handle.SetFutureExecDirectQuery(std::move(fut_query));

  handle.EnableCancellation();
  std::string queryStr = "Select 1";
  SQLCHAR* query = (SQLCHAR*)queryStr.c_str();
  SQLINTEGER len = queryStr.length();

  SQLRETURN status = SQLExecDirectInternal(&handle, query, len);

  ASSERT_FALSE(handle.IsOperationCanceled());
  ASSERT_EQ(handle.GetStmtState(), StmtStates::kStatementNotPrepared);

  EXPECT_EQ(SQLStates::k_HY008(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Operation canceled"));
}

TEST(SQLSetCursorNameInternal, Fail_NullHandle) {
  SQLRETURN status = SQLSetCursorNameInternal(nullptr, nullptr, 0);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLSetCursorNameInternal, Fail_InvalidName_SQLCUR) {
  StatementHandle stmt_handle = CreateStatementHandle();
  std::string cursor_name = "SQLCUR_1";

  SQLRETURN status = SQLSetCursorNameInternal(
      &stmt_handle, ToSqlChar(cursor_name.c_str()), SQL_NTS);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(1, stmt_handle.GetDiagnostics().GetStatusRecords().size());
  EXPECT_EQ(SQLStates::k_34000(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Invalid cursor name",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLSetCursorNameInternal, Fail_InvalidName_SQL_CUR) {
  StatementHandle stmt_handle = CreateStatementHandle();
  std::string cursor_name = "SQL_CUR_1";

  SQLRETURN status = SQLSetCursorNameInternal(
      &stmt_handle, ToSqlChar(cursor_name.c_str()), SQL_NTS);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(1, stmt_handle.GetDiagnostics().GetStatusRecords().size());
  EXPECT_EQ(SQLStates::k_34000(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Invalid cursor name",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLSetCursorNameInternal, Fail_InvalidLength) {
  StatementHandle stmt_handle = CreateStatementHandle();
  std::string cursor_name = "name_1";

  SQLRETURN status = SQLSetCursorNameInternal(
      &stmt_handle, ToSqlChar(cursor_name.c_str()), -2);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(1, stmt_handle.GetDiagnostics().GetStatusRecords().size());
  EXPECT_EQ(SQLStates::k_HY090(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Invalid string length",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLSetCursorNameInternal, Fail_InvalidState) {
  StatementHandle stmt_handle = CreateStatementHandle();
  stmt_handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  std::string cursor_name = "name_1";

  SQLRETURN status = SQLSetCursorNameInternal(
      &stmt_handle, ToSqlChar(cursor_name.c_str()), SQL_NTS);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(1, stmt_handle.GetDiagnostics().GetStatusRecords().size());
  EXPECT_EQ(SQLStates::k_24000(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Invalid cursor state",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLSetCursorNameInternal, SetCursorName) {
  StatementHandle stmt_handle = CreateStatementHandle();
  std::string cursor_name = "name_1";

  SQLRETURN status = SQLSetCursorNameInternal(
      &stmt_handle, ToSqlChar(cursor_name.c_str()), SQL_NTS);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(cursor_name, stmt_handle.GetCursorName());
}

TEST(SQLGetCursorNameInternal, Fail_NullHandle) {
  SQLRETURN status = SQLGetCursorNameInternal(nullptr, nullptr, 0, nullptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGSetCursorNameInternal, GetCursorName) {
  StatementHandle stmt_handle = CreateStatementHandle();
  std::string cursor_name = "name_1";
  stmt_handle.SetCursorName(cursor_name);

  SQLCHAR buf[20];
  SQLRETURN status = SQLGetCursorNameInternal(&stmt_handle, buf, 20, nullptr);

  std::string actual = reinterpret_cast<char*>(buf);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(cursor_name, actual);
}

TEST(SQLGSetCursorNameInternal, GetCursorName_Truncated) {
  StatementHandle stmt_handle = CreateStatementHandle();
  std::string cursor_name = "name_1";
  stmt_handle.SetCursorName(cursor_name);

  SQLCHAR buf[20];
  SQLRETURN status = SQLGetCursorNameInternal(&stmt_handle, buf, 5, nullptr);

  std::string actual = reinterpret_cast<char*>(buf);
  EXPECT_EQ(SQL_SUCCESS_WITH_INFO, status);
  EXPECT_EQ("name", actual);
  ASSERT_EQ(1, stmt_handle.GetDiagnostics().GetStatusRecords().size());
  EXPECT_EQ(SQLStates::k_01004(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("String data, right truncated",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLPutDataInternal, InvalidStatementState) {
  StatementHandle stmt_handle = CreateStatementHandle();
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);

  char const* test_data = "test_data";
  SQLLEN data_length = strlen(test_data);

  SQLRETURN status =
      SQLPutDataInternal(&stmt_handle, (SQLPOINTER)test_data, data_length);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(stmt_handle.GetDiagnostics().GetStatusRecords().size(), 1);
  EXPECT_EQ(SQLStates::k_HY010(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Function sequence error: Incorrect statement state.",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLPutDataInternal, NoParameterExpectingData) {
  StatementHandle stmt_handle = CreateStatementHandle();
  stmt_handle.SetStmtState(StmtStates::kNeedsPutData);

  char const* test_data = "test_data";
  SQLLEN data_length = strlen(test_data);

  // Simulate a case where no parameter is expecting data
  stmt_handle.SetCurrentParamIndex(1);

  SQLRETURN status =
      SQLPutDataInternal(&stmt_handle, (SQLPOINTER)test_data, data_length);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(stmt_handle.GetDiagnostics().GetStatusRecords().size(), 1);
  EXPECT_EQ(SQLStates::k_HY000(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("No parameter currently expecting data.",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLPutDataInternal, NoDescriptorRecordForParameter) {
  StatementHandle stmt_handle = CreateStatementHandle();
  stmt_handle.SetStmtState(StmtStates::kNeedsPutData);

  char const* test_data = "test_data";
  SQLLEN data_length = strlen(test_data);
  google::cloud::bigquery_v2_minimal_internal::QueryParameter query_parameters;
  query_parameters.parameter_value.value = "";
  // Simulate no descriptor record for the
  // current parameter
  stmt_handle.SetCurrentParamIndex(1);
  stmt_handle.SetQueryParameters({query_parameters});

  SQLRETURN status =
      SQLPutDataInternal(&stmt_handle, (SQLPOINTER)test_data, data_length);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_EQ(stmt_handle.GetDiagnostics().GetStatusRecords().size(), 1);
  EXPECT_EQ(SQLStates::k_07002(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Descriptor record does not exist for parameter.",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLParamDataInternal, Fail_InvalidStatementState) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kNeedsPutData);
  SQLPOINTER param_or_target_value = nullptr;
  SQLRETURN status = SQLParamDataInternal(&stmt_handle, &param_or_target_value);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY010(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Function sequence error: Incorrect statement state",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLParamDataInternal, Fail_ParameterOutOfBounds) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kNeedsParams);

  stmt_handle.SetCurrentParamIndex(1);
  SQLPOINTER param_or_target_value = nullptr;
  SQLRETURN status = SQLParamDataInternal(&stmt_handle, &param_or_target_value);
  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY000(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ("Parameter out of bounds",
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].message);
}

TEST(SQLParamDataInternal, Success_HandlesDataAtExec) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kNeedsParams);

  DescriptorRecord param_record;
  SQLLEN indicator_value = SQL_DATA_AT_EXEC;
  param_record.indicator_ptr = &indicator_value;
  param_record.data_ptr = nullptr;

  std::vector<google::cloud::bigquery_v2_minimal_internal::QueryParameter>
      query_parameters = {{"test_col1", {"string"}, {"test_val1"}},
                          {"test_col2", {"string"}, {"test_val2"}}};

  stmt_handle.GetDescriptorHandle(DescriptorType::kAPD)
      .BindNewDescriptorRecord(1, param_record);
  stmt_handle.SetQueryParameters(query_parameters);

  SQLPOINTER param_or_target_value = nullptr;
  SQLRETURN status = SQLParamDataInternal(&stmt_handle, &param_or_target_value);

  EXPECT_EQ(SQL_NEED_DATA, status);
  EXPECT_EQ(stmt_handle.GetStmtState(), StmtStates::kNeedsPutData);
}

TEST(SQLParamDataInternal, Success_SQL_NO_Data) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kNeedsParams);

  Job mock_job;
  mock_job.statistics.job_query_stats.statement_type = "UPDATE";
  stmt_handle.SetPreparedJob(mock_job);

  SQLPOINTER param_or_target_value = nullptr;
  SQLRETURN status = SQLParamDataInternal(&stmt_handle, &param_or_target_value);
  EXPECT_EQ(SQL_NO_DATA, status);

  mock_job.statistics.job_query_stats.statement_type = "DELETE";
  stmt_handle.SetPreparedJob(mock_job);

  status = SQLParamDataInternal(&stmt_handle, &param_or_target_value);
  EXPECT_EQ(SQL_NO_DATA, status);
}
}  // namespace google::cloud::odbc_bq_driver
