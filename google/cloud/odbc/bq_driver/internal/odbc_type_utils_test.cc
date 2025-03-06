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

TEST(StringValueToOutputBufferResponse, Success_DestBufferLen_GT_SrcLen) {
  std::string expected = "sample-test";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 15;
  SQLCHAR dest[15];

  StatusRecord status_record = StringValueToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("sample-test", actual);
  EXPECT_EQ(11, str_len);
}

TEST(StringValueToOutputBufferResponse,
     Success_DestBufferLen_GT_SrcLen_Output_SQLINTEGER_Explicit) {
  std::string expected = "sample-test";
  SQLINTEGER str_len;
  SQLINTEGER buffer_len = 15;
  SQLCHAR dest[15];

  StatusRecord status_record = StringValueToOutputBufferResponse<SQLINTEGER>(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("sample-test", actual);
  EXPECT_EQ(11, str_len);
}

TEST(StringValueToOutputBufferResponse,
     Success_DestBufferLen_GT_SrcLen_Output_SQLINTEGER_Implicit) {
  std::string expected = "sample-test";
  SQLINTEGER str_len;
  SQLINTEGER buffer_len = 15;
  SQLCHAR dest[15];

  StatusRecord status_record = StringValueToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("sample-test", actual);
  EXPECT_EQ(11, str_len);
}

TEST(StringValueToOutputBufferResponse,
     SuccessWithInfo_DestBufferLen_LT_SrcLen) {
  std::string expected = "sample-test";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 5;
  SQLCHAR dest[5];

  StatusRecord status_record = StringValueToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("String data, right truncated", status_record.message);
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("samp", actual);
  EXPECT_EQ(4, str_len);
}

TEST(StringValueToOutputBufferResponse,
     SuccessWithInfo_DestBufferLen_EQ_SrcLen) {
  std::string expected = "sampl";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 5;
  SQLCHAR dest[5];

  StatusRecord status_record = StringValueToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("String data, right truncated", status_record.message);
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("samp", actual);
  EXPECT_EQ(4, str_len);
}

TEST(StringValueToOutputBufferResponse, Success_DestBufferLen_Zero) {
  std::string expected = "sample-test";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 0;
  SQLCHAR dest[15];

  StatusRecord status_record = StringValueToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("", actual);
  EXPECT_EQ(0, str_len);
}

TEST(StringValueToOutputBufferResponse, Success_StcLenLen_Zero) {
  std::string expected = "";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 15;
  SQLCHAR dest[15];

  StatusRecord status_record = StringValueToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_TRUE(status_record.ok());
  std::string actual = reinterpret_cast<char*>(dest);
  EXPECT_EQ("", actual);
  EXPECT_EQ(0, str_len);
}

TEST(StringValueToOutputBufferResponse, Failure_BufferLen_Negative) {
  std::string expected = "sample-test";
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = -15;
  SQLCHAR dest[15];

  StatusRecord status_record = StringValueToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, &str_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_HY090(), status_record.sql_state);
  EXPECT_EQ("Buffer length is negative", status_record.message);
  EXPECT_EQ(11, str_len);
}

TEST(IntValueToOutputBufferResponse, Success_SQLINTEGER) {
  int expected = 42;
  SQLSMALLINT str_len;
  SQLINTEGER dest[15];

  SQLRETURN return_code =
      IntValueToOutputBufferResponse<SQLINTEGER>(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(IntValueToOutputBufferResponse, Success_Dest_Null) {
  int expected = 42;
  SQLSMALLINT str_len;

  SQLRETURN return_code =
      IntValueToOutputBufferResponse<SQLINTEGER>(expected, nullptr, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(IntValueToOutputBufferResponse, Success_SQLLEN) {
  int expected = 42;
  SQLSMALLINT str_len;
  SQLLEN dest[15];

  SQLRETURN return_code =
      IntValueToOutputBufferResponse<SQLLEN>(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(IntValueToOutputBufferResponse, Success_SQLLEN_Output_SQLINTEGER) {
  int expected = 42;
  SQLINTEGER str_len;
  SQLLEN dest[15];

  SQLRETURN return_code = IntValueToOutputBufferResponse<SQLLEN, SQLINTEGER>(
      expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(IntValueToOutputBufferResponse, Success_Implicit_SQLLEN) {
  SQLLEN expected = 42;
  SQLSMALLINT str_len;
  SQLLEN dest[15];

  SQLRETURN return_code =
      IntValueToOutputBufferResponse(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(IntValueToOutputBufferResponse,
     Success_Implicit_SQLLEN_Output_SQLINTEGER) {
  SQLLEN expected = 42;
  SQLINTEGER str_len;
  SQLLEN dest[15];

  SQLRETURN return_code =
      IntValueToOutputBufferResponse(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(AddressToPointer, SetPointer) {
  SQLSMALLINT ptr[] = {1, 2, 3};
  SQLSMALLINT* out_buf = nullptr;
  SQLINTEGER str_len = 0;

  SQLRETURN return_code = AddressToPointer(ptr, &out_buf, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(ptr, out_buf);
  EXPECT_EQ(1, out_buf[0]);
  EXPECT_EQ(2, out_buf[1]);
  EXPECT_EQ(3, out_buf[2]);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(AddressToPointer, SetPointer_Null) {
  SQLSMALLINT* out_buf = nullptr;
  SQLINTEGER str_len = 0;

  SQLRETURN return_code = AddressToPointer(nullptr, &out_buf, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(nullptr, out_buf);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(AddressToPointer, SetPointer_Null_WasNotNull) {
  SQLSMALLINT value = 5;
  SQLSMALLINT* out_buf = &value;
  SQLINTEGER str_len = 0;

  SQLRETURN return_code = AddressToPointer(nullptr, &out_buf, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(nullptr, out_buf);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(AddressToPointer, DoNotSetPointerToNull) {
  SQLSMALLINT ptr[] = {1, 2, 3};
  SQLINTEGER str_len = 0;

  SQLRETURN return_code = AddressToPointer(ptr, nullptr, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(AddressToPointer, SetPointer_NullStrLen) {
  SQLSMALLINT ptr[] = {1, 2, 3};
  SQLSMALLINT* out_buf = nullptr;

  SQLRETURN return_code = AddressToPointer(ptr, &out_buf, nullptr);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(ptr, out_buf);
  EXPECT_EQ(1, out_buf[0]);
  EXPECT_EQ(2, out_buf[1]);
  EXPECT_EQ(3, out_buf[2]);
}

TEST(WStrToOutputBufferResponse, Success_DestBufferLen_GT_SrcLen) {
  std::wstring expected = L"sample-test";
  SQLSMALLINT buffer_len = 15;
  SQLWCHAR dest[15];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_TRUE(status_record.ok());
  std::wstring actual(dest);
  EXPECT_EQ(L"sample-test", actual);
  EXPECT_EQ(res_len, expected.size() * sizeof(SQLWCHAR));
}

TEST(WStrToOutputBufferResponse, SuccessWithInfo_DestBufferLen_LT_SrcLen) {
  std::wstring expected = L"sample-test";
  SQLSMALLINT buffer_len = 5;
  SQLWCHAR dest[5];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("Data truncated", status_record.message);
  std::wstring actual = reinterpret_cast<SQLWCHAR*>(dest);
  EXPECT_EQ(L"samp", actual);
  EXPECT_EQ(res_len, (buffer_len * sizeof(SQLWCHAR)));
}

TEST(WStrToOutputBufferResponse, SuccessWithInfo_DestBufferLen_EQ_SrcLen) {
  std::wstring expected = L"sampl";
  SQLSMALLINT buffer_len = 5;
  SQLWCHAR dest[5];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("Data truncated", status_record.message);
  std::wstring actual = reinterpret_cast<SQLWCHAR*>(dest);
  EXPECT_EQ(L"samp", actual);
  EXPECT_EQ(res_len, (buffer_len * sizeof(SQLWCHAR)));
}

TEST(WStrToOutputBufferResponse, Success_StcLenLen_Zero) {
  std::wstring expected = L"";
  SQLSMALLINT buffer_len = 15;
  SQLWCHAR dest[15];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_TRUE(status_record.ok());
  std::wstring actual = reinterpret_cast<SQLWCHAR*>(dest);
  EXPECT_EQ(L"", actual);
  EXPECT_EQ(0, res_len);
}

TEST(WStrToOutputBufferResponse, Failure_BufferLen_Negative) {
  std::wstring expected = L"sample-test";
  SQLSMALLINT buffer_len = -15;
  SQLWCHAR dest[15];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected.c_str(), dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Buffer length is insufficient", status_record.message);
}

}  // namespace google::cloud::odbc_bq_driver_internal
