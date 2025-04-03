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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/testing/bq_driver_utils/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::optional;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_bq_driver_utils::CastToSQLCHAR;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;
using ::testing::StrEq;

TEST(GetSQLDataType, Date) {
  auto date_status = GetSQLDataType(SQL_TYPE_DATE);
  ASSERT_STATUS_RECORD_OK(date_status);
  optional<SQLSMALLINT> date_opt = *date_status;
  EXPECT_EQ(SQL_DATETIME, *date_opt);
}

TEST(GetSQLDataType, Time) {
  auto time_status = GetSQLDataType(SQL_TYPE_TIME);
  ASSERT_STATUS_RECORD_OK(time_status);
  optional<SQLSMALLINT> time_opt = *time_status;
  EXPECT_EQ(SQL_DATETIME, *time_opt);
}

TEST(GetSQLDataType, Timestamp) {
  auto timestamp_status = GetSQLDataType(SQL_TYPE_TIMESTAMP);
  ASSERT_STATUS_RECORD_OK(timestamp_status);
  optional<SQLSMALLINT> time_stamp_opt = *timestamp_status;
  EXPECT_EQ(SQL_DATETIME, *time_stamp_opt);
}

TEST(GetSQLDataType, String) {
  auto string_status = GetSQLDataType(SQL_VARCHAR);
  ASSERT_STATUS_RECORD_OK(string_status);
  EXPECT_EQ(SQL_VARCHAR, *string_status);
}

TEST(GetSQLDateTimeSub, Date) {
  auto date_status = GetSQLDateTimeSub(SQL_DATETIME, SQL_TYPE_DATE);
  ASSERT_STATUS_RECORD_OK(date_status);
  optional<SQLSMALLINT> date_opt = *date_status;
  EXPECT_EQ(SQL_CODE_DATE, *date_opt);
}

TEST(GetSQLDateTimeSub, Time) {
  auto time_status = GetSQLDateTimeSub(SQL_DATETIME, SQL_TYPE_TIME);
  ASSERT_STATUS_RECORD_OK(time_status);
  optional<SQLSMALLINT> time_opt = *time_status;
  EXPECT_EQ(SQL_CODE_TIME, *time_opt);
}

TEST(GetSQLDateTimeSub, TIMESTAMP) {
  auto timestamp_status = GetSQLDateTimeSub(SQL_DATETIME, SQL_TYPE_TIMESTAMP);
  ASSERT_STATUS_RECORD_OK(timestamp_status);
  optional<SQLSMALLINT> time_stamp_opt = *timestamp_status;
  EXPECT_EQ(SQL_CODE_TIMESTAMP, *time_stamp_opt);
}

TEST(GetSQLDateTimeSub, InvalidDateTimeSub) {
  auto invalid_status = GetSQLDateTimeSub(SQL_DATETIME, SQL_VARCHAR);
  EXPECT_THAT(invalid_status,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Invalid data_type for SQL_DATETIME")));
}

TEST(GetSQLDateTimeSub, Other) {
  auto other_status = GetSQLDateTimeSub(SQL_VARCHAR, SQL_VARCHAR);
  ASSERT_STATUS_RECORD_OK(other_status);
  EXPECT_FALSE(other_status->has_value());
}

TEST(GetRadix, Decimal) {
  TableFieldSchema schema;
  schema.precision = 0;
  schema.scale = 0;
  schema.type = "NUMERIC";
  auto radix_status = GetRadix(schema);
  ASSERT_STATUS_RECORD_OK(radix_status);
  optional<SQLSMALLINT> radix_opt = *radix_status;
  EXPECT_EQ(10, *radix_opt);
}

TEST(GetRadix, Binary) {
  TableFieldSchema schema;
  schema.precision = 53;
  schema.type = "INTEGER";
  auto radix_status = GetRadix(schema);
  ASSERT_STATUS_RECORD_OK(radix_status);
  optional<SQLSMALLINT> radix_opt = *radix_status;
  EXPECT_EQ(2, *radix_opt);
}

TEST(GetRadix, Null_Numeric) {
  TableFieldSchema schema;
  schema.type = "NUMERIC";
  auto radix_status = GetRadix(schema);
  ASSERT_STATUS_RECORD_OK(radix_status);
  optional<SQLSMALLINT> radix_opt = *radix_status;
  EXPECT_EQ(10, *radix_opt);
}

TEST(GetRadix, Null_String) {
  TableFieldSchema schema;
  schema.type = "STRING";
  auto radix_status = GetRadix(schema);
  ASSERT_STATUS_RECORD_OK(radix_status);
  optional<SQLSMALLINT> radix_opt = *radix_status;
  EXPECT_EQ(radix_opt, 10);
}

TEST(GetDecimalDigits, ScaleFromDS) {
  TableFieldSchema schema;
  schema.scale = 5;
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_EQ(5, *scale_opt);
}

TEST(GetDecimalDigits, FixedScaleString) {
  TableFieldSchema schema;
  schema.type = "STRING";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_FALSE(scale_opt.has_value());
}

TEST(GetDecimalDigits, FixedScaleInt) {
  TableFieldSchema schema;
  schema.type = "INT64";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_EQ(0, *scale_opt);
}

TEST(GetDecimalDigits, FixedScaleBool) {
  TableFieldSchema schema;
  schema.type = "BOOL";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_FALSE(scale_opt.has_value());
}

TEST(GetDecimalDigits, FixedScaleTime) {
  TableFieldSchema schema;
  schema.type = "TIME";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_EQ(6, *scale_opt);
}

TEST(GetDecimalDigits, FixedScaleDate) {
  TableFieldSchema schema;
  schema.type = "DATE";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_FALSE(scale_opt.has_value());
}

TEST(GetDecimalDigits, FixedScaleDateTime) {
  TableFieldSchema schema;
  schema.type = "DATETIME";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_EQ(6, *scale_opt);
}

TEST(GetDecimalDigits, FixedScaleTimeStamp) {
  TableFieldSchema schema;
  schema.type = "TIMESTAMP";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_EQ(6, *scale_opt);
}

TEST(GetDecimalDigits, FixedScaleNumeric) {
  TableFieldSchema schema;
  schema.type = "NUMERIC";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_EQ(9, *scale_opt);
}

TEST(GetDecimalDigits, FixedScaleBigNumeric) {
  TableFieldSchema schema;
  schema.type = "BIGNUMERIC";
  auto scale_status = GetDecimalDigits(schema);
  ASSERT_STATUS_RECORD_OK(scale_status);
  optional<SQLSMALLINT> scale_opt = *scale_status;
  EXPECT_EQ(38, *scale_opt);
}

TEST(GetDecimalDigits, InvalidType) {
  TableFieldSchema schema;
  schema.type = "Invalid";
  auto invalid_status = GetDecimalDigits(schema);
  EXPECT_THAT(invalid_status,
              StatusRecordIs(SQLStates::k_HY000(),
                             StrEq("Invalid Data Type: Invalid")));
}

TEST(GetColSize, PrecisionFromDS) {
  TableFieldSchema schema;
  schema.precision = 5;
  auto col_size_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(col_size_status);
  optional<SQLINTEGER> col_size_opt = *col_size_status;
  EXPECT_EQ(5, *col_size_opt);
}

TEST(GetColSize, MaxLengthFromDS) {
  TableFieldSchema schema;
  schema.max_length = 5;
  auto col_size_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(col_size_status);
  optional<SQLINTEGER> col_size_opt = *col_size_status;
  EXPECT_EQ(5, *col_size_opt);
}

TEST(GetColSize, FixedPrecisionString) {
  TableFieldSchema schema;
  schema.type = "STRING";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(16384, *prec_opt);
}

TEST(GetColSize, FixedPrecisionInt) {
  TableFieldSchema schema;
  schema.type = "INT64";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(19, *prec_opt);
}

TEST(GetColSize, FixedPrecisionBool) {
  TableFieldSchema schema;
  schema.type = "BOOL";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(1, *prec_opt);
}

TEST(GetColSize, FixedPrecisionTime) {
  TableFieldSchema schema;
  schema.type = "TIME";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(15, *prec_opt);
}

TEST(GetColSize, FixedPrecisionDate) {
  TableFieldSchema schema;
  schema.type = "DATE";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(10, *prec_opt);
}

TEST(GetColSize, FixedPrecisionDateTime) {
  TableFieldSchema schema;
  schema.type = "DATETIME";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(26, *prec_opt);
}

TEST(GetColSize, FixedPrecisionTimeStamp) {
  TableFieldSchema schema;
  schema.type = "TIMESTAMP";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(26, *prec_opt);
}

TEST(GetColSize, FixedPrecisionNumeric) {
  TableFieldSchema schema;
  schema.type = "NUMERIC";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(38, *prec_opt);
}

TEST(GetColSize, FixedPrecisionBigNumeric) {
  TableFieldSchema schema;
  schema.type = "BIGNUMERIC";
  auto precision_status = GetColSize(schema);
  ASSERT_STATUS_RECORD_OK(precision_status);
  optional<SQLINTEGER> prec_opt = *precision_status;
  EXPECT_EQ(77, *prec_opt);
}

TEST(GetColSize, InvalidType) {
  TableFieldSchema schema;
  schema.type = "Invalid";
  auto invalid_status = GetColSize(schema);
  EXPECT_THAT(invalid_status,
              StatusRecordIs(SQLStates::k_HY000(),
                             StrEq("Invalid Data Type: Invalid")));
}

TEST(GetBufferLen, BufferLenFromDS_WithMaxLen) {
  TableFieldSchema schema;
  schema.max_length = 5000;
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(5000, *buf_len_opt);
}

TEST(GetBufferLen, BufferLenFromDS_WithPrecision) {
  TableFieldSchema schema;
  schema.precision = 20;
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(22, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenString) {
  TableFieldSchema schema;
  schema.type = "STRING";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(16384, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenInt) {
  TableFieldSchema schema;
  schema.type = "INT64";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(20, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenBool) {
  TableFieldSchema schema;
  schema.type = "BOOL";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(1, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenTime) {
  TableFieldSchema schema;
  schema.type = "TIME";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(6, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenDate) {
  TableFieldSchema schema;
  schema.type = "DATE";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(6, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenDateTime) {
  TableFieldSchema schema;
  schema.type = "DATETIME";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(16, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenTimeStamp) {
  TableFieldSchema schema;
  schema.type = "TIMESTAMP";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(16, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenNumeric) {
  TableFieldSchema schema;
  schema.type = "NUMERIC";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(40, *buf_len_opt);
}

TEST(GetBufferLen, FixedBufferLenBigNumeric) {
  TableFieldSchema schema;
  schema.type = "BIGNUMERIC";
  auto buf_len_status = GetBufferLen(schema);
  ASSERT_STATUS_RECORD_OK(buf_len_status);
  optional<SQLINTEGER> buf_len_opt = *buf_len_status;
  EXPECT_EQ(79, *buf_len_opt);
}

TEST(GetBufferLen, InvalidType) {
  TableFieldSchema schema;
  schema.type = "Invalid";
  auto invalid_status = GetBufferLen(schema);
  EXPECT_THAT(invalid_status,
              StatusRecordIs(SQLStates::k_HY000(),
                             StrEq("Invalid Data Type: Invalid")));
}

TEST(GetCharOctetLen, CharOctetLenFromDS) {
  TableFieldSchema schema;
  schema.max_length = 5000;
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_EQ(5000, *char_octet_len_opt);
}

TEST(GetCharOctetLen, FixedCharOctetLenString) {
  TableFieldSchema schema;
  schema.type = "STRING";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_EQ(16384, *char_octet_len_opt);
}

TEST(GetCharOctetLen, FixedCharOctetLenInt) {
  TableFieldSchema schema;
  schema.type = "INT64";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_FALSE(char_octet_len_opt.has_value());
}

TEST(GetCharOctetLen, FixedCharOctetLenBool) {
  TableFieldSchema schema;
  schema.type = "BOOL";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_EQ(16384, char_octet_len_opt);
}

TEST(GetCharOctetLen, FixedCharOctetLenTime) {
  TableFieldSchema schema;
  schema.type = "TIME";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_FALSE(char_octet_len_opt.has_value());
}

TEST(GetCharOctetLen, FixedCharOctetLenDate) {
  TableFieldSchema schema;
  schema.type = "DATE";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_FALSE(char_octet_len_opt.has_value());
}

TEST(GetCharOctetLen, FixedCharOctetLenDateTime) {
  TableFieldSchema schema;
  schema.type = "DATETIME";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_EQ(16384, char_octet_len_opt);
}

TEST(GetCharOctetLen, FixedCharOctetLenTimeStamp) {
  TableFieldSchema schema;
  schema.type = "TIMESTAMP";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_EQ(16384, char_octet_len_opt);
}

TEST(GetCharOctetLen, FixedCharOctetLenNumeric) {
  TableFieldSchema schema;
  schema.type = "NUMERIC";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_FALSE(char_octet_len_opt.has_value());
}

TEST(GetCharOctetLen, FixedCharOctetLenBigNumeric) {
  TableFieldSchema schema;
  schema.type = "BIGNUMERIC";
  auto char_octet_len_status = GetCharOctetLen(schema);
  ASSERT_STATUS_RECORD_OK(char_octet_len_status);
  optional<SQLINTEGER> char_octet_len_opt = *char_octet_len_status;
  EXPECT_FALSE(char_octet_len_opt.has_value());
}

TEST(GetCharOctetLen, InvalidType) {
  TableFieldSchema schema;
  schema.type = "Invalid";
  auto invalid_status = GetCharOctetLen(schema);
  EXPECT_THAT(invalid_status,
              StatusRecordIs(SQLStates::k_HY000(),
                             StrEq("Invalid Data Type: Invalid")));
}

TEST(ValidateColumnParameters, Success_MetadataId_TRUE) {
  StatusRecord status = ValidateColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, CastToSQLCHAR("column"), 6, SQL_TRUE);

  EXPECT_TRUE(status.ok());
}

TEST(ValidateColumnParameters, Success_MetadataId_FALSE) {
  StatusRecord status = ValidateColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, CastToSQLCHAR("column"), 6, SQL_FALSE);

  EXPECT_TRUE(status.ok());
}

TEST(ValidateColumnParameters, Success_EmptyColumn) {
  StatusRecord status = ValidateColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, CastToSQLCHAR(""), 0, SQL_FALSE);

  EXPECT_TRUE(status.ok());
}

TEST(ValidateColumnParameters, Failure_ColumnNameLengthNegative) {
  StatusRecord status = ValidateColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, CastToSQLCHAR("column"), -6, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message, HasSubstr("column name length is invalid"));
}

TEST(ValidateColumnParameters,
     Failure_CatalogNameIsSearchPattern_MetadataId_TRUE) {
  StatusRecord status = ValidateColumnParameters(
      CastToSQLCHAR("project%"), 8, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, CastToSQLCHAR("column"), 6, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message,
              HasSubstr("Catalog name cannot be a search pattern"));
}

TEST(ValidateColumnParameters,
     Failure_CatalogNameIsSearchPattern_MetadataId_FALSE) {
  StatusRecord status = ValidateColumnParameters(
      CastToSQLCHAR("project%"), 8, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, CastToSQLCHAR("column"), 6, SQL_FALSE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message,
              HasSubstr("Catalog name cannot be a search pattern"));
}

TEST(GetTypeDescription, TypeDescriptionDiffThanType_Integer) {
  auto type_status = GetTypeDescription("INTEGER");
  ASSERT_STATUS_RECORD_OK(type_status);
  ASSERT_EQ("INT64", *type_status);
}

TEST(GetTypeDescription, TypeDescriptionDiffThanType_Bool) {
  auto type_status = GetTypeDescription("BOOLEAN");
  ASSERT_STATUS_RECORD_OK(type_status);
  ASSERT_EQ("BOOL", *type_status);
}

TEST(GetTypeDescription, TypeDescriptionSameAsType_Time) {
  auto type_status = GetTypeDescription("TIME");
  ASSERT_STATUS_RECORD_OK(type_status);
  ASSERT_EQ("TIME", *type_status);
}
}  // namespace google::cloud::odbc_bq_driver_internal
