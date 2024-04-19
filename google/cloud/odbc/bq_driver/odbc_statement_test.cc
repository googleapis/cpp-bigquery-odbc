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

#include "google/cloud/odbc/bq_driver/odbc_statement.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

TEST(SQLAllocStmtHandle, AllocateStmtHandle) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  SQLPOINTER output;

  auto status = SQLAllocStmtHandle(&conn_handle, &output);

  ASSERT_EQ(SQL_SUCCESS, status);
  auto* stmt_handle = reinterpret_cast<StatementHandle*>(output);
  std::set<StatementHandle*>& stmt_handles = conn_handle.GetStatementHandles();
  EXPECT_FALSE(stmt_handles.empty());
  EXPECT_TRUE(stmt_handles.find(stmt_handle) != stmt_handles.end());
  delete stmt_handle;
}

TEST(SQLSetStmtAttrInternal, FailsToSet_SQL_ATTR_IMP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC, &output, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY017(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, FailsToSet_SQL_ATTR_IMP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_IMP_ROW_DESC, &output, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY017(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_APP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &desc, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DESC_ALLOC_USER,
            handle.GetDescriptorHandle(DescriptorType::kARD)
                .GetHeaderRecord()
                .GetAllocType());
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_PARAM_DESC, &desc, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DESC_ALLOC_USER,
            handle.GetDescriptorHandle(DescriptorType::kAPD)
                .GetHeaderRecord()
                .GetAllocType());
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLLEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_OFFSET_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                           .GetHeaderRecord()
                           .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAM_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_OFFSET_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kAPD)
                         .GetHeaderRecord()
                         .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_BIND_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  SQLINTEGER expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                          .GetHeaderRecord()
                          .bind_type);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_OPERATION_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAM_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_OPERATION_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kAPD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_STATUS_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIPD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAM_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_STATUS_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIPD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAMS_PROCESSED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAMS_PROCESSED_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIPD)
                           .GetHeaderRecord()
                           .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAMS_PROCESSED_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAMS_PROCESSED_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIPD)
                         .GetHeaderRecord()
                         .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAMSET_SIZE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAMSET_SIZE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                          .GetHeaderRecord()
                          .array_size);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_ARRAY_SIZE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_ARRAY_SIZE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                          .GetHeaderRecord()
                          .array_size);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLLEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_OFFSET_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                           .GetHeaderRecord()
                           .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROW_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_OFFSET_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kARD)
                         .GetHeaderRecord()
                         .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_BIND_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  SQLINTEGER expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                          .GetHeaderRecord()
                          .bind_type);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_OPERATION_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROW_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_OPERATION_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kARD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_STATUS_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIRD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROW_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_STATUS_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIRD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROWS_FETCHED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROWS_FETCHED_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIRD)
                           .GetHeaderRecord()
                           .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROWS_FETCHED_PTR) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROWS_FETCHED_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIRD)
                         .GetHeaderRecord()
                         .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ASYNC_ENABLE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_ASYNC_ENABLE_ON;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ASYNC_ENABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE));
}

TEST(SQLSetStmtAttrInternal, Fails_SQL_ATTR_ASYNC_ENABLE_InvalidValue) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ASYNC_ENABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY024(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CONCURRENCY) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_CONCUR_READ_ONLY;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CONCURRENCY,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CONCURRENCY));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CURSOR_SCROLLABLE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_NONSCROLLABLE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SCROLLABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_SCROLLABLE));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CURSOR_SENSITIVITY) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_INSENSITIVE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SENSITIVITY,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_SENSITIVITY));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CURSOR_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_CURSOR_FORWARD_ONLY;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_TYPE));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ENABLE_AUTO_IPD) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_FALSE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ENABLE_AUTO_IPD,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_ENABLE_AUTO_IPD));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_MAX_LENGTH) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_MAX_LENGTH,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_MAX_LENGTH));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_MAX_ROWS) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_MAX_ROWS,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_MAX_ROWS));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_METADATA_ID) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_FALSE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_METADATA_ID,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_METADATA_ID));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_NOSCAN) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_NOSCAN_ON;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_NOSCAN, (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_NOSCAN));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_QUERY_TIMEOUT) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_QUERY_TIMEOUT,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_QUERY_TIMEOUT));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_RETRIEVE_DATA) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_RD_OFF;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_RETRIEVE_DATA,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_RETRIEVE_DATA));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_USE_BOOKMARKS) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_UB_OFF;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_USE_BOOKMARKS,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_USE_BOOKMARKS));
}

TEST(SQLSetStmtAttrInternal, Fails_SQL_ATTR_ROW_NUMBER) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_NUMBER,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

}  // namespace google::cloud::odbc_bq_driver
