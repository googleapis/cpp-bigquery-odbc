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

#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>
#ifdef _WIN32
#include <cstdint>
#endif

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateExplicitDescriptor;

TEST(SQLAllocDescHandle, SQLAllocDescHandle) {
  ConnectionHandle conn_handle = CreateConnectionHandle(true);
  SQLPOINTER output;

  auto status = SQLAllocDescHandle(&conn_handle, &output);

  ASSERT_EQ(SQL_SUCCESS, status);
  auto* desc_handle = reinterpret_cast<DescriptorHandle*>(output);
  std::set<DescriptorHandle*>& desc_handles =
      conn_handle.GetDescriptorHandles();
  EXPECT_FALSE(desc_handles.empty());
  EXPECT_TRUE(desc_handles.find(desc_handle) != desc_handles.end());
  delete desc_handle;
}

TEST(SQLSetDescFieldInternal, Fails_InvalidHandle) {
  EnvironmentHandle handle;

  auto status = SQLSetDescFieldInternal(&handle, 0, SQL_DESC_ARRAY_SIZE,
                                        (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLSetDescFieldInternal,
     Fails_InvalidFieldIdentifier_ApplicationDescriptor) {
  DescriptorHandle handle;

  auto status = SQLSetDescFieldInternal(&handle, 0, SQL_DESC_ROWS_PROCESSED_PTR,
                                        (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Fails_InvalidFieldIdentifier_IRD) {
  DescriptorHandle handle(DescriptorType::kIRD);

  auto status = SQLSetDescFieldInternal(&handle, 0, SQL_DESC_BIND_TYPE,
                                        (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Fails_InvalidFieldIdentifier_IPD) {
  DescriptorHandle handle(DescriptorType::kIPD);

  auto status = SQLSetDescFieldInternal(&handle, 0, SQL_DESC_BIND_TYPE,
                                        (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_BIND_TYPE_NullPointer) {
  DescriptorHandle handle;

  auto status =
      SQLSetDescFieldInternal(&handle, 0, SQL_DESC_BIND_TYPE, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(0, handle.GetHeaderRecord().bind_type);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_ARRAY_SIZE) {
  DescriptorHandle handle;
#ifdef _WIN32
  uint64_t arr_size = 18446744073709551615UL;  // long long max
#else
  u_long arr_size = 18446744073709551615UL;  // long long max
#endif
  auto status = SQLSetDescFieldInternal(&handle, 0, SQL_DESC_ARRAY_SIZE,
                                        (SQLPOINTER)arr_size, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(arr_size, handle.GetHeaderRecord().array_size);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_COUNT) {
  DescriptorHandle handle;
  int count = 3;

  auto status =
      SQLSetDescFieldInternal(&handle, 0, SQL_DESC_COUNT, (SQLPOINTER)count, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(count, handle.GetHeaderRecord().count);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_COUNT_Negative) {
  DescriptorHandle handle;
  int count = -3;

  auto status =
      SQLSetDescFieldInternal(&handle, 0, SQL_DESC_COUNT, (SQLPOINTER)count, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_COUNT_AndUnbindRecords) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  handle.BindNewDescriptorRecord(3, descriptor_record);
  SQLSMALLINT count = 0;

  auto status =
      SQLSetDescFieldInternal(&handle, 0, SQL_DESC_COUNT, (SQLPOINTER)count, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(count, handle.GetHeaderRecord().count);
  EXPECT_FALSE(handle.HasDescriptorRecord(1));
  EXPECT_FALSE(handle.HasDescriptorRecord(3));
}

TEST(SQLSetDescFieldInternal, Fails_RecNumberNegative) {
  DescriptorHandle handle;
  SQLSMALLINT rec_number = -5;

  auto status = SQLSetDescFieldInternal(
      &handle, rec_number, SQL_DESC_CONCISE_TYPE, (SQLPOINTER)3, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLCHAR buf[256] = "test";

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NAME, (SQLPOINTER)buf, 4);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("test", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_NAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_Truncated) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLCHAR buf[256] = "test";

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NAME, (SQLPOINTER)buf, 2);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("te", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_NAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_ZeroLength) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLCHAR buf[256] = "test";

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NAME, (SQLPOINTER)buf, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_UNNAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_NullPointer) {
  DescriptorHandle handle(DescriptorType::kIPD);

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NAME, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_UNNAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_NAME_TooBigLength) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLCHAR buf[256] = "test";

  auto status = SQLSetDescFieldInternal(
      &handle, 1, SQL_DESC_NAME, (SQLPOINTER)buf, SQL_MAX_IDENTIFIER_LEN + 10);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_22001(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_NAME_NegativeLength) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLCHAR buf[256] = "test";

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NAME, (SQLPOINTER)buf, -5);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_Length_SQL_NTS) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLCHAR buf[256] = "test";

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, SQL_NTS);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("test", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_NAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_EmptyBuffer) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLCHAR buf[256] = "";

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, SQL_NTS);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_UNNAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NUM_PREC_RADIX) {
  DescriptorHandle handle;
  int radix = 0;

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NUM_PREC_RADIX,
                                        (SQLPOINTER)radix, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(radix, handle.GetDescriptorRecord(1).num_prec_radix);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_NUM_PREC_RADIX_WrongValue) {
  DescriptorHandle handle;
  int radix = 1;

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_NUM_PREC_RADIX,
                                        (SQLPOINTER)radix, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_PARAMETER_TYPE) {
  DescriptorHandle handle(DescriptorType::kIPD);
  int type = SQL_PARAM_OUTPUT;

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_PARAMETER_TYPE,
                                        (SQLPOINTER)type, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(type, handle.GetDescriptorRecord(1).parameter_type);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_PARAMETER_TYPE_WrongValue) {
  DescriptorHandle handle(DescriptorType::kIPD);
  int type = 222;

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_PARAMETER_TYPE,
                                        (SQLPOINTER)type, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY105(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_UNNAMED) {
  DescriptorHandle handle(DescriptorType::kIPD);
  int val = SQL_UNNAMED;

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_UNNAMED, (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(val, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_UNNAMED_WrongValue) {
  DescriptorHandle handle(DescriptorType::kIPD);
  int val = SQL_NAMED;

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_UNNAMED, (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_TYPE) {
  DescriptorHandle handle;
  int val = SQL_C_NUMERIC;

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_TYPE, (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(val, handle.GetDescriptorRecord(1).type);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_TYPE_WrongValue) {
  DescriptorHandle handle;
  int val = SQL_FLOAT;

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_TYPE, (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_CONCISE_TYPE) {
  DescriptorHandle handle;
  int val = SQL_C_NUMERIC;

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_CONCISE_TYPE,
                                        (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(val, handle.GetDescriptorRecord(1).type);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_CONCISE_TYPE_WrongValue) {
  DescriptorHandle handle;
  int val = SQL_FLOAT;

  auto status =
      SQLSetDescFieldInternal(&handle, 1, SQL_DESC_TYPE, (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_DATA_PTR) {
  DescriptorHandle handle;
  int val = 0;

  auto status = SQLSetDescFieldInternal(&handle, 1, SQL_DESC_DATA_PTR, &val,
                                        SQL_IS_POINTER);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(&val, handle.GetDescriptorRecord(1).data_ptr);
}

TEST(SQLGetDescFieldInternal, Fails_InvalidHandle) {
  EnvironmentHandle handle;
  SQLPOINTER buff;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_ARRAY_SIZE, buff,
                                        0, nullptr);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetDescFieldInternal,
     Fails_InvalidFieldIdentifier_ApplicationDescriptor) {
  DescriptorHandle handle;
  SQLPOINTER buff;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_ROWS_PROCESSED_PTR,
                                        buff, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescFieldInternal, Fails_InvalidFieldIdentifier_IRD) {
  DescriptorHandle handle(DescriptorType::kIRD);
  SQLPOINTER buff;

  auto status =
      SQLGetDescFieldInternal(&handle, 0, SQL_DESC_BIND_TYPE, buff, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescFieldInternal, Fails_InvalidFieldIdentifier_IPD) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLPOINTER buff;

  auto status =
      SQLGetDescFieldInternal(&handle, 0, SQL_DESC_BIND_TYPE, buff, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_ALLOC_TYPE) {
  DescriptorHandle handle = CreateExplicitDescriptor();
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_ALLOC_TYPE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(handle.GetHeaderRecord().GetAllocType(), out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_ARRAY_SIZE) {
  DescriptorHandle handle;
  handle.GetHeaderRecord().array_size = 15;
  SQLULEN out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_ARRAY_SIZE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(15, out_buf);
  EXPECT_EQ(sizeof(SQLULEN), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_ARRAY_STATUS_PTR) {
  DescriptorHandle handle;
  SQLUSMALLINT array_status_ptr[] = {1, 2, 3};
  handle.GetHeaderRecord().array_status_ptr = array_status_ptr;
  SQLUSMALLINT* out_buf;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_ARRAY_STATUS_PTR,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(array_status_ptr, out_buf);
  EXPECT_EQ(1, out_buf[0]);
  EXPECT_EQ(2, out_buf[1]);
  EXPECT_EQ(3, out_buf[2]);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_BIND_OFFSET_PTR) {
  DescriptorHandle handle;
  SQLLEN bind_offset_ptr[] = {1, 2, 3};
  handle.GetHeaderRecord().bind_offset_ptr = bind_offset_ptr;
  SQLLEN* out_buf;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_BIND_OFFSET_PTR,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(bind_offset_ptr, out_buf);
  EXPECT_EQ(1, out_buf[0]);
  EXPECT_EQ(2, out_buf[1]);
  EXPECT_EQ(3, out_buf[2]);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_BIND_TYPE) {
  DescriptorHandle handle;
  handle.GetHeaderRecord().bind_type = 42;
  SQLINTEGER out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_BIND_TYPE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_COUNT) {
  DescriptorHandle handle;
  handle.GetHeaderRecord().count = 42;
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_COUNT, &out_buf, 0,
                                        &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Fail_StatementIsNotPrepared_IRD) {
  StatementHandle stmt_handle;
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  SQLULEN* out_buf;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_CASE_SENSITIVE,
                                        &out_buf, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY007(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_ROWS_PROCESSED_PTR) {
  DescriptorHandle handle(DescriptorType::kIPD);
  SQLULEN rows_processed_ptr[] = {1, 2, 3};
  handle.GetHeaderRecord().rows_processed_ptr = rows_processed_ptr;
  SQLULEN* out_buf;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, 0, SQL_DESC_ROWS_PROCESSED_PTR,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(rows_processed_ptr, out_buf);
  EXPECT_EQ(1, out_buf[0]);
  EXPECT_EQ(2, out_buf[1]);
  EXPECT_EQ(3, out_buf[2]);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Fails_RecNumberNegative) {
  DescriptorHandle handle;
  SQLINTEGER* out_buf;
  SQLSMALLINT rec_number = -5;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_CONCISE_TYPE, &out_buf, 0, nullptr);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescFieldInternal, Succeed_NoData_BigRecNumber) {
  DescriptorHandle handle;
  SQLINTEGER* out_buf;
  SQLSMALLINT rec_number = 5;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_CONCISE_TYPE, &out_buf, 0, nullptr);

  EXPECT_EQ(SQL_NO_DATA, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescFieldInternal, GetDefault_RecNumberNotExist) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.length = 42;
  handle.BindNewDescriptorRecord(3, descriptor_record);
  SQLULEN out_buf = 0;
  SQLSMALLINT rec_number = 1;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_LENGTH,
                                        &out_buf, 0, nullptr);

  EXPECT_EQ(SQL_SUCCESS, status);
  DescriptorRecord default_descriptor_record;
  EXPECT_EQ(default_descriptor_record.length, out_buf);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_AUTO_UNIQUE_VALUE) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.auto_unique_value = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLINTEGER out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_AUTO_UNIQUE_VALUE, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_BASE_COLUMN_NAME) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.base_column_name = "column";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR* out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_BASE_COLUMN_NAME, out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("column", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(6, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_BASE_TABLE_NAME) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.base_table_name = "table";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR* out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_BASE_TABLE_NAME, out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("table", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(5, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_CASE_SENSITIVE) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.case_sensitive = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLINTEGER out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_CASE_SENSITIVE, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_CATALOG_NAME) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.catalog_name = "catalog";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR* out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_CATALOG_NAME, out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("catalog", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(7, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_CONCISE_TYPE) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.concise_type = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_CONCISE_TYPE, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_DATA_PTR) {
  DescriptorHandle handle;
  SQLINTEGER val = 5;
  DescriptorRecord descriptor_record;
  descriptor_record.data_ptr = &val;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_DATA_PTR,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&val, out_buf);
  EXPECT_EQ(5, *reinterpret_cast<SQLINTEGER*>(out_buf));
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_DATETIME_INTERVAL_CODE) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number,
                                        SQL_DESC_DATETIME_INTERVAL_CODE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_DATETIME_INTERVAL_PRECISION) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_precision = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLINTEGER out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number,
                                        SQL_DESC_DATETIME_INTERVAL_PRECISION,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_DISPLAY_SIZE) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.display_size = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLLEN out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_DISPLAY_SIZE, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_FIXED_PREC_SCALE) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.fixed_prec_scale = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_FIXED_PREC_SCALE, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_INDICATOR_PTR) {
  DescriptorHandle handle;
  SQLLEN val = 5;
  DescriptorRecord descriptor_record;
  descriptor_record.indicator_ptr = &val;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLLEN* out_buf = nullptr;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_INDICATOR_PTR, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(5, *out_buf);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_INDICATOR_PTR_NullPtr) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLLEN* out_buf = nullptr;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_INDICATOR_PTR, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, out_buf);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_LABEL) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.label = "label";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_LABEL,
                                        out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("label", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(5, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_LENGTH) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.length = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLULEN out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_LENGTH,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLULEN), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_LITERAL_PREFIX) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.literal_prefix = "prefix";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_LITERAL_PREFIX, out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("prefix", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(6, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_LITERAL_SUFFIX) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.literal_suffix = "sufix";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_LITERAL_SUFFIX, out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("sufix", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(5, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_LOCAL_TYPE_NAME) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.local_type_name = "local name";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[11];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_LOCAL_TYPE_NAME, out_buf, 11, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("local name", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(10, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_NAME) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.name = "name";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_NAME,
                                        out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("name", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(4, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_NULLABLE) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.nullable = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_NULLABLE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_NUM_PREC_RADIX) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.num_prec_radix = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLINTEGER out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_NUM_PREC_RADIX, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_OCTET_LENGTH) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.octet_length = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLLEN out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_OCTET_LENGTH, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_OCTET_LENGTH_PTR) {
  DescriptorHandle handle;
  SQLLEN val = 5;
  DescriptorRecord descriptor_record;
  descriptor_record.octet_length_ptr = &val;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLLEN* out_buf = nullptr;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_OCTET_LENGTH_PTR, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(5, *out_buf);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_OCTET_LENGTH_PTR_NullPtr) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLLEN* out_buf = nullptr;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_OCTET_LENGTH_PTR, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(nullptr, out_buf);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_PARAMETER_TYPE) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.parameter_type = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_PARAMETER_TYPE, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_PRECISION) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.precision = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_PRECISION,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_SCALE) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.scale = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_SCALE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_SCHEMA_NAME) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.schema_name = "schema name";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[12];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_SCHEMA_NAME, out_buf, 12, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("schema name", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(11, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_SEARCHABLE) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.searchable = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_SEARCHABLE, &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_TABLE_NAME) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.table_name = "table name";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[11];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(
      &handle, rec_number, SQL_DESC_TABLE_NAME, out_buf, 11, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("table name", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(10, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_TYPE) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.type = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_TYPE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_TYPE_NAME) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.type_name = "type name";
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLCHAR out_buf[10];
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_TYPE_NAME,
                                        out_buf, 10, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("type name", std::string(reinterpret_cast<char*>(out_buf)));
  EXPECT_EQ(9, str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_UNNAMED) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.unnamed = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_UNNAMED,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_UNSIGNED) {
  DescriptorHandle handle(DescriptorType::kIPD);
  DescriptorRecord descriptor_record;
  descriptor_record.sql_desc_unsigned = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_UNSIGNED,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLGetDescFieldInternal, Get_SQL_DESC_UPDATABLE) {
  StatementHandle stmt_handle;
  stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
  DescriptorHandle handle(DescriptorType::kIRD);
  handle.GetAssociatedStatementHandles().emplace(&stmt_handle,
                                                 DescriptorType::kIRD);
  DescriptorRecord descriptor_record;
  descriptor_record.updatable = 42;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);
  SQLSMALLINT out_buf = 0;
  SQLINTEGER str_len = 0;

  auto status = SQLGetDescFieldInternal(&handle, rec_number, SQL_DESC_UPDATABLE,
                                        &out_buf, 0, &str_len);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(42, out_buf);
  EXPECT_EQ(sizeof(SQLSMALLINT), str_len);
}

TEST(SQLSetDescRecInternal, Fails_InvalidHandle) {
  EnvironmentHandle handle;
  int data = 10;
  SQLLEN string_length_ptr = 0;
  SQLLEN indicator[3];

  auto status = SQLSetDescRecInternal(&handle, 0, 0, 0, 0, 0, 0, &data,
                                      &string_length_ptr, indicator);
  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLSetDescRecInternal, Fails_RecNumberNegative) {
  DescriptorHandle handle;
  SQLSMALLINT rec_number = -5;
  int data = 10;
  SQLPOINTER data_ptr = &data;
  SQLLEN string_length_ptr = 0;
  SQLLEN indicator[3];

  auto status = SQLSetDescRecInternal(&handle, rec_number, 0, 0, 0, 0, 0,
                                      data_ptr, &string_length_ptr, indicator);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescRecInternal, Success) {
  DescriptorHandle handle;
  SQLSMALLINT type = SQL_DATETIME;
  SQLSMALLINT sub_type = SQL_CODE_DATE;
  SQLLEN length = 3;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  int data = 10;
  SQLLEN string_length = 0;
  SQLLEN indicator[3];

  auto status =
      SQLSetDescRecInternal(&handle, 1, type, sub_type, length, precision,
                            scale, &data, &string_length, indicator);
  EXPECT_EQ(SQL_SUCCESS, status);

  DescriptorRecord desc = handle.GetDescriptorRecord(1);
  EXPECT_EQ(type, desc.type);
  EXPECT_EQ(sub_type, desc.datetime_interval_code);
  EXPECT_EQ(length, desc.octet_length);
  EXPECT_EQ(precision, desc.precision);
  EXPECT_EQ(scale, desc.scale);
  EXPECT_EQ(&data, desc.data_ptr);
  EXPECT_EQ(&string_length, desc.octet_length_ptr);
  EXPECT_EQ(indicator, desc.indicator_ptr);
}

TEST(SQLSetDescRecInternal, DoNothing_ConcistencyCheckFails) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  SQLSMALLINT type = 555;
  SQLSMALLINT sub_type = 555;
  SQLLEN length = 3;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  int data = 10;
  SQLLEN string_length = 0;
  SQLLEN indicator[3];

  auto status =
      SQLSetDescRecInternal(&handle, 1, type, sub_type, length, precision,
                            scale, &data, &string_length, indicator);
  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);

  DescriptorRecord desc = handle.GetDescriptorRecord(1);
  EXPECT_EQ(descriptor_record.type, desc.type);
  EXPECT_EQ(descriptor_record.datetime_interval_code,
            desc.datetime_interval_code);
  EXPECT_EQ(descriptor_record.length, desc.octet_length);
  EXPECT_EQ(descriptor_record.precision, desc.precision);
  EXPECT_EQ(descriptor_record.scale, desc.scale);
  EXPECT_EQ(descriptor_record.data_ptr, desc.data_ptr);
  EXPECT_EQ(descriptor_record.octet_length_ptr, desc.octet_length_ptr);
  EXPECT_EQ(descriptor_record.indicator_ptr, desc.indicator_ptr);
}

TEST(SQLSetDescRecInternal, DoNotChangeExistingField) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.name = "test";
  handle.BindNewDescriptorRecord(1, descriptor_record);

  SQLSMALLINT type = SQL_DATETIME;
  SQLSMALLINT sub_type = SQL_CODE_DATE;
  SQLLEN length = 3;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  int data = 10;
  SQLLEN string_length = 0;
  SQLLEN indicator[3];

  auto status =
      SQLSetDescRecInternal(&handle, 1, type, sub_type, length, precision,
                            scale, &data, &string_length, indicator);
  EXPECT_EQ(SQL_SUCCESS, status);

  DescriptorRecord desc = handle.GetDescriptorRecord(1);
  EXPECT_EQ("test", desc.name);
}

TEST(SQLGetDescRecInternal, Fails_InvalidHandle) {
  EnvironmentHandle handle;
  SQLCHAR name[10];
  SQLSMALLINT buffer_length = 10;
  SQLSMALLINT string_length = 0;
  SQLSMALLINT type = 0;
  SQLSMALLINT sub_type = 0;
  SQLLEN length = 0;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  SQLSMALLINT nullable = 0;

  auto status = SQLGetDescRecInternal(&handle, 1, name, buffer_length,
                                      &string_length, &type, &sub_type, &length,
                                      &precision, &scale, &nullable);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLGetDescRecInternal, Fails_RecNumberNegative) {
  DescriptorHandle handle;
  SQLSMALLINT rec_number = -5;
  SQLCHAR name[10];
  SQLSMALLINT buffer_length = 10;
  SQLSMALLINT string_length = 0;
  SQLSMALLINT type = 0;
  SQLSMALLINT sub_type = 0;
  SQLLEN length = 0;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  SQLSMALLINT nullable = 0;

  auto status = SQLGetDescRecInternal(&handle, rec_number, name, buffer_length,
                                      &string_length, &type, &sub_type, &length,
                                      &precision, &scale, &nullable);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescRecInternal, Succeed_NoData_BigRecNumber) {
  DescriptorHandle handle;
  SQLSMALLINT rec_number = 5;
  SQLCHAR name[10];
  SQLSMALLINT buffer_length = 10;
  SQLSMALLINT string_length = 0;
  SQLSMALLINT type = 0;
  SQLSMALLINT sub_type = 0;
  SQLLEN length = 0;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  SQLSMALLINT nullable = 0;

  auto status = SQLGetDescRecInternal(&handle, rec_number, name, buffer_length,
                                      &string_length, &type, &sub_type, &length,
                                      &precision, &scale, &nullable);

  EXPECT_EQ(SQL_NO_DATA, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLGetDescRecInternal, Success) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.name = "test";
  descriptor_record.type = SQL_INTEGER;
  descriptor_record.datetime_interval_code = 11;
  descriptor_record.octet_length = 12;
  descriptor_record.precision = 13;
  descriptor_record.scale = 14;
  descriptor_record.nullable = SQL_NO_NULLS;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);

  SQLCHAR name[10];
  SQLSMALLINT buffer_length = 10;
  SQLSMALLINT string_length = 0;
  SQLSMALLINT type = 0;
  SQLSMALLINT sub_type = 0;
  SQLLEN length = 0;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  SQLSMALLINT nullable = 0;

  auto status = SQLGetDescRecInternal(&handle, rec_number, name, buffer_length,
                                      &string_length, &type, &sub_type, &length,
                                      &precision, &scale, &nullable);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(descriptor_record.name, std::string(reinterpret_cast<char*>(name)));
  EXPECT_EQ(descriptor_record.name.size(), string_length);
  EXPECT_EQ(descriptor_record.type, type);
  EXPECT_EQ(descriptor_record.datetime_interval_code, sub_type);
  EXPECT_EQ(descriptor_record.octet_length, length);
  EXPECT_EQ(descriptor_record.precision, precision);
  EXPECT_EQ(descriptor_record.scale, scale);
  EXPECT_EQ(descriptor_record.nullable, nullable);
}

TEST(SQLGetDescRecInternal, GetDefault_RecNumberNotExist) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.length = 42;
  handle.BindNewDescriptorRecord(3, descriptor_record);

  SQLCHAR name[10];
  SQLSMALLINT buffer_length = 10;
  SQLSMALLINT string_length = 0;
  SQLSMALLINT type = 0;
  SQLSMALLINT sub_type = 0;
  SQLLEN length = 0;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  SQLSMALLINT nullable = 0;

  auto status = SQLGetDescRecInternal(&handle, 1, name, buffer_length,
                                      &string_length, &type, &sub_type, &length,
                                      &precision, &scale, &nullable);

  ASSERT_EQ(SQL_SUCCESS, status);
  DescriptorRecord default_descriptor_record;
  EXPECT_EQ(default_descriptor_record.name,
            std::string(reinterpret_cast<char*>(name)));
  EXPECT_EQ(default_descriptor_record.name.size(), string_length);
  EXPECT_EQ(default_descriptor_record.type, type);
  EXPECT_EQ(default_descriptor_record.datetime_interval_code, sub_type);
  EXPECT_EQ(default_descriptor_record.octet_length, length);
  EXPECT_EQ(default_descriptor_record.precision, precision);
  EXPECT_EQ(default_descriptor_record.scale, scale);
  EXPECT_EQ(descriptor_record.nullable, nullable);
}

TEST(SQLGetDescRecInternal, SuccessWithInfo_BufferIsSmall) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  descriptor_record.name = "test";
  descriptor_record.type = SQL_INTEGER;
  descriptor_record.datetime_interval_code = 11;
  descriptor_record.octet_length = 12;
  descriptor_record.precision = 13;
  descriptor_record.scale = 14;
  descriptor_record.nullable = SQL_NO_NULLS;
  SQLSMALLINT rec_number = 1;
  handle.BindNewDescriptorRecord(rec_number, descriptor_record);

  SQLCHAR name[3];
  SQLSMALLINT buffer_length = 3;
  SQLSMALLINT string_length = 0;
  SQLSMALLINT type = 0;
  SQLSMALLINT sub_type = 0;
  SQLLEN length = 0;
  SQLSMALLINT precision = 0;
  SQLSMALLINT scale = 0;
  SQLSMALLINT nullable = 0;

  auto status = SQLGetDescRecInternal(&handle, rec_number, name, buffer_length,
                                      &string_length, &type, &sub_type, &length,
                                      &precision, &scale, &nullable);

  ASSERT_EQ(SQL_SUCCESS_WITH_INFO, status);
  EXPECT_EQ("te", std::string(reinterpret_cast<char*>(name)));
  EXPECT_EQ(2, string_length);
  EXPECT_EQ(descriptor_record.type, type);
  EXPECT_EQ(descriptor_record.datetime_interval_code, sub_type);
  EXPECT_EQ(descriptor_record.octet_length, length);
  EXPECT_EQ(descriptor_record.precision, precision);
  EXPECT_EQ(descriptor_record.scale, scale);
  EXPECT_EQ(descriptor_record.nullable, nullable);
}

TEST(SQLCopyDescInternal, CopyDescriptor) {
  DescriptorHandle src_handle = CreateExplicitDescriptor();
  src_handle.GetHeaderRecord().bind_type = 5;
  SQLLEN bind_offset = 3;
  src_handle.GetHeaderRecord().bind_offset_ptr = &bind_offset;
  DescriptorRecord descriptor_record;
  descriptor_record.octet_length = 6;
  descriptor_record.type = SQL_INTEGER;
  descriptor_record.concise_type = SQL_INTEGER;
  descriptor_record.precision = 1;
  src_handle.BindNewDescriptorRecord(3, descriptor_record);

  DescriptorHandle target_handle(DescriptorType::kApplication,
                                 SQL_DESC_ALLOC_AUTO);

  auto status = SQLCopyDescInternal(&src_handle, &target_handle);

  ASSERT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DESC_ALLOC_AUTO,
            target_handle.GetHeaderRecord().GetAllocType());
  EXPECT_EQ(src_handle.GetHeaderRecord().bind_type,
            target_handle.GetHeaderRecord().bind_type);
  EXPECT_EQ(src_handle.GetHeaderRecord().bind_offset_ptr,
            target_handle.GetHeaderRecord().bind_offset_ptr);
  DescriptorRecord target_descriptor_record =
      target_handle.GetDescriptorRecord(3);
  EXPECT_EQ(descriptor_record.octet_length,
            target_descriptor_record.octet_length);
  EXPECT_EQ(descriptor_record.type, target_descriptor_record.type);
  EXPECT_EQ(descriptor_record.concise_type,
            target_descriptor_record.concise_type);
  EXPECT_EQ(descriptor_record.precision, target_descriptor_record.precision);
}

TEST(SQLSetDescFieldInternal, Fails_InvalidHandle_Source) {
  EnvironmentHandle src_handle;
  DescriptorHandle target_handle(DescriptorType::kApplication,
                                 SQL_DESC_ALLOC_AUTO);

  auto status = SQLCopyDescInternal(&src_handle, &target_handle);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLSetDescFieldInternal, Fails_InvalidHandle_Target) {
  DescriptorHandle src_handle = CreateExplicitDescriptor();
  EnvironmentHandle target_handle;

  auto status = SQLCopyDescInternal(&src_handle, &target_handle);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLSetDescFieldInternal, Fails_InvalidHandleTargetType_IRD) {
  DescriptorHandle src_handle = CreateExplicitDescriptor();
  DescriptorHandle target_handle(DescriptorType::kIRD, SQL_DESC_ALLOC_AUTO);

  auto status = SQLCopyDescInternal(&src_handle, &target_handle);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY016(),
            target_handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLCopyDescInternal, Fails_InconsistentDescriptor) {
  DescriptorHandle src_handle = CreateExplicitDescriptor();
  DescriptorRecord descriptor_record_1;
  descriptor_record_1.type = SQL_INTEGER;
  descriptor_record_1.concise_type = SQL_CHAR;
  src_handle.BindNewDescriptorRecord(1, descriptor_record_1);
  DescriptorRecord descriptor_record_3;
  descriptor_record_3.type = SQL_INTEGER;
  descriptor_record_3.concise_type = SQL_CHAR;
  int data = 0;
  descriptor_record_3.data_ptr = &data;
  src_handle.BindNewDescriptorRecord(3, descriptor_record_3);
  DescriptorRecord descriptor_record_5;
  src_handle.BindNewDescriptorRecord(5, descriptor_record_5);

  DescriptorHandle target_handle(DescriptorType::kApplication,
                                 SQL_DESC_ALLOC_AUTO);

  auto status = SQLCopyDescInternal(&src_handle, &target_handle);

  ASSERT_EQ(SQL_ERROR, status);
  DescriptorRecord target_descriptor_record_1 =
      target_handle.GetDescriptorRecord(1);
  EXPECT_EQ(descriptor_record_1.type, target_descriptor_record_1.type);
  EXPECT_EQ(descriptor_record_1.concise_type,
            target_descriptor_record_1.concise_type);
  DescriptorRecord target_descriptor_record =
      target_handle.GetDescriptorRecord(3);
  EXPECT_EQ(descriptor_record_3.type, target_descriptor_record.type);
  EXPECT_EQ(descriptor_record_3.concise_type,
            target_descriptor_record.concise_type);
  EXPECT_EQ(nullptr, target_descriptor_record.data_ptr);
  EXPECT_FALSE(target_handle.HasDescriptorRecord(5));
}

}  // namespace google::cloud::odbc_bq_driver
