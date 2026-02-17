// Copyright 2025 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/data_translation_inv.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

using ::fuzztest::Arbitrary;
using ::fuzztest::InRange;
using odbc_internal::SQLStates;
using odbc_internal::StatusRecordOr;

TEST(ConvertFromBuffer, FromSqlCFloat) {
  SQLREAL value = 12345.67;
  SQLLEN data_size = sizeof(SQLREAL);
  DataBuffer data = {SQL_C_FLOAT, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_VARCHAR);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_REAL);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLSMALLINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCDouble) {
  SQLDOUBLE value = 123456789123.45;
  SQLLEN data_size = sizeof(SQLDOUBLE);
  DataBuffer data = {SQL_C_DOUBLE, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_VARCHAR);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  // We have to use EXPECT_NEAR here because value is too large to be accurately
  // represented as float
  EXPECT_NEAR(value, stof(*conv_status), 10000);

  conv_status = ConvertFromBuffer(data, SQL_REAL);
  ASSERT_STATUS_RECORD_OK(conv_status);
  // We have to use EXPECT_NEAR here because value is too large to be accurately
  // represented as float
  EXPECT_NEAR(value, stof(*conv_status), 10000);

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCSbigint) {
  SQLBIGINT value = -12345;
  SQLLEN data_size = sizeof(SQLBIGINT);
  DataBuffer data = {SQL_C_SBIGINT, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stof(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stod(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLSMALLINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCUbigint) {
  SQLUBIGINT value = 12345;
  SQLLEN data_size = sizeof(SQLUBIGINT);
  DataBuffer data = {SQL_C_UBIGINT, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stof(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stod(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLSMALLINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCSshort) {
  SQLSMALLINT value = -12345;
  SQLLEN data_size = sizeof(SQLSMALLINT);
  DataBuffer data = {SQL_C_SSHORT, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stof(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stod(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLSMALLINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCUshort) {
  SQLUSMALLINT value = 12345;
  SQLLEN data_size = sizeof(SQLUSMALLINT);
  DataBuffer data = {SQL_C_USHORT, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stof(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stod(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLSMALLINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCSlong) {
  SQLINTEGER value = -12345678912;
  SQLLEN data_size = sizeof(SQLINTEGER);
  DataBuffer data = {SQL_C_SLONG, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stof(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stod(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCUlong) {
  SQLUINTEGER value = 12345678912;
  SQLLEN data_size = sizeof(SQLUINTEGER);
  DataBuffer data = {SQL_C_ULONG, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(std::to_string(value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  // We have to use EXPECT_NEAR here because value is too large to be accurately
  // represented as float
  EXPECT_NEAR(value, stof(*conv_status), 10000);

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stod(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCCharBasic) {
  std::string value = "Testing String";
  SQLCHAR cstr[50];
  strcpy(reinterpret_cast<char*>(cstr), value.c_str());
  SQLLEN data_size = value.size();
  DataBuffer data = {SQL_C_CHAR, cstr, 50, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(value, *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Conversion is unsupported");
}

TEST(ConvertFromBuffer, FromSqlCCharArithmeticStr) {
  std::string value = "12345.67";
  SQLCHAR cstr[50];
  strcpy(reinterpret_cast<char*>(cstr), value.c_str());
  SQLLEN data_size = value.size();
  DataBuffer data = {SQL_C_CHAR, cstr, 50, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(value, *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  EXPECT_NEAR(std::stod(value), std::stod(*conv_status), 1e-2);

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  EXPECT_NEAR(std::stol(value), std::stol(*conv_status), 1e-2);

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Numeric value out of range");
}

TEST(ConvertFromBuffer, FromSqlCWcharBasic) {
  std::wstring value = L"Testing WChar String";
  SQLWCHAR wstr[50] = {0};
  std::copy(value.begin(), value.end(), wstr);
  wstr[value.size()] = 0;

  // Case 1: Explicitly set buffer length
  SQLLEN data_size = value.size() * sizeof(SQLWCHAR);
  DataBuffer data = {SQL_C_WCHAR, wstr, 50 * sizeof(SQLWCHAR), &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_WCHAR);
  EXPECT_TRUE(conv_status);

  auto utf16_value = Utf8ToUtf16(*conv_status);
  ASSERT_TRUE(utf16_value);
  EXPECT_STREQ(utf16_value->c_str(), value.c_str());

  // // Case 2: SQL_NTS
  data_size = SQL_NTS;
  DataBuffer data_nts = {SQL_C_WCHAR, wstr, 50 * sizeof(SQLWCHAR), &data_size};
  conv_status = ConvertFromBuffer(data_nts, SQL_WCHAR);
  EXPECT_TRUE(conv_status);

  auto utf16_value_nts = Utf8ToUtf16(*conv_status);
  ASSERT_TRUE(utf16_value_nts);
  EXPECT_STREQ(utf16_value_nts->c_str(), value.c_str());

  // Case 3: Test unsupported conversion
  conv_status = ConvertFromBuffer(data, SQL_INTEGER);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Conversion is unsupported");
}

TEST(ConvertFromBuffer, FromSqlCWcharInvalidBufferLength) {
  SQLWCHAR wstr[10] = {0};
  SQLLEN data_size = -1;  // Invalid length
  DataBuffer data = {SQL_C_WCHAR, wstr, 10 * sizeof(SQLWCHAR), &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Invalid buffer length");
}

TEST(ConvertFromBuffer, FromSqlCBit) {
  SQLCHAR bit_val_false = 0;
  SQLCHAR bit_val_true = 1;
  SQLCHAR bit_val_invalid = 2;

  SQLLEN data_size = sizeof(SQLCHAR);
  StatusRecordOr<std::string> conv_status;

  DataBuffer bit_buf_false = {SQL_C_BIT, &bit_val_false, sizeof(SQLCHAR),
                              &data_size};
  DataBuffer bit_buf_true = {SQL_C_BIT, &bit_val_true, sizeof(SQLCHAR),
                             &data_size};
  DataBuffer bit_buf_invalid = {SQL_C_BIT, &bit_val_invalid, sizeof(SQLCHAR),
                                &data_size};

  conv_status = ConvertFromBuffer(bit_buf_false, SQL_CHAR);
  EXPECT_EQ(*conv_status, "false");

  conv_status = ConvertFromBuffer(bit_buf_true, SQL_VARCHAR);
  EXPECT_EQ(*conv_status, "true");

  conv_status = ConvertFromBuffer(bit_buf_false, SQL_BIT);
  EXPECT_EQ(*conv_status, "false");

  conv_status = ConvertFromBuffer(bit_buf_true, SQL_INTEGER);
  EXPECT_EQ(*conv_status, "1");

  conv_status = ConvertFromBuffer(bit_buf_false, SQL_FLOAT);
  EXPECT_NEAR(std::stof(*conv_status), 0.0F, 1e-6);

  conv_status = ConvertFromBuffer(bit_buf_true, SQL_SMALLINT);
  EXPECT_EQ(*conv_status, "1");

  conv_status = ConvertFromBuffer(bit_buf_false, SQL_REAL);
  EXPECT_NEAR(std::stof(*conv_status), 0.0F, 1e-6);

  conv_status = ConvertFromBuffer(bit_buf_true, SQL_DOUBLE);
  EXPECT_NEAR(std::stof(*conv_status), 1.0F, 1e-6);

  conv_status = ConvertFromBuffer(bit_buf_false, SQL_DATE);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Conversion is unsupported");

  conv_status = ConvertFromBuffer(bit_buf_invalid, SQL_FLOAT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message,
            "Invalid BIT value (must be 0 or 1)");
}

TEST(ConvertFromBuffer, FromSqlCBinary) {
  uint8_t binary_data_first[] = {0x12, 0x34, 0x56, 0x78};
  uint8_t binary_data_sec[] = {0xAB, 0xCD, 0xEF};
  uint8_t* binary_data_null = nullptr;

  SQLLEN data_size_first = sizeof(binary_data_first);
  SQLLEN data_size_sec = sizeof(binary_data_sec);
  DataBuffer binary_buf_first = {SQL_C_BINARY, binary_data_first,
                                 data_size_first, &data_size_first};
  DataBuffer binary_buf_sec = {SQL_C_BINARY, binary_data_sec, data_size_sec,
                               &data_size_sec};

  StatusRecordOr<std::string> conversion_result;
  conversion_result = ConvertFromBuffer(binary_buf_first, SQL_VARCHAR);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "EjRWeA==");

  conversion_result = ConvertFromBuffer(binary_buf_sec, SQL_LONGVARCHAR);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "q83v");

  conversion_result = ConvertFromBuffer(binary_buf_first, SQL_BINARY);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "EjRWeA==");

  conversion_result = ConvertFromBuffer(binary_buf_sec, SQL_LONGVARBINARY);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "q83v");

  conversion_result = ConvertFromBuffer(binary_buf_first, SQL_VARBINARY);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "EjRWeA==");

  DataBuffer binary_buf_empty = {SQL_C_BINARY, binary_data_first,
                                 data_size_first, nullptr};
  conversion_result = ConvertFromBuffer(binary_buf_empty, SQL_VARCHAR);
  EXPECT_FALSE(conversion_result);
  EXPECT_EQ(conversion_result.GetStatusRecord().sql_state,
            SQLStates::k_HY000());
  EXPECT_EQ(conversion_result.GetStatusRecord().message, "Invalid binary data");

  conversion_result = ConvertFromBuffer(binary_buf_sec, SQL_INTEGER);
  EXPECT_FALSE(conversion_result);
  EXPECT_EQ(conversion_result.GetStatusRecord().sql_state,
            SQLStates::k_HY000());
  EXPECT_EQ(conversion_result.GetStatusRecord().message,
            "Conversion is unsupported");
}

TEST(ConvertFromBuffer, FromSqlNumericToAllTypes) {
  SQL_NUMERIC_STRUCT numeric_base = {};
  numeric_base.scale = 1;
  numeric_base.precision = 4;
  numeric_base.sign = 1;
  int64_t scaled_val = 1235;

  for (size_t i = 0; i < sizeof(numeric_base.val); ++i) {
    numeric_base.val[i] =
        (i < sizeof(scaled_val))
            ? static_cast<unsigned char>((scaled_val >> (i * 8)) & 0xFF)
            : 0;
  }
  SQLLEN data_size = sizeof(SQL_NUMERIC_STRUCT);
  DataBuffer data = {SQL_C_NUMERIC, &numeric_base, 0, &data_size};

  // SQL_REAL
  auto conv_real = ConvertFromBuffer(data, SQL_REAL);
  ASSERT_STATUS_RECORD_OK(conv_real);
  EXPECT_EQ(*conv_real, "123.500000");

  // SQL_FLOAT
  auto conv_float = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_float);
  EXPECT_EQ(*conv_float, "123.500000");

  // SQL_DOUBLE
  auto conv_double = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_double);
  EXPECT_EQ(*conv_double, "123.500000");

  // SQL_INTEGER
  auto conv_int = ConvertFromBuffer(data, SQL_INTEGER);
  ASSERT_STATUS_RECORD_OK(conv_int);
  EXPECT_EQ(*conv_int, "123");

  // SQL_BIGINT
  auto conv_bigint = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_bigint);
  EXPECT_EQ(*conv_bigint, "123");

  // SQL_BIT (invalid case)
  {
    SQL_NUMERIC_STRUCT bit_numeric_invalid = {2, 0, 1, {2}};
    DataBuffer bit_data_invalid = {SQL_C_NUMERIC, &bit_numeric_invalid, 0,
                                   &data_size};
    auto conv_status = ConvertFromBuffer(bit_data_invalid, SQL_BIT);
    ASSERT_FALSE(conv_status);
    EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  }
  // Unsupported type
  {
    auto conv_status = ConvertFromBuffer(data, SQL_TYPE_DATE);
    ASSERT_FALSE(conv_status);
    EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  }
}

TEST(ConvertFromBuffer, FromSqlCTinyint) {
  // SQL_C_TINYINT
  {
    SQLCHAR value = 200;
    DataBuffer data = {SQL_C_TINYINT, &value, 0, nullptr};
    StatusRecordOr<std::string> conv_status;

    conv_status = ConvertFromBuffer(data, SQL_CHAR);
    EXPECT_EQ(std::to_string(value), *conv_status);

    conv_status = ConvertFromBuffer(data, SQL_FLOAT);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(value, std::stof(*conv_status));

    conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(value, std::stod(*conv_status));

    conv_status = ConvertFromBuffer(data, SQL_BIGINT);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);
  }

  // SQL_C_STINYINT
  {
    SQLSCHAR value = -100;
    DataBuffer data = {SQL_C_STINYINT, &value, 0, nullptr};
    StatusRecordOr<std::string> conv_status;

    conv_status = ConvertFromBuffer(data, SQL_CHAR);
    EXPECT_EQ(std::to_string(value), *conv_status);

    conv_status = ConvertFromBuffer(data, SQL_FLOAT);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(value, std::stof(*conv_status));

    conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(value, std::stod(*conv_status));

    conv_status = ConvertFromBuffer(data, SQL_BIGINT);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);
  }

  // SQL_C_UTINYINT
  {
    SQLCHAR value = 250;
    DataBuffer data = {SQL_C_UTINYINT, &value, 0, nullptr};
    StatusRecordOr<std::string> conv_status;

    conv_status = ConvertFromBuffer(data, SQL_CHAR);
    EXPECT_EQ(std::to_string(value), *conv_status);

    conv_status = ConvertFromBuffer(data, SQL_FLOAT);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(value, std::stof(*conv_status));

    conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(value, std::stod(*conv_status));

    conv_status = ConvertFromBuffer(data, SQL_BIGINT);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);
  }
}

TEST(ConvertFromBuffer, FromSqlCTypeDate) {
  SQL_DATE_STRUCT date_data_first = {2024, 5, 13};
  SQL_DATE_STRUCT date_data_sec = {1999, 12, 31};

  SQLLEN data_size = sizeof(SQL_DATE_STRUCT);

  DataBuffer date_buf_first = {SQL_C_TYPE_DATE, &date_data_first, data_size,
                               &data_size};

  DataBuffer date_buf_sec = {SQL_C_TYPE_DATE, &date_data_sec, data_size,
                             &data_size};

  StatusRecordOr<std::string> conversion_result;

  // SQL_TYPE_DATE
  conversion_result = ConvertFromBuffer(date_buf_first, SQL_TYPE_DATE);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "2024-05-13");

  // SQL_CHAR
  conversion_result = ConvertFromBuffer(date_buf_sec, SQL_CHAR);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "1999-12-31");

  // SQL_WCHAR
  conversion_result = ConvertFromBuffer(date_buf_first, SQL_WCHAR);
  EXPECT_TRUE(conversion_result);
  EXPECT_EQ(*conversion_result, "2024-05-13");

  // Unsupported target SQL type (SQL_INTEGER)
  conversion_result = ConvertFromBuffer(date_buf_first, SQL_INTEGER);
  EXPECT_FALSE(conversion_result);
  EXPECT_EQ(conversion_result.GetStatusRecord().sql_state,
            SQLStates::k_HY000());
  EXPECT_EQ(conversion_result.GetStatusRecord().message,
            "Conversion is unsupported");
}

TEST(ConvertFromBuffer, FromSqlCTypeTime) {
  TIME_STRUCT ts_val = {14, 30, 45};
  SQLLEN data_size = sizeof(TIME_STRUCT);
  DataBuffer data = {SQL_C_TYPE_TIME, &ts_val, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  std::string const expected_time = "14:30:45";

  // Supported timestamp-compatible types
  std::vector<SQLSMALLINT> supported_types = {
      SQL_TYPE_TIME, SQL_CHAR,     SQL_VARCHAR,      SQL_LONGVARCHAR,
      SQL_WCHAR,     SQL_WVARCHAR, SQL_WLONGVARCHAR,
  };

  for (SQLSMALLINT sql_type : supported_types) {
    conv_status = ConvertFromBuffer(data, sql_type);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, expected_time);
  }

  // Unsupported SQL type (e.g., SQL_INTEGER)
  conv_status = ConvertFromBuffer(data, SQL_INTEGER);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Conversion is unsupported");
}

TEST(ConvertFromBuffer, FromSqlCTypeTimestamp) {
  TIMESTAMP_STRUCT ts_val = {2024, 5, 9, 14, 30, 45, 123456};
  SQLLEN data_size = sizeof(TIMESTAMP_STRUCT);
  DataBuffer data = {SQL_C_TYPE_TIMESTAMP, &ts_val, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  std::string const expected_timestamp = "2024-05-09 14:30:45.123456";

  // Supported timestamp-compatible types
  std::vector<SQLSMALLINT> supported_types = {
      SQL_TYPE_TIMESTAMP, SQL_CHAR,     SQL_VARCHAR,      SQL_LONGVARCHAR,
      SQL_WCHAR,          SQL_WVARCHAR, SQL_WLONGVARCHAR,
  };

  for (SQLSMALLINT sql_type : supported_types) {
    conv_status = ConvertFromBuffer(data, sql_type);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, expected_timestamp);
  }

  conv_status = ConvertFromBuffer(data, SQL_TYPE_DATE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(*conv_status, "2024-05-09");

  conv_status = ConvertFromBuffer(data, SQL_TYPE_TIME);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(*conv_status, "14:30:45");

  // Unsupported SQL type (e.g., SQL_INTEGER)
  conv_status = ConvertFromBuffer(data, SQL_INTEGER);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Conversion is unsupported");
}

TEST(ConvertFromBuffer, FromSqlCYearMonthIntervalStruct) {
  SQLLEN data_size = sizeof(SQL_INTERVAL_STRUCT);
  StatusRecordOr<std::string> conv_status;
  // 1. SQL_CHAR
  {
    SQL_INTERVAL_STRUCT interval = {SQL_IS_YEAR_TO_MONTH, 0, {2, 6}};
    DataBuffer data = {SQL_C_INTERVAL_YEAR_TO_MONTH, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_CHAR);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "2-6 0 0:0:0");
  }
  // 2. SQL_INTERVAL_YEAR
  {
    SQL_INTERVAL_STRUCT interval = {SQL_IS_YEAR, 0, {5, 0}};  // 5 years
    DataBuffer data = {SQL_C_INTERVAL_YEAR, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_INTERVAL_YEAR);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "5-0 0 0:0:0");
  }
  // 3. SQL_INTERVAL_MONTH
  {
    SQL_INTERVAL_STRUCT interval = {SQL_IS_MONTH, 0, {0, 15}};  // 15 months
    DataBuffer data = {SQL_C_INTERVAL_MONTH, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_INTERVAL_MONTH);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "0-15 0 0:0:0");
  }
  // 4. SQL_INTERVAL_YEAR
  {
    SQL_INTERVAL_STRUCT interval = {SQL_IS_MONTH, 0, {0, 8}};  // Wrong type
    DataBuffer data = {SQL_C_INTERVAL_YEAR, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_INTERVAL_YEAR);
    EXPECT_FALSE(conv_status);
    EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
    EXPECT_EQ(conv_status.GetStatusRecord().message,
              "Invalid Year Interval value");
  }
  // 5. Unsupported conversion
  {
    SQL_INTERVAL_STRUCT interval = {SQL_IS_YEAR_TO_MONTH, 0, {1, 1}};
    DataBuffer data = {SQL_C_INTERVAL_YEAR_TO_MONTH, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, -999);  // Invalid type
    EXPECT_FALSE(conv_status);
    EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
    EXPECT_EQ(conv_status.GetStatusRecord().message,
              "Conversion is unsupported");
  }
}

TEST(ConvertFromBuffer, FromSqlCDaySecondIntervalStruct) {
  SQLLEN data_size = sizeof(SQL_INTERVAL_STRUCT);
  StatusRecordOr<std::string> conv_status;
  // 1. SQL_CHAR
  {
    SQL_INTERVAL_STRUCT interval = {
        SQL_IS_DAY_TO_SECOND,
        0,
        {.day_second = {3, 4, 5, 6,
                        789000}}  // 3 days, 4 hrs, 5 mins, 6.789 secs
    };
    DataBuffer data = {SQL_C_INTERVAL_DAY_TO_SECOND, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_CHAR);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "0-0 3 4:5:6.000789000");
  }
  // 2. SQL_INTERVAL_DAY
  {
    SQL_INTERVAL_STRUCT interval = {
        SQL_IS_DAY, 0, {.day_second = {10, 0, 0, 0, 0}}  // 10 days
    };
    DataBuffer data = {SQL_C_INTERVAL_DAY, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_INTERVAL_DAY);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "0-0 10 0:0:0");
  }
  // 3. SQL_INTERVAL_SECOND
  {
    SQL_INTERVAL_STRUCT interval = {
        SQL_IS_SECOND,
        0,
        {.day_second = {0, 0, 0, 15, 123000}}  // 15.123 seconds
    };
    DataBuffer data = {SQL_C_INTERVAL_SECOND, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_INTERVAL_SECOND);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "0-0 0 0:0:15.000123000");
  }
  // 4. Invalid type mismatch
  {
    SQL_INTERVAL_STRUCT interval = {
        SQL_IS_MINUTE,  // real type is MINUTE
        0,
        {.day_second = {0, 0, 40, 0, 0}}  // 40 minutes
    };
    DataBuffer data = {SQL_C_INTERVAL_HOUR, &interval, 0,
                       &data_size};  // expected HOUR
    conv_status = ConvertFromBuffer(data, SQL_INTERVAL_HOUR);
    EXPECT_FALSE(conv_status);
    EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
    EXPECT_EQ(conv_status.GetStatusRecord().message,
              "Invalid Hour Interval value");
  }
  // 5. Unsupported SQL type
  {
    SQL_INTERVAL_STRUCT interval = {
        SQL_IS_DAY_TO_SECOND, 0, {.day_second = {1, 2, 3, 4, 0}}  // 1d 2h 3m 4s
    };
    DataBuffer data = {SQL_C_INTERVAL_DAY_TO_SECOND, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, -999);  // Invalid SQL type
    EXPECT_FALSE(conv_status);
    EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
    EXPECT_EQ(conv_status.GetStatusRecord().message,
              "Conversion is unsupported");
  }
}

TEST(ConvertFromBuffer, FromSqlCIntervalSinglePrecision) {
  SQLLEN data_size = sizeof(SQL_INTERVAL_STRUCT);
  StatusRecordOr<std::string> conv_status;
  {  // SQL_INTERVAL_DAY
    SQL_INTERVAL_STRUCT interval = {
        SQL_IS_DAY, 0, {.day_second = {3, 0, 0, 0, 0}}};
    DataBuffer data = {SQL_C_INTERVAL_DAY, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_DECIMAL);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "3");
  }
  {  // SQL_INTERVAL_HOUR
    SQL_INTERVAL_STRUCT interval = {
        SQL_IS_HOUR, 0, {.day_second = {0, 12, 0, 0, 0}}};  // 12 hours
    DataBuffer data = {SQL_C_INTERVAL_HOUR, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "12");
  }
  {  // SQL_INTERVAL_YEAR
    SQL_INTERVAL_STRUCT interval = {SQL_IS_YEAR, 0, {5, 0}};  // 5 years
    DataBuffer data = {SQL_C_INTERVAL_YEAR, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_INTEGER);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "5");
  }
  {  // SQL_INTERVAL_MONTH
    SQL_INTERVAL_STRUCT interval = {SQL_IS_MONTH, 0, {0, 4}};
    DataBuffer data = {SQL_C_INTERVAL_YEAR, &interval, 0, &data_size};
    conv_status = ConvertFromBuffer(data, SQL_NUMERIC);
    ASSERT_STATUS_RECORD_OK(conv_status);
    EXPECT_EQ(*conv_status, "4");
  }
}

// Helper to manage aligned buffers in fuzz tests.
struct ScopedAlignedBuffer {
  std::vector<uint64_t> aligned_buffer;

  explicit ScopedAlignedBuffer(size_t size_in_bytes) {
    size_t vec_size = (size_in_bytes + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    if (vec_size == 0) {
      vec_size = 1;
    }
    aligned_buffer.resize(vec_size, 0);
  }

  void* data() { return reinterpret_cast<void*>(aligned_buffer.data()); }
  [[nodiscard]] size_t size_bytes() const {
    return aligned_buffer.size() * sizeof(uint64_t);
  }
};

SQLSMALLINT IntervalCType(SQLINTERVAL interval_type) {
  switch (interval_type) {
    case SQL_IS_YEAR:
      return SQL_C_INTERVAL_YEAR;
    case SQL_IS_MONTH:
      return SQL_C_INTERVAL_MONTH;
    case SQL_IS_YEAR_TO_MONTH:
      return SQL_C_INTERVAL_YEAR_TO_MONTH;
    case SQL_IS_DAY:
      return SQL_C_INTERVAL_DAY;
    case SQL_IS_HOUR:
      return SQL_C_INTERVAL_HOUR;
    case SQL_IS_MINUTE:
      return SQL_C_INTERVAL_MINUTE;
    case SQL_IS_SECOND:
      return SQL_C_INTERVAL_SECOND;
    case SQL_IS_DAY_TO_HOUR:
      return SQL_C_INTERVAL_DAY_TO_HOUR;
    case SQL_IS_DAY_TO_MINUTE:
      return SQL_C_INTERVAL_DAY_TO_MINUTE;
    case SQL_IS_DAY_TO_SECOND:
      return SQL_C_INTERVAL_DAY_TO_SECOND;
    case SQL_IS_HOUR_TO_MINUTE:
      return SQL_C_INTERVAL_HOUR_TO_MINUTE;
    case SQL_IS_HOUR_TO_SECOND:
      return SQL_C_INTERVAL_HOUR_TO_SECOND;
    case SQL_IS_MINUTE_TO_SECOND:
      return SQL_C_INTERVAL_MINUTE_TO_SECOND;
    default:
      return SQL_C_INTERVAL_DAY;
  }
}

void FuzzConvertFromArithmetic(int64_t signed_value, uint64_t unsigned_value,
                               double double_value, float float_value,
                               SQLSMALLINT c_type, SQLSMALLINT dest_type) {
  SQLLEN result_len = 0;
  DataBuffer src_data = {c_type, nullptr, 0, &result_len};

  switch (c_type) {
    case SQL_C_FLOAT: {
      auto value = static_cast<SQLREAL>(float_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_DOUBLE: {
      auto value = static_cast<SQLDOUBLE>(double_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_SBIGINT: {
      auto value = static_cast<SQLBIGINT>(signed_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_UBIGINT: {
      auto value = static_cast<SQLUBIGINT>(unsigned_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_SLONG: {
      auto value = static_cast<SQLINTEGER>(signed_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_ULONG: {
      auto value = static_cast<SQLUINTEGER>(unsigned_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_SSHORT: {
      auto value = static_cast<SQLSMALLINT>(signed_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_USHORT: {
      auto value = static_cast<SQLUSMALLINT>(unsigned_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_STINYINT: {
      auto value = static_cast<SQLSCHAR>(signed_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    case SQL_C_TINYINT:
    case SQL_C_UTINYINT: {
      auto value = static_cast<SQLCHAR>(unsigned_value);
      result_len = sizeof(value);
      src_data.buf = &value;
      src_data.buflen = sizeof(value);
      ConvertFromBuffer(src_data, dest_type);
      break;
    }
    default:
      break;
  }
}

void FuzzConvertFromChar(std::string const& input_str, SQLSMALLINT dest_type,
                         int buffer_size, bool use_nts) {
  size_t buf_size = NormalizeBufferSize(buffer_size);
  ScopedAlignedBuffer buffer(buf_size);
  auto* buf = static_cast<char*>(buffer.data());

  size_t copy_len = std::min(input_str.size(), buf_size);
  std::fill(buf, buf + buf_size, 0);
  std::copy_n(input_str.data(), copy_len, buf);

  SQLLEN result_len = use_nts ? SQL_NTS : static_cast<SQLLEN>(copy_len);
  DataBuffer src_data{SQL_C_CHAR, buf, static_cast<SQLLEN>(buf_size),
                      &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromWchar(std::string const& input_str, SQLSMALLINT dest_type,
                          int buffer_size, bool use_nts) {
  size_t buf_size =
      BufferSizeForType(SQL_C_WCHAR, NormalizeBufferSize(buffer_size));
  auto wchar_count = std::max<size_t>(1, buf_size / sizeof(SQLWCHAR));
  std::vector<SQLWCHAR> buffer(wchar_count, 0);

  auto utf16_val = Utf8ToUtf16(input_str);
  std::wstring wide = utf16_val ? *utf16_val : L"";
  size_t copy_len =
      std::min(wide.size(), wchar_count > 0 ? wchar_count - 1 : 0);
  std::copy_n(wide.data(), copy_len, buffer.data());
  buffer[copy_len] = 0;

  SQLLEN result_len =
      use_nts ? SQL_NTS : static_cast<SQLLEN>(copy_len * sizeof(SQLWCHAR));
  DataBuffer src_data{SQL_C_WCHAR, buffer.data(), static_cast<SQLLEN>(buf_size),
                      &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromBinary(std::string const& input_str, SQLSMALLINT dest_type,
                           int buffer_size) {
  size_t buf_size =
      BufferSizeForType(SQL_C_BINARY, NormalizeBufferSize(buffer_size, 512));
  ScopedAlignedBuffer buffer(buf_size);
  auto* buf = static_cast<uint8_t*>(buffer.data());

  size_t copy_len = std::min(input_str.size(), buf_size);
  std::fill(buf, buf + buf_size, 0);
  std::copy_n(reinterpret_cast<uint8_t const*>(input_str.data()), copy_len,
              buf);

  SQLLEN result_len = static_cast<SQLLEN>(copy_len);
  DataBuffer src_data{SQL_C_BINARY, buf, static_cast<SQLLEN>(buf_size),
                      &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromNumeric(uint64_t raw_value, uint8_t precision,
                            uint8_t scale, uint8_t sign,
                            SQLSMALLINT dest_type) {
  SQL_NUMERIC_STRUCT numeric_struct = {};
  numeric_struct.precision = precision;
  numeric_struct.scale = static_cast<SQLSCHAR>(scale);
  numeric_struct.sign = static_cast<SQLCHAR>(sign % 2);

  for (size_t i = 0; i < sizeof(numeric_struct.val); ++i) {
    numeric_struct.val[i] =
        static_cast<unsigned char>((raw_value >> (i * 8)) & 0xFF);
  }

  SQLLEN result_len = sizeof(SQL_NUMERIC_STRUCT);
  DataBuffer src_data{SQL_C_NUMERIC, &numeric_struct, result_len, &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromBit(uint8_t bit_value, SQLSMALLINT dest_type) {
  auto value = static_cast<SQLCHAR>(bit_value);
  SQLLEN result_len = sizeof(value);
  DataBuffer src_data{SQL_C_BIT, &value, result_len, &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromDate(int16_t year, uint16_t month, uint16_t day,
                         SQLSMALLINT dest_type) {
  SQL_DATE_STRUCT date{year, month, day};
  SQLLEN result_len = sizeof(SQL_DATE_STRUCT);
  DataBuffer src_data{SQL_C_TYPE_DATE, &date, result_len, &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromTime(uint16_t hour, uint16_t minute, uint16_t second,
                         SQLSMALLINT dest_type) {
  SQL_TIME_STRUCT time{hour, minute, second};
  SQLLEN result_len = sizeof(SQL_TIME_STRUCT);
  DataBuffer src_data{SQL_C_TYPE_TIME, &time, result_len, &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromTimestamp(int16_t year, uint16_t month, uint16_t day,
                              uint16_t hour, uint16_t minute, uint16_t second,
                              uint32_t fraction, SQLSMALLINT dest_type) {
  TIMESTAMP_STRUCT timestamp{year, month, day, hour, minute, second, fraction};
  SQLLEN result_len = sizeof(TIMESTAMP_STRUCT);
  DataBuffer src_data{SQL_C_TYPE_TIMESTAMP, &timestamp, result_len,
                      &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

void FuzzConvertFromInterval(SQLINTERVAL interval_type, bool sign, int32_t year,
                             int32_t month, int32_t day, int32_t hour,
                             int32_t minute, int32_t second, int32_t fraction,
                             SQLSMALLINT dest_type) {
  SQL_INTERVAL_STRUCT interval = {};
  interval.interval_type = interval_type;
  interval.interval_sign = sign ? SQL_TRUE : SQL_FALSE;
  interval.intval.year_month.year = year;
  interval.intval.year_month.month = month;
  interval.intval.day_second.day = day;
  interval.intval.day_second.hour = hour;
  interval.intval.day_second.minute = minute;
  interval.intval.day_second.second = second;
  interval.intval.day_second.fraction = fraction;

  SQLLEN result_len = sizeof(SQL_INTERVAL_STRUCT);
  SQLSMALLINT c_type = IntervalCType(interval_type);
  DataBuffer src_data{c_type, &interval, result_len, &result_len};
  ConvertFromBuffer(src_data, dest_type);
}

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromArithmetic)
    .WithDomains(Arbitrary<int64_t>(), Arbitrary<uint64_t>(),
                 Arbitrary<double>(), Arbitrary<float>(),
                 fuzztest::ElementOf<SQLSMALLINT>(
                     {SQL_C_FLOAT, SQL_C_DOUBLE, SQL_C_SBIGINT, SQL_C_UBIGINT,
                      SQL_C_SLONG, SQL_C_ULONG, SQL_C_SSHORT, SQL_C_USHORT,
                      SQL_C_STINYINT, SQL_C_TINYINT, SQL_C_UTINYINT}),
                 fuzztest::ElementOf<SQLSMALLINT>({SQL_CHAR, SQL_VARCHAR,
                                                   SQL_LONGVARCHAR, SQL_REAL,
                                                   SQL_FLOAT, SQL_DOUBLE,
                                                   SQL_BIGINT, SQL_SMALLINT,
                                                   SQL_TINYINT, SQL_INTEGER}));

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromChar)
    .WithDomains(Arbitrary<std::string>(),
                 fuzztest::ElementOf<SQLSMALLINT>(
                     {SQL_CHAR, SQL_VARCHAR, SQL_WCHAR, SQL_WVARCHAR,
                      SQL_LONGVARCHAR, SQL_FLOAT, SQL_DOUBLE, SQL_BIGINT,
                      SQL_SMALLINT, SQL_TINYINT, SQL_INTEGER}),
                 Arbitrary<int>(), Arbitrary<bool>());

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromWchar)
    .WithDomains(Arbitrary<std::string>(),
                 fuzztest::ElementOf<SQLSMALLINT>(
                     {SQL_WCHAR, SQL_WVARCHAR, SQL_CHAR, SQL_VARCHAR,
                      SQL_LONGVARCHAR, SQL_FLOAT, SQL_DOUBLE, SQL_BIGINT,
                      SQL_SMALLINT, SQL_TINYINT, SQL_INTEGER}),
                 Arbitrary<int>(), Arbitrary<bool>());

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromBinary)
    .WithDomains(Arbitrary<std::string>(),
                 fuzztest::ElementOf<SQLSMALLINT>({SQL_CHAR, SQL_VARCHAR,
                                                   SQL_LONGVARCHAR, SQL_WCHAR,
                                                   SQL_WVARCHAR, SQL_BINARY,
                                                   SQL_VARBINARY,
                                                   SQL_LONGVARBINARY}),
                 Arbitrary<int>());

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromNumeric)
    .WithDomains(Arbitrary<uint64_t>(), Arbitrary<uint8_t>(),
                 Arbitrary<uint8_t>(), Arbitrary<uint8_t>(),
                 fuzztest::ElementOf<SQLSMALLINT>(
                     {SQL_CHAR, SQL_VARCHAR, SQL_WCHAR, SQL_DECIMAL,
                      SQL_NUMERIC, SQL_REAL, SQL_FLOAT, SQL_DOUBLE, SQL_BIT,
                      SQL_TINYINT, SQL_SMALLINT, SQL_INTEGER, SQL_BIGINT,
                      SQL_INTERVAL_YEAR, SQL_INTERVAL_MONTH, SQL_INTERVAL_DAY,
                      SQL_INTERVAL_HOUR, SQL_INTERVAL_MINUTE,
                      SQL_INTERVAL_SECOND}));

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromBit)
    .WithDomains(Arbitrary<uint8_t>(),
                 fuzztest::ElementOf<SQLSMALLINT>(
                     {SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR, SQL_BIT,
                      SQL_INTEGER, SQL_SMALLINT, SQL_TINYINT, SQL_FLOAT,
                      SQL_REAL, SQL_DOUBLE, SQL_BIGINT}));

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromDate)
    .WithDomains(Arbitrary<int16_t>(), InRange<uint16_t>(0, 15),
                 InRange<uint16_t>(0, 40),
                 fuzztest::ElementOf<SQLSMALLINT>({SQL_CHAR, SQL_VARCHAR,
                                                   SQL_WCHAR, SQL_WVARCHAR,
                                                   SQL_TYPE_DATE,
                                                   SQL_TYPE_TIMESTAMP}));

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromTime)
    .WithDomains(InRange<uint16_t>(0, 30), InRange<uint16_t>(0, 90),
                 InRange<uint16_t>(0, 90),
                 fuzztest::ElementOf<SQLSMALLINT>({SQL_CHAR, SQL_VARCHAR,
                                                   SQL_WCHAR, SQL_WVARCHAR,
                                                   SQL_TYPE_TIME}));

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromTimestamp)
    .WithDomains(InRange<int16_t>(0, 9999), InRange<uint16_t>(0, 15),
                 InRange<uint16_t>(0, 40), InRange<uint16_t>(0, 30),
                 InRange<uint16_t>(0, 90), InRange<uint16_t>(0, 90),
                 InRange<uint32_t>(0, 999999),
                 fuzztest::ElementOf<SQLSMALLINT>(
                     {SQL_CHAR, SQL_VARCHAR, SQL_WCHAR, SQL_WVARCHAR,
                      SQL_TIMESTAMP, SQL_TYPE_TIMESTAMP, SQL_TYPE_DATE,
                      SQL_TYPE_TIME}));

FUZZ_TEST(DataTranslationInvFuzz, FuzzConvertFromInterval)
    .WithDomains(
        fuzztest::ElementOf<SQLINTERVAL>(
            {SQL_IS_YEAR, SQL_IS_MONTH, SQL_IS_YEAR_TO_MONTH, SQL_IS_DAY,
             SQL_IS_HOUR, SQL_IS_MINUTE, SQL_IS_SECOND, SQL_IS_DAY_TO_HOUR,
             SQL_IS_DAY_TO_MINUTE, SQL_IS_DAY_TO_SECOND, SQL_IS_HOUR_TO_MINUTE,
             SQL_IS_HOUR_TO_SECOND, SQL_IS_MINUTE_TO_SECOND}),
        Arbitrary<bool>(), Arbitrary<int32_t>(), Arbitrary<int32_t>(),
        Arbitrary<int32_t>(), Arbitrary<int32_t>(), Arbitrary<int32_t>(),
        Arbitrary<int32_t>(), Arbitrary<int32_t>(),
        fuzztest::ElementOf<SQLSMALLINT>({SQL_CHAR,
                                          SQL_VARCHAR,
                                          SQL_WCHAR,
                                          SQL_WVARCHAR,
                                          SQL_INTERVAL_YEAR,
                                          SQL_INTERVAL_MONTH,
                                          SQL_INTERVAL_YEAR_TO_MONTH,
                                          SQL_INTERVAL_DAY,
                                          SQL_INTERVAL_HOUR,
                                          SQL_INTERVAL_MINUTE,
                                          SQL_INTERVAL_SECOND,
                                          SQL_INTERVAL_DAY_TO_HOUR,
                                          SQL_INTERVAL_DAY_TO_MINUTE,
                                          SQL_INTERVAL_DAY_TO_SECOND,
                                          SQL_INTERVAL_HOUR_TO_MINUTE,
                                          SQL_INTERVAL_HOUR_TO_SECOND,
                                          SQL_INTERVAL_MINUTE_TO_SECOND,
                                          SQL_DECIMAL,
                                          SQL_NUMERIC,
                                          SQL_SMALLINT,
                                          SQL_TINYINT,
                                          SQL_INTEGER,
                                          SQL_BIGINT}));
}  // namespace google::cloud::odbc_bq_driver_internal
