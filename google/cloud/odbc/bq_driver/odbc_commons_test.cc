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

#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSValue;
using google::cloud::odbc_bq_driver_internal::DSValueToString;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StringToDSValue;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

struct NativeDataTypesStruct {
  bool flag;
  char character;
  short short_var;
  int int_var;
  long long_var;
  long long long_long_var;
  float float_var;
  double double_var;
};

TEST(SQLFreeHandleInternal, InvalidType) {
  int val = 10;

  SQLRETURN status = SQLFreeHandleInternal(55, &val);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
}

TEST(SQLFreeHandleInternal, ConnectionHandle_Basic) {
  auto* conn_handle = new ConnectionHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DBC, conn_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, ConnectionHandle_IncorrectHandleType) {
  auto* conn_handle = new ConnectionHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_ENV, conn_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete conn_handle;
}

TEST(SQLFreeHandleInternal, EnvironmentHandle_Basic) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_ENV, env_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, EnvironmentHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DBC, env_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
}

TEST(SQLFreeHandleInternal, StatementHandle_Basic) {
  ConnectionHandle conn_handle;
  auto* stmt_handle = new StatementHandle(&conn_handle);
  conn_handle.GetStatementHandles().emplace(stmt_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_STMT, stmt_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_TRUE(conn_handle.GetStatementHandles().empty());
}

TEST(SQLFreeHandleInternal, StatementHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_STMT, env_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
}

TEST(SQLFreeHandleInternal, DescriptorHandle_Basic) {
  auto* desc_handle = new DescriptorHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, desc_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
}

TEST(SQLFreeHandleInternal, DescriptorHandle_IncorrectHandleType) {
  auto* env_handle = new EnvironmentHandle();

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, env_handle);

  EXPECT_EQ(status, SQL_INVALID_HANDLE);
  delete env_handle;
}

TEST(SQLFreeHandleInternal, DissociateDescriptorHandle) {
  StatementHandle stmt_handle = CreateStatementHandle();
  auto* desc_handle =
      new DescriptorHandle(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);
  stmt_handle.SetDescriptorHandle(DescriptorType::kAPD, desc_handle);

  SQLRETURN status = SQLFreeHandleInternal(SQL_HANDLE_DESC, desc_handle);

  EXPECT_EQ(status, SQL_SUCCESS);
  EXPECT_EQ(SQL_DESC_ALLOC_AUTO,
            stmt_handle.GetDescriptorHandle(DescriptorType::kAPD)
                .GetHeaderRecord()
                .GetAllocType());
}

TEST(DSValue, Basic_String) {
  std::string expected = "Some string which should be converted to DSValue";
  DSValue value;
  StringToDSValue(expected, value);

  std::string returned;

  DSValueToString(value, returned);
  EXPECT_EQ(expected, returned);
}

TEST(DSValue, Basic_ComplexStruct) {
  DSValue bq_value(sizeof(NativeDataTypesStruct));

  NativeDataTypesStruct custom_data = {
      true, 'A', 100, 12345, 1234567890L, 98765432101234LL, 3.14f, 2.71828};
  memcpy(bq_value.data(), &custom_data, sizeof(NativeDataTypesStruct));

  NativeDataTypesStruct* expected =
      reinterpret_cast<NativeDataTypesStruct*>(bq_value.data());
  EXPECT_EQ(custom_data.flag, expected->flag);
  EXPECT_EQ(custom_data.character, expected->character);
  EXPECT_EQ(custom_data.short_var, expected->short_var);
  EXPECT_EQ(custom_data.int_var, expected->int_var);
  EXPECT_EQ(custom_data.long_var, expected->long_var);
  EXPECT_EQ(custom_data.long_long_var, expected->long_long_var);
  EXPECT_EQ(custom_data.float_var, expected->float_var);
  EXPECT_EQ(custom_data.double_var, expected->double_var);
}

}  // namespace google::cloud::odbc_bq_driver
