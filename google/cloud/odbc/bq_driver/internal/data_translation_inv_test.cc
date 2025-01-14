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
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Numeric value out of range");
}

TEST(ConvertFromBuffer, From_SQL_C_SBIGINT) {
  SQLBIGINT value = 12345;
  SQLLEN data_size = sizeof(SQLBIGINT);
  DataBuffer data = {SQL_C_SBIGINT, &value, 0, &data_size};
  StatusRecordOr<std::string> conv_status;

  conv_status = ConvertFromBuffer(data, SQL_CHAR);
  EXPECT_EQ(std::to_string(value), *conv_status);
  
  conv_status = ConvertFromBuffer(data, SQL_FLOAT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stol(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_DOUBLE);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(value, std::stol(*conv_status));

  conv_status = ConvertFromBuffer(data, SQL_BIGINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLBIGINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_SMALLINT);
  ASSERT_STATUS_RECORD_OK(conv_status);
  EXPECT_EQ(std::to_string((SQLSMALLINT)value), *conv_status);

  conv_status = ConvertFromBuffer(data, SQL_TINYINT);
  EXPECT_FALSE(conv_status);
  EXPECT_EQ(conv_status.GetStatusRecord().sql_state, SQLStates::k_22003());
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Numeric value out of range");
}

TEST(ConvertFromBuffer, From_SQL_C_CHAR_basic) {
  std::string value = "Testing String";
  SQLCHAR cstr[50];
  strcpy(reinterpret_cast<char *>(cstr), value.c_str());
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
  strcpy(reinterpret_cast<char *>(cstr), value.c_str());
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
  EXPECT_EQ(conv_status.GetStatusRecord().message, "Numeric value out of range");
}


}  // namespace google::cloud::odbc_bq_driver_internal
