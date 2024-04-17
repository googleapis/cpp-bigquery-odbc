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
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::HandleType;
using google::cloud::odbc_bq_driver_internal::HandleWrapped;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;
using google::cloud::odbc_testing_bq_driver_utils::DeleteStatementHandle;

TEST(SQLSetStmtAttrInternal, FailsToSet_SQL_ATTR_IMP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_IMP_PARAM_DESC, &output, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY017(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, FailsToSet_SQL_ATTR_IMP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_IMP_ROW_DESC, &output, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY017(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_ROW_DESC_Null) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_APP_ROW_DESC, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DESC_ALLOC_AUTO,
            handle.GetDescriptorHandle(DescriptorType::kARD)
                ->GetHeaderRecord()
                .GetAllocType());
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  auto* desc =
      new DescriptorHandle(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);
  auto* desc_wrapped = new HandleWrapped(HandleType::kDescriptorHandle, desc);

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_APP_ROW_DESC, desc_wrapped, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DESC_ALLOC_USER,
            handle.GetDescriptorHandle(DescriptorType::kARD)
                ->GetHeaderRecord()
                .GetAllocType());
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_PARAM_DESC_Null) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_APP_PARAM_DESC, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DESC_ALLOC_AUTO,
            handle.GetDescriptorHandle(DescriptorType::kAPD)
                ->GetHeaderRecord()
                .GetAllocType());
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  auto* desc =
      new DescriptorHandle(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);
  auto* desc_wrapped = new HandleWrapped(HandleType::kDescriptorHandle, desc);

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_APP_PARAM_DESC,
                                       desc_wrapped, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DESC_ALLOC_USER,
            handle.GetDescriptorHandle(DescriptorType::kAPD)
                ->GetHeaderRecord()
                .GetAllocType());
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLLEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAM_BIND_OFFSET_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                           ->GetHeaderRecord()
                           .bind_offset_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAM_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAM_BIND_OFFSET_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kAPD)
                         ->GetHeaderRecord()
                         .bind_offset_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_BIND_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLINTEGER expected = 10;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAM_BIND_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                          ->GetHeaderRecord()
                          .bind_type);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLUSMALLINT expected = 0;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAM_OPERATION_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                           ->GetHeaderRecord()
                           .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAM_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAM_OPERATION_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kAPD)
                         ->GetHeaderRecord()
                         .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAM_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAM_STATUS_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIPD)
                           ->GetHeaderRecord()
                           .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAM_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAM_STATUS_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIPD)
                         ->GetHeaderRecord()
                         .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAMS_PROCESSED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAMS_PROCESSED_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIPD)
                           ->GetHeaderRecord()
                           .rows_processed_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_PARAMS_PROCESSED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAMS_PROCESSED_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIPD)
                         ->GetHeaderRecord()
                         .rows_processed_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_PARAMSET_SIZE) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 10;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_PARAMSET_SIZE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                          ->GetHeaderRecord()
                          .array_size);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_ARRAY_SIZE) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 10;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_ARRAY_SIZE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                          ->GetHeaderRecord()
                          .array_size);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLLEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_BIND_OFFSET_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                           ->GetHeaderRecord()
                           .bind_offset_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROW_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_BIND_OFFSET_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kARD)
                         ->GetHeaderRecord()
                         .bind_offset_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_BIND_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLINTEGER expected = 10;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_BIND_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                          ->GetHeaderRecord()
                          .bind_type);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLUSMALLINT expected = 0;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_OPERATION_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                           ->GetHeaderRecord()
                           .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROW_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_OPERATION_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kARD)
                         ->GetHeaderRecord()
                         .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROW_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_STATUS_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIRD)
                           ->GetHeaderRecord()
                           .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROW_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_STATUS_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIRD)
                         ->GetHeaderRecord()
                         .array_status_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ROWS_FETCHED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROWS_FETCHED_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIRD)
                           ->GetHeaderRecord()
                           .rows_processed_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_ROWS_FETCHED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);

  auto status =
      SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROWS_FETCHED_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIRD)
                         ->GetHeaderRecord()
                         .rows_processed_ptr);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ASYNC_ENABLE) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_ASYNC_ENABLE_ON;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ASYNC_ENABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Fails_SQL_ATTR_ASYNC_ENABLE_InvalidValue) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ASYNC_ENABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY024(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CONCURRENCY) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_CONCUR_READ_ONLY;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_CONCURRENCY,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CONCURRENCY));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CURSOR_SCROLLABLE) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_NONSCROLLABLE;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_CURSOR_SCROLLABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_SCROLLABLE));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CURSOR_SENSITIVITY) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_INSENSITIVE;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_CURSOR_SENSITIVITY,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_SENSITIVITY));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_CURSOR_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_CURSOR_FORWARD_ONLY;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_CURSOR_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_TYPE));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_ENABLE_AUTO_IPD) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_FALSE;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ENABLE_AUTO_IPD,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_ENABLE_AUTO_IPD));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_MAX_LENGTH) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_MAX_LENGTH,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_MAX_LENGTH));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_MAX_ROWS) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_MAX_ROWS,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_MAX_ROWS));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_METADATA_ID) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_FALSE;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_METADATA_ID,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_METADATA_ID));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_NOSCAN) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_NOSCAN_ON;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_NOSCAN,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_NOSCAN));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_QUERY_TIMEOUT) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_QUERY_TIMEOUT,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_QUERY_TIMEOUT));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_RETRIEVE_DATA) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_RD_OFF;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_RETRIEVE_DATA,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_RETRIEVE_DATA));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_USE_BOOKMARKS) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = SQL_UB_OFF;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_USE_BOOKMARKS,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_USE_BOOKMARKS));
  DeleteStatementHandle(handle);
}

TEST(SQLSetStmtAttrInternal, Fails_SQL_ATTR_ROW_NUMBER) {
  StatementHandle handle = CreateStatementHandle();
  HandleWrapped wrapped(HandleType::kStatementHandle, &handle);
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&wrapped, SQL_ATTR_ROW_NUMBER,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  DeleteStatementHandle(handle);
}

}  // namespace google::cloud::odbc_bq_driver
