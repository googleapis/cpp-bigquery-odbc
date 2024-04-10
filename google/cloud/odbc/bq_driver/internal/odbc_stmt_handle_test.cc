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

#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

StatusRecord GetLastStatusRecord(StatementHandle& handle) {
  auto status_records = handle.GetDiagnostics().GetStatusRecords();
  return status_records[status_records.size() - 1];
}

TEST(StatementHandle, BindColumn_Basic) {
  SQLCHAR buf[10];
  SQLLEN res_len = 0;
  StatementHandle handle;
  EXPECT_EQ(handle.BindColumn(0, SQL_C_CHAR, buf, 10, &res_len), SQL_SUCCESS);
  EXPECT_TRUE(handle.GetDiagnostics().GetStatusRecords().empty());
}

TEST(StatementHandle, BindColumn_NullBuffer) {
  SQLLEN res_len = 0;
  StatementHandle handle;
  ASSERT_EQ(handle.BindColumn(0, SQL_C_CHAR, nullptr, 10, &res_len), SQL_ERROR);
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY001());
  EXPECT_EQ(status_record.message, "TargetValuePtr should not be null");
}

TEST(StatementHandle, BindColumn_BuflenLessThanZero) {
  SQLCHAR buf[10];
  SQLLEN res_len = 0;
  StatementHandle handle;
  ASSERT_EQ(handle.BindColumn(0, SQL_C_CHAR, buf, -1, &res_len), SQL_ERROR);
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY090());
  EXPECT_EQ(status_record.message, "BufferLength should not be less than zero");
}

TEST(StatementHandle, BindColumn_NullResLen) {
  SQLCHAR buf[10];
  StatementHandle handle;
  ASSERT_EQ(handle.BindColumn(0, SQL_C_CHAR, buf, 10, nullptr), SQL_ERROR);
  ASSERT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());
  StatusRecord status_record = GetLastStatusRecord(handle);
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY000());
  EXPECT_EQ(status_record.message, "TargetValueStrLen should not be null");
}

TEST(GetDescriptorHandle, GetARD_impl) {
  DescriptorHandle ard(DescriptorType::kARD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle apd;
  DescriptorHandle ird;
  DescriptorHandle ipd;
  StatementHandle handle({ard, apd, ird, ipd});

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kARD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(DescriptorType::kARD, (*status)->GetType());
}

TEST(GetDescriptorHandle, GetAPD_impl) {
  DescriptorHandle ard;
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ird;
  DescriptorHandle ipd;
  StatementHandle handle({ard, apd, ird, ipd});

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kAPD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(DescriptorType::kAPD, (*status)->GetType());
}

TEST(GetDescriptorHandle, GetIRD_impl) {
  DescriptorHandle ard;
  DescriptorHandle apd;
  DescriptorHandle ird(DescriptorType::kIRD, SQL_DESC_ALLOC_AUTO);
  ;
  DescriptorHandle ipd;
  StatementHandle handle({ard, apd, ird, ipd});

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kIRD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(DescriptorType::kIRD, (*status)->GetType());
}

TEST(GetDescriptorHandle, GetIPD_impl) {
  DescriptorHandle ard;
  DescriptorHandle apd;
  DescriptorHandle ird;
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);
  ;
  StatementHandle handle({ard, apd, ird, ipd});

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kIPD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(DescriptorType::kIPD, (*status)->GetType());
}

TEST(GetDescriptorHandle, Fails_InvalidType) {
  StatementHandle handle;

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kApplication);

  EXPECT_THAT(status,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Descriptor Type is not supported")));
}

TEST(SetDescriptorHandle, SetAndGetARD) {
  DescriptorHandle desc_impl;
  StatementHandle handle({desc_impl, desc_impl, desc_impl, desc_impl});
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kARD, &desc);

  EXPECT_TRUE(status_record.ok());

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kARD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(&desc, *status);
}

TEST(SetDescriptorHandle, SetAndGetAPD) {
  DescriptorHandle desc_impl;
  StatementHandle handle({desc_impl, desc_impl, desc_impl, desc_impl});
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kAPD, &desc);

  EXPECT_TRUE(status_record.ok());

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kAPD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(&desc, *status);
}

TEST(SetDescriptorHandle, Fails_InvalidAllocType) {
  StatementHandle handle;
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_AUTO);

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kAPD, &desc);

  EXPECT_EQ(SQLStates::k_HY017(), status_record.sql_state);
}

TEST(SetDescriptorHandle, Fails_InvalidType_IRD) {
  StatementHandle handle;
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kIRD, &desc);

  EXPECT_EQ(SQLStates::k_HY017(), status_record.sql_state);
}

TEST(SetDescriptorHandle, Fails_InvalidType_IPD) {
  StatementHandle handle;
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kIPD, &desc);

  EXPECT_EQ(SQLStates::k_HY017(), status_record.sql_state);
}

TEST(SetDescriptorHandle, SetExplicitDescAndThenSetNull) {
  DescriptorHandle desc_impl;
  StatementHandle handle({desc_impl, desc_impl, desc_impl, desc_impl});
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_USER);

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kAPD, &desc);

  EXPECT_TRUE(status_record.ok());

  StatusRecordOr<DescriptorHandle*> status =
      handle.GetDescriptorHandle(DescriptorType::kAPD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(&desc, *status);

  status_record = handle.SetDescriptorHandle(DescriptorType::kAPD, nullptr);

  EXPECT_TRUE(status_record.ok());

  status = handle.GetDescriptorHandle(DescriptorType::kAPD);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_NE(nullptr, *status);
}

}  // namespace google::cloud::odbc_bq_driver_internal
