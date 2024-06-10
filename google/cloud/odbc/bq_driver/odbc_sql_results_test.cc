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
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/odbc_diagnostics.h"
#include "google/cloud/odbc/bq_driver/odbc_statement.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::
    CreateDescRecordWithRandomValues;
using google::cloud::odbc_testing_bq_driver_utils::
    CreatePreparedStatementHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

inline SQLUSMALLINT GetDescCount(SQLPOINTER ard) {
  SQLUSMALLINT out_desc_count;
  SQLRETURN status = SQLGetDescFieldInternal(ard, 0, SQL_DESC_COUNT,
                                             &out_desc_count, 0, nullptr);
  EXPECT_EQ(SQL_SUCCESS, status);
  return out_desc_count;
}

TEST(SQLBindColInternal, Basic) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLBindColInternal(&handle, 1, SQL_C_FLOAT, buf, 20, &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATA_PTR, &out_buf, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(buf, out_buf);

  SQLSMALLINT out_c_type;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_TYPE, &out_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_C_FLOAT, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_CONCISE_TYPE,
                                   &out_concise_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_C_FLOAT, out_concise_c_type);

  SQLLEN out_octet_length;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH,
                                   &out_octet_length, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(20, out_octet_length);

  SQLPOINTER out_desc_ind_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_desc_ind_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_desc_ind_ptr);

  SQLPOINTER out_octet_length_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH_PTR,
                                   &out_octet_length_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_octet_length_ptr);
}

TEST(SQLBindColInternal, Type_SQL_C_TYPE_DATE) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLBindColInternal(&handle, 1, SQL_C_TYPE_DATE, buf, 20, &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATA_PTR, &out_buf, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(buf, out_buf);

  SQLSMALLINT out_c_type;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_TYPE, &out_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DATETIME, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_CONCISE_TYPE,
                                   &out_concise_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_C_TYPE_DATE, out_concise_c_type);

  SQLSMALLINT out_datetime_interval_code;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
                                   &out_datetime_interval_code, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CODE_DATE, out_datetime_interval_code);

  SQLLEN out_octet_length;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH,
                                   &out_octet_length, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(20, out_octet_length);

  SQLPOINTER out_desc_ind_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_desc_ind_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_desc_ind_ptr);

  SQLPOINTER out_octet_length_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH_PTR,
                                   &out_octet_length_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_octet_length_ptr);
}

TEST(SQLBindColInternal, Type_SQL_C_INTERVAL_MONTH) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status = SQLBindColInternal(&handle, 1, SQL_C_INTERVAL_MONTH, buf,
                                        20, &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATA_PTR, &out_buf, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(buf, out_buf);

  SQLSMALLINT out_c_type;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_TYPE, &out_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_INTERVAL, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_CONCISE_TYPE,
                                   &out_concise_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_C_INTERVAL_MONTH, out_concise_c_type);

  SQLSMALLINT out_datetime_interval_code;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
                                   &out_datetime_interval_code, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CODE_MONTH, out_datetime_interval_code);

  SQLLEN out_octet_length;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH,
                                   &out_octet_length, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(20, out_octet_length);

  SQLPOINTER out_desc_ind_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_desc_ind_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_desc_ind_ptr);

  SQLPOINTER out_octet_length_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH_PTR,
                                   &out_octet_length_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_octet_length_ptr);
}

TEST(SQLBindColInternal, UnBindinding_Basic) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;

  // Binding a column
  SQLRETURN status = SQLBindColInternal(&handle, 1, SQL_C_INTERVAL_MONTH, buf,
                                        20, &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  EXPECT_EQ(1, GetDescCount(ard));

  // Unbinding a column
  status = SQLBindColInternal(&handle, 1, SQL_C_INTERVAL_MONTH, nullptr, 20,
                              &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(0, GetDescCount(ard));
}

// This test binds multiple columns, unbinds the highest col number,
// and verifies if SQL_DESC_COUNT is correct
TEST(SQLBindColInternal, UnBindinding_Complex) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status;

  // Binding 2 columns
  status = SQLBindColInternal(&handle, 5, SQL_C_INTERVAL_MONTH, buf, 20,
                              &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);
  status = SQLBindColInternal(&handle, 2, SQL_C_INTERVAL_MONTH, buf, 20,
                              &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  EXPECT_EQ(5, GetDescCount(ard));

  // Unbinding the column with highest index
  status = SQLBindColInternal(&handle, 5, SQL_C_INTERVAL_MONTH, nullptr, 20,
                              &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(2, GetDescCount(ard));
}

TEST(SQLBindColInternal, InvalidColNumber) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status = SQLBindColInternal(&handle, 0, SQL_C_INTERVAL_MONTH,
                                        nullptr, 20, &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLBindColInternal, InvalidBufLen) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status = SQLBindColInternal(&handle, 0, SQL_C_INTERVAL_MONTH, buf,
                                        -1, &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLBindColInternal, InvalidCType) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status = SQLBindColInternal(&handle, 2, SQL_UNKNOWN_TYPE, buf, 20,
                                        &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  // If SQLBindColInternal has failed, SQL_DESC_COUNT should remain unchanged
  EXPECT_EQ(0, GetDescCount(ard));
}
void AssertDescribeColumnResults(
    SQLRETURN status, DescriptorRecord const& record, SQLCHAR column_name[15],
    SQLSMALLINT column_name_Le, SQLSMALLINT data_type, SQLULEN column_size,
    SQLSMALLINT decimal_digits, SQLSMALLINT nullable) {
  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(record.concise_type, data_type);
  switch (data_type) {
    case SQL_NUMERIC:
    case SQL_DECIMAL:
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:
    case SQL_BIGINT:
      EXPECT_EQ(record.precision, column_size);
      break;
    default:
      EXPECT_EQ(record.length, column_size);
  }
  switch (data_type) {
    case SQL_TYPE_DATE:
    case SQL_TYPE_TIME:
    case SQL_TYPE_TIMESTAMP:
    case SQL_CODE_SECOND:
    case SQL_CODE_DAY_TO_SECOND:
    case SQL_CODE_HOUR_TO_SECOND:
    case SQL_CODE_MINUTE_TO_SECOND:
      EXPECT_EQ(record.precision, decimal_digits);
      break;
    default:
      EXPECT_EQ(record.scale, decimal_digits);
  }
  EXPECT_EQ(record.nullable, nullable);
  EXPECT_EQ(record.name, std::string(reinterpret_cast<char*>(column_name)));
  EXPECT_EQ(record.name.size(), column_name_Le);
}

TEST(SQLDescribeColumn, Fail_InvalidHandle) {
  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR* column_name = nullptr;
  SQLSMALLINT column_name_Le = 0;

  SQLRETURN status = SQLDescribeColInternal(
      nullptr, 1, column_name, 1, &column_name_Le, &data_type, &column_size,
      &decimal_digits, &nullable);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLDescribeColumn, Fail_ColumnNumberIsZero) {
  StatementHandle stmt_handle = CreatePreparedStatementHandle();
  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR* column_name = nullptr;
  SQLSMALLINT column_name_Le = 0;

  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, 0, column_name, 1, &column_name_Le, &data_type,
      &column_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07006(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeColumn, Fail_InvalidColumnNumber) {
  StatementHandle stmt_handle = CreatePreparedStatementHandle();
  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR* column_name = nullptr;
  SQLSMALLINT column_name_Le = 0;

  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, 10, column_name, 1, &column_name_Le, &data_type,
      &column_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeColumn, Fail_StatementIsNotPrepared) {
  StatementHandle stmt_handle = CreateStatementHandle();
  DescriptorRecord record;
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR* column_name = nullptr;
  SQLSMALLINT column_name_Le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 1, &column_name_Le, &data_type,
      &column_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY010(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeColumn, Describe_SQL_NUMERIC) {
  StatementHandle stmt_handle = CreatePreparedStatementHandle();
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_NUMERIC);
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_Le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 1, &column_name_Le, &data_type,
      &column_size, &decimal_digits, &nullable);

  AssertDescribeColumnResults(status, record, column_name, column_name_Le,
                              data_type, column_size, decimal_digits, nullable);
}

TEST(SQLDescribeColumn, Describe_SQL_CHAR) {
  StatementHandle stmt_handle = CreatePreparedStatementHandle();
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_CHAR);
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_Le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 1, &column_name_Le, &data_type,
      &column_size, &decimal_digits, &nullable);

  AssertDescribeColumnResults(status, record, column_name, column_name_Le,
                              data_type, column_size, decimal_digits, nullable);
}

TEST(SQLDescribeColumn, Describe_SQL_DATE) {
  StatementHandle stmt_handle = CreatePreparedStatementHandle();
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_TYPE_DATE);
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_Le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 1, &column_name_Le, &data_type,
      &column_size, &decimal_digits, &nullable);

  AssertDescribeColumnResults(status, record, column_name, column_name_Le,
                              data_type, column_size, decimal_digits, nullable);
}
}  // namespace google::cloud::odbc_bq_driver
