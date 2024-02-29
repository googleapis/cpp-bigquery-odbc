// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_UTILS_STATUS_MATCHERS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_UTILS_STATUS_MATCHERS_H

#include <gmock/gmock.h>

namespace google::cloud::odbc_testing_utils {

#define ASSERT_STATUS_OK(expression) \
  ASSERT_TRUE(expression.ok())       \
      << "Error message: " << expression.status().message() << "\n"

#define ASSERT_STATUS_RECORD_OK(expression) \
  ASSERT_TRUE(expression.Ok())              \
      << "Error message: " << expression.GetStatusRecord().message << "\n"

MATCHER_P2(StatusIs, code, matcher, "") {
  EXPECT_EQ(arg.status().code(), code)
      << "Expected code to be: " << StatusCodeToString(code)
      << ", but was: " << StatusCodeToString(arg.status().code());
  EXPECT_THAT(arg.status().message(), matcher);
  return true;
}

MATCHER_P2(StatusRecordIs, sql_state, matcher, "") {
  EXPECT_EQ(arg.GetStatusRecord().sql_state, sql_state)
      << "Expected code to be: " << sql_state
      << ", but was: " << arg.GetStatusRecord().sql_state;
  EXPECT_THAT(arg.GetStatusRecord().message, matcher);
  return true;
}

}  // namespace google::cloud::odbc_testing_utils

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_UTILS_STATUS_MATCHERS_H
