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
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace google::cloud::odbc_bq_driver_internal {

using odbc_internal::SQLStates;
using odbc_internal::StatusRecordOr;

TEST(ConvertFromBuffer, From_SQL_C_FLOAT) {
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

TEST(ConvertFromBuffer, From_SQL_C_DOUBLE) {
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

TEST(ConvertFromBuffer, From_SQL_C_SBIGINT) {
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

TEST(ConvertFromBuffer, From_SQL_C_UBIGINT) {
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

TEST(ConvertFromBuffer, From_SQL_C_SSHORT) {
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

TEST(ConvertFromBuffer, From_SQL_C_USHORT) {
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

TEST(ConvertFromBuffer, From_SQL_C_SLONG) {
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

TEST(ConvertFromBuffer, From_SQL_C_ULONG) {
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

TEST(ConvertFromBuffer, From_SQL_C_CHAR_basic) {
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

TEST(ConvertFromBuffer, From_SQL_C_CHAR_ArithmeticStr) {
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

TEST(ConvertFromBuffer, From_SQL_C_WCHAR_Basic) {
  std::wstring value = L"Testing WChar String";
  SQLWCHAR wstr[50];
  std::copy(value.begin(), value.end(), reinterpret_cast<wchar_t*>(wstr));
  SQLLEN data_size = value.size() * sizeof(SQLWCHAR);
  DataBuffer data = {SQL_C_WCHAR, wstr, 50 * sizeof(SQLWCHAR), &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_WCHAR);
  EXPECT_TRUE(conv_status);
  auto utf8_value = Utf16ToUtf8(value);
  ASSERT_TRUE(utf8_value);
  EXPECT_EQ(*utf8_value, *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_INTEGER);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Conversion is unsupported");
}

TEST(ConvertFromBuffer, From_SQL_C_WCHAR_InvalidBufferLength) {
  SQLWCHAR wstr[10] = {0};
  SQLLEN data_size = -1;  // Invalid length
  DataBuffer data = {SQL_C_WCHAR, wstr, 10 * sizeof(SQLWCHAR), &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Invalid buffer length");
}

TEST(ConvertFromBuffer, From_SQL_C_BIT) {
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
  EXPECT_EQ(*conv_status, "0.000000");

  conv_status = ConvertFromBuffer(bit_buf_true, SQL_SMALLINT);
  EXPECT_EQ(*conv_status, "1");

  conv_status = ConvertFromBuffer(bit_buf_false, SQL_REAL);
  EXPECT_EQ(*conv_status, "0.000000");

  conv_status = ConvertFromBuffer(bit_buf_true, SQL_DOUBLE);
  EXPECT_EQ(*conv_status, "1.000000");

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

TEST(ConvertFromBuffer, From_SQL_C_Binary) {
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
                                 data_size_first, 0};
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
}  // namespace google::cloud::odbc_bq_driver_internal
