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

#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_attr.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

TEST(ValidateStatementAttributeToSet, Fails_InvalidAttribute) {
  StatusRecord status_record = ValidateStatementAttributeToSet(1111, 1111);

  EXPECT_EQ(SQLStates::k_HY092(), status_record.sql_state);
}

TEST(ValidateStatementAttributeToSet, ReturnValid_SQL_ASYNC_ENABLE_OFF) {
  StatusRecord status_record = ValidateStatementAttributeToSet(
      SQL_ATTR_ASYNC_ENABLE, SQL_ASYNC_ENABLE_OFF);

  EXPECT_TRUE(status_record.ok());
}

TEST(ValidateStatementAttributeToSet, ReturnValid_SQL_ASYNC_ENABLE_ON) {
  StatusRecord status_record = ValidateStatementAttributeToSet(
      SQL_ATTR_ASYNC_ENABLE, SQL_ASYNC_ENABLE_ON);

  EXPECT_TRUE(status_record.ok());
}

TEST(ValidateStatementAttributeToSet, ReturnInvalid_SQL_ATTR_ASYNC_ENABLE) {
  StatusRecord status_record =
      ValidateStatementAttributeToSet(SQL_ATTR_ASYNC_ENABLE, 1111);

  EXPECT_EQ(SQLStates::k_HY024(), status_record.sql_state);
}

}  // namespace google::cloud::odbc_bq_driver_internal
