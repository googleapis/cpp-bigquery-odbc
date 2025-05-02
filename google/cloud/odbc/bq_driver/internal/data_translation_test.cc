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
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::odbc_testing_utils::StatusRecIs;
using ::testing::StrEq;
using json = nlohmann::json;

TEST(CheckLimitsArithmetic, Basic) {
  StatusRecord status_record;
  status_record = CheckLimitsArithmetic<int, double>(100);
  EXPECT_TRUE(status_record.ok());

  status_record = CheckLimitsArithmetic<int, double>(-100);
  EXPECT_TRUE(status_record.ok());

  status_record = CheckLimitsArithmetic<int, unsigned char>(-1);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Numeric value out of range", status_record.message);

  status_record = CheckLimitsArithmetic<int, char>(-100);
  EXPECT_TRUE(status_record.ok());

  status_record = CheckLimitsArithmetic<int, char>(-200);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Numeric value out of range", status_record.message);

  status_record = CheckLimitsArithmetic<double, int>(100.0);
  EXPECT_TRUE(status_record.ok());

  status_record = CheckLimitsArithmetic<double, int>(100.5);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01S07(), status_record.sql_state);
  EXPECT_EQ("Fractional truncation", status_record.message);

  status_record = CheckLimitsArithmetic<double, unsigned char>(-200.5);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Numeric value out of range", status_record.message);

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

template <typename SQLType, typename CType>
void FromIntervalToExpectedTest(SQLINTERVAL interval_type, CType interval_value,
                                SQLSMALLINT c_type) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = interval_type;
  switch (interval_type) {
    case SQL_IS_DAY:
      interval.intval.day_second.day = interval_value;
      break;
    case SQL_IS_MINUTE:
      interval.intval.day_second.minute = interval_value;
      break;
    case SQL_IS_HOUR:
      interval.intval.day_second.hour = interval_value;
      break;
    case SQL_IS_MONTH:
      interval.intval.year_month.month = interval_value;
      break;
    case SQL_IS_YEAR:
      interval.intval.year_month.year = interval_value;
      break;
    default:
      throw std::runtime_error("Invalid interval Type");
      break;
  }
  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[80];

  DataBuffer dest_data{c_type, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  auto returned_val = reinterpret_cast<SQLType*>(dest_data.buf);
  auto expected_val = static_cast<CType>(interval_value);

  EXPECT_EQ(*returned_val, expected_val);
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

TEST(ConvertFromArithmeticDSValue, To_SQL_C_BIT) {
  FromArithmeticToArithmeticTest<int64_t, SQLCHAR>(1, 1, SQL_C_BIT);
  FromArithmeticToArithmeticTest<int64_t, SQLCHAR>(0, 0, SQL_C_BIT);
  FromArithmeticToArithmeticTest<int64_t, SQLCHAR>(
      2, 42 /* doesn't matter */, SQL_C_BIT, SQLStates::k_22003(),
      "Numeric value out of range");
  FromArithmeticToArithmeticTest<int64_t, SQLCHAR>(
      -1, 42 /* doesn't matter */, SQL_C_BIT, SQLStates::k_22003(),
      "Numeric value out of range");
}

// Numeric Function Unit Test
TEST(GetNumericDetailsFromStr, To_Numeric_Val) {
  SQL_NUMERIC_STRUCT numst;
  std::string str_input = "121.66";
  SQLCHAR precision = 5;
  SQLSCHAR scale = 2;
  SQLCHAR sign = 1; /* 0 for negative, 1 for positive */
  SQLCHAR val[SQL_MAX_NUMERIC_LEN] = "12166";
  auto status_record = GetNumericDetailsFromStr(str_input, numst);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(numst.precision, precision);
  EXPECT_EQ(numst.sign, sign);
  EXPECT_EQ(numst.scale, scale);
}

template <typename DestType>
void FromNumericToAllTest(std::string src_val, DestType expected_val,
                          SQLSMALLINT dest_type,
                          std::string expected_state = "",
                          std::string expected_message = "") {
  SQLPOINTER buf = malloc(50);
  SQLLEN result_len = 0;
  DataBuffer data = {dest_type, buf, 50, &result_len};
  DSValue ds_value;

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);
  if (expected_state.empty() || expected_state == SQLStates::k_01S07()) {
    DestType* returned_val = (DestType*)data.buf;
    EXPECT_EQ(*returned_val, expected_val);
    EXPECT_EQ(result_len, sizeof(DestType));
    EXPECT_EQ(expected_message, status_record.message);
  } else {
    EXPECT_EQ(expected_state, status_record.sql_state);
    EXPECT_EQ(expected_message, status_record.message);
  }

  free(buf);
}

TEST(ConvertFromNumericDSValue, To_SQL_C_FLOAT) {
  FromNumericToAllTest<SQLREAL>("42", 42, SQL_C_FLOAT);
  FromNumericToAllTest<SQLREAL>("42.1", 42.1, SQL_C_FLOAT);
  FromNumericToAllTest<SQLREAL>("-1.1", -1.1, SQL_C_FLOAT);
  FromNumericToAllTest<SQLREAL>("abc", 0, SQL_C_FLOAT, SQLStates::k_HY000(),
                                "Invalid conversion");
  FromNumericToAllTest<SQLREAL>("-1.0e100", 0, SQL_C_FLOAT,
                                SQLStates::k_22003(),
                                "Numeric value out of range");
}

TEST(ConvertFromNumericDSValue, To_SQL_C_Numeric) {
  // Prepare buffer for SQL_NUMERIC_STRUCT
  SQL_NUMERIC_STRUCT numeric_struct;
  memset(&numeric_struct, 0, sizeof(numeric_struct));

  // Successful conversion cases
  SQLPOINTER buf = &numeric_struct;
  SQLLEN result_len = 0;
  DataBuffer data = {SQL_C_NUMERIC, buf, sizeof(SQL_NUMERIC_STRUCT),
                     &result_len};

  {
    DSValue ds_value;
    StringToDSValue("42", ds_value);
    StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);

    EXPECT_EQ(*reinterpret_cast<uint64_t*>(numeric_struct.val), 42);
    EXPECT_EQ(numeric_struct.sign, 1);  // Positive number
    EXPECT_EQ(result_len, sizeof(SQL_NUMERIC_STRUCT));
  }

  {
    DSValue ds_value;
    StringToDSValue("-99", ds_value);
    StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);

    EXPECT_EQ(*reinterpret_cast<uint64_t*>(numeric_struct.val), 99);
    EXPECT_EQ(numeric_struct.sign, 0);  // Negative number
    EXPECT_EQ(result_len, sizeof(SQL_NUMERIC_STRUCT));
  }

  // Failure cases
  {
    DSValue ds_value;
    StringToDSValue("abc", ds_value);  // Invalid numeric input
    StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);

    EXPECT_EQ(status_record.sql_state,
              SQLStates::k_HY000());  // Invalid conversion error
    EXPECT_EQ(status_record.message, "Invalid conversion");
  }

  {
    DSValue ds_value;
    StringToDSValue("123456789123456789123456789123456789.123456789", ds_value);
    StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);
    EXPECT_EQ(status_record.sql_state,
              SQLStates::k_22003());  // Expect "Numeric value out of range"
    EXPECT_EQ(status_record.message, "Numeric value out of range");
  }

  {
    DSValue ds_value;
    StringToDSValue("-0.00000000000000000000000000000000000001", ds_value);
    StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);

    EXPECT_EQ(*reinterpret_cast<uint64_t*>(numeric_struct.val), 0);
    EXPECT_EQ(numeric_struct.scale, 0);
    EXPECT_EQ(numeric_struct.sign, 1);
    EXPECT_EQ(result_len, sizeof(SQL_NUMERIC_STRUCT));
    EXPECT_EQ(status_record.CalculateReturnCode(), SQL_SUCCESS);
  }

  {
    DSValue ds_value;
    StringToDSValue("0.123456789123456789", ds_value);
    StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);

    EXPECT_EQ(*reinterpret_cast<uint64_t*>(numeric_struct.val), 123456789);
    EXPECT_EQ(numeric_struct.scale, 9);
    EXPECT_EQ(numeric_struct.sign, 1);
    EXPECT_EQ(result_len, sizeof(SQL_NUMERIC_STRUCT));
    EXPECT_EQ(status_record.sql_state, SQLStates::k_01S07());
    EXPECT_EQ(status_record.message,
              "Fractional truncation (loss of precision)");
    EXPECT_EQ(status_record.CalculateReturnCode(), SQL_SUCCESS_WITH_INFO);
  }
}

TEST(ConvertFromNumericDSValue, To_SQL_C_SSHORT) {
  FromNumericToAllTest<SQLSMALLINT>("42", 42, SQL_C_SSHORT);
  FromNumericToAllTest<SQLSMALLINT>(
      "42.1", 42, SQL_C_SSHORT, SQLStates::k_01S07(), "Fractional truncation");
  FromNumericToAllTest<SQLSMALLINT>("-3", -3, SQL_C_SSHORT);
  FromNumericToAllTest<SQLSMALLINT>("-17.1", -17, SQL_C_SSHORT,
                                    SQLStates::k_01S07(),
                                    "Fractional truncation");
}

TEST(ConvertFromNumericDSValue, To_SQL_C_USHORT) {
  FromNumericToAllTest<SQLUSMALLINT>("42", 42, SQL_C_USHORT);
  FromNumericToAllTest<SQLUSMALLINT>(
      "42.1", 42, SQL_C_USHORT, SQLStates::k_01S07(), "Fractional truncation");
  FromNumericToAllTest<SQLUSMALLINT>("-3", 11111 /* doesn't matter */,
                                     SQL_C_USHORT, SQLStates::k_22003(),
                                     "Numeric value out of range");
  FromNumericToAllTest<SQLUSMALLINT>("-17.1", 11111 /* doesn't matter */,
                                     SQL_C_USHORT, SQLStates::k_22003(),
                                     "Numeric value out of range");
}

TEST(ConvertFromNumericDSValue, To_SQL_C_CHAR_success) {
  SQLPOINTER buf = malloc(50);
  SQLLEN result_len = 0;
  DataBuffer data = {SQL_C_CHAR, buf, 50, &result_len};
  DSValue ds_value;
  std::string src_val = "123";

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromNumericDSValue(ds_value, data);
  ASSERT_TRUE(status_record.ok());
  std::string returned_val = (char*)(SQLCHAR*)data.buf;
  EXPECT_EQ(returned_val, src_val);
  free(buf);
}
// End Numeric conversion function unit test

TEST(ConvertFromStringDSValue, To_SQL_C_CHAR_success) {
  SQLPOINTER buf = malloc(50);
  SQLLEN result_len = 0;
  DataBuffer data = {SQL_C_CHAR, buf, 50, &result_len};
  DSValue ds_value;
  std::string src_val = "Hello";

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromStringDSValue(ds_value, data);
  ASSERT_TRUE(status_record.ok());
  std::string returned_val = (char*)(SQLCHAR*)data.buf;
  EXPECT_EQ(returned_val, src_val);
  EXPECT_EQ(result_len, (SQLLEN)src_val.size());

  free(buf);
}

TEST(ConvertFromStringDSValue, To_SQL_C_CHAR_truncation) {
  SQLLEN buflen = 10;
  SQLLEN result_len = 0;
  SQLPOINTER buf = malloc(buflen);
  DataBuffer data = {SQL_C_CHAR, buf, buflen, &result_len};
  DSValue ds_value;
  std::string src_val = "1234567891011";

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromStringDSValue(ds_value, data);
  ASSERT_FALSE(status_record.ok());
  std::string returned_val = (char*)(SQLCHAR*)data.buf;
  EXPECT_EQ(returned_val, src_val.substr(0, buflen - 1));
  EXPECT_EQ(result_len, buflen - 1);
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
  SQLLEN result_len = 0;
  DataBuffer data = {dest_type, buf, 50, &result_len};
  DSValue ds_value;

  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromStringDSValue(ds_value, data);
  if (expected_state.empty() || expected_state == SQLStates::k_01S07()) {
    DestType* returned_val = (DestType*)data.buf;
    EXPECT_EQ(*returned_val, expected_val);
    EXPECT_EQ(result_len, sizeof(DestType));
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
TEST(ConvertFromDateDSValue, Unsupported_Conversion) {
  SQL_DATE_STRUCT date;
  date.year = 2020;
  date.month = 10;
  date.day = 10;
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  char dest_buf[11];
  DataBuffer dest_data{SQL_C_ULONG, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromDateDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY000(),
                                  StrEq("Conversion is unsupported")));
}

TEST(ConvertFromDateDSValue, convertToDate) {
  SQL_DATE_STRUCT date;
  date.year = 2020;
  date.month = 10;
  date.day = 10;
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  SQLLEN result_len = 0;

  alignas(SQL_DATE_STRUCT) char dest_buf[sizeof(SQL_DATE_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_DATE, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromDateDSValue(src_dsval, dest_data);
  SQL_DATE_STRUCT* data = reinterpret_cast<SQL_DATE_STRUCT*>(dest_data.buf);

  EXPECT_EQ(data->year, date.year);
  EXPECT_EQ(data->month, date.month);
  EXPECT_EQ(data->day, date.day);
  EXPECT_EQ(result_len, sizeof(SQL_DATE_STRUCT));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromDateDSValue, convertToTimestamp) {
  SQL_DATE_STRUCT date;
  date.year = 2020;
  date.month = 10;
  date.day = 10;
  SQLLEN result_len = 0;

  DSValue src_dsval;
  DateToDSValue(date, src_dsval);

  char dest_buf[sizeof(SQL_TIMESTAMP_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_TIMESTAMP, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromDateDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());
  SQL_TIMESTAMP_STRUCT* data =
      reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(dest_buf);
  EXPECT_EQ(data->year, date.year);
  EXPECT_EQ(data->month, date.month);
  EXPECT_EQ(data->day, date.day);
  EXPECT_EQ(result_len, sizeof(SQL_TIMESTAMP_STRUCT));
}

TEST(ConvertFromDateDSValue, convertToBinary_Success) {
  SQL_DATE_STRUCT date;
  date.year = 2020;
  date.month = 10;
  date.day = 10;
  SQLLEN result_len = 0;

  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  char dest_buf[20];
  DataBuffer dest_data = {SQL_C_BINARY, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromDateDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  SQL_DATE_STRUCT* data = reinterpret_cast<SQL_DATE_STRUCT*>(dest_buf);

  EXPECT_EQ(data->year, date.year);
  EXPECT_EQ(data->month, date.month);
  EXPECT_EQ(data->day, date.day);
  EXPECT_EQ(result_len, sizeof(SQL_DATE_STRUCT));
}

TEST(ConvertFromDateDSValue, convertToChar) {
  SQL_DATE_STRUCT date;
  date.year = 2020;
  date.month = 10;
  date.day = 10;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  char dest_buf[11];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromDateDSValue(src_dsval, dest_data);
  std::string expected_date = "2020-10-10";
  std::string data(dest_buf);
  EXPECT_EQ(data, expected_date);
  EXPECT_EQ(result_len, (SQLLEN)expected_date.size());
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromDateDSValue, Failure_Incorrect_Conversion) {
  SQL_DATE_STRUCT date;
  date.year = 2020;
  date.month = 10;
  date.day = 10;
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  wchar_t dest_buf[11];
  DataBuffer dest_data = {SQL_DATE, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromDateDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY000(),
                                  StrEq("Conversion is unsupported")));
}

TEST(ConvertFromDateDSValue, convertToBinary_InsufficientBuffer) {
  SQL_DATE_STRUCT date;
  date.year = 2020;
  date.month = 10;
  date.day = 10;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  DateToDSValue(date, src_dsval);
  char dest_buf[5];
  DataBuffer dest_data = {SQL_C_BINARY, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromDateDSValue(src_dsval, dest_data);
  EXPECT_EQ(result_len, sizeof(dest_buf));
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_01004(),
                                  StrEq("Binary data, right truncated")));
}

TEST(ConvertFromDateDSValue, NullDestinationBuffer) {
  SQL_DATE_STRUCT date_struct = {2024, 9, 10};
  DSValue ds_value;
  DateToDSValue(date_struct, ds_value);

  DataBuffer dest_data = {SQL_C_TYPE_DATE, nullptr, 10, nullptr};

  auto result = ConvertFromDateDSValue(ds_value, dest_data);
  EXPECT_THAT(result, StatusRecIs(SQLStates::k_HY090(),
                                  StrEq("Destination buffer is null")));
}

TEST(ConvertFromDateDSValue, NegativeBufferLength) {
  SQL_DATE_STRUCT date_struct = {2024, 9, 10};
  DSValue ds_value;
  DateToDSValue(date_struct, ds_value);

  char buffer[10];
  DataBuffer dest_data = {SQL_C_CHAR, buffer, -1, nullptr};

  auto result = ConvertFromDateDSValue(ds_value, dest_data);
  EXPECT_THAT(result, StatusRecIs(SQLStates::k_HY090(),
                                  StrEq("Invalid Buffer length")));
}

TEST(ConvertFromDateDSValue, SmallBufferForStringOutput) {
  SQL_DATE_STRUCT date_struct = {2024, 9, 10};
  DSValue ds_value;
  SQLLEN result_len = 0;
  DateToDSValue(date_struct, ds_value);

  char buffer[5];
  DataBuffer dest_data = {SQL_C_CHAR, buffer, sizeof(buffer), &result_len};

  auto result = ConvertFromDateDSValue(ds_value, dest_data);
  EXPECT_THAT(result, StatusRecIs(SQLStates::k_01004(),
                                  StrEq("String data, right truncated")));
  EXPECT_EQ(result_len, sizeof(buffer));
  EXPECT_STREQ(buffer, "YYYY");
}

TEST(ConvertFromTimeDSValue, ToTime) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 33;
  time.second = 48;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimeToDSValue(time, src_dsval);
  alignas(SQL_TIME_STRUCT) char dest_buf[sizeof(SQL_TIME_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_TIME, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  SQL_TIME_STRUCT* data = reinterpret_cast<SQL_TIME_STRUCT*>(dest_data.buf);

  EXPECT_EQ(data->hour, time.hour);
  EXPECT_EQ(data->minute, time.minute);
  EXPECT_EQ(data->second, time.second);
  EXPECT_EQ(result_len, sizeof(SQL_TIME_STRUCT));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimeDSValue, ToTime_InsufficientBufferCase) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 33;
  time.second = 48;
  DSValue src_dsval;
  TimeToDSValue(time, src_dsval);
  char dest_buf[5];
  DataBuffer dest_data = {SQL_C_TYPE_TIME, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_01004());
}

TEST(ConvertFromTimeDSValue, ToTimestamp) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 33;
  time.second = 48;

  DSValue src_dsval;
  TimeToDSValue(time, src_dsval);

  char dest_buf[sizeof(SQL_TIMESTAMP_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_TIMESTAMP, dest_buf, sizeof(dest_buf),
                          nullptr};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());
  SQL_TIMESTAMP_STRUCT* data =
      reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(dest_buf);
  EXPECT_EQ(data->hour, time.hour);
  EXPECT_EQ(data->minute, time.minute);
  EXPECT_EQ(data->second, time.second);
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimeDSValue, ToBinary) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 33;
  time.second = 48;
  SQLLEN result_len = 0;

  DSValue src_dsval;
  TimeToDSValue(time, src_dsval);
  char dest_buf[20];
  DataBuffer dest_data = {SQL_C_BINARY, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());
  SQL_TIME_STRUCT* data = reinterpret_cast<SQL_TIME_STRUCT*>(dest_buf);

  EXPECT_EQ(data->hour, time.hour);
  EXPECT_EQ(data->minute, time.minute);
  EXPECT_EQ(data->second, time.second);
  EXPECT_EQ(result_len, sizeof(SQL_TIME_STRUCT));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimeDSValue, ToWChar) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 07;
  time.second = 20;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimeToDSValue(time, src_dsval);
  SQLWCHAR dest_buf[32] = {0};
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  std::string expected_time = "19:07:20.000000";
  StatusRecordOr<std::string> data = ConvertSQLWCHARToString(dest_buf, 15);
  EXPECT_STREQ(data->c_str(), expected_time.c_str());
  EXPECT_EQ(result_len, expected_time.size() * sizeof(SQLWCHAR));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimeDSValue, ToChar) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 33;
  time.second = 48;
  SQLLEN result_len = 0;
  SQLLEN expected_size = 8;
  DSValue src_dsval;
  TimeToDSValue(time, src_dsval);
  char dest_buf[16];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  std::string expected_time = "19:33:48.000000";
  std::string data(dest_buf);
  EXPECT_EQ(data, expected_time);
  EXPECT_EQ(result_len, expected_size);
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimeDSValue, InsufficientBufferCase) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 33;
  time.second = 48;
  DSValue src_dsval;
  SQLLEN result_len = 0;
  TimeToDSValue(time, src_dsval);
  char dest_buf[5];
  DataBuffer dest_data = {SQL_C_BINARY, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  EXPECT_EQ(result_len, sizeof(dest_buf));
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_01004());
}

TEST(ConvertFromTimeDSValue, convertToInvalidType_Failed) {
  SQL_TIME_STRUCT time;
  time.hour = 19;
  time.minute = 33;
  time.second = 48;
  DSValue src_dsval;
  TimeToDSValue(time, src_dsval);
  char dest_buf[16];
  DataBuffer dest_data = {SQL_C_SLONG, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromTimeDSValue(src_dsval, dest_data);
  ASSERT_EQ(status.sql_state, odbc_internal::SQLStates::k_HY000());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromJsonDSValue, To_SQL_C_CHAR_success) {
  SQLPOINTER buf = malloc(100);
  DataBuffer data = {SQL_C_CHAR, buf, 100, nullptr};
  DSValue ds_value;
  json src_val = nlohmann::json({{"age", 30}, {"name", "Sita"}});
  std::string expected_val = "{\"age\":30,\"name\":\"Sita\"}";
  std::string str = src_val.dump();
  StringToDSValue(str, ds_value);
  StatusRecord status_record = ConvertFromJsonDSValue(ds_value, data);
  std::string returned_val = (char*)data.buf;
  EXPECT_EQ(returned_val, expected_val);
  free(buf);
}

TEST(ConvertFromJsonDSValue, To_SQL_C_CHAR_Failure) {
  SQLPOINTER buf = malloc(10);
  DataBuffer data = {SQL_C_CHAR, buf, 10, nullptr};
  DSValue ds_value;
  json src_val = nlohmann::json({{"age", 30}, {"name", "Suzan"}});
  std::string expected_val = "{\"age\":30,\"name\":\"Suzan\"}";
  std::string str = src_val.dump();
  StringToDSValue(str, ds_value);
  StatusRecord status_record = ConvertFromJsonDSValue(ds_value, data);
  EXPECT_THAT(
      status_record,
      StatusRecIs(SQLStates::k_01004(), StrEq("String data, right truncated")));
  free(buf);
}

TEST(ConvertFromJsonDSValue, To_SQL_C_WCHAR_success) {
  SQLWCHAR dest_buf[100] = {0};
  SQLLEN data_len;
  DataBuffer data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf), &data_len};
  DSValue ds_value;
  json src_val = nlohmann::json({{"age", 30}, {"name", "Shivam"}});
  std::string expected_val = "{\"age\":30,\"name\":\"Shivam\"}";
  std::string str = src_val.dump();
  StringToDSValue(str, ds_value);
  StatusRecord status_record = ConvertFromJsonDSValue(ds_value, data);
  SQLINTEGER length = data_len / sizeof(SQLWCHAR);
  StatusRecordOr<std::string> returned_val =
      ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(dest_buf), 26);
  EXPECT_STREQ(returned_val.GetValue().c_str(), expected_val.c_str());
}

TEST(ConvertFromJsonDSValue, convertToWchar_Failed) {
  DSValue ds_value;
  json src_val = nlohmann::json({{"age", 30}, {"name", "Shivam"}});
  std::string expected_val = "{\"age\":30,\"name\":\"Shivam\"}";
  std::string str = src_val.dump();
  StringToDSValue(str, ds_value);
  char dest_buf[10];
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromJsonDSValue(ds_value, dest_data);
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_22003());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromJsonDSValue, To_SQL_C_BINARY_success) {
  SQLPOINTER buf = new char[100];
  SQLLEN data_len;
  DataBuffer data = {SQL_C_BINARY, buf, 100, &data_len};

  DSValue ds_value;
  json src_val = nlohmann::json({{"age", 30}, {"name", "Ravi"}});
  std::string expected_val = "{\"age\":30,\"name\":\"Ravi\"}";

  std::string str = src_val.dump();
  StringToDSValue(str, ds_value);

  StatusRecord status_record = ConvertFromJsonDSValue(ds_value, data);

  std::string returned_val(static_cast<char*>(buf), data_len);

  EXPECT_EQ(returned_val, expected_val);
  delete[] static_cast<char*>(buf);
}

TEST(ConvertFromJsonDSValue, To_SQL_C_BINARY_Failure) {
  SQLPOINTER buf = new char[5];
  SQLLEN data_len;
  DataBuffer data = {SQL_C_BINARY, buf, 5, &data_len};

  DSValue ds_value;
  json src_val = nlohmann::json({{"age", 30}, {"name", "Ravi"}});
  std::string expected_val = "{\"age\":30,\"name\":\"Ravi\"}";

  std::string str = src_val.dump();
  StringToDSValue(str, ds_value);

  StatusRecord status_record = ConvertFromJsonDSValue(ds_value, data);

  EXPECT_THAT(
      status_record,
      StatusRecIs(SQLStates::k_01004(), StrEq("String data, right truncated")));
  delete[] static_cast<char*>(buf);
}

TEST(ConvertFromTimestampDSValue, convertToDate_InsufficientBuffer) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2020;
  Timestamp.month = 1;
  Timestamp.day = 10;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 123456;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  // Ensure alignment for SQL_DATE_STRUCT
  alignas(SQL_DATE_STRUCT) char dest_buf[sizeof(SQL_DATE_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_DATE, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  SQL_DATE_STRUCT* data = reinterpret_cast<SQL_DATE_STRUCT*>(dest_data.buf);

  EXPECT_EQ(data->year, Timestamp.year);
  EXPECT_EQ(data->month, Timestamp.month);
  EXPECT_EQ(data->day, Timestamp.day);
  EXPECT_EQ(result_len, sizeof(SQL_DATE_STRUCT));
  EXPECT_EQ(status.sql_state, SQLStates::k_01S07());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToDate_Success) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2020;
  Timestamp.month = 1;
  Timestamp.day = 10;
  Timestamp.hour = 00;
  Timestamp.minute = 00;
  Timestamp.second = 00;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  // Ensure alignment for SQL_DATE_STRUCT
  alignas(SQL_DATE_STRUCT) char dest_buf[sizeof(SQL_DATE_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_DATE, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  SQL_DATE_STRUCT* data = reinterpret_cast<SQL_DATE_STRUCT*>(dest_data.buf);

  EXPECT_EQ(data->year, Timestamp.year);
  EXPECT_EQ(data->month, Timestamp.month);
  EXPECT_EQ(data->day, Timestamp.day);
  EXPECT_EQ(result_len, sizeof(SQL_DATE_STRUCT));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToTime_InsufficientBuffer) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2020;
  Timestamp.month = 1;
  Timestamp.day = 10;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 123456;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  // Ensure alignment for SQL_TIME_STRUCT
  alignas(SQL_TIME_STRUCT) char dest_buf[sizeof(SQL_TIME_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_TIME, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  SQL_TIME_STRUCT* data = reinterpret_cast<SQL_TIME_STRUCT*>(dest_data.buf);

  EXPECT_EQ(data->hour, Timestamp.hour);
  EXPECT_EQ(data->minute, Timestamp.minute);
  EXPECT_EQ(data->second, Timestamp.second);
  EXPECT_EQ(result_len, sizeof(SQL_TIME_STRUCT));
  EXPECT_EQ(status.sql_state, SQLStates::k_01S07());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToTime_Success) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 0;
  Timestamp.month = 0;
  Timestamp.day = 0;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 0;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  // Ensure alignment for SQL_TIME_STRUCT
  alignas(SQL_TIME_STRUCT) char dest_buf[sizeof(SQL_TIME_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_TIME, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  SQL_TIME_STRUCT* data = reinterpret_cast<SQL_TIME_STRUCT*>(dest_data.buf);

  EXPECT_EQ(data->hour, Timestamp.hour);
  EXPECT_EQ(data->minute, Timestamp.minute);
  EXPECT_EQ(data->second, Timestamp.second);
  EXPECT_EQ(result_len, sizeof(SQL_TIME_STRUCT));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToTimestamp_Success) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 0;
  Timestamp.month = 0;
  Timestamp.day = 0;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 0;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  // Ensure alignment for SQL_TIMESTAMP_STRUCT
  alignas(SQL_TIMESTAMP_STRUCT) char dest_buf[sizeof(SQL_TIMESTAMP_STRUCT)];
  DataBuffer dest_data = {SQL_C_TYPE_TIMESTAMP, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  SQL_TIMESTAMP_STRUCT* data =
      reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(dest_data.buf);

  EXPECT_EQ(data->year, Timestamp.year);
  EXPECT_EQ(data->month, Timestamp.month);
  EXPECT_EQ(data->day, Timestamp.day);
  EXPECT_EQ(data->hour, Timestamp.hour);
  EXPECT_EQ(data->minute, Timestamp.minute);
  EXPECT_EQ(data->second, Timestamp.second);
  EXPECT_EQ(result_len, sizeof(SQL_TIMESTAMP_STRUCT));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToBinary_InsufficientBuffer) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  char dest_buf[10];
  DataBuffer dest_data = {SQL_C_BINARY, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_22003());
}

TEST(ConvertFromTimestampDSValue, convertToBinary_Success) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  SQL_TIMESTAMP_STRUCT expectedTimestamp;
  expectedTimestamp.year = 2024;
  expectedTimestamp.month = 10;
  expectedTimestamp.day = 20;
  expectedTimestamp.hour = 01;
  expectedTimestamp.minute = 59;
  expectedTimestamp.second = 43;
  expectedTimestamp.fraction = 112233000;
  char dest_buf[30];
  DataBuffer dest_data = {SQL_C_BINARY, dest_buf, sizeof(dest_buf),
                          &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  SQL_TIMESTAMP_STRUCT* data =
      reinterpret_cast<SQL_TIMESTAMP_STRUCT*>(dest_buf);

  EXPECT_EQ(data->year, expectedTimestamp.year);
  EXPECT_EQ(data->month, expectedTimestamp.month);
  EXPECT_EQ(data->day, expectedTimestamp.day);
  EXPECT_EQ(data->hour, expectedTimestamp.hour);
  EXPECT_EQ(data->minute, expectedTimestamp.minute);
  EXPECT_EQ(data->second, expectedTimestamp.second);
  EXPECT_EQ(data->fraction, expectedTimestamp.fraction);
  EXPECT_EQ(result_len, sizeof(SQL_TIMESTAMP_STRUCT));
}

TEST(ConvertFromTimestampDSValue, convertToChar_Success) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  char dest_buf[30];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  std::string expected_date = "2024-10-20 01:59:43.112233";
  std::string data(dest_buf);
  EXPECT_EQ(data, expected_date);
  EXPECT_EQ(result_len, expected_date.size());
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToChar_InsufficientBuffer) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  SQLLEN result_len = 0;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  char dest_buf[24];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  std::string data(dest_buf);
  std::string expected_date = "2024-10-20 01:59:43.112";
  EXPECT_EQ(data, expected_date);
  EXPECT_EQ(result_len, sizeof(dest_buf));
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_01004());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToChar_Failed) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  char dest_buf[10];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_22003());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToWchar_Success) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  wchar_t dest_buf[30];
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  std::string expected_date = "2";
  auto* dest = reinterpret_cast<char*>(dest_buf);
  std::string data(dest);
  EXPECT_EQ(data, expected_date);
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToWchar_InsufficientBuffer) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  wchar_t dest_buf[24];
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, 24, nullptr};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  std::string expected_date = "2";
  auto* dest = reinterpret_cast<char*>(dest_buf);
  std::string data(dest);
  EXPECT_EQ(data, expected_date);
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToWchar_Failed) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  char dest_buf[10];
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_22003());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromTimestampDSValue, convertToInvalidType_Failed) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2024;
  Timestamp.month = 10;
  Timestamp.day = 20;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 112233;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);

  char dest_buf[10];
  DataBuffer dest_data = {SQL_C_SLONG, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromTimestampDSValue(src_dsval, dest_data);
  ASSERT_EQ(status.sql_state, odbc_internal::SQLStates::k_HY000());
  ASSERT_FALSE(status.ok());
}
TEST(ConvertFromIntervalDSValue, To_SQL_C_Char) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_YEAR;
  interval.intval.year_month.year = 5;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);

  char dest_buf[80];
  DataBuffer dest_data{SQL_C_CHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  auto* returned_val = reinterpret_cast<char*>(dest_data.buf);
  EXPECT_EQ(interval_str, returned_val);
}
TEST(ConvertFromIntervalDSValue, To_SQL_C_WChar) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_HOUR;
  interval.intval.day_second.hour = 7;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);

  SQLWCHAR dest_buf[80] = {0};
  DataBuffer dest_data{SQL_C_WCHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());
  auto returned_val = ConvertSQLWCHARToString(
      reinterpret_cast<SQLWCHAR*>(dest_data.buf), interval_str.length());
  EXPECT_STREQ(returned_val.GetValue().c_str(), interval_str.data());
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_STinyInt) {
  FromIntervalToExpectedTest<SQLCHAR, SQLCHAR>(SQL_IS_DAY, 5, SQL_C_STINYINT);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_UTinyInt) {
  FromIntervalToExpectedTest<SQLCHAR, SQLCHAR>(SQL_IS_MINUTE, 25,
                                               SQL_C_UTINYINT);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_SSHORT) {
  FromIntervalToExpectedTest<SQLSMALLINT, SQLSMALLINT>(SQL_IS_MONTH, 10,
                                                       SQL_C_SSHORT);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_UShort) {
  FromIntervalToExpectedTest<SQLUSMALLINT, SQLSMALLINT>(SQL_IS_MONTH, 7,
                                                        SQL_C_USHORT);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_ULong) {
  FromIntervalToExpectedTest<SQLUINTEGER, SQLUINTEGER>(SQL_IS_YEAR, 9,
                                                       SQL_C_ULONG);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_SBigint) {
  FromIntervalToExpectedTest<SQLBIGINT, SQLUINTEGER>(SQL_IS_HOUR, 20,
                                                     SQL_C_SBIGINT);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_Numeric) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_YEAR;
  interval.intval.day_second.hour = 20;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[80];

  DataBuffer dest_data{SQL_C_NUMERIC, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  SQL_NUMERIC_STRUCT* returned_val =
      reinterpret_cast<SQL_NUMERIC_STRUCT*>(dest_data.buf);

  EXPECT_EQ(returned_val->sign, 1);
  EXPECT_EQ(returned_val->precision, 10);
  EXPECT_EQ(returned_val->scale, 0);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_Interval) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_DAY_TO_SECOND;
  interval.intval.day_second.day = 20;
  interval.intval.day_second.hour = 10;
  interval.intval.day_second.minute = 5;
  interval.intval.day_second.second = 16;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[80];

  DataBuffer dest_data{SQL_C_INTERVAL_DAY_TO_SECOND, dest_buf,
                       sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  SQL_INTERVAL_STRUCT* data = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_buf);
  EXPECT_EQ(data->interval_type, interval.interval_type);
  EXPECT_EQ(data->intval.day_second.day, interval.intval.day_second.day);
  EXPECT_EQ(data->intval.day_second.hour, interval.intval.day_second.hour);
  EXPECT_EQ(data->intval.day_second.minute, interval.intval.day_second.minute);
  EXPECT_EQ(data->intval.day_second.second, interval.intval.day_second.second);
}

TEST(ConvertFromIntervalDSValue, To_SQL_C_Neg_Interval) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_FALSE;
  interval.interval_type = SQL_IS_YEAR_TO_MONTH;
  interval.intval.year_month.year = 20;
  interval.intval.year_month.month = 10;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[80];

  DataBuffer dest_data{SQL_C_INTERVAL_YEAR_TO_MONTH, dest_buf,
                       sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  SQL_INTERVAL_STRUCT* data = reinterpret_cast<SQL_INTERVAL_STRUCT*>(dest_buf);
  EXPECT_EQ(data->interval_type, interval.interval_type);
  EXPECT_EQ(data->intval.year_month.year, interval.intval.year_month.year);
  EXPECT_EQ(data->intval.year_month.month, interval.intval.year_month.month);
}

TEST(ConvertFromIntervalDSValue, Unsupported_DataType) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_YEAR;
  interval.intval.day_second.hour = 20;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[50];

  DataBuffer dest_data{SQL_C_FLOAT, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY000(),
                                  StrEq("Conversion is unsupported")));
}

TEST(ConvertFromIntervalDSValue, Negative_Buffer_Length) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_YEAR;
  interval.intval.year_month.year = 1;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[50];

  DataBuffer dest_data{SQL_C_INTERVAL_YEAR, dest_buf,
                       -1};  // Negative buffer length
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY090(),
                                  StrEq("Invalid Buffer length")));
}

TEST(ConvertFromIntervalDSValue, Insufficient_Buffer_length) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_MONTH;
  interval.intval.year_month.year = 1;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[5];

  DataBuffer dest_data{SQL_C_CHAR, dest_buf,
                       sizeof(dest_buf)};  // Negative buffer length
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_22003(),
                                  StrEq("Buffer length is insufficient")));
}

TEST(ConvertFromIntervalDSValue, Data_Truncated_Char) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_sign = SQL_TRUE;
  interval.interval_type = SQL_IS_YEAR_TO_MONTH;
  interval.intval.year_month.year = 4;
  interval.intval.year_month.month = 2;

  DSValue src_dsval;
  std::string interval_str = FormatIntervalToString(interval);
  StringToDSValue(interval_str, src_dsval);
  char dest_buf[20];  // Smaller buffer than required for the string

  DataBuffer dest_data{SQL_C_CHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromIntervalDSValue(src_dsval, dest_data);
  EXPECT_THAT(status,
              StatusRecIs(SQLStates::k_01004(), StrEq("Data truncated")));
}

TEST(ConvertFromBooleanDSValue, Unsupported_Conversion) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  char dest_buf[11];
  DataBuffer dest_data{SQL_C_TYPE_DATE, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY000(),
                                  StrEq("Conversion is unsupported")));
}

TEST(ConvertFromBooleanDSValue, convertToChar) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  char dest_buf[2];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_EQ(dest_buf[0], '1');
  EXPECT_EQ(dest_buf[1], '\0');
  ASSERT_TRUE(status.ok());
}

// On linux bool value only accepts true and false,else it throws out of scope
// error. But on windows it supports TRUE and FALSE as well.
#ifdef _WIN32
TEST(ConvertFromBooleanDSValue, caseSensitive) {
  DSValue src_dsval;
  BooleanToDSValue(TRUE, src_dsval);
  char dest_buf[2];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_EQ(dest_buf[0], '1');
  EXPECT_EQ(dest_buf[1], '\0');
  ASSERT_TRUE(status.ok());
}
#endif  //_WIN32

TEST(ConvertFromBooleanDSValue, convertToWChar) {
  DSValue src_dsval;
  BooleanToDSValue(false, src_dsval);
  wchar_t dest_buf[2];
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_EQ(dest_buf[0], L'0');
  EXPECT_EQ(dest_buf[1], L'\0');
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromBooleanDSValue, convertToBinary) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  char dest_buf[sizeof(bool)];
  DataBuffer dest_data = {SQL_C_BINARY, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_TRUE(*reinterpret_cast<bool*>(dest_buf));
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromBooleanDSValue, convertToLong) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  SQLINTEGER dest_value = 0;
  DataBuffer dest_data = {SQL_C_LONG, &dest_value, sizeof(dest_value)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_EQ(dest_value, 1);
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromBooleanDSValue, convertToDouble) {
  DSValue src_dsval;
  BooleanToDSValue(false, src_dsval);
  SQLDOUBLE dest_value = 1.0;
  DataBuffer dest_data = {SQL_C_DOUBLE, &dest_value, sizeof(dest_value)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_EQ(dest_value, 0.0);
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromBooleanDSValue, convertToBit) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  SQLCHAR dest_value = 0;
  DataBuffer dest_data = {SQL_C_BIT, &dest_value, sizeof(dest_value)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_EQ(dest_value, 1);
  ASSERT_TRUE(status.ok());
}

TEST(ConvertFromBooleanDSValue, NullDestinationBuffer) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  DataBuffer dest_data = {SQL_C_CHAR, nullptr, 2};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY090(),
                                  StrEq("Destination buffer is null")));
}

TEST(ConvertFromBooleanDSValue, NegativeBufferLength) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  char dest_buf[2];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, -1};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY090(),
                                  StrEq("Buffer length is negative")));
}

TEST(ConvertFromBooleanDSValue, InsufficientBufferForChar) {
  DSValue src_dsval;
  BooleanToDSValue(true, src_dsval);
  char dest_buf[1];
  DataBuffer dest_data = {SQL_C_CHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromBooleanDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_01004(),
                                  StrEq("String data, right truncated")));
  EXPECT_EQ(dest_buf[0], '\0');
}

TEST(ConvertFromGeographyDSValue, InsufficientBufferForWChar) {
  DSValue src_dsval;
  std::string geo_str = "POINT(120.987 14.599)";

  StringToDSValue(geo_str, src_dsval);

  SQLWCHAR dest_buf[2];
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf)};

  auto status = ConvertFromGeographyDSValue(src_dsval, dest_data);

  EXPECT_THAT(status, StatusRecIs(SQLStates::k_22003(),
                                  StrEq("Buffer length is insufficient")));
  EXPECT_EQ(dest_buf[0], L'\0');
}

TEST(ConvertFromGeographyDSValue, InvalidConversion) {
  DSValue src_dsval;
  std::string geo_str = "POINT(120.987 14.599)";
  StringToDSValue(geo_str, src_dsval);

  char dest_buf[50];
  DataBuffer dest_data{SQL_C_SSHORT, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromGeographyDSValue(src_dsval, dest_data);
  EXPECT_THAT(status, StatusRecIs(SQLStates::k_HY000(),
                                  StrEq("Conversion is unsupported")));
}

TEST(ConvertFromGeographyDSValue, To_SQL_C_CHAR) {
  DSValue src_dsval;
  std::string geo_str = "LINESTRING(121.1 14.5, 122.1 15.5)";
  StringToDSValue(geo_str, src_dsval);

  char dest_buf[100];
  DataBuffer dest_data{SQL_C_CHAR, dest_buf, sizeof(dest_buf)};
  auto status = ConvertFromGeographyDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());

  auto* returned_val = reinterpret_cast<char*>(dest_data.buf);
  EXPECT_EQ(geo_str, returned_val);
}

TEST(ConvertFromGeographyDSValue, TO_SQL_C_WCHAR) {
  DSValue src_dsval;
  std::string geo_str = "POLYGON((120 14, 121 14, 121 15, 120 15, 120 14))";
  StringToDSValue(geo_str, src_dsval);

  SQLWCHAR dest_buf[100];
  SQLLEN result_len;
  DataBuffer dest_data{SQL_C_WCHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromGeographyDSValue(src_dsval, dest_data);

  ASSERT_TRUE(status.ok());

  auto returned_val =
      ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(dest_data.buf),
                              result_len / sizeof(SQLWCHAR));
  EXPECT_EQ(returned_val.GetValue().c_str(), geo_str);
}

TEST(ConvertFromGeographyDSValue, TO_SQL_C_Binary) {
  DSValue src_dsval;
  std::string geo_str = "POINT(7.67999999999928 12.4)";
  StringToDSValue(geo_str, src_dsval);

  char dest_buf[100];
  SQLLEN res_len;

  DataBuffer dest_data{SQL_C_BINARY, dest_buf, sizeof(dest_buf), &res_len};
  auto status = ConvertFromGeographyDSValue(src_dsval, dest_data);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(res_len, (SQLLEN)geo_str.size());

  std::string returned_val = reinterpret_cast<char*>(dest_data.buf);
  EXPECT_EQ(returned_val, geo_str);
}

TEST(ConvertFromBytesDSValueTest, HandlesBinaryConversion) {
  std::string input = "SGk=";  // "Hi" in Base64
  DSValue src_dsval;
  StringToDSValue(input, src_dsval);
  DataBuffer dest_data;
  DSValue buffer(2);
  dest_data.buf = buffer.data();
  dest_data.buflen = buffer.size();
  dest_data.type = SQL_C_BINARY;
  dest_data.result_len = nullptr;

  StatusRecord status = ConvertFromBytesDSValue(src_dsval, dest_data);

  EXPECT_TRUE(status.ok());
  EXPECT_EQ(std::string(reinterpret_cast<char*>(dest_data.buf), 2), "Hi");
}

TEST(ConvertFromBytesDSValueTest, HandlesCharConversion) {
  std::string input = "VGVzdA==";  // "Test" in Base64
  DSValue src_dsval;
  StringToDSValue(input, src_dsval);
  DataBuffer dest_data;
  std::vector<char> buffer(22);
  dest_data.buf = buffer.data();
  dest_data.buflen = buffer.size();
  dest_data.type = SQL_C_CHAR;
  dest_data.result_len = nullptr;

  StatusRecord status = ConvertFromBytesDSValue(src_dsval, dest_data);

  EXPECT_TRUE(status.ok());
  EXPECT_STREQ(buffer.data(), "54657374");  // SQL_C_CHAR returns data is hex
}

TEST(ConvertFromBytesDSValueTest, HandlesNullBuffer) {
  std::string input = "SEVMTE9XT1JMRA==";  // "Hello world" in Base64
  DSValue src_dsval;
  StringToDSValue(input, src_dsval);
  DataBuffer dest_data;
  dest_data.buf = nullptr;
  dest_data.buflen = 20;
  dest_data.type = SQL_C_BINARY;
  dest_data.result_len = nullptr;

  StatusRecord status = ConvertFromBytesDSValue(src_dsval, dest_data);

  EXPECT_EQ(status.sql_state, SQLStates::k_HY090());
}

TEST(ConvertFromBytesDSValueTest, HandlesNegativeBufferLength) {
  std::string input = "SEVMTE9XT1JMRA==";  // "Hello world" in Base64
  DSValue src_dsval;
  StringToDSValue(input, src_dsval);
  DataBuffer dest_data;
  DSValue buffer(20);
  dest_data.buf = buffer.data();
  dest_data.buflen = -1;  // Invalid length
  dest_data.type = SQL_C_BINARY;
  dest_data.result_len = nullptr;

  StatusRecord status = ConvertFromBytesDSValue(src_dsval, dest_data);

  EXPECT_EQ(status.sql_state, SQLStates::k_HY090());
}

TEST(ConvertFromBytesDSValueTest, HandlesUnsupportedType) {
  std::string input = "SEVMTE9XT1JMRA==";  // "Hello world" in Base64
  DSValue src_dsval;
  StringToDSValue(input, src_dsval);
  DataBuffer dest_data;
  DSValue buffer(20);
  dest_data.buf = buffer.data();
  dest_data.buflen = buffer.size();
  dest_data.type = SQL_C_DOUBLE;  // Unsupported type
  dest_data.result_len = nullptr;

  StatusRecord status = ConvertFromBytesDSValue(src_dsval, dest_data);

  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
}

TEST(ConvertFromArrayDSValue, To_SQL_C_CHAR_success) {
  char buf[100];
  SQLLEN data_len;
  DataBuffer data = {SQL_C_CHAR, buf, 100, &data_len};
  DSValue ds_value;
  std::string src_val = R"({"v":[{"v":"121"},{"v":"123"},{"v":"1212"}]})";
  std::string expected_val =
      "{\"v\":[{\"v\":\"121\"},{\"v\":\"123\"},{\"v\":\"1212\"}]}";
  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromArrayDSValue(ds_value, data);
  std::string returned_val = (char*)data.buf;
  EXPECT_EQ(returned_val, expected_val);
  EXPECT_EQ(data_len, expected_val.length());
}

TEST(ConvertFromArrayDSValue, To_SQL_C_CHAR_InsufficientBuffer) {
  char buf[10];
  SQLLEN data_len;
  SQLLEN buf_len = 10;
  DataBuffer data = {SQL_C_CHAR, buf, buf_len, &data_len};
  DSValue ds_value;
  std::string src_val = R"({"v":[{"v":"121"},{"v":"123"},{"v":"1212"}]})";
  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromArrayDSValue(ds_value, data);
  EXPECT_EQ(data_len, buf_len - 1);
  EXPECT_THAT(
      status_record,
      StatusRecIs(SQLStates::k_01004(), StrEq("String data, right truncated")));
}

TEST(ConvertFromArrayDSValue, To_SQL_C_WCHAR_success) {
  SQLWCHAR dest_buf[100] = {0};
  SQLLEN data_len;
  DataBuffer data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf), &data_len};
  DSValue ds_value;
  std::string src_val = R"({"v":[{"v":"121"},{"v":"123"},{"v":"1212"}]})";
  std::string expected_val =
      "{\"v\":[{\"v\":\"121\"},{\"v\":\"123\"},{\"v\":\"1212\"}]}";
  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromArrayDSValue(ds_value, data);
  SQLINTEGER length = data_len / sizeof(SQLWCHAR);
  StatusRecordOr<std::string> returned_val =
      ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(dest_buf), length);
  EXPECT_EQ(length, expected_val.length());
  EXPECT_STREQ(returned_val->c_str(), expected_val.c_str());
}

TEST(ConvertFromArrayDSValue, convertToWchar_InsufficientBuffer) {
  DSValue ds_value;
  std::string src_val = R"({"v":[{"v":"121"},{"v":"123"},{"v":"1212"}]})";
  StringToDSValue(src_val, ds_value);
  char dest_buf[10];
  DataBuffer dest_data = {SQL_C_WCHAR, dest_buf, sizeof(dest_buf), nullptr};
  auto status = ConvertFromArrayDSValue(ds_value, dest_data);
  EXPECT_EQ(status.sql_state, odbc_internal::SQLStates::k_22003());
  ASSERT_FALSE(status.ok());
}

TEST(ConvertFromArrayDSValue, To_SQL_C_BINARY_success) {
  char buf[100] = {0};
  SQLLEN data_len;
  DataBuffer data = {SQL_C_BINARY, buf, 100, &data_len};
  DSValue ds_value;
  std::string src_val = R"({"v":[{"v":"121"},{"v":"123"},{"v":"1212"}]})";
  std::string expected_val =
      "{\"v\":[{\"v\":\"121\"},{\"v\":\"123\"},{\"v\":\"1212\"}]}";
  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromArrayDSValue(ds_value, data);
  std::string returned_val = (char*)data.buf;
  EXPECT_EQ(returned_val, expected_val);
  EXPECT_EQ(data_len, expected_val.length());
}

TEST(ConvertFromArrayDSValue, To_SQL_C_BINARY_InsufficientBuffer) {
  char buf[10];
  SQLLEN data_len;
  SQLLEN buf_len = 10;
  DataBuffer data = {SQL_C_BINARY, buf, buf_len, &data_len};
  DSValue ds_value;
  std::string src_val = R"({"v":[{"v":"121"},{"v":"123"},{"v":"1212"}]})";
  StringToDSValue(src_val, ds_value);
  StatusRecord status_record = ConvertFromArrayDSValue(ds_value, data);
  EXPECT_EQ(data_len, buf_len - 1);
  EXPECT_THAT(
      status_record,
      StatusRecIs(SQLStates::k_01004(), StrEq("Binary data, right truncated")));
}

TEST(ConvertFromRangeDSValueTest, ValidDateRangeConversionToSQL_C_CHAR) {
  DSValue src_dsval;
  StringToDSValue("[2024-01-01, 2024-12-31)", src_dsval);
  char buffer[50];
  DataBuffer dest_data{SQL_C_CHAR, buffer, sizeof(buffer), nullptr};

  StatusRecord status = ConvertFromRangeDSValue(src_dsval, dest_data);

  EXPECT_TRUE(status.ok());
  EXPECT_STREQ(buffer, "[2024-01-01, 2024-12-31)");
}

TEST(ConvertFromRangeDSValueTest, ValidTimeStampRangeConversionToSQL_C_CHAR) {
  DSValue src_dsval;
  StringToDSValue("[1708432245.000000, 1710944130.000425)", src_dsval);
  char buffer[100];
  DataBuffer dest_data{SQL_C_CHAR, buffer, sizeof(buffer), nullptr};

  StatusRecord status = ConvertFromRangeDSValue(src_dsval, dest_data);

  EXPECT_TRUE(status.ok());
  EXPECT_STREQ(buffer,
               "[2024-02-20 12:30:45.000000, 2024-03-20 14:15:30.000425)");
}

TEST(ConvertFromRangeDSValueTest, ValidDateRangeConversionToSQL_C_BINARY) {
  DSValue src_dsval;
  StringToDSValue("[2024-01-01, 2024-12-31)", src_dsval);

  char buffer[50];
  DataBuffer dest_data{SQL_C_BINARY, buffer, sizeof(buffer), nullptr};

  StatusRecord status = ConvertFromRangeDSValue(src_dsval, dest_data);

  EXPECT_TRUE(status.ok());
  EXPECT_STREQ(buffer, "[2024-01-01, 2024-12-31)");
}

TEST(ConvertFromRangeDSValueTest, ValidTimeStampRangeConversionToSQL_C_BINARY) {
  DSValue src_dsval;
  StringToDSValue("[1708432245.000000, 1710944130.000425)", src_dsval);
  char buffer[100];
  DataBuffer dest_data{SQL_C_BINARY, buffer, sizeof(buffer), nullptr};

  StatusRecord status = ConvertFromRangeDSValue(src_dsval, dest_data);

  EXPECT_TRUE(status.ok());
  std::string expected =
      "[2024-02-20 12:30:45.000000, 2024-03-20 14:15:30.000425)";
#ifdef _WIN32
  expected.append(" ");
#else
  expected.append(":");
#endif  //_WIN32
  EXPECT_EQ(buffer, expected);
}

TEST(ConvertFromRangeDSValueTest, ValidDateRangeConversionToSQL_C_WCHAR) {
  DSValue src_dsval;
  StringToDSValue("[2024-01-01, 2024-12-31)", src_dsval);

  SQLWCHAR dest_buf[100];
  SQLLEN result_len;
  DataBuffer dest_data{SQL_C_WCHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromRangeDSValue(src_dsval, dest_data);

  ASSERT_TRUE(status.ok());

  auto returned =
      ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(dest_data.buf),
                              result_len / sizeof(SQLWCHAR));
  std::string expected = "[2024-01-01, 2024-12-31)";
  EXPECT_EQ(returned.GetValue().c_str(), expected);
}

TEST(ConvertFromRangeDSValueTest, ValidTimeStampRangeConversionToSQL_C_WCHAR) {
  DSValue src_dsval;
  StringToDSValue("[1708432245.000000, 1710944130.000425)", src_dsval);

  SQLWCHAR dest_buf[100];
  SQLLEN result_len;
  DataBuffer dest_data{SQL_C_WCHAR, dest_buf, sizeof(dest_buf), &result_len};
  auto status = ConvertFromRangeDSValue(src_dsval, dest_data);

  ASSERT_TRUE(status.ok());

  auto returned =
      ConvertSQLWCHARToString(reinterpret_cast<SQLWCHAR*>(dest_data.buf),
                              result_len / sizeof(SQLWCHAR));
  std::string expected =
      "[2024-02-20 12:30:45.000000, 2024-03-20 14:15:30.000425)";
  EXPECT_EQ(returned.GetValue().c_str(), expected);
}

TEST(ConvertFromRangeDSValueTest, BufferTooSmall) {
  DSValue src_dsval;
  StringToDSValue("2024/01/01 - 2024/12/31", src_dsval);
  char buffer[5];  // Too small buffer
  DataBuffer dest_data{SQL_C_CHAR, buffer, sizeof(buffer), nullptr};

  StatusRecord status = ConvertFromRangeDSValue(src_dsval, dest_data);

  EXPECT_FALSE(status.ok());
}

TEST(ConvertFromBytesDSValue, WCharDataExactFit) {
  std::string input = "YWIA";  // Base64 string
  DSValue source_dsval;

  StringToDSValue(input, source_dsval);
  DataBuffer dest_data;
  std::vector<SQLWCHAR> dest_buf(8, 0);
  dest_data.buf = dest_buf.data();
  dest_data.buflen = dest_buf.size() * sizeof(SQLWCHAR);
  SQLLEN result_len = 0;
  dest_data.result_len = &result_len;
  dest_data.type = SQL_C_WCHAR;

  auto status = ConvertFromBytesDSValue(source_dsval, dest_data);
  ASSERT_TRUE(status.ok());
  ASSERT_EQ(std::wstring(reinterpret_cast<wchar_t*>(dest_buf.data())),
            L"616200");  // SQL_C_WCHAR returns data is hex
}

TEST(ConvertFromBytesDSValue, WCharDataWithTruncation) {
  std::string input = "SEVMTE9XT1JMRA==";  // "Hello world" in Base64
  DSValue source_dsval;
  StringToDSValue(input, source_dsval);

  DataBuffer dest_data;
  std::vector<SQLWCHAR> dest_buf(4);
  dest_data.buf = dest_buf.data();
  dest_data.buflen = 4 * sizeof(SQLWCHAR);
  SQLLEN result_len = 0;
  dest_data.result_len = &result_len;
  dest_data.type = SQL_C_WCHAR;

  auto status = ConvertFromBytesDSValue(source_dsval, dest_data);

  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.sql_state, SQLStates::k_01004());
}

TEST(ConvertFromBytesDSValue, WCharDataEmptyInput) {
  std::string input = "";  // Empty Base64 string
  DSValue source_dsval;
  StringToDSValue(input, source_dsval);

  DataBuffer dest_data;
  std::vector<SQLWCHAR> dest_buf(8, 0);
  dest_data.buf = dest_buf.data();
  dest_data.buflen = dest_buf.size() * sizeof(SQLWCHAR);
  SQLLEN result_len = 0;
  dest_data.result_len = &result_len;
  dest_data.type = SQL_C_WCHAR;

  auto status = ConvertFromBytesDSValue(source_dsval, dest_data);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.sql_state, SQLStates::k_01004());
}

TEST(ConvertFromBytesDSValue, WCharDataNullBuffer) {
  std::string input = "YWIA";  // Base64 string
  DSValue source_dsval;
  StringToDSValue(input, source_dsval);

  DataBuffer dest_data;
  dest_data.buf = nullptr;
  dest_data.buflen = 8 * sizeof(SQLWCHAR);
  SQLLEN result_len = 0;
  dest_data.result_len = &result_len;
  dest_data.type = SQL_C_WCHAR;

  auto status = ConvertFromBytesDSValue(source_dsval, dest_data);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.sql_state, SQLStates::k_HY090());  // Null buffer error
}

TEST(ConvertFromBytesDSValue, WCharDataNegativeBufferLength) {
  std::string input = "YWIA";  // Base64 string
  DSValue source_dsval;
  StringToDSValue(input, source_dsval);

  DataBuffer dest_data;
  std::vector<SQLWCHAR> dest_buf(8, 0);
  dest_data.buf = dest_buf.data();
  dest_data.buflen = -1;  // Invalid buffer length
  SQLLEN result_len = 0;
  dest_data.result_len = &result_len;
  dest_data.type = SQL_C_WCHAR;

  auto status = ConvertFromBytesDSValue(source_dsval, dest_data);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.sql_state,
            SQLStates::k_HY090());  // Negative buffer length error
}
}  // namespace google::cloud::odbc_bq_driver_internal
