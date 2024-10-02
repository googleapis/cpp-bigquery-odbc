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

struct TestingResultSetRow {
  SQLBIGINT int_field;
  SQLDOUBLE double_field;
  std::string str_field;
};

// It is hard to define null values in these data types
// To keep the tests simple, we define these magic numbers to assume null values
const SQLBIGINT kNullInt = 31415926;
const SQLDOUBLE kNullDouble = 3141.5926;
std::string const kNullStr = "3.1415926";

std::vector<TestingResultSetRow> const kTestingResultSetValues = {
    {9223372036854775807, /* highest int64 */
     7.123, "12a"},
    {12, 1.2, "c12b"},
    {13, kNullDouble, "12c"},
    {14, 1.4, kNullStr},
    {15, 1.5, kNullStr},
    {kNullInt, kNullDouble, kNullStr}};

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
                           }};
  for (TestingResultSetRow rs_row : kTestingResultSetValues) {
    DSValue int_val, double_val, str_val;
    if (rs_row.int_field == kNullInt) {
      int_val = kNullValue;
    } else {
      ArithmeticToDSValue<SQLBIGINT>(rs_row.int_field, int_val);
    }
    if (rs_row.double_field == kNullDouble) {
      double_val = kNullValue;
    } else {
      ArithmeticToDSValue<SQLDOUBLE>(rs_row.double_field, double_val);
    }
    if (rs_row.str_field == kNullStr) {
      str_val = kNullValue;
    } else {
      StringToDSValue(rs_row.str_field, str_val);
    }
    DSRow ds_row{int_val, double_val, str_val};
    result_set.rows.emplace_back(ds_row);
  }
}

TEST(WriteRowset, Success_Basic) {
  SQLRETURN status;
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLCHAR int_buf[20], double_buf[20], str_buf[20];
  SQLLEN strlen_ind_int, strlen_ind_double, strlen_ind_str;
  status = SQLBindColInternal(&stmt_handle, 1, SQL_C_SBIGINT, int_buf, 20,
                              &strlen_ind_int);
  ASSERT_EQ(SQL_SUCCESS, status);

  status = SQLBindColInternal(&stmt_handle, 2, SQL_C_DOUBLE, double_buf, 20,
                              &strlen_ind_double);
  ASSERT_EQ(SQL_SUCCESS, status);

  status = SQLBindColInternal(&stmt_handle, 3, SQL_C_CHAR, str_buf, 20,
                              &strlen_ind_str);
  ASSERT_EQ(SQL_SUCCESS, status);

  ResultSet result_set;
  CreateTestingResultSet(result_set);

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);

  SQLBIGINT* int_populated = (SQLBIGINT*)int_buf;
  SQLDOUBLE* double_populated = (SQLDOUBLE*)double_buf;
  for (int i = 0; i < kTestingResultSetValues.size(); i++) {
    result_set.cursor++;
    StatusRecord status_record = WriteRowset(result_set, 1, ard);
    EXPECT_TRUE(status_record.ok());
    SQLBIGINT int_expected = kTestingResultSetValues[i].int_field;
    if (int_expected == kNullInt) {
      EXPECT_EQ(strlen_ind_int, SQL_NULL_DATA);
    } else {
      EXPECT_EQ(*int_populated, int_expected);
    }
    SQLDOUBLE double_expected = kTestingResultSetValues[i].double_field;
    if (double_expected == kNullDouble) {
      EXPECT_EQ(strlen_ind_double, SQL_NULL_DATA);
    } else {
      EXPECT_EQ(*double_populated, double_expected);
    }
    std::string str_populated = (char*)(SQLCHAR*)str_buf;
    std::string str_expected = kTestingResultSetValues[i].str_field;
    if (str_expected == kNullStr) {
      EXPECT_EQ(strlen_ind_str, SQL_NULL_DATA);
    } else {
      EXPECT_EQ(str_populated, str_expected);
    }
  }
}

TEST(WriteRowset, Success_WithOffset) {
  SQLRETURN status;
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLLEN bound_offset = 8;
  SQLCHAR int_buf[20];
  SQLLEN strlen_ind_int, strlen_ind_double, strlen_ind_str;
  status = SQLBindColInternal(&stmt_handle, 1, SQL_C_SBIGINT, int_buf, 20,
                              &strlen_ind_int);
  ASSERT_EQ(SQL_SUCCESS, status);

  ResultSet result_set;
  CreateTestingResultSet(result_set);

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);
  ard.GetHeaderRecord().bind_offset_ptr = &bound_offset;

  SQLBIGINT* int_populated = (SQLBIGINT*)(int_buf + bound_offset);
  for (int i = 0; i < kTestingResultSetValues.size(); i++) {
    result_set.cursor++;
    StatusRecord status_record = WriteRowset(result_set, 1, ard);
    EXPECT_TRUE(status_record.ok());
    SQLBIGINT int_expected = kTestingResultSetValues[i].int_field;
    if (int_expected == kNullInt) {
      EXPECT_EQ(strlen_ind_int, SQL_NULL_DATA);
    } else {
      EXPECT_EQ(*int_populated, int_expected);
    }
  }
}

// Null StrLen_or_IndPtr should cause error during SQLFetch if there were any
// null values in the result set
TEST(WriteRowset, Success_Fail_NullIndicator) {
  SQLRETURN status;
  StatementHandle stmt_handle = CreateStatementHandle();
  SQLCHAR int_buf[20];
  status =
      SQLBindColInternal(&stmt_handle, 1, SQL_C_SBIGINT, int_buf, 20, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  // Create result set for a single column and single row
  ResultSet result_set = {{{
      0,
      BQDataType::kInt64,
  }}};
  result_set.rows.emplace_back(DSRow{kNullValue});

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);

  SQLBIGINT* int_populated = (SQLBIGINT*)int_buf;
  result_set.cursor++;
  StatusRecord status_record = WriteRowset(result_set, 1, ard);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22002(), status_record.sql_state);
  EXPECT_EQ("Indicator variable required but not supplied",
            status_record.message);
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
  result_set.cursor++;
  StatusRecord status_record = WriteRowset(result_set, 1, ard);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_22003(), status_record.sql_state);
  EXPECT_EQ("Numeric value out of range", status_record.message);
  EXPECT_EQ(result_set.cursor, 0);
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
  result_set.cursor++;
  StatusRecord status_record = WriteRowset(result_set, 1, ard);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(SQLStates::k_01S07(), status_record.sql_state);
  EXPECT_EQ("Fractional truncation", status_record.message);
  EXPECT_EQ(result_set.cursor, 0);
  SQLINTEGER* double_populated = (SQLINTEGER*)double_buf;
  EXPECT_EQ(*double_populated, floor(kTestingResultSetValues[0].double_field));
}

}  // namespace google::cloud::odbc_bq_driver_internal
