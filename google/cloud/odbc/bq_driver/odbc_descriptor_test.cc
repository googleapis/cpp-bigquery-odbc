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
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

TEST(SQLSetDescFieldInternal, Fails_InvalidHandle) {
  odbc_bq_driver_internal::EnvironmentHandle handle;
  HandleWrapped wrapped_handle(HandleType::kEnvHandle, &handle);

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_ARRAY_SIZE,
                                        (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_INVALID_HANDLE, status);
}

TEST(SQLSetDescFieldInternal,
     Fails_InvalidFieldIdentifier_ApplicationDescriptor) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);

  auto status = SQLSetDescFieldInternal(
      &wrapped_handle, 0, SQL_DESC_ROWS_PROCESSED_PTR, (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Fails_InvalidFieldIdentifier_IRD) {
  DescriptorHandle handle(DescriptorType::kIRD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_BIND_TYPE,
                                        (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Fails_InvalidFieldIdentifier_IPD) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_BIND_TYPE,
                                        (SQLPOINTER)10, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_BIND_TYPE_NullPointer) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_BIND_TYPE,
                                        nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(0, handle.GetHeaderRecord().bind_type);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_ARRAY_SIZE) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  u_long arr_size = 18446744073709551615UL;  // long long max

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_ARRAY_SIZE,
                                        (SQLPOINTER)arr_size, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(arr_size, handle.GetHeaderRecord().array_size);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_COUNT) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int count = 3;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_COUNT,
                                        (SQLPOINTER)count, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(count, handle.GetHeaderRecord().count);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_COUNT_Negative) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int count = -3;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_COUNT,
                                        (SQLPOINTER)count, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_COUNT_AndUnbindRecords) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  handle.BindNewDescriptorRecord(3, descriptor_record);
  SQLSMALLINT count = 0;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 0, SQL_DESC_COUNT,
                                        (SQLPOINTER)count, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(count, handle.GetHeaderRecord().count);
  EXPECT_FALSE(handle.HasDescriptorRecord(1));
  EXPECT_FALSE(handle.HasDescriptorRecord(3));
}

TEST(SQLSetDescFieldInternal, Fails_RecNumberNegative) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int count = 3;

  auto status = SQLSetDescFieldInternal(
      &wrapped_handle, -5, SQL_DESC_CONCISE_TYPE, (SQLPOINTER)count, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_07009(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  SQLCHAR buf[256] = "test";

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, 4);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("test", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_NAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_Truncated) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  SQLCHAR buf[256] = "test";

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, 2);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ("te", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_NAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_ZeroLength) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  SQLCHAR buf[256] = "test";

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_UNNAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_NullPointer) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);

  auto status =
      SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME, nullptr, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_UNNAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_NAME_TooBigLength) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  SQLCHAR buf[256] = "test";

  auto status =
      SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME,
                              (SQLPOINTER)buf, SQL_MAX_IDENTIFIER_LEN + 10);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_22001(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_NAME_NegativeLength) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  SQLCHAR buf[256] = "test";

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, -5);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY090(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_Length_SQL_NTS) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  SQLCHAR buf[256] = "test";

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, SQL_NTS);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("test", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_NAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NAME_EmptyBuffer) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  SQLCHAR buf[256] = "";

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_NAME,
                                        (SQLPOINTER)buf, SQL_NTS);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ("", handle.GetDescriptorRecord(1).name);
  EXPECT_EQ(SQL_UNNAMED, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_NUM_PREC_RADIX) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int radix = 0;

  auto status = SQLSetDescFieldInternal(
      &wrapped_handle, 1, SQL_DESC_NUM_PREC_RADIX, (SQLPOINTER)radix, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(radix, handle.GetDescriptorRecord(1).num_prec_radix);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_NUM_PREC_RADIX_WrongValue) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int radix = 1;

  auto status = SQLSetDescFieldInternal(
      &wrapped_handle, 1, SQL_DESC_NUM_PREC_RADIX, (SQLPOINTER)radix, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY092(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_PARAMETER_TYPE) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int type = SQL_PARAM_OUTPUT;

  auto status = SQLSetDescFieldInternal(
      &wrapped_handle, 1, SQL_DESC_PARAMETER_TYPE, (SQLPOINTER)type, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(type, handle.GetDescriptorRecord(1).parameter_type);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_PARAMETER_TYPE_WrongValue) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int type = 222;

  auto status = SQLSetDescFieldInternal(
      &wrapped_handle, 1, SQL_DESC_PARAMETER_TYPE, (SQLPOINTER)type, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY105(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_UNNAMED) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int val = SQL_UNNAMED;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_UNNAMED,
                                        (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(val, handle.GetDescriptorRecord(1).unnamed);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_UNNAMED_WrongValue) {
  DescriptorHandle handle(DescriptorType::kIPD);
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int val = SQL_NAMED;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_UNNAMED,
                                        (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY091(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_TYPE) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int val = SQL_INTEGER;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_TYPE,
                                        (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(val, handle.GetDescriptorRecord(1).type);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_TYPE_WrongValue) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int val = SQL_FLOAT;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_TYPE,
                                        (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_CONCISE_TYPE) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int val = SQL_INTEGER;

  auto status = SQLSetDescFieldInternal(
      &wrapped_handle, 1, SQL_DESC_CONCISE_TYPE, (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(val, handle.GetDescriptorRecord(1).type);
}

TEST(SQLSetDescFieldInternal, Fails_SQL_DESC_CONCISE_TYPE_WrongValue) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int val = SQL_FLOAT;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_TYPE,
                                        (SQLPOINTER)val, 0);

  EXPECT_EQ(SQL_ERROR, status);
  EXPECT_EQ(SQLStates::k_HY021(),
            handle.GetDiagnostics().GetStatusRecords()[0].sql_state);
}

TEST(SQLSetDescFieldInternal, Set_SQL_DESC_DATA_PTR) {
  DescriptorHandle handle;
  HandleWrapped wrapped_handle(HandleType::kDescriptorHandle, &handle);
  int* val;

  auto status = SQLSetDescFieldInternal(&wrapped_handle, 1, SQL_DESC_DATA_PTR,
                                        val, SQL_IS_POINTER);

  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_TRUE(handle.HasDescriptorRecord(1));
  EXPECT_EQ(val, handle.GetDescriptorRecord(1).data_ptr);
}

}  // namespace google::cloud::odbc_bq_driver
