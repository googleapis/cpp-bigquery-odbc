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

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(BindNewDescriptorRecord, UpdateCount) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;

  handle.BindNewDescriptorRecord(1, descriptor_record);

  EXPECT_EQ(1, handle.GetHeaderRecord().count);

  handle.BindNewDescriptorRecord(3, descriptor_record);

  EXPECT_EQ(3, handle.GetHeaderRecord().count);
}

TEST(BindNewDescriptorRecord, DoNotUpdateCount) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;

  handle.BindNewDescriptorRecord(3, descriptor_record);

  EXPECT_EQ(3, handle.GetHeaderRecord().count);

  handle.BindNewDescriptorRecord(1, descriptor_record);

  EXPECT_EQ(3, handle.GetHeaderRecord().count);
}

TEST(UnbindDescriptorRecord, CountIsOldMaxIndex) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  handle.BindNewDescriptorRecord(3, descriptor_record);
  EXPECT_EQ(3, handle.GetHeaderRecord().count);

  StatusRecordOr<DescriptorRecord> status = handle.UnbindDescriptorRecord(1);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(3, handle.GetHeaderRecord().count);
}

TEST(UnbindDescriptorRecord, CountIsNewMaxIndex) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  handle.BindNewDescriptorRecord(3, descriptor_record);
  EXPECT_EQ(3, handle.GetHeaderRecord().count);

  StatusRecordOr<DescriptorRecord> status = handle.UnbindDescriptorRecord(3);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(1, handle.GetHeaderRecord().count);
}

TEST(UnbindDescriptorRecord, CountIsZero) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  EXPECT_EQ(1, handle.GetHeaderRecord().count);

  StatusRecordOr<DescriptorRecord> status = handle.UnbindDescriptorRecord(1);

  ASSERT_STATUS_RECORD_OK(status);
  EXPECT_EQ(0, handle.GetHeaderRecord().count);
}

TEST(UnbindDescriptorRecord, Fails_WrongIndex) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;

  StatusRecordOr<DescriptorRecord> status = handle.UnbindDescriptorRecord(3);

  EXPECT_THAT(status,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("non-existent descriptor record")));
}

TEST(UnbindAllDescriptorRecordsFrom, UnbindAll) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  handle.BindNewDescriptorRecord(3, descriptor_record);
  EXPECT_EQ(3, handle.GetHeaderRecord().count);

  StatusRecord status = handle.UnbindAllDescriptorRecordsFrom(0);

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(0, handle.GetHeaderRecord().count);
}

TEST(UnbindAllDescriptorRecordsFrom, UnbindNothing_NewIndexIsTooBig) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  handle.BindNewDescriptorRecord(3, descriptor_record);
  EXPECT_EQ(3, handle.GetHeaderRecord().count);

  StatusRecord status = handle.UnbindAllDescriptorRecordsFrom(5);

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(3, handle.GetHeaderRecord().count);
}

TEST(UnbindAllDescriptorRecordsFrom, UnbindNothing_NoRecords) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  EXPECT_EQ(0, handle.GetHeaderRecord().count);

  StatusRecord status = handle.UnbindAllDescriptorRecordsFrom(5);

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(0, handle.GetHeaderRecord().count);
}

TEST(UnbindAllDescriptorRecordsFrom, UnbindNothing_NegativeIndex) {
  DescriptorHandle handle;
  DescriptorRecord descriptor_record;
  handle.BindNewDescriptorRecord(1, descriptor_record);
  handle.BindNewDescriptorRecord(3, descriptor_record);
  EXPECT_EQ(3, handle.GetHeaderRecord().count);

  StatusRecord status = handle.UnbindAllDescriptorRecordsFrom(-5);

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(SQLStates::k_07009(), status.sql_state);
  EXPECT_EQ(3, handle.GetHeaderRecord().count);
}

TEST(SetDescriptorRecords, SetRecords) {
  DescriptorHandle handle;
  std::map<SQLSMALLINT, DescriptorRecord> records;
  DescriptorRecord record_1;
  record_1.type = SQL_INTEGER;
  records[1] = record_1;
  DescriptorRecord record_3;
  record_3.type = SQL_CHAR;
  records[3] = record_3;

  handle.SetDescriptorRecords(records);

  EXPECT_EQ(3, handle.GetHeaderRecord().count);
  EXPECT_EQ(record_1.type, handle.GetDescriptorRecord(1).type);
  EXPECT_FALSE(handle.HasDescriptorRecord(2));
  EXPECT_EQ(record_3.type, handle.GetDescriptorRecord(3).type);
}

TEST(SetDescriptorRecords, ClearRecords) {
  DescriptorHandle handle;
  std::map<SQLSMALLINT, DescriptorRecord> records;
  DescriptorRecord record_1;
  record_1.type = SQL_INTEGER;
  handle.BindNewDescriptorRecord(1, record_1);

  handle.SetDescriptorRecords(records);

  EXPECT_EQ(0, handle.GetHeaderRecord().count);
  EXPECT_FALSE(handle.HasDescriptorRecord(1));
}

TEST(SetDescriptorRecords, FailInconsistentData) {
  DescriptorHandle handle;
  std::map<SQLSMALLINT, DescriptorRecord> records;
  DescriptorRecord record_1;
  record_1.type = SQL_INTEGER;
  records[1] = record_1;
  DescriptorRecord record_3;
  record_3.type = SQL_CHAR;
  int data = 10;
  record_3.data_ptr = &data;
  records[3] = record_3;
  DescriptorRecord record_5;
  record_5.type = SQL_INTEGER;
  records[5] = record_5;

  handle.SetDescriptorRecords(records);

  EXPECT_EQ(3, handle.GetHeaderRecord().count);
  EXPECT_EQ(record_1.type, handle.GetDescriptorRecord(1).type);
  EXPECT_FALSE(handle.HasDescriptorRecord(2));
  EXPECT_EQ(record_3.type, handle.GetDescriptorRecord(3).type);
  EXPECT_EQ(nullptr, handle.GetDescriptorRecord(3).data_ptr);
  EXPECT_FALSE(handle.HasDescriptorRecord(4));
  EXPECT_FALSE(handle.HasDescriptorRecord(5));
}

}  // namespace google::cloud::odbc_bq_driver_internal
