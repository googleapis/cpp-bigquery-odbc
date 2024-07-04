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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fetch.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bq_driver::SQLBindColInternal;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;
using SQLDATE = std::string;
struct TestingResultSetRow {
  SQLBIGINT int_field;
  SQLDOUBLE double_field;
  std::string str_field;
  SQLDATE date_field;
};

std::vector<TestingResultSetRow> const kTestingResultSetValues = {
    {9223372036854775807, /* highest int64 */
     7.123, "12a", "2020-10-10"},
    {12, 1.2, "12a", "2020-10-10"}};

void CreateTestingResultSet(ResultSet& result_set) {
  result_set.row_schema = {{
                               0,
                               BQDataType::kInt64,
                           },
                           {
                               1,
                               BQDataType::kFloat64,
                           },
                           {
                               2,
                               BQDataType::kString,
                           },
                           {
                               3,
                               BQDataType::kDate,
                           }};
  for (TestingResultSetRow rs_row : kTestingResultSetValues) {
    DSValue int_val, double_val, str_val, date_val;
    ArithmeticToDSValue<SQLBIGINT>(rs_row.int_field, int_val);
    ArithmeticToDSValue<SQLDOUBLE>(rs_row.double_field, double_val);
    StringToDSValue(rs_row.str_field, str_val);
    DateToDSValue(rs_row.date_field, date_val);
    DSRow ds_row{int_val, double_val, str_val, date_val};
    result_set.rows.emplace_back(ds_row);
  }
}

TEST(WriteRowset, Success_Basic) {
  SQLRETURN status;
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLCHAR int_buf[20], double_buf[20], str_buf[20], date_buf[20];
  status =
      SQLBindColInternal(&stmt_handle, 1, SQL_C_SBIGINT, int_buf, 20, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  status = SQLBindColInternal(&stmt_handle, 2, SQL_C_DOUBLE, double_buf, 20,
                              nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  status =
      SQLBindColInternal(&stmt_handle, 3, SQL_C_CHAR, str_buf, 20, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  status = SQLBindColInternal(&stmt_handle, 4, SQL_C_TYPE_DATE, date_buf, 20,
                              nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  ResultSet result_set;
  CreateTestingResultSet(result_set);
  EXPECT_EQ(SQL_SUCCESS, status);

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);

  // Writing first row
  StatusRecord status_record = WriteRowset(result_set, 1, ard);
  EXPECT_TRUE(status_record.ok());
  SQLBIGINT* int_populated = (SQLBIGINT*)int_buf;
  EXPECT_EQ(*int_populated, kTestingResultSetValues[0].int_field);
  SQLDOUBLE* double_populated = (SQLDOUBLE*)double_buf;
  EXPECT_EQ(*double_populated, kTestingResultSetValues[0].double_field);
  std::string str_populated = (char*)(SQLCHAR*)str_buf;
  EXPECT_EQ(str_populated, kTestingResultSetValues[0].str_field);
  SQLDATE date_populated(reinterpret_cast<char*>(date_buf));
  EXPECT_EQ(date_populated, kTestingResultSetValues[0].date_field);

  // Writing second row
  status_record = WriteRowset(result_set, 1, ard);
  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(*int_populated, kTestingResultSetValues[1].int_field);
  EXPECT_EQ(*double_populated, kTestingResultSetValues[1].double_field);
  EXPECT_EQ(str_populated, kTestingResultSetValues[1].str_field);
  EXPECT_EQ(date_populated, kTestingResultSetValues[1].date_field);
}

TEST(WriteRowset, Failure_TranslationOutOfRange) {
  SQLRETURN status;
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLCHAR int_buf[20];
  status =
      SQLBindColInternal(&stmt_handle, 1, SQL_C_SSHORT, int_buf, 20, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  ResultSet result_set;
  CreateTestingResultSet(result_set);
  EXPECT_EQ(SQL_SUCCESS, status);

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);
  StatusRecord status_record = WriteRowset(result_set, 1, ard);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Numeric value out of range", status_record.message);
  EXPECT_EQ(result_set.cursor, 1);
}

TEST(WriteRowset, Failure_FractionalTruncation) {
  SQLRETURN status;
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLCHAR double_buf[20];

  // Here we are trying to translate the 2nd column from double to int
  status =
      SQLBindColInternal(&stmt_handle, 2, SQL_C_SLONG, double_buf, 20, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  ResultSet result_set;
  CreateTestingResultSet(result_set);
  EXPECT_EQ(SQL_SUCCESS, status);

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);
  StatusRecord status_record = WriteRowset(result_set, 1, ard);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01S07(), status_record.sql_state);
  EXPECT_EQ("Fractional truncation", status_record.message);
  EXPECT_EQ(result_set.cursor, 1);
  SQLINTEGER* double_populated = (SQLINTEGER*)double_buf;
  EXPECT_EQ(*double_populated, floor(kTestingResultSetValues[0].double_field));
}

}  // namespace google::cloud::odbc_bq_driver_internal
