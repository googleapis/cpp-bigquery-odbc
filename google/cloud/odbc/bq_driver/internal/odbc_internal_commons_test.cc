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

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

struct NativeDataTypesStruct {
  bool flag;
  char character;
  short short_var;
  int int_var;
  long long_var;
  long long long_long_var;
  float float_var;
  double double_var;
};

TEST(DSValue, Basic_String) {
  std::string expected = "Some string which should be converted to DSValue";
  DSValue value;
  StringToDSValue(expected, value);

  std::string returned;

  DSValueToString(value, returned);
  EXPECT_EQ(expected, returned);
}

TEST(DSValue, Basic_ComplexStruct) {
  DSValue bq_value(sizeof(NativeDataTypesStruct));

  NativeDataTypesStruct custom_data = {
      true, 'A', 100, 12345, 1234567890L, 98765432101234LL, 3.14f, 2.71828};
  memcpy(bq_value.data(), &custom_data, sizeof(NativeDataTypesStruct));

  NativeDataTypesStruct* expected =
      reinterpret_cast<NativeDataTypesStruct*>(bq_value.data());
  EXPECT_EQ(custom_data.flag, expected->flag);
  EXPECT_EQ(custom_data.character, expected->character);
  EXPECT_EQ(custom_data.short_var, expected->short_var);
  EXPECT_EQ(custom_data.int_var, expected->int_var);
  EXPECT_EQ(custom_data.long_var, expected->long_var);
  EXPECT_EQ(custom_data.long_long_var, expected->long_long_var);
  EXPECT_EQ(custom_data.float_var, expected->float_var);
  EXPECT_EQ(custom_data.double_var, expected->double_var);
}

TEST(DSValue, Basic_Int) {
  SQLINTEGER expected = 10;
  DSValue value;
  IntToDSValue(expected, value);

  SQLINTEGER actual;

  actual = DSValueToInt(value);
  EXPECT_EQ(expected, actual);
}

}  // namespace google::cloud::odbc_bq_driver_internal
