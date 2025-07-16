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
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gmock/gmock.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSRow;
using google::cloud::odbc_bq_driver_internal::kNullValue;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateExplicitDescriptor;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateStmtHandleWithState;
using ::testing::HasSubstr;
using ::testing::StartsWith;

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
  EXPECT_THAT(stmt_handle->GetCursorName(), StartsWith("SQL_CUR"));
  delete stmt_handle;
}

TEST(SQLSetStmtAttrInternal, FailsInvalidhandle) {
  DescriptorHandle handle = CreateExplicitDescriptor();
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC, &output, 0);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLSetStmtAttrInternal, FailsToSetSqlAttrImpParamDesc) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC, &output, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY017(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, FailsToSetSqlAttrImpRowDesc) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER output;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_IMP_ROW_DESC, &output, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY017(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, FailsToSetSqlAttrConcurrencyPreparedstatement) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementPrepared);
  SQLULEN concurrency = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_CONCURRENCY, &concurrency, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY011(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrAppRowDesc) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle expl_desc = CreateExplicitDescriptor();
  handle.SetDescriptorHandle(DescriptorType::kARD, &expl_desc);
  EXPECT_EQ(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kARD)));

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kARD)));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrAppRowDesc) {
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

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrAppParamDesc) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle expl_desc = CreateExplicitDescriptor();
  handle.SetDescriptorHandle(DescriptorType::kAPD, &expl_desc);
  EXPECT_EQ(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kAPD)));

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_APP_PARAM_DESC, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(&expl_desc, &(handle.GetDescriptorHandle(DescriptorType::kAPD)));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrAppParamDesc) {
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

TEST(SQLSetStmtAttrInternal, SetSqlAttrParamBindOffsetPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLLEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_OFFSET_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                           .GetHeaderRecord()
                           .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrParamBindOffsetPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_OFFSET_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kAPD)
                         .GetHeaderRecord()
                         .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrParamBindType) {
  StatementHandle handle = CreateStatementHandle();
  SQLINTEGER expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                          .GetHeaderRecord()
                          .bind_type);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrParamOperationPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_OPERATION_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrParamOperationPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_OPERATION_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kAPD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrParamStatusPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_STATUS_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIPD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrParamStatusPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAM_STATUS_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIPD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrParamsProcessedPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAMS_PROCESSED_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIPD)
                           .GetHeaderRecord()
                           .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrParamsProcessedPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAMS_PROCESSED_PTR,
                                       nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIPD)
                         .GetHeaderRecord()
                         .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrParamsetSize) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_PARAMSET_SIZE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kAPD)
                          .GetHeaderRecord()
                          .array_size);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrRowArraySize) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_ARRAY_SIZE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                          .GetHeaderRecord()
                          .array_size);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrRowBindOffsetPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLLEN expected = 0;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_OFFSET_PTR,
                                       &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                           .GetHeaderRecord()
                           .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrRowBindOffsetPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_OFFSET_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kARD)
                         .GetHeaderRecord()
                         .bind_offset_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrRowBindType) {
  StatementHandle handle = CreateStatementHandle();
  SQLINTEGER expected = 10;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                          .GetHeaderRecord()
                          .bind_type);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrRowOperationPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_OPERATION_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kARD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrRowOperationPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_OPERATION_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kARD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrRowStatusPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLUSMALLINT expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_STATUS_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIRD)
                           .GetHeaderRecord()
                           .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrRowStatusPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_STATUS_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIRD)
                         .GetHeaderRecord()
                         .array_status_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrRowsFetchedPtr) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 0;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROWS_FETCHED_PTR, &expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&expected, handle.GetDescriptorHandle(DescriptorType::kIRD)
                           .GetHeaderRecord()
                           .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, SetNullSqlAttrRowsFetchedPtr) {
  StatementHandle handle = CreateStatementHandle();

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROWS_FETCHED_PTR, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, handle.GetDescriptorHandle(DescriptorType::kIRD)
                         .GetHeaderRecord()
                         .rows_processed_ptr);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrAsyncEnable) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_ASYNC_ENABLE_ON;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ASYNC_ENABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE));
}

TEST(SQLSetStmtAttrInternal, FailsSqlAttrAsyncEnableInvalidvalue) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ASYNC_ENABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY024(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrConcurrency) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_CONCUR_READ_ONLY;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CONCURRENCY,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CONCURRENCY));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrCursorScrollable) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_NONSCROLLABLE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SCROLLABLE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_SCROLLABLE));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrCursorSensitivity) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_INSENSITIVE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SENSITIVITY,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_SENSITIVITY));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrCursorType) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_CURSOR_FORWARD_ONLY;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_TYPE,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_CURSOR_TYPE));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrEnableAutoIpd) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_FALSE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ENABLE_AUTO_IPD,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_ENABLE_AUTO_IPD));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrMaxLength) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_MAX_LENGTH,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_MAX_LENGTH));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrMaxRows) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_MAX_ROWS,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_MAX_ROWS));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrMetadataId) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_FALSE;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_METADATA_ID,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_METADATA_ID));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrNoscan) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_NOSCAN_ON;

  auto status =
      SQLSetStmtAttrInternal(&handle, SQL_ATTR_NOSCAN, (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_NOSCAN));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrQueryTimeout) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_QUERY_TIMEOUT,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_QUERY_TIMEOUT));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrRetrieveData) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_RD_OFF;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_RETRIEVE_DATA,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_RETRIEVE_DATA));
}

TEST(SQLSetStmtAttrInternal, SetSqlAttrUseBookmarks) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = SQL_UB_OFF;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_USE_BOOKMARKS,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(expected, *handle.GetAttribute(SQL_ATTR_USE_BOOKMARKS));
}

TEST(SQLSetStmtAttrInternal, FailsSqlAttrRowNumber) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_ROW_NUMBER,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, FailsInvalidattribute) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN expected = 111;

  auto status = SQLSetStmtAttrInternal(&handle, 1111, (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetStmtAttrInternal, FailsOpencursor) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);

  SQLULEN expected = SQL_CONCUR_READ_ONLY;

  auto status = SQLSetStmtAttrInternal(&handle, SQL_ATTR_CONCURRENCY,
                                       (SQLPOINTER)expected, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_24000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetStmtAttrInternal, FailsInvalidhandle) {
  DescriptorHandle handle = CreateExplicitDescriptor();
  SQLPOINTER output;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC,
                                       &output, 0, nullptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrAppRowDesc) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kARD)));
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrAppRowDescExplicit) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrAppParamDesc) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_PARAM_DESC,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kAPD)));
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrAppParamDescExplicit) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrImpRowDesc) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_IMP_ROW_DESC, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kIRD)));
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrImpParamDesc) {
  StatementHandle handle = CreateStatementHandle();
  SQLPOINTER actual = nullptr;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_IMP_PARAM_DESC,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_NE(actual, nullptr);
  EXPECT_EQ(actual, &(handle.GetDescriptorHandle(DescriptorType::kIPD)));
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrParamBindOffsetPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrParamBindType) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
  apd.GetHeaderRecord().bind_type = 15;
  SQLINTEGER actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAM_BIND_TYPE,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrParamOperationPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrParamStatusPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrParamsProcessedPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrParamsetSize) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
  apd.GetHeaderRecord().array_size = 15;
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_PARAMSET_SIZE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrRowArraySize) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
  ard.GetHeaderRecord().array_size = 15;
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_ARRAY_SIZE,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrRowBindOffsetPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrRowBindType) {
  StatementHandle handle = CreateStatementHandle();
  DescriptorHandle& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
  ard.GetHeaderRecord().bind_type = 15;
  SQLINTEGER actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_BIND_TYPE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrRowOperationPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrRowStatusPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrRowsFetchedPtr) {
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

TEST(SQLGetStmtAttrInternal, GetSqlAttrAsyncEnable) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ASYNC_ENABLE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_ASYNC_ENABLE_OFF, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrConcurrency) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CONCURRENCY, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CONCUR_READ_ONLY, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrCursorScrollable) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SCROLLABLE,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_NONSCROLLABLE, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrCursorSensitivity) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_SENSITIVITY,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_UNSPECIFIED, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrCursorType) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_CURSOR_TYPE, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CURSOR_FORWARD_ONLY, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrEnableAutoIpd) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_ENABLE_AUTO_IPD,
                                       &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_TRUE, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrMaxLength) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetAttribute(SQL_ATTR_MAX_LENGTH, 15);
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_MAX_LENGTH, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrMaxRows) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetAttribute(SQL_ATTR_MAX_ROWS, 15);
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_MAX_ROWS, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrMetadataId) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_METADATA_ID, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_FALSE, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrNoscan) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_NOSCAN, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_NOSCAN_OFF, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrQueryTimeout) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetAttribute(SQL_ATTR_QUERY_TIMEOUT, 15);
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_QUERY_TIMEOUT, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrRetrieveData) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_RETRIEVE_DATA, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_RD_ON, actual);
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrRowNumber) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);
  ResultSet result_set;
  DSRow row = {kNullValue};
  result_set.rows.push_back(row);
  handle.SetResultSet(result_set);
  handle.GetResultSet().cursor++;
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_NUMBER, &actual, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(1, actual);
}

TEST(SQLGetStmtAttrInternal, FailsSqlAttrRowNumberCursorisnotopen) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_NUMBER, &actual, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_24000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("cursor is not open"));
}

TEST(SQLGetStmtAttrInternal, FailsSqlAttrRowNumberCursorisbeforestart) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_NUMBER, &actual, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_24000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("cursor is positioned before the start"));
}

TEST(SQLGetStmtAttrInternal, FailsSqlAttrRowNumberCursorisafterend) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);
  handle.GetResultSet().cursor++;
  SQLULEN actual = 0;

  auto status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_ROW_NUMBER, &actual, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_24000(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
  EXPECT_THAT(handle.GetDiagnostics().GetStatusRecords()[0].message,
              HasSubstr("cursor is positioned after the end"));
}

TEST(SQLGetStmtAttrInternal, GetSqlAttrUseBookmarks) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, SQL_ATTR_USE_BOOKMARKS, &actual,
                                       0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_UB_OFF, actual);
}

TEST(SQLGetStmtAttrInternal, FailsInvalidattribute) {
  StatementHandle handle = CreateStatementHandle();
  SQLULEN actual = 0;

  auto status = SQLGetStmtAttrInternal(&handle, 1111, &actual, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLFreeStmtInternal, FailInvalidhandle) {
  SQLRETURN status = SQLFreeStmtInternal(nullptr, SQL_CLOSE);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLFreeStmtInternal, FailInvalidoption) {
  StatementHandle handle = CreateStatementHandle();

  SQLRETURN status = SQLFreeStmtInternal(&handle, 111);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLFreeStmtInternal, CloseCursorAfterexecute) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);
  handle.SetStatementPrepared();

  SQLRETURN status = SQLFreeStmtInternal(&handle, SQL_CLOSE);

  EXPECT_EQ(StmtStates::kStatementPrepared, handle.GetStmtState());
  EXPECT_FALSE(handle.IsCursorOpen());
}

TEST(SQLFreeStmtInternal, CloseCursorAfterexecutedirect) {
  StatementHandle handle =
      CreateStmtHandleWithState(StmtStates::kStatementExecutedWithRs);

  SQLRETURN status = SQLFreeStmtInternal(&handle, SQL_CLOSE);

  EXPECT_EQ(StmtStates::kStatementNotPrepared, handle.GetStmtState());
  EXPECT_FALSE(handle.IsCursorOpen());
}

TEST(SQLFreeStmtInternal, CloseCursorCursornotopen) {
  StatementHandle handle = CreateStatementHandle();

  SQLRETURN status = SQLFreeStmtInternal(&handle, SQL_CLOSE);

  EXPECT_EQ(SQL_SUCCESS, status);
}

TEST(SQLFreeStmtInternal, UnbindBuffers) {
  StatementHandle handle = CreateStatementHandle();
  auto& ard = handle.GetDescriptorHandle(DescriptorType::kARD);
  DescriptorRecord record;
  ard.BindNewDescriptorRecord(1, record);
  EXPECT_EQ(1, ard.GetHeaderRecord().count);

  SQLRETURN status = SQLFreeStmtInternal(&handle, SQL_UNBIND);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(0, ard.GetHeaderRecord().count);
}

TEST(SQLFreeStmtInternal, UnbindBuffersNoboundbuffers) {
  StatementHandle handle = CreateStatementHandle();

  SQLRETURN status = SQLFreeStmtInternal(&handle, SQL_UNBIND);

  EXPECT_EQ(SQL_SUCCESS, status);
}

TEST(SQLFreeStmtInternal, UnbindParameters) {
  StatementHandle handle = CreateStatementHandle();
  auto& apd = handle.GetDescriptorHandle(DescriptorType::kAPD);
  DescriptorRecord record;
  apd.BindNewDescriptorRecord(1, record);
  EXPECT_EQ(1, apd.GetHeaderRecord().count);

  SQLRETURN status = SQLFreeStmtInternal(&handle, SQL_RESET_PARAMS);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(0, apd.GetHeaderRecord().count);
}

TEST(SQLFreeStmtInternal, UnbindParametersNoboundbuffers) {
  StatementHandle handle = CreateStatementHandle();

  SQLRETURN status = SQLFreeStmtInternal(&handle, SQL_RESET_PARAMS);

  EXPECT_EQ(SQL_SUCCESS, status);
}

TEST(SQLCancelInternal, FailsInvalidhandle) {
  ASSERT_EQ(SQL_INVALID_HANDLE, SQLCancelInternal(nullptr));
}

TEST(SQLCancelInternal, OperationCanceledPreparedState) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementPrepared);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_TRUE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, OperationCanceledAsyncPrepareState) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementAsyncPrepare);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_TRUE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, OperationCanceledAsyncExecuteState) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementAsyncExecute);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_TRUE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, OperationCanceledStillExecutingState) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementStillExecuting);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_TRUE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, OperationCanceledNeedsParamState) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kNeedsParams);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_TRUE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, OperationCanceledNeedsPutDataState) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kNeedsPutData);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_TRUE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, NoPreviousOperation) {
  StatementHandle handle = CreateStatementHandle();

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_FALSE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, OperationNotCanceledStmtexecutedwithrs) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  ASSERT_FALSE(handle.IsOperationCanceled());
}

TEST(SQLCancelInternal, OperationNotCanceledStmtexecutedwithoutrs) {
  StatementHandle handle = CreateStatementHandle();
  handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);

  SQLRETURN status = SQLCancelInternal(&handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  // TODO(jsrinivasan): Uncomment once PR-667 is merged.
  // ASSERT_FALSE(handle.IsOperationCanceled());
}
}  // namespace google::cloud::odbc_bq_driver
