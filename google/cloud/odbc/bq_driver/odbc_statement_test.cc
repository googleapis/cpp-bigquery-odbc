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
using google::cloud::odbc_testing_bq_driver_utils::CreateExplicitDescriptor;
using google::cloud::odbc_testing_bq_driver_utils::
    CreatePreparedStatementHandle;
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
  DescriptorHandle& ard =
      stmt_handle->GetDescriptorHandle(DescriptorType::kARD);
  EXPECT_FALSE(ard.GetAssociatedStatementHandles().empty());
  DescriptorHandle& apd =
      stmt_handle->GetDescriptorHandle(DescriptorType::kAPD);
  EXPECT_FALSE(apd.GetAssociatedStatementHandles().empty());
  DescriptorHandle& ird =
      stmt_handle->GetDescriptorHandle(DescriptorType::kIRD);
  EXPECT_FALSE(ird.GetAssociatedStatementHandles().empty());
  DescriptorHandle& ipd =
      stmt_handle->GetDescriptorHandle(DescriptorType::kIPD);
  EXPECT_FALSE(ipd.GetAssociatedStatementHandles().empty());
  delete stmt_handle;
}

TEST(SQLSetStmtAttrInternal, Fails_InvalidHandle) {
  DescriptorHandle handle = CreateExplicitDescriptor();
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC, &output, 0);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
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

TEST(SQLSetStmtAttrInternal,
     FailsToSet_SQL_ATTR_CONCURRENCY_PreparedStatement) {
  StatementHandle handle = CreatePreparedStatementHandle();
  SQLULEN concurrency = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_CONCURRENCY, &concurrency, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY011(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_APP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle expl_desc = CreateExplicitDescriptor();
  handle.SetDescriptorHandle(DescriptorType::kARD, &expl_desc);
  EXPECT_EQ(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kARD)));

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kARD)));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle desc = CreateExplicitDescriptor();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &desc, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(desc.GetHeaderRecord().GetAllocType(),
            handle.GetDescriptorHandle(DescriptorType::kARD)
                .GetHeaderRecord()
                .GetAllocType());
}

TEST(SQLSetStmtAttrInternal, SetNull_SQL_ATTR_APP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle expl_desc = CreateExplicitDescriptor();
  handle.SetDescriptorHandle(DescriptorType::kAPD, &expl_desc);
  EXPECT_EQ(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kAPD)));

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_PARAM_DESC, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kAPD)));
}

TEST(SQLSetStmtAttrInternal, Set_SQL_ATTR_APP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle desc = CreateExplicitDescriptor();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_PARAM_DESC, &desc, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(desc.GetHeaderRecord().GetAllocType(),
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

TEST(SQLSetStmtAttrInternal, Fails_InvalidAttribute) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, 1111, (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetStmtAttrInternal, Fails_InvalidHandle) {
  DescriptorHandle handle = CreateExplicitDescriptor();
  SQLPOINTER output;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC,
                                       &output, 0, nullptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_APP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kARD)));
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_APP_ROW_DESC_Explicit) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle desc = CreateExplicitDescriptor();
  handle.SetDescriptorHandle(DescriptorType::kARD, &desc);
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &desc);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_APP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_PARAM_DESC,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kAPD)));
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_APP_PARAM_DESC_Explicit) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle desc = CreateExplicitDescriptor();
  handle.SetDescriptorHandle(DescriptorType::kAPD, &desc);
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_PARAM_DESC,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &desc);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_IMP_ROW_DESC) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_IMP_ROW_DESC, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kIRD)));
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_IMP_PARAM_DESC) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kIPD)));
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_PARAM_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
  SQLLEN data = 10;
  apd.GetHeaderRecord().bind_offset_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_OFFSET_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_PARAM_BIND_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
  apd.GetHeaderRecord().bind_type = 15;
  SQLINTEGER actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_TYPE,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_PARAM_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
  SQLUSMALLINT data = 10;
  apd.GetHeaderRecord().array_status_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAM_OPERATION_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_PARAM_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
  SQLUSMALLINT data = 10;
  ipd.GetHeaderRecord().array_status_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAM_STATUS_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_PARAMS_PROCESSED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ipd = handle.GetDescriptorHandle(DescriptorType::kIPD);
  SQLULEN data = 10;
  ipd.GetHeaderRecord().rows_processed_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAMS_PROCESSED_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_PARAMSET_SIZE) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
  apd.GetHeaderRecord().array_size = 15;
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAMSET_SIZE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ROW_ARRAY_SIZE) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
  ard.GetHeaderRecord().array_size = 15;
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_ARRAY_SIZE,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ROW_BIND_OFFSET_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
  SQLLEN data = 10;
  ard.GetHeaderRecord().bind_offset_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_OFFSET_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ROW_BIND_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
  ard.GetHeaderRecord().bind_type = 15;
  SQLINTEGER actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_TYPE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ROW_OPERATION_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
  SQLUSMALLINT data = 10;
  ard.GetHeaderRecord().array_status_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_OPERATION_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ROW_STATUS_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLUSMALLINT data = 10;
  ird.GetHeaderRecord().array_status_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_STATUS_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ROWS_FETCHED_PTR) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  SQLULEN data = 10;
  ird.GetHeaderRecord().rows_processed_ptr = &data;
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROWS_FETCHED_PTR,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&data, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ASYNC_ENABLE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ASYNC_ENABLE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_ASYNC_ENABLE_OFF, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_CONCURRENCY) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CONCURRENCY, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CONCUR_READ_ONLY, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_CURSOR_SCROLLABLE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SCROLLABLE,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_NONSCROLLABLE, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_CURSOR_SENSITIVITY) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SENSITIVITY,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_UNSPECIFIED, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_CURSOR_TYPE) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_TYPE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CURSOR_FORWARD_ONLY, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ENABLE_AUTO_IPD) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ENABLE_AUTO_IPD,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_TRUE, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_MAX_LENGTH) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetAttribute(SQL_ATTR_MAX_LENGTH, 15);
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_MAX_LENGTH, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_MAX_ROWS) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetAttribute(SQL_ATTR_MAX_ROWS, 15);
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_MAX_ROWS, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_METADATA_ID) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_METADATA_ID, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_FALSE, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_NOSCAN) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_NOSCAN, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_NOSCAN_OFF, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_QUERY_TIMEOUT) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetAttribute(SQL_ATTR_QUERY_TIMEOUT, 15);
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_QUERY_TIMEOUT, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_RETRIEVE_DATA) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_RETRIEVE_DATA, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_RD_ON, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_ROW_NUMBER) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_NUMBER, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(0, actual);
}

TEST(SQLGetStmtAttrInternal, Get_SQL_ATTR_USE_BOOKMARKS) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_USE_BOOKMARKS, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_UB_OFF, actual);
}

TEST(SQLGetStmtAttrInternal, Fails_InvalidAttribute) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, 1111, &actual, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

}  // namespace google::cloud::odbc_bq_driver
