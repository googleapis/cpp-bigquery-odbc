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

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

TEST(CheckLimitsArithmetic, Basic) {
  StatusRecord status_record;
  status_record = CheckLimitsArithmetic<int, double>(100);
  EXPECT_TRUE(status_record.ok());

  status_record = CheckLimitsArithmetic<double, int>(100.0);
  EXPECT_TRUE(status_record.ok());

  status_record = CheckLimitsArithmetic<double, int>(100.5);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01S07(), status_record.sql_state);
  EXPECT_EQ("Fractional truncation", status_record.message);

  status_record = CheckLimitsArithmetic<int64_t, int>(100);
  EXPECT_TRUE(status_record.ok());

  status_record = CheckLimitsArithmetic<int64_t, int>(9223372036854775807);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Numeric value out of range", status_record.message);

  status_record = CheckLimitsArithmetic<double, int>(922337203685477.1);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Numeric value out of range", status_record.message);

  status_record = CheckLimitsArithmetic<double, int>(92233720.1);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01S07(), status_record.sql_state);
  EXPECT_EQ("Fractional truncation", status_record.message);

  status_record = CheckLimitsArithmetic<bool, int>(true);
  EXPECT_TRUE(status_record.ok());
}

template <typename SrcType, typename DestType>
void FromArithmeticToArithmeticTest(SrcType src_val, DestType expected_val,
                                    SQLSMALLINT dest_type,
                                    std::string expected_state = "",
                                    std::string expected_message = "") {
  SQLPOINTER buf = malloc(50);
  DataBuffer data = {dest_type, buf, 50, nullptr};
  DSValue ds_value;

  ArithmeticToDSValue<SrcType>(src_val, ds_value);
  StatusRecord status_record =
      ConvertFromArithmeticDSValue<SrcType>(ds_value, data);
  if (expected_state.empty() || expected_state == SQLStates::k_01S07()) {
    DestType* returned_val = (DestType*)data.buf;
    EXPECT_EQ(*returned_val, expected_val);
    EXPECT_EQ(expected_message, status_record.message);
  } else {
    EXPECT_EQ(expected_state, status_record.sql_state);
    EXPECT_EQ(expected_message, status_record.message);
  }

  free(buf);
}

template <typename SrcType>
void FromArithmeticToStringTest(SrcType src_val, std::string expected_val,
                                SQLSMALLINT dest_type,
                                std::string expected_state = "",
                                std::string expected_message = "") {
  SQLPOINTER buf = malloc(50);
  DataBuffer data = {dest_type, buf, 50, nullptr};
  DSValue ds_value;

  ArithmeticToDSValue<SrcType>(src_val, ds_value);
  StatusRecord status_record =
      ConvertFromArithmeticDSValue<SrcType>(ds_value, data);
  if (expected_state.empty() || expected_state == SQLStates::k_01S07()) {
    std::string returned_val = (char*)(SQLCHAR*)data.buf;
    if constexpr (std::is_same_v<SrcType, SQLCHAR*>) {
      EXPECT_EQ(returned_val, expected_val);
    } else if constexpr (std::is_same_v<SrcType, float>) {
      EXPECT_EQ(std::stof(returned_val), std::stof(expected_val));
    } else if constexpr (std::is_same_v<SrcType, double>) {
      EXPECT_EQ(std::stod(returned_val), std::stod(expected_val));
    } else if constexpr (std::is_same_v<SrcType, int>) {
      EXPECT_EQ(std::stoi(returned_val), std::stoi(expected_val));
    } else if constexpr (std::is_same_v<SrcType, long>) {
      EXPECT_EQ(std::stol(returned_val), std::stol(expected_val));
    } else {
      EXPECT_EQ(std::stod(returned_val), std::stod(expected_val));
    }
    EXPECT_EQ(expected_message, status_record.message);
  } else {
    EXPECT_EQ(expected_state, status_record.sql_state);
    EXPECT_EQ(expected_message, status_record.message);
  }

  free(buf);
}

TEST(ConvertFromArithmeticDSValue, To_SQL_C_FLOAT) {
  FromArithmeticToArithmeticTest<int64_t, SQLREAL>(42, 42, SQL_C_FLOAT);
  FromArithmeticToArithmeticTest<int64_t, SQLREAL>(
      922337203437347, 922337203437347, SQL_C_FLOAT);
  FromArithmeticToArithmeticTest<double, SQLREAL>(42.1, 42.1, SQL_C_FLOAT);
  FromArithmeticToArithmeticTest<double, SQLREAL>(-1.1, -1.1, SQL_C_FLOAT);
}

TEST(ConvertFromArithmeticDSValue, To_SQL_C_SSHORT) {
  FromArithmeticToArithmeticTest<int64_t, SQLSMALLINT>(42, 42, SQL_C_SSHORT);
  FromArithmeticToArithmeticTest<int64_t, SQLSMALLINT>(
      92233720368547, 1111, SQL_C_SSHORT, SQLStates::k_22003(),
      "Numeric value out of range");
  FromArithmeticToArithmeticTest<int64_t, SQLSMALLINT>(-13, -13, SQL_C_SSHORT);
  FromArithmeticToArithmeticTest<float, SQLSMALLINT>(
      1.1, 1, SQL_C_SSHORT, SQLStates::k_01S07(), "Fractional truncation");
  FromArithmeticToArithmeticTest<double, SQLSMALLINT>(
      -42.1, -42, SQL_C_SSHORT, SQLStates::k_01S07(), "Fractional truncation");
}

TEST(ConvertFromArithmeticDSValue, To_SQL_C_USHORT) {
  FromArithmeticToArithmeticTest<int64_t, SQLUSMALLINT>(42, 42, SQL_C_USHORT);
  FromArithmeticToArithmeticTest<int64_t, SQLUSMALLINT>(
      92233720368547, 1111, SQL_C_USHORT, SQLStates::k_22003(),
      "Numeric value out of range");
  FromArithmeticToArithmeticTest<int64_t, SQLUSMALLINT>(
      -13, -13, SQL_C_USHORT, SQLStates::k_22003(),
      "Numeric value out of range");
  FromArithmeticToArithmeticTest<float, SQLUSMALLINT>(
      1.1, 1, SQL_C_USHORT, SQLStates::k_01S07(), "Fractional truncation");
  FromArithmeticToArithmeticTest<double, SQLUSMALLINT>(
      -42.1, -42, SQL_C_USHORT, SQLStates::k_22003(),
      "Numeric value out of range");
}

TEST(ConvertFromArithmeticDSValue, To_SQL_C_CHAR) {
  FromArithmeticToStringTest<int64_t>(42, "42", SQL_C_CHAR);
  FromArithmeticToStringTest<int64_t>(-42, "-42", SQL_C_CHAR);
  FromArithmeticToStringTest<int64_t>(9223372036854775807,
                                      "9223372036854775807", SQL_C_CHAR);
  FromArithmeticToStringTest<float>(42.1, "42.1", SQL_C_CHAR);
  FromArithmeticToStringTest<double>(92233720367.1, "92233720367.1",
                                     SQL_C_CHAR);
}

TEST(ConvertFromStringDSValue, To_SQL_C_CHAR_success) {
  SQLPOINTER buf = malloc(50);
  DataBuffer data = {SQL_C_CHAR, buf, 50, nullptr};
  DSValue ds_value;
  std::string src_val = "Hello";

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromStringDSValue(ds_value, data);
  ASSERT_TRUE(status_record.ok());
  std::string returned_val = (char*)(SQLCHAR*)data.buf;
  EXPECT_EQ(returned_val, src_val);

  free(buf);
}

TEST(ConvertFromStringDSValue, To_SQL_C_CHAR_truncation) {
  SQLLEN buflen = 10;
  SQLPOINTER buf = malloc(buflen);
  DataBuffer data = {SQL_C_CHAR, buf, buflen, nullptr};
  DSValue ds_value;
  std::string src_val = "1234567891011";

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromStringDSValue(ds_value, data);
  ASSERT_FALSE(status_record.ok());
  std::string returned_val = (char*)(SQLCHAR*)data.buf;
  EXPECT_EQ(returned_val, src_val.substr(0, buflen - 1));
  EXPECT_EQ(SQLStates::k_01004(), status_record.sql_state);
  EXPECT_EQ("String data, right truncated", status_record.message);

  free(buf);
}

template <typename DestType>
void FromStringToArithmeticTest(std::string src_val, DestType expected_val,
                                SQLSMALLINT dest_type,
                                std::string expected_state = "",
                                std::string expected_message = "") {
  SQLPOINTER buf = malloc(50);
  DataBuffer data = {dest_type, buf, 50, nullptr};
  DSValue ds_value;

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromStringDSValue(ds_value, data);
  if (expected_state.empty() || expected_state == SQLStates::k_01S07()) {
    DestType* returned_val = (DestType*)data.buf;
    EXPECT_EQ(*returned_val, expected_val);
    EXPECT_EQ(expected_message, status_record.message);
  } else {
    EXPECT_EQ(expected_state, status_record.sql_state);
    EXPECT_EQ(expected_message, status_record.message);
  }

  free(buf);
}

TEST(ConvertFromStringDSValue, To_SQL_C_FLOAT) {
  FromStringToArithmeticTest<SQLREAL>("42", 42, SQL_C_FLOAT);
  FromStringToArithmeticTest<SQLREAL>("42.1", 42.1, SQL_C_FLOAT);
  FromStringToArithmeticTest<SQLREAL>("-1.1", -1.1, SQL_C_FLOAT);
}

TEST(ConvertFromStringDSValue, To_SQL_C_SSHORT) {
  FromStringToArithmeticTest<SQLSMALLINT>("42", 42, SQL_C_SSHORT);
  FromStringToArithmeticTest<SQLSMALLINT>(
      "42.1", 42, SQL_C_SSHORT, SQLStates::k_01S07(), "Fractional truncation");
  FromStringToArithmeticTest<SQLSMALLINT>("-3", -3, SQL_C_SSHORT);
  FromStringToArithmeticTest<SQLSMALLINT>("-17.1", -17, SQL_C_SSHORT,
                                          SQLStates::k_01S07(),
                                          "Fractional truncation");
}

TEST(ConvertFromStringDSValue, To_SQL_C_USHORT) {
  FromStringToArithmeticTest<SQLUSMALLINT>("42", 42, SQL_C_USHORT);
  FromStringToArithmeticTest<SQLUSMALLINT>(
      "42.1", 42, SQL_C_USHORT, SQLStates::k_01S07(), "Fractional truncation");
  FromStringToArithmeticTest<SQLUSMALLINT>("-3", 11111 /* doesn't matter */,
                                           SQL_C_USHORT, SQLStates::k_22003(),
                                           "Numeric value out of range");
  FromStringToArithmeticTest<SQLUSMALLINT>("-17.1", 11111 /* doesn't matter */,
                                           SQL_C_USHORT, SQLStates::k_22003(),
                                           "Numeric value out of range");
}
TEST(ConvertDate, Unsupported_Conversion) {
  std::string date = "2024-02-27";
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  char dest_buf[11]; 
  DataBuffer dest_data{SQL_C_TYPE_TIMESTAMP, dest_buf, sizeof(dest_buf)};
  auto status = ConvertDate<DSValue>(src_dsval, dest_data);
  assert(status.sql_state == odbc_internal::SQLStates::k_HY000());
}
TEST(ConvertDate, insufficient_bufferlength) {
  std::string date = "2024-02-27";
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  char dest_buf[5]; 
  DataBuffer dest_data{SQL_C_BINARY, dest_buf, sizeof(dest_buf)};
  auto status = ConvertDate<DSValue>(src_dsval, dest_data);
  assert(status.sql_state == odbc_internal::SQLStates::k_22003());
}

TEST(ConvertDate, success) {
  std::string date = "2024-02-27";
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  wchar_t dest_buf[11];
  DataBuffer dest_data = {SQL_C_TYPE_DATE, dest_buf, 50, nullptr};
  auto status = ConvertDate<DSValue>(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());
}

TEST(ConvertDate, failure_incorrect_conversion) {
  std::string date = "2024-02-27";
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  wchar_t dest_buf[11];
  DataBuffer dest_data = {SQL_DATE, dest_buf, 50, nullptr};
  auto status = ConvertDate<DSValue>(src_dsval, dest_data);
  assert(status.sql_state == odbc_internal::SQLStates::k_HY000());
}

}  // namespace google::cloud::odbc_bq_driver_internal
