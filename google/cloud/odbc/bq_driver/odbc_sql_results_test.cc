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

using google::cloud::odbc_bq_driver_internal::ArithmeticToDSValue;
using google::cloud::odbc_bq_driver_internal::BQDataType;
using google::cloud::odbc_bq_driver_internal::ColumnSchema;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSRow;
using google::cloud::odbc_bq_driver_internal::DSValue;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::RowSchema;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::StringToDSValue;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
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

TEST(SQLFetchInternal, Fail_InvalidHandle) {
  ConnectionHandle handle;
  SQLRETURN status = SQLFetchInternal(&handle);
  ASSERT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLFetchInternal, Fail_UnprepraredHandle) {
  StatementHandle handle = CreateStatementHandle();
  SQLRETURN status = SQLFetchInternal(&handle);
  ASSERT_EQ(SQL_ERROR, status);
}

TEST(SQLFetchInternal, Fail_EmptyResultSet) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  SQLRETURN status = SQLFetchInternal(&handle);
  ASSERT_EQ(SQL_NO_DATA, status);
}

void AddGenericResultSet(StatementHandle& handle, SQLBIGINT int_value,
                         std::string str_value, SQLDOUBLE double_value) {
  DSRow dsrow;
  DSValue ds_value;
  ArithmeticToDSValue<SQLBIGINT>(int_value, ds_value);
  dsrow.push_back(ds_value);
  StringToDSValue(str_value, ds_value);
  dsrow.push_back(ds_value);
  ArithmeticToDSValue<SQLDOUBLE>(double_value, ds_value);
  dsrow.push_back(ds_value);

  ResultSet& result_set = handle.GetResultSet();
  result_set.row_schema = {{0, BQDataType::kInt64},
                           {1, BQDataType::kString},
                           {2, BQDataType::kFloat64}};
  result_set.rows.emplace_back(dsrow);
  handle.SetResultSet(result_set);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
}

TEST(SQLFetchInternal, Basic) {
  StatementHandle handle = CreateStatementHandle();
  SQLBIGINT int_value = 101;
  std::string str_value = "Hello";
  SQLDOUBLE double_value = 3.14;
  AddGenericResultSet(handle, int_value, str_value, double_value);

  SQLBIGINT buf1;
  SQLRETURN status = SQLBindColInternal(&handle, 1, SQL_C_SLONG, &buf1,
                                        sizeof(SQL_C_SLONG), nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLCHAR buf2[20];
  status = SQLBindColInternal(&handle, 2, SQL_C_CHAR, buf2, 20, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLDOUBLE buf3;
  status = SQLBindColInternal(&handle, 3, SQL_C_DOUBLE, &buf3,
                              sizeof(SQL_C_DOUBLE), nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  status = SQLFetchInternal(&handle);
  ASSERT_EQ(status, SQL_SUCCESS);

  EXPECT_EQ(buf1, int_value);
  std::string buf_str((char*)buf2);
  EXPECT_EQ(buf_str, str_value);
  EXPECT_EQ(buf3, double_value);

  status = SQLFetchInternal(&handle);
  ASSERT_EQ(status, SQL_NO_DATA);
}

}  // namespace google::cloud::odbc_bq_driver
