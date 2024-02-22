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

#include "google/cloud/odbc/bq_driver/internal/odbc_statement_handle.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

TEST(StatementHandle, BindColumn_Basic) {
  SQLCHAR buf[10];
  SQLLEN* res_len;
  StatementHandle handle;
  EXPECT_EQ(handle.BindColumn(0, SQL_C_CHAR, buf, 10, res_len), SQL_SUCCESS);
  EXPECT_TRUE(handle.GetDiagnostics().GetStatusRecords().empty());
}

TEST(StatementHandle, BindColumn_NullBuffer) {
  SQLLEN* res_len;
  StatementHandle handle;
  ASSERT_EQ(handle.BindColumn(0, SQL_C_CHAR, nullptr, 10, res_len), SQL_ERROR);
  // TODO(#228): Test if diagnostic records are set properly
}

TEST(StatementHandle, BindColumn_BuflenLessThanZero) {
  SQLLEN* res_len;
  StatementHandle handle;
  ASSERT_EQ(handle.BindColumn(0, SQL_C_CHAR, nullptr, -1, res_len), SQL_ERROR);
  // TODO(#228): Test if diagnostic records are set properly
}

TEST(StatementHandle, BindColumn_NullResLen) {
  StatementHandle handle;
  ASSERT_EQ(handle.BindColumn(0, SQL_C_CHAR, nullptr, 10, nullptr), SQL_ERROR);
  // TODO(#228): Test if diagnostic records are set properly
}

}  // namespace google::cloud::odbc_bq_driver_internal
