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
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::
    CreateDescRecordWithRandomValues;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

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

  SQLRETURN status = SQLBindParameter(
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

  SQLRETURN status = SQLBindParameter(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
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

  SQLRETURN status = SQLBindParameter(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
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

  SQLRETURN status = SQLBindParameter(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ(0, stmt_handle.GetDescriptorHandle(DescriptorType::kAPD)
                   .GetHeaderRecord()
                   .count);
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

  SQLRETURN status = SQLBindParameter(
      &stmt_handle, param_number, in_out_type, value_type, param_type, col_size,
      decimal_digits, &param_val, buff_len, &str_len);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ(0, stmt_handle.GetDescriptorHandle(DescriptorType::kIPD)
                   .GetHeaderRecord()
                   .count);
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

void AssertDescribeParamResults(SQLRETURN status,
                                DescriptorRecord const& record,
                                SQLSMALLINT data_type, SQLSMALLINT nullable) {
  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(record.concise_type, data_type);
  EXPECT_EQ(record.nullable, nullable);
}

TEST(SQLDescribeParam, Fail_ParameterNumberIsZero) {
  StatementHandle stmt_handle = CreateStatementHandle();
  // TODO(b/340440354) Change stmt handle state to 'prepared'
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
  StatementHandle stmt_handle = CreateStatementHandle();
  // TODO(b/340440354) Change stmt handle state to 'prepared'
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

TEST(SQLDescribeParam, Describe_SQL_NUMERIC) {
  StatementHandle stmt_handle = CreateStatementHandle();
  // TODO(b/340440354) Change stmt handle state to 'prepared'
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

  AssertDescribeParamResults(status, record, data_type, nullable);
  EXPECT_EQ(record.precision, param_size);
  EXPECT_EQ(record.scale, decimal_digits);
}

TEST(SQLDescribeParam, Describe_SQL_CHAR) {
  StatementHandle stmt_handle = CreateStatementHandle();
  // TODO(b/340440354) Change stmt handle state to 'prepared'
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

  AssertDescribeParamResults(status, record, data_type, nullable);
  EXPECT_EQ(record.length, param_size);
  EXPECT_EQ(record.scale, decimal_digits);
}

TEST(SQLDescribeParam, Describe_SQL_DATE) {
  StatementHandle stmt_handle = CreateStatementHandle();
  // TODO(b/340440354) Change stmt handle state to 'prepared'
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

  AssertDescribeParamResults(status, record, data_type, nullable);
  EXPECT_EQ(record.length, param_size);
  EXPECT_EQ(record.precision, decimal_digits);
}

TEST(SQLNumParamsInternal, Fails_InvalidHandle) {
  SQLSMALLINT num_param = 0;

  SQLRETURN status = SQLNumParamsInternal(nullptr, &num_param);

  ASSERT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLNumParamsInternal, ReturnsParamCount) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetParamCount(2);
  SQLSMALLINT num_param = 0;

  SQLRETURN status = SQLNumParamsInternal(&handle, &num_param);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(2, num_param);
}

}  // namespace google::cloud::odbc_bq_driver
