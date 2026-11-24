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
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

TEST(StringValueToOutputBufferResponse,
     SuccessWhenDestBufferLenGreaterThanSrcLen) {
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
     SuccessWhenDestBufferLenGreaterThanSrcLenWithSqlIntegerOutputExplicit) {
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
     SuccessWhenDestBufferLenGreaterThanSrcLenWithSqlIntegerOutputImplicit) {
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
     SuccessWithInfoWhenDestBufferLenLessThanSrcLen) {
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
     SuccessWithInfoWhenDestBufferLenEqualsSrcLen) {
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

TEST(StringValueToOutputBufferResponse, SuccessWhenDestBufferLenIsZero) {
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

TEST(StringValueToOutputBufferResponse, SuccessWhenStcLenLenIsZero) {
  std::string expected;
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

TEST(StringValueToOutputBufferResponse, FailureWhenBufferLenIsNegative) {
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

TEST(IntValueToOutputBufferResponse, SuccessWithSqlInteger) {
  int expected = 42;
  SQLSMALLINT str_len;
  SQLINTEGER dest[15];

  SQLRETURN return_code =
      IntValueToOutputBufferResponse<SQLINTEGER>(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(IntValueToOutputBufferResponse, SuccessWhenDestIsNull) {
  int expected = 42;
  SQLSMALLINT str_len;

  SQLRETURN return_code =
      IntValueToOutputBufferResponse<SQLINTEGER>(expected, nullptr, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(sizeof(SQLINTEGER), str_len);
}

TEST(IntValueToOutputBufferResponse, SuccessWithSqlLen) {
  int expected = 42;
  SQLSMALLINT str_len;
  SQLLEN dest[15];

  SQLRETURN return_code =
      IntValueToOutputBufferResponse<SQLLEN>(expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(IntValueToOutputBufferResponse, SuccessWithSqlLenOutputSqlInteger) {
  int expected = 42;
  SQLINTEGER str_len;
  SQLLEN dest[15];

  SQLRETURN return_code = IntValueToOutputBufferResponse<SQLLEN, SQLINTEGER>(
      expected, dest, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(42, *dest);
  EXPECT_EQ(sizeof(SQLLEN), str_len);
}

TEST(IntValueToOutputBufferResponse, SuccessWithImplicitSqlLen) {
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
     SuccessWithImplicitSqlLenOutputSqlInteger) {
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

TEST(AddressToPointer, SetPointerToNull) {
  SQLSMALLINT* out_buf = nullptr;
  SQLINTEGER str_len = 0;

  SQLRETURN return_code = AddressToPointer(nullptr, &out_buf, &str_len);

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(nullptr, out_buf);
  EXPECT_EQ(sizeof(SQLPOINTER), str_len);
}

TEST(AddressToPointer, SetPointerToNullWhenWasNotNull) {
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

TEST(AddressToPointer, SetPointerNullStrLen) {
  SQLSMALLINT ptr[] = {1, 2, 3};
  SQLSMALLINT* out_buf = nullptr;

  SQLRETURN return_code =
      AddressToPointer(ptr, &out_buf, static_cast<SQLSMALLINT*>(nullptr));

  ASSERT_EQ(SQL_SUCCESS, return_code);
  EXPECT_EQ(ptr, out_buf);
  EXPECT_EQ(1, out_buf[0]);
  EXPECT_EQ(2, out_buf[1]);
  EXPECT_EQ(3, out_buf[2]);
}

TEST(WStrToOutputBufferResponse, SuccessWhenDestBufferLenGreaterThanSrcLen) {
  std::wstring expected = L"sample-test";
  SQLSMALLINT buffer_len = 15;
  SQLWCHAR dest[15];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected, dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_TRUE(status_record.ok());
  std::wstring actual = SQLWcharToWstring(dest);
  EXPECT_EQ(L"sample-test", actual);
  EXPECT_EQ(res_len, expected.size() * sizeof(SQLWCHAR));
}

TEST(WStrToOutputBufferResponse,
     SuccessWithInfoWhenDestBufferLenLessThanSrcLen) {
  std::wstring expected = L"sample-test";
  SQLSMALLINT buffer_len = 5;
  SQLWCHAR dest[5];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected, dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("Data truncated", status_record.message);
  std::wstring actual = SQLWcharToWstring(dest);
  EXPECT_EQ(L"samp", actual);
  EXPECT_EQ(res_len, (buffer_len * sizeof(SQLWCHAR)));
}

TEST(WStrToOutputBufferResponse, SuccessWithInfoWhenDestBufferLenEqualsSrcLen) {
  std::wstring expected = L"sampl";
  SQLSMALLINT buffer_len = 5;
  SQLWCHAR dest[5];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected, dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("Data truncated", status_record.message);
  std::wstring actual = SQLWcharToWstring(dest);
  EXPECT_EQ(L"samp", actual);
  EXPECT_EQ(res_len, (buffer_len * sizeof(SQLWCHAR)));
}

TEST(WStrToOutputBufferResponse, SuccessWhenStcLenLenIsZero) {
  std::wstring expected;
  SQLSMALLINT buffer_len = 15;
  SQLWCHAR dest[15];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected, dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_TRUE(status_record.ok());
  std::wstring actual = SQLWcharToWstring(dest);
  EXPECT_EQ(L"", actual);
  EXPECT_EQ(0, res_len);
}

TEST(WStrToOutputBufferResponse, FailureWhenBufferLenIsNegative) {
  std::wstring expected = L"sample-test";
  SQLSMALLINT buffer_len = -15;
  SQLWCHAR dest[15];
  SQLLEN res_len = 0;

  StatusRecord status_record = WStrToOutputBufferResponse(
      expected, dest, buffer_len, expected.size(), 0, &res_len);

  ASSERT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Buffer length is insufficient", status_record.message);
}

TEST(WriteWideToWireBufferTruncating, FitsWithNulTermination) {
  std::wstring src = L"BigQuery";
  std::vector<SQLWCHAR> dest(16, static_cast<SQLWCHAR>(0x7F7F));

  StatusRecord status =
      WriteWideToWireBufferTruncating(src, dest.data(), dest.size());

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(src, SQLWcharToWstring(dest.data()));
  EXPECT_EQ(static_cast<SQLWCHAR>(0), dest[src.size()]);
  EXPECT_EQ(static_cast<SQLWCHAR>(0x7F7F), dest[src.size() + 1]);
}

TEST(WriteWideToWireBufferTruncating,
     TruncatesAndNulTerminatesWhenEqualOrSmall) {
  std::wstring src = L"sample";
  std::vector<SQLWCHAR> dest(7, static_cast<SQLWCHAR>(0x5A5A));

  // Capacity == src.size() (6) leaves room for 5 chars + NUL; index 6 is
  // canary.
  StatusRecord status = WriteWideToWireBufferTruncating(src, dest.data(), 6);

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(SQLStates::k_01004(), status.sql_state);
  EXPECT_EQ(L"sampl", SQLWcharToWstring(dest.data()));
  EXPECT_EQ(static_cast<SQLWCHAR>(0), dest[5]);
  EXPECT_EQ(static_cast<SQLWCHAR>(0x5A5A), dest[6]);
}

TEST(WriteWideToWireBufferTruncating, NullAndZeroCapacityGuards) {
  std::wstring src = L"sample";
  EXPECT_TRUE(WriteWideToWireBufferTruncating(src, nullptr, 0).ok());
  EXPECT_TRUE(WriteWideToWireBufferTruncating(src, nullptr, 10).ok());

  auto canary = static_cast<SQLWCHAR>(0x1234);
  StatusRecord zero_cap = WriteWideToWireBufferTruncating(src, &canary, 0);
  ASSERT_FALSE(zero_cap.ok());
  EXPECT_EQ(SQLStates::k_01004(), zero_cap.sql_state);
  EXPECT_EQ(static_cast<SQLWCHAR>(0x1234), canary);

  std::vector<SQLWCHAR> one_cap(2, static_cast<SQLWCHAR>(0x1234));
  StatusRecord empty_ok =
      WriteWideToWireBufferTruncating(L"", one_cap.data(), 1);
  EXPECT_TRUE(empty_ok.ok());
  EXPECT_EQ(static_cast<SQLWCHAR>(0), one_cap[0]);
  EXPECT_EQ(static_cast<SQLWCHAR>(0x1234), one_cap[1]);
}

#ifndef _WIN32
TEST(WriteWideToWireBufferTruncating, Utf8WireEncodingRespectsMultibyteBounds) {
  struct EncodingGuard {
    ~EncodingGuard() { SetWcharEncodingFromConfig(""); }
  } guard;
  SetWcharEncodingFromConfig("UTF-8");

  // "a" (1B) + "é" (2B) + "東" (3B) = 6 UTF-8 bytes
  auto wstr_or = Utf8ToUtf16("a\xC3\xA9\xE6\x9D\xB1");
  ASSERT_TRUE(wstr_or.Ok());
  EXPECT_EQ(6U, WireCharCount(*wstr_or));

  // Capacity 5 bytes -> max 4 payload bytes; "東" starts at byte 3 (3B), so
  // step-back must stop after "aé" (3 bytes) + NUL at index 3, leaving canary
  // at index 5 untouched.
  std::vector<unsigned char> buf(8, 0xCC);
  StatusRecord status =
      WriteWideToWireBufferTruncating(*wstr_or, buf.data(), 5);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(SQLStates::k_01004(), status.sql_state);
  EXPECT_STREQ("a\xC3\xA9", reinterpret_cast<char const*>(buf.data()));
  EXPECT_EQ(0xCCU, buf[5]);
}

TEST(WriteWideToWireBufferTruncating, Utf16WireEncodingTruncatesAndTerminates) {
  struct EncodingGuard {
    ~EncodingGuard() { SetWcharEncodingFromConfig(""); }
  } guard;
  SetWcharEncodingFromConfig("UTF-16LE");

  std::wstring src = L"hello";
  EXPECT_EQ(5U, WireCharCount(src));
  std::vector<uint16_t> buf(5, 0xBEEF);
  StatusRecord status = WriteWideToWireBufferTruncating(src, buf.data(), 4);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(SQLStates::k_01004(), status.sql_state);
  EXPECT_EQ(static_cast<uint16_t>('h'), buf[0]);
  EXPECT_EQ(static_cast<uint16_t>('e'), buf[1]);
  EXPECT_EQ(static_cast<uint16_t>('l'), buf[2]);
  EXPECT_EQ(0U, buf[3]);
  EXPECT_EQ(0xBEEFU, buf[4]);
}
#endif

}  // namespace google::cloud::odbc_bq_driver_internal
