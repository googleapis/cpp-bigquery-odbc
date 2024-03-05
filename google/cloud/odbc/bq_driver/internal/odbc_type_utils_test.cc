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

#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

TEST(WriteStringToBufferOutput, Success_DestBufferLen_GT_SrcLen) {
  std::string expected = "sample-test";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 15;
  SQLCHAR dest[15];

  StatusRecord status_record =
      WriteStringToBufferOutput(expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("sample-test", actual);
  EXPECT_EQ(11, str_len);
}

TEST(WriteStringToBufferOutput, SuccessWithInfo_DestBufferLen_LT_SrcLen) {
  std::string expected = "sample-test";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 5;
  SQLCHAR dest[5];

  StatusRecord status_record =
      WriteStringToBufferOutput(expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("String data, right truncated", status_record.message);
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("samp", actual);
  EXPECT_EQ(11, str_len);
}

TEST(WriteStringToBufferOutput, Success_DestBufferLen_EQ_SrcLen) {
  std::string expected = "sampl";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 5;
  SQLCHAR dest[5];

  StatusRecord status_record =
      WriteStringToBufferOutput(expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("String data, right truncated", status_record.message);
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("samp", actual);
  EXPECT_EQ(5, str_len);
}

TEST(WriteStringToBufferOutput, Success_DestBufferLen_Zero) {
  std::string expected = "sample-test";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 0;
  SQLCHAR dest[15];

  StatusRecord status_record =
      WriteStringToBufferOutput(expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("", actual);
  EXPECT_EQ(11, str_len);
}

TEST(WriteStringToBufferOutput, Success_StcLenLen_Zero) {
  std::string expected = "";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 15;
  SQLCHAR dest[15];

  StatusRecord status_record =
      WriteStringToBufferOutput(expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("", actual);
  EXPECT_EQ(0, str_len);
}

TEST(WriteToBufferOutput, Success_SQLINTEGER) {
  int expected = 42;
  SQLSMALLINT str_len;
  SQLINTEGER dest[15];

  SQLRETURN return_code =
      WriteToBufferOutput<SQLINTEGER>(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(WriteToBufferOutput, Success_Dest_Null) {
  int expected = 42;
  SQLSMALLINT str_len;

  SQLRETURN return_code =
      WriteToBufferOutput<SQLINTEGER>(expected, nullptr, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(WriteToBufferOutput, Success_SQLLEN) {
  int expected = 42;
  SQLSMALLINT str_len;
  SQLLEN dest[15];

  SQLRETURN return_code = WriteToBufferOutput<SQLLEN>(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

}  // namespace google::cloud::odbc_bq_driver_internal
