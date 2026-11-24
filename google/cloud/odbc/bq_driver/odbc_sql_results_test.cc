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
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::
    CreateDescRecordWithRandomValues;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateStmtHandleWithState;
using ::testing::HasSubstr;

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

TEST(SQLBindColInternal, TypeSqlCTypeDate) {
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

TEST(SQLBindColInternal, TypeSqlCIntervalMonth) {
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

TEST(SQLBindColInternal, UnBindindingBasic) {
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
TEST(SQLBindColInternal, UnBindindingComplex) {
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

TEST(SQLNumResultColsInternal, InvalidHandle) {
  StatementHandle* stmt_handle = nullptr;
  SQLSMALLINT column_count;

  SQLRETURN ret = SQLNumResultColsInternal(stmt_handle, &column_count);

  EXPECT_EQ(ret, SQL_INVALID_HANDLE);
}

TEST(SQLNumResultColsInternal, NullColumnCountPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLRETURN ret = SQLNumResultColsInternal(&handle, nullptr);

  ASSERT_EQ(ret, SQL_ERROR);
  EXPECT_EQ(SQLStates::k_HY001(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLNumResultColsInternal, StateCheck) {
  StatementHandle handle = CreateStatementHandle();

  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);

  SQLSMALLINT column_count;
  SQLRETURN ret = SQLNumResultColsInternal(&handle, &column_count);

  EXPECT_EQ(ret, SQL_SUCCESS);
  EXPECT_EQ(column_count, 0);
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
  EXPECT_EQ(record.name, std::string(reinterpret_cast<char*>(column_name)));
  EXPECT_EQ(record.name.size(), column_name_Le);
}

TEST(SQLDescribeColumn, FailInvalidhandle) {
  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_le = 0;

  SQLRETURN status = SQLDescribeColInternal(
      nullptr, 1, column_name, 20, &column_name_le, &data_type, &column_size,
      &decimal_digits, &nullable);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLDescribeColumn, FailColumnnumberiszero) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_le = 0;

  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, 0, column_name, 20, &column_name_le, &data_type,
      &column_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07006(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeColumn, FailInvalidcolumnnumber) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_le = 0;

  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, 10, column_name, 20, &column_name_le, &data_type,
      &column_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeColumn, FailStatementisnotprepared) {
  StatementHandle stmt_handle = CreateStatementHandle();
  DescriptorRecord record;
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 20, &column_name_le, &data_type,
      &column_size, &decimal_digits, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY010(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeColumn, DescribeSqlNumeric) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_NUMERIC);
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 20, &column_name_le, &data_type,
      &column_size, &decimal_digits, &nullable);

  AssertDescribeColumnResults(status, record, column_name, column_name_le,
                              data_type, column_size, decimal_digits, nullable);
}

TEST(SQLDescribeColumn, DescribeSqlChar) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_CHAR);
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 20, &column_name_le, &data_type,
      &column_size, &decimal_digits, &nullable);

  AssertDescribeColumnResults(status, record, column_name, column_name_le,
                              data_type, column_size, decimal_digits, nullable);
}

TEST(SQLDescribeColumn, DescribeSqlDate) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_TYPE_DATE);
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT column_number = 1;
  ird.BindNewDescriptorRecord(column_number, record);

  SQLSMALLINT data_type = 0;
  SQLULEN column_size = 0;
  SQLSMALLINT decimal_digits = 0;
  SQLSMALLINT nullable = 0;
  SQLCHAR column_name[15];
  SQLSMALLINT column_name_le = 0;
  SQLRETURN status = SQLDescribeColInternal(
      &stmt_handle, column_number, column_name, 20, &column_name_le, &data_type,
      &column_size, &decimal_digits, &nullable);

  AssertDescribeColumnResults(status, record, column_name, column_name_le,
                              data_type, column_size, decimal_digits, nullable);
}

TEST(SQLColAttributeInternal, FailInvalidhandle) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);

  SQLRETURN status =
      SQLColAttributeInternal(&conn_handle, 0, 0, nullptr, 0, nullptr, nullptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLColAttributeInternal, FailInvalidfieldidentifier) {
  StatementHandle stmt_handle = CreateStatementHandle();

  SQLRETURN status = SQLColAttributeInternal(&stmt_handle, 1, 111, nullptr, 0,
                                             nullptr, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_FALSE(stmt_handle.GetDiagnostics().GetStatusRecords().empty());
  EXPECT_EQ(SQLStates::k_HY091(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLCloseCursorInternal, FailInvalidhandle) {
  SQLRETURN status = SQLCloseCursorInternal(nullptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLCloseCursorInternal, FailCursorisnotopen) {
  StatementHandle stmt_handle = CreateStatementHandle();

  SQLRETURN status = SQLCloseCursorInternal(&stmt_handle);

  EXPECT_EQ(SQL_ERROR, status);
  ASSERT_FALSE(stmt_handle.GetDiagnostics().GetStatusRecords().empty());
  EXPECT_EQ(SQLStates::k_24000(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLCloseCursorInternal, CloseCursorAftersqlexecute) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);
  stmt_handle.SetStatementPrepared();

  SQLRETURN status = SQLCloseCursorInternal(&stmt_handle);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(StmtStates::kStatementPrepared, stmt_handle.GetStmtState());
  EXPECT_FALSE(stmt_handle.IsCursorOpen());
}

TEST(SQLCloseCursorInternal, CloseCursorAftersqlexecdirect) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);

  SQLRETURN status = SQLCloseCursorInternal(&stmt_handle);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(StmtStates::kStatementNotPrepared, stmt_handle.GetStmtState());
  EXPECT_FALSE(stmt_handle.IsCursorOpen());
}

TEST(SQLRowCountInternal, NullStatementHandle) {
  SQLLEN row_count = 0;
  SQLRETURN result = SQLRowCountInternal(nullptr, &row_count);

  EXPECT_EQ(result, SQL_INVALID_HANDLE);
}

TEST(SQLRowCountInternal, NullRowCountPointer) {
  StatementHandle handle = CreateStatementHandle();

  SQLRETURN result = SQLRowCountInternal(&handle, nullptr);
  ASSERT_EQ(result, SQL_ERROR);
  EXPECT_EQ(SQLStates::k_HY001(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLRowCountInternal, WrongState) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementNotPrepared);
  SQLLEN* row_count = nullptr;
  SQLRETURN ret = SQLRowCountInternal(&handle, row_count);

  ASSERT_EQ(ret, SQL_ERROR);
  EXPECT_EQ(SQLStates::k_HY001(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetData, InvalidColumnBookmark) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementPrepared);
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLGetDataInternal(&handle, 0, SQL_CHAR, buf, 1024, &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Invalid descriptor index"));
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetData, FailNullTargetvalue) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementPrepared);
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLGetDataInternal(&handle, 1, SQL_CHAR, nullptr, 1024, &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Invalid use of null pointer"));
  EXPECT_EQ(SQLStates::k_HY009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetData, InvalidBufferLength) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementPrepared);
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLGetDataInternal(&handle, 1, SQL_CHAR, buf, -20, &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Invalid string or buffer length"));
  EXPECT_EQ(SQLStates::k_HY090(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetData, InvalidColumnNumber) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementPrepared);
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLGetDataInternal(&handle, 10, SQL_CHAR, buf, 1024, &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Invalid Column In Result Set"));
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetData, InvalidTargetType) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementPrepared);
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLGetDataInternal(&handle, 1, SQL_C_DATE, buf, 1024, &target_str_len);
  ASSERT_EQ(SQL_ERROR, status);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("Program type out of range"));
  EXPECT_EQ(SQLStates::k_HY003(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLDescribeColumn, NullDecimalDigitsPtrDoesNotCrash) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_VARCHAR);
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  ird.BindNewDescriptorRecord(1, record);

  SQLSMALLINT name_len = 0;
  SQLRETURN status =
      SQLDescribeColInternal(&stmt_handle, 1, nullptr, 0, &name_len, nullptr,
                             nullptr, nullptr, nullptr);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(static_cast<SQLSMALLINT>(record.name.size()), name_len);
}

TEST(SQLDescribeColW, LargeNameNullProbeAndTruncation) {
  StatementHandle stmt_handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  DescriptorRecord record = CreateDescRecordWithRandomValues(SQL_VARCHAR);
  record.name = std::string(5000, 'C');
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  ird.BindNewDescriptorRecord(1, record);

  // 1. NULL output-buffer length probe (and NULL metadata pointers)
  SQLSMALLINT name_len = 0;
  ASSERT_EQ(SQL_SUCCESS,
            ::SQLDescribeColW(&stmt_handle, 1, nullptr, 0, &name_len, nullptr,
                              nullptr, nullptr, nullptr));
  EXPECT_EQ(5000, name_len);

  // 2. >4096-byte column name without overflowing stack staging
  std::vector<SQLWCHAR> large_buf(8192, static_cast<SQLWCHAR>(0x7F));
  ASSERT_EQ(SQL_SUCCESS,
            ::SQLDescribeColW(&stmt_handle, 1, large_buf.data(),
                              static_cast<SQLSMALLINT>(large_buf.size()),
                              &name_len, nullptr, nullptr, nullptr, nullptr));
  EXPECT_EQ(5000, name_len);
  EXPECT_EQ(std::wstring(5000, L'C'),
            google::cloud::odbc_bq_driver_internal::SQLWcharToWstring(
                large_buf.data()));
  EXPECT_EQ(static_cast<SQLWCHAR>(0), large_buf[5000]);

  // 3. Truncation NUL-terminates, reports untruncated wide length, and logs
  // 01004
  std::vector<SQLWCHAR> small_buf(6, static_cast<SQLWCHAR>(0x7F));
  ASSERT_EQ(SQL_SUCCESS_WITH_INFO,
            ::SQLDescribeColW(&stmt_handle, 1, small_buf.data(), 5, &name_len,
                              nullptr, nullptr, nullptr, nullptr));
  EXPECT_EQ(5000, name_len);
  EXPECT_EQ(std::wstring(4, L'C'),
            google::cloud::odbc_bq_driver_internal::SQLWcharToWstring(
                small_buf.data()));
  EXPECT_EQ(static_cast<SQLWCHAR>(0), small_buf[4]);
  EXPECT_EQ(static_cast<SQLWCHAR>(0x7F), small_buf[5]);
  ASSERT_FALSE(stmt_handle.GetDiagnostics().GetStatusRecords().empty());
  EXPECT_EQ(SQLStates::k_01004(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);

  // 4. Negative BufferLength returns HY090
  EXPECT_EQ(SQL_ERROR,
            ::SQLDescribeColW(&stmt_handle, 1, small_buf.data(), -1, &name_len,
                              nullptr, nullptr, nullptr, nullptr));
  ASSERT_FALSE(stmt_handle.GetDiagnostics().GetStatusRecords().empty());
  EXPECT_EQ(SQLStates::k_HY090(),
            stmt_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLNativeSqlW, LargeSqlTruncationAndNullGuards) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);

  std::wstring long_wsql = L"SELECT '" + std::wstring(5000, L'A') + L"'";
  std::vector<SQLWCHAR> in_wbuf(long_wsql.size() + 1, 0);
  google::cloud::odbc_bq_driver_internal::WriteWideToWireBuffer(
      long_wsql, in_wbuf.data(), long_wsql.size(), /*null_terminate=*/true);

  // 1. >4096-byte SQL succeeds without stack overflow
  std::vector<SQLWCHAR> out_wbuf(8192, static_cast<SQLWCHAR>(0x5A));
  SQLINTEGER out_len = 0;
  ASSERT_EQ(
      SQL_SUCCESS,
      ::SQLNativeSqlW(&conn_handle, in_wbuf.data(), SQL_NTS, out_wbuf.data(),
                      static_cast<SQLINTEGER>(out_wbuf.size()), &out_len));
  EXPECT_EQ(static_cast<SQLINTEGER>(long_wsql.size()), out_len);
  EXPECT_EQ(long_wsql,
            google::cloud::odbc_bq_driver_internal::SQLWcharToWstring(
                out_wbuf.data()));
  EXPECT_EQ(static_cast<SQLWCHAR>(0), out_wbuf[long_wsql.size()]);

  // 2. Truncation writes NUL-terminated prefix and returns 01004
  std::vector<SQLWCHAR> trunc_buf(11, static_cast<SQLWCHAR>(0x5A));
  out_len = 0;
  ASSERT_EQ(SQL_SUCCESS_WITH_INFO,
            ::SQLNativeSqlW(&conn_handle, in_wbuf.data(), SQL_NTS,
                            trunc_buf.data(), 10, &out_len));
  EXPECT_EQ(static_cast<SQLINTEGER>(long_wsql.size()), out_len);
  EXPECT_EQ(long_wsql.substr(0, 9),
            google::cloud::odbc_bq_driver_internal::SQLWcharToWstring(
                trunc_buf.data()));
  EXPECT_EQ(static_cast<SQLWCHAR>(0), trunc_buf[9]);
  EXPECT_EQ(static_cast<SQLWCHAR>(0x5A), trunc_buf[10]);
  ASSERT_FALSE(conn_handle.GetDiagnostics().GetStatusRecords().empty());
  EXPECT_EQ(SQLStates::k_01004(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);

  // 3. NULL output buffer with positive BufferLength probes length with
  // SQL_SUCCESS
  out_len = 0;
  EXPECT_EQ(SQL_SUCCESS, ::SQLNativeSqlW(&conn_handle, in_wbuf.data(), SQL_NTS,
                                         nullptr, 64, &out_len));
  EXPECT_EQ(static_cast<SQLINTEGER>(long_wsql.size()), out_len);

  // 4. BufferLength == 0 returns 01004 without touching output buffer
  SQLWCHAR canary = static_cast<SQLWCHAR>(0x33);
  out_len = 0;
  EXPECT_EQ(SQL_SUCCESS_WITH_INFO,
            ::SQLNativeSqlW(&conn_handle, in_wbuf.data(), SQL_NTS, &canary, 0,
                            &out_len));
  EXPECT_EQ(static_cast<SQLWCHAR>(0x33), canary);
  EXPECT_EQ(static_cast<SQLINTEGER>(long_wsql.size()), out_len);

  // 5. Negative lengths return HY090
  EXPECT_EQ(SQL_ERROR, ::SQLNativeSqlW(&conn_handle, in_wbuf.data(), SQL_NTS,
                                       out_wbuf.data(), -1, &out_len));
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_EQ(SQL_ERROR, ::SQLNativeSqlW(&conn_handle, in_wbuf.data(), -5,
                                       out_wbuf.data(), 16, &out_len));
  EXPECT_EQ(SQLStates::k_HY090(),
            conn_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}
}  // namespace google::cloud::odbc_bq_driver
