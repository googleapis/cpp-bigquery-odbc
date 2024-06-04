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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

TypeInfoRow const kTestingTypeInfoRow = {
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("RANDOM_STR1")),  // type_name
    SQL_NUMERIC,                                           // data_type
    11,                                                    // col_size
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("RANDOM_STR2")),  // literal_prefix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("RANDOM_STR3")),  // literal_suffix
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("RANDOM_STR4")),  // create_params
    10,                                                    // nullable
    9,                                                     // case_sensitive
    8,                                                     // searchable
    7,                                                     // unsigned_attribute
    6,                                                     // fixed_prec_scale
    5,                                                     // auto_unique_value
    const_cast<SQLCHAR*>(
        reinterpret_cast<const SQLCHAR*>("RANDOM_STR5")),  // local_type_name
    4,                                                     // minimum_scale
    3,                                                     // maximum_scale
    SQL_NUMERIC,                                           // sql_data_type
    3,                                                     // sql_datetime_sub
    2,                                                     // num_prec_radix
    1,                                                     // interval_precision
};

TEST(CreateDSRowFromTypeInfo, Basic) {
  DSRow dsrow = CreateDSRowFromTypeInfo(kTestingTypeInfoRow);
  EXPECT_EQ(dsrow.size(), 19);

  std::string type_name_expected;
  DSValueToString(dsrow[0], type_name_expected);
  EXPECT_STREQ(type_name_expected.c_str(),
               reinterpret_cast<char const*>(kTestingTypeInfoRow.type_name));

  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[1]),
            kTestingTypeInfoRow.data_type);
  EXPECT_EQ(DSValueToArithmetic<SQLINTEGER>(dsrow[2]),
            kTestingTypeInfoRow.col_size);

  std::string literal_prefix_expected;
  DSValueToString(dsrow[3], literal_prefix_expected);
  EXPECT_STREQ(
      literal_prefix_expected.c_str(),
      reinterpret_cast<char const*>(kTestingTypeInfoRow.literal_prefix));

  std::string literal_suffix_expected;
  DSValueToString(dsrow[4], literal_suffix_expected);
  EXPECT_STREQ(
      literal_suffix_expected.c_str(),
      reinterpret_cast<char const*>(kTestingTypeInfoRow.literal_suffix));

  std::string create_params_expected;
  DSValueToString(dsrow[5], create_params_expected);
  EXPECT_STREQ(
      create_params_expected.c_str(),
      reinterpret_cast<char const*>(kTestingTypeInfoRow.create_params));

  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[6]),
            kTestingTypeInfoRow.nullable);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[7]),
            kTestingTypeInfoRow.case_sensitive);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[8]),
            kTestingTypeInfoRow.searchable);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[9]),
            kTestingTypeInfoRow.unsigned_attribute);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[10]),
            kTestingTypeInfoRow.fixed_prec_scale);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[11]),
            kTestingTypeInfoRow.auto_unique_value);

  std::string local_type_name_expected;
  DSValueToString(dsrow[12], local_type_name_expected);
  EXPECT_STREQ(
      local_type_name_expected.c_str(),
      reinterpret_cast<char const*>(kTestingTypeInfoRow.local_type_name));

  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[13]),
            kTestingTypeInfoRow.minimum_scale);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[14]),
            kTestingTypeInfoRow.maximum_scale);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[15]),
            kTestingTypeInfoRow.sql_data_type);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[16]),
            kTestingTypeInfoRow.sql_datetime_sub);
  EXPECT_EQ(DSValueToArithmetic<SQLINTEGER>(dsrow[17]),
            kTestingTypeInfoRow.num_prec_radix);
  EXPECT_EQ(DSValueToArithmetic<SQLSMALLINT>(dsrow[18]),
            kTestingTypeInfoRow.interval_precision);
}

}  // namespace google::cloud::odbc_bq_driver_internal
