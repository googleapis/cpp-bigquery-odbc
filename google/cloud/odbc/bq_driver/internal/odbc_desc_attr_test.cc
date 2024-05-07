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

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

TEST(CopyHeaderRecordsFrom, CopyHeader) {
  HeaderRecord src(SQL_DESC_ALLOC_AUTO);
  src.array_size = 3;
  SQLUSMALLINT array_status[3];
  src.array_status_ptr = array_status;
  SQLLEN bind_offset = 5;
  src.bind_offset_ptr = &bind_offset;
  src.bind_type = 11;
  src.count = 10;
  SQLULEN rows_processed = 5;
  src.rows_processed_ptr = &rows_processed;
  HeaderRecord target(SQL_DESC_ALLOC_USER);

  target.CopyHeaderRecordsFrom(src);

  EXPECT_NE(src.GetAllocType(), target.GetAllocType());
  EXPECT_NE(src.count, target.count);
  EXPECT_EQ(src.array_size, target.array_size);
  EXPECT_EQ(src.array_status_ptr, target.array_status_ptr);
  EXPECT_EQ(src.bind_offset_ptr, target.bind_offset_ptr);
  EXPECT_EQ(src.bind_type, target.bind_type);
  EXPECT_EQ(src.rows_processed_ptr, target.rows_processed_ptr);
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

TEST(SetNumPrecRadix, SetForNonNumericValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetNumPrecRadix(kNumPrecRadixForNonNumeric);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(kNumPrecRadixForNonNumeric, descriptor_record.num_prec_radix);
}

TEST(SetNumPrecRadix, SetForApproximateNumericValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetNumPrecRadix(kNumPrecRadixForApproximateNumeric);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(kNumPrecRadixForApproximateNumeric,
            descriptor_record.num_prec_radix);
}

TEST(SetNumPrecRadix, SetForExactNumericValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetNumPrecRadix(kNumPrecRadixForExactNumeric);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(kNumPrecRadixForExactNumeric, descriptor_record.num_prec_radix);
}

TEST(SetNumPrecRadix, Fails_InvalidValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetNumPrecRadix(8);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY092(), status_record.sql_state);
}

TEST(SetParameterType, Set_SQL_PARAM_INPUT) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetParameterType(SQL_PARAM_INPUT);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_PARAM_INPUT, descriptor_record.parameter_type);
}

TEST(SetParameterType, Set_SQL_PARAM_INPUT_OUTPUT) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetParameterType(SQL_PARAM_INPUT_OUTPUT);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_PARAM_INPUT_OUTPUT, descriptor_record.parameter_type);
}

TEST(SetParameterType, Set_SQL_PARAM_OUTPUT) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetParameterType(SQL_PARAM_OUTPUT);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_PARAM_OUTPUT, descriptor_record.parameter_type);
}

TEST(SetParameterType, Fails_InvalidValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetParameterType(111);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY105(), status_record.sql_state);
}

TEST(SetUnnamed, Set_SQL_UNNAMED) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetUnnamed(SQL_UNNAMED);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_UNNAMED, descriptor_record.unnamed);
}

TEST(SetUnnamed, Fails_InvalidValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetUnnamed(111);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY091(), status_record.sql_state);
}

TEST(SetType, FailsToSet_SQL_INTERVAL_NoIntervalCodeSet) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY021(), status_record.sql_state);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_MONTH) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_MONTH;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_MONTH, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_YEAR) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_YEAR;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_YEAR, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_YEAR_TO_MONTH) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_YEAR_TO_MONTH;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_YEAR_TO_MONTH, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_DAY) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_DAY;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_HOUR) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_HOUR;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_HOUR, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_MINUTE) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_MINUTE;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_MINUTE, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_SECOND) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_SECOND;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_DAY_TO_HOUR) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_DAY_TO_HOUR;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY_TO_HOUR, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_DAY_TO_MINUTE) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_DAY_TO_MINUTE;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY_TO_MINUTE, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_DAY_TO_SECOND) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_DAY_TO_SECOND;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY_TO_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_HOUR_TO_MINUTE) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_HOUR_TO_MINUTE;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_HOUR_TO_MINUTE, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_HOUR_TO_SECOND) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_HOUR_TO_SECOND;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_HOUR_TO_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, Set_SQL_INTERVAL_With_SQL_CODE_MINUTE_TO_SECOND) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_MINUTE_TO_SECOND;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_INTERVAL, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_MINUTE_TO_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetType, FailsToSet_SQL_DATETIME_NoIntervalCodeSet) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_DATETIME, DescriptorType::kApplication);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY021(), status_record.sql_state);
}

TEST(SetType, Set_SQL_DATETIME_With_SQL_CODE_DATE) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_DATE;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_DATETIME, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_DATETIME, descriptor_record.type);
  EXPECT_EQ(SQL_C_TYPE_DATE, descriptor_record.concise_type);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(0, descriptor_record.length);
}

TEST(SetType, Set_SQL_DATETIME_With_SQL_CODE_TIME) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_TIME;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_DATETIME, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_DATETIME, descriptor_record.type);
  EXPECT_EQ(SQL_C_TYPE_TIME, descriptor_record.concise_type);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(0, descriptor_record.length);
}

TEST(SetType, Set_SQL_DATETIME_With_SQL_CODE_TIMESTAMP) {
  DescriptorRecord descriptor_record;
  descriptor_record.datetime_interval_code = SQL_CODE_TIMESTAMP;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_DATETIME, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_DATETIME, descriptor_record.type);
  EXPECT_EQ(SQL_C_TYPE_TIMESTAMP, descriptor_record.concise_type);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(0, descriptor_record.length);
}

TEST(SetType, Set_SQL_C_CHAR) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_C_CHAR, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_CHAR, descriptor_record.type);
  EXPECT_EQ(SQL_C_CHAR, descriptor_record.concise_type);
  EXPECT_EQ(1, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(1, descriptor_record.precision);
  EXPECT_EQ(1, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetType, Set_SQL_C_NUMERIC) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_C_NUMERIC, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_NUMERIC, descriptor_record.type);
  EXPECT_EQ(SQL_C_NUMERIC, descriptor_record.concise_type);
  EXPECT_EQ(38, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(38, descriptor_record.precision);
  EXPECT_EQ(38, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetType, Set_SQL_C_FLOAT) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_C_FLOAT, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_FLOAT, descriptor_record.type);
  EXPECT_EQ(SQL_C_FLOAT, descriptor_record.concise_type);
  EXPECT_EQ(24, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(24, descriptor_record.precision);
  EXPECT_EQ(24, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetType, Set_SQL_C_DOUBLE) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_C_DOUBLE, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_DOUBLE, descriptor_record.type);
  EXPECT_EQ(SQL_C_DOUBLE, descriptor_record.concise_type);
  EXPECT_EQ(53, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(53, descriptor_record.precision);
  EXPECT_EQ(53, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetType, Set_SQL_C_BIT) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_C_BIT, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_BIT, descriptor_record.type);
  EXPECT_EQ(SQL_C_BIT, descriptor_record.concise_type);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetType, Set_SQL_C_GUID) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_C_GUID, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_GUID, descriptor_record.type);
  EXPECT_EQ(SQL_C_GUID, descriptor_record.concise_type);
  EXPECT_EQ(16, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(16, descriptor_record.precision);
  EXPECT_EQ(16, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetType, FailsToSet_SQL_FLOAT_NotValidValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetType(SQL_FLOAT, DescriptorType::kApplication);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY021(), status_record.sql_state);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_MONTH) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_MONTH, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_MONTH, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_MONTH, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_YEAR) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_YEAR, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_YEAR, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_YEAR, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_YEAR_TO_MONTH) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_YEAR_TO_MONTH, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_YEAR_TO_MONTH, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_YEAR_TO_MONTH, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_DAY) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_DAY, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_DAY, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_HOUR) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_HOUR, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_HOUR, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_HOUR, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_MINUTE) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_MINUTE, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_MINUTE, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_MINUTE, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_SECOND) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_SECOND, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_SECOND, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_DAY_TO_HOUR) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_DAY_TO_HOUR, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY_TO_HOUR, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_DAY_TO_HOUR, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_DAY_TO_MINUTE) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_DAY_TO_MINUTE, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY_TO_MINUTE, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_DAY_TO_MINUTE, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_DAY_TO_SECOND) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_DAY_TO_SECOND, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_DAY_TO_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_DAY_TO_SECOND, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_HOUR_TO_MINUTE) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_HOUR_TO_MINUTE, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_HOUR_TO_MINUTE, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_HOUR_TO_MINUTE, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_HOUR_TO_SECOND) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_HOUR_TO_SECOND, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_HOUR_TO_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_HOUR_TO_SECOND, descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_INTERVAL_MINUTE_TO_SECOND) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_INTERVAL_MINUTE_TO_SECOND, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_INTERVAL, descriptor_record.type);
  EXPECT_EQ(SQL_C_INTERVAL_MINUTE_TO_SECOND, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_MINUTE_TO_SECOND,
            descriptor_record.datetime_interval_code);
  EXPECT_EQ(2, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(2, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_TYPE_DATE) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_TYPE_DATE, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_DATETIME, descriptor_record.type);
  EXPECT_EQ(SQL_C_TYPE_DATE, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_DATE, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(0, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_TYPE_TIME) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_TYPE_TIME, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_DATETIME, descriptor_record.type);
  EXPECT_EQ(SQL_C_TYPE_TIME, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_TIME, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.scale);
  EXPECT_EQ(0, descriptor_record.length);
}

TEST(SetConciseType, Set_SQL_C_TYPE_TIMESTAMP) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_TYPE_TIMESTAMP, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_DATETIME, descriptor_record.type);
  EXPECT_EQ(SQL_C_TYPE_TIMESTAMP, descriptor_record.concise_type);
  EXPECT_EQ(SQL_CODE_TIMESTAMP, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(6, descriptor_record.precision);
  EXPECT_EQ(6, descriptor_record.scale);
  EXPECT_EQ(0, descriptor_record.length);
}

TEST(SetConciseType, Set_C_SQL_CHAR) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_CHAR, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_CHAR, descriptor_record.type);
  EXPECT_EQ(SQL_C_CHAR, descriptor_record.concise_type);
  EXPECT_EQ(1, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(1, descriptor_record.precision);
  EXPECT_EQ(1, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetConciseType, Set_SQL_C_NUMERIC) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_NUMERIC, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_NUMERIC, descriptor_record.type);
  EXPECT_EQ(SQL_C_NUMERIC, descriptor_record.concise_type);
  EXPECT_EQ(38, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(38, descriptor_record.precision);
  EXPECT_EQ(38, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetConciseType, Set_SQL_C_FLOAT) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_FLOAT, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_FLOAT, descriptor_record.type);
  EXPECT_EQ(SQL_C_FLOAT, descriptor_record.concise_type);
  EXPECT_EQ(24, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(24, descriptor_record.precision);
  EXPECT_EQ(24, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetConciseType, Set_SQL_C_DOUBLE) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_DOUBLE, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_DOUBLE, descriptor_record.type);
  EXPECT_EQ(SQL_C_DOUBLE, descriptor_record.concise_type);
  EXPECT_EQ(53, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(53, descriptor_record.precision);
  EXPECT_EQ(53, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetConciseType, Set_SQL_C_BIT) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetConciseType(SQL_C_BIT, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_BIT, descriptor_record.type);
  EXPECT_EQ(SQL_C_BIT, descriptor_record.concise_type);
  EXPECT_EQ(0, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(0, descriptor_record.precision);
  EXPECT_EQ(0, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetConciseType, Set_SQL_C_GUID) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.SetConciseType(
      SQL_C_GUID, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(SQL_C_GUID, descriptor_record.type);
  EXPECT_EQ(SQL_C_GUID, descriptor_record.concise_type);
  EXPECT_EQ(16, descriptor_record.datetime_interval_precision);
  EXPECT_EQ(16, descriptor_record.precision);
  EXPECT_EQ(16, descriptor_record.length);
  EXPECT_EQ(0, descriptor_record.datetime_interval_code);
  EXPECT_EQ(0, descriptor_record.scale);
}

TEST(SetConciseType, FailsToSet_NotValidValue) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record =
      descriptor_record.SetConciseType(115, DescriptorType::kApplication);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY021(), status_record.sql_state);
}

TEST(ConsistencyCheck, Succeed_Default) {
  DescriptorRecord descriptor_record;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Succeed_Interval) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_INTERVAL;
  descriptor_record.concise_type = SQL_INTERVAL_MONTH;
  descriptor_record.datetime_interval_code = SQL_CODE_MONTH;
  descriptor_record.precision = 0;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Fails_Interval_WrongCode) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_INTERVAL;
  descriptor_record.concise_type = SQL_INTERVAL_MONTH;
  descriptor_record.datetime_interval_code = SQL_CODE_YEAR;
  descriptor_record.precision = 0;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_FALSE(status_record.ok());
}

TEST(ConsistencyCheck, Fails_Interval_WrongPrecision) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_INTERVAL;
  descriptor_record.concise_type = SQL_INTERVAL_MONTH;
  descriptor_record.datetime_interval_code = SQL_CODE_MONTH;
  descriptor_record.precision = 6;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_FALSE(status_record.ok());
}

TEST(ConsistencyCheck, Succeed_Datetime) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_DATETIME;
  descriptor_record.concise_type = SQL_TYPE_DATE;
  descriptor_record.datetime_interval_code = SQL_CODE_DATE;
  descriptor_record.precision = 0;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Fails_Datetime_WrongCode) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_DATETIME;
  descriptor_record.concise_type = SQL_TYPE_DATE;
  descriptor_record.datetime_interval_code = SQL_CODE_TIME;
  descriptor_record.precision = 0;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_FALSE(status_record.ok());
}

TEST(ConsistencyCheck, Fails_Datetime_WrongPrecision) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_DATETIME;
  descriptor_record.concise_type = SQL_TYPE_DATE;
  descriptor_record.datetime_interval_code = SQL_CODE_DATE;
  descriptor_record.precision = 6;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_FALSE(status_record.ok());
}

TEST(ConsistencyCheck, Succeed_SQL_DATE) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_DATETIME;
  descriptor_record.concise_type = SQL_DATE;
  descriptor_record.datetime_interval_code = SQL_CODE_DATE;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Succeed_SQL_TIME) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_DATETIME;
  descriptor_record.concise_type = SQL_TIME;
  descriptor_record.datetime_interval_code = SQL_CODE_TIME;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Succeed_SQL_TIMESTAMP) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_DATETIME;
  descriptor_record.concise_type = SQL_TIMESTAMP;
  descriptor_record.datetime_interval_code = SQL_CODE_TIMESTAMP;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Succeed_OtherTypes_SQL_NUMERIC) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_NUMERIC;
  descriptor_record.concise_type = SQL_NUMERIC;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Succeed_OtherTypes_SQL_CHAR) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_CHAR;
  descriptor_record.concise_type = SQL_CHAR;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_TRUE(status_record.ok());
}

TEST(ConsistencyCheck, Fails_OtherTypes_DifferentTypes) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = SQL_CHAR;
  descriptor_record.concise_type = SQL_NUMERIC;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_FALSE(status_record.ok());
}

TEST(ConsistencyCheck, Fails_InvalidType) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = 11111;
  descriptor_record.concise_type = 11111;

  StatusRecord status_record = descriptor_record.ConsistencyCheck();

  ASSERT_FALSE(status_record.ok());
}

TEST(SetDataPointer, SetPointer_ApplicationDescriptor) {
  DescriptorRecord descriptor_record;
  char buf[8];

  StatusRecord status_record =
      descriptor_record.SetDataPointer(&buf, DescriptorType::kApplication);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(buf, descriptor_record.data_ptr);
}

TEST(SetDataPointer, DoNotSetPointer_IPD) {
  DescriptorRecord descriptor_record;
  char buf[8];

  StatusRecord status_record =
      descriptor_record.SetDataPointer(&buf, DescriptorType::kIPD);

  ASSERT_TRUE(status_record.ok());
  EXPECT_EQ(nullptr, descriptor_record.data_ptr);
}

TEST(SetDataPointer, Fails_ConsistencyCheck_IPD) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = 11111;
  char buf[8];

  StatusRecord status_record =
      descriptor_record.SetDataPointer(&buf, DescriptorType::kIPD);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(nullptr, descriptor_record.data_ptr);
}

TEST(SetDataPointer, Fails_ConsistencyCheck_ApplicationDescriptor) {
  DescriptorRecord descriptor_record;
  descriptor_record.type = 11111;
  char buf[8];

  StatusRecord status_record =
      descriptor_record.SetDataPointer(&buf, DescriptorType::kApplication);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(nullptr, descriptor_record.data_ptr);
}

}  // namespace google::cloud::odbc_bq_driver_internal
