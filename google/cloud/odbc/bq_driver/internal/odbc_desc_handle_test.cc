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

TEST(SetName, SetName) {
  DescriptorRecord descriptor_record;

  descriptor_record.SetName("test", 4);

  EXPECT_EQ("test", descriptor_record.name);
  EXPECT_EQ(SQL_NAMED, descriptor_record.unnamed);
}

TEST(SetName, SetName_SQL_NTS) {
  DescriptorRecord descriptor_record;

  descriptor_record.SetName("test", SQL_NTS);

  EXPECT_EQ("test", descriptor_record.name);
  EXPECT_EQ(SQL_NAMED, descriptor_record.unnamed);
}

TEST(SetName, SetName_Truncated) {
  DescriptorRecord descriptor_record;

  descriptor_record.SetName("test", 2);

  EXPECT_EQ("te", descriptor_record.name);
  EXPECT_EQ(SQL_NAMED, descriptor_record.unnamed);
}

TEST(SetName, SetName_EmptyString) {
  DescriptorRecord descriptor_record;

  descriptor_record.SetName("", 2);

  EXPECT_EQ("", descriptor_record.name);
  EXPECT_EQ(SQL_UNNAMED, descriptor_record.unnamed);
}

TEST(SetName, SetName_ZeroLength) {
  DescriptorRecord descriptor_record;

  descriptor_record.SetName("test", 0);

  EXPECT_EQ("", descriptor_record.name);
  EXPECT_EQ(SQL_UNNAMED, descriptor_record.unnamed);
}

}  // namespace google::cloud::odbc_bq_driver_internal
