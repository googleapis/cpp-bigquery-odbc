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

#include "google/cloud/odbc/bq_driver/internal/odbc_query.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

struct ResultSetRow {
  SQLBIGINT int_field;
  SQLDOUBLE double_field;
  std::string str_field;
};

// It is hard to define null values in these data types
// To keep the tests simple, we define these magic numbers to assume null values
const SQLBIGINT kNullInt = 31415926;
const SQLDOUBLE kNullDouble = 3141.5926;
std::string const kNullStr = "3.1415926";

std::vector<ResultSetRow> const kResultSetValues = {
    {9223372036854775807, /* highest int64 */
     7.123, "12a"},
    {12, 1.2, "c12b"},
    {13, 1.3, "12c"}};

void CreateResultSet(ResultSet& result_set) {
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
  for (ResultSetRow rs_row : kResultSetValues) {
    DSValue int_val, double_val, str_val;

    ArithmeticToDSValue<SQLBIGINT>(rs_row.int_field, int_val);

    ArithmeticToDSValue<SQLDOUBLE>(rs_row.double_field, double_val);

    StringToDSValue(rs_row.str_field, str_val);

    DSRow ds_row{int_val, double_val, str_val};
    result_set.rows.emplace_back(ds_row);
  }
}

TEST(GetColumnData, Success_MultipleRows) {
  SQLRETURN status;
  StatementHandle stmt_handle = CreateStatementHandle();
  constexpr int rs_size = 3;

  ResultSet result_set;
  CreateResultSet(result_set);

  DescriptorHandle& ard = stmt_handle.GetDescriptorHandle(DescriptorType::kARD);
  int cursor = ++result_set.cursor;
  while (cursor < kResultSetValues.size()) {
    for (int i = 0; i < rs_size; i++) {
      int column_number = i + 1;
      DSRow const& ds_row = result_set.rows[cursor];
      RowSchema const& schema = result_set.row_schema;
      BQDataType bq_data_type;
      for (auto const& col_schema : schema) {
        if (col_schema.col_index == column_number - 1)
          bq_data_type = col_schema.col_type;
      }
      DSValue const& ds_val = ds_row[column_number - 1];
      SQLCHAR buf[20];
      SQLLEN target_str_len;

      StatusRecord status_record = GetColumnData(
          ds_val, bq_data_type, SQL_C_CHAR, buf, 1024, &target_str_len);
      EXPECT_TRUE(status_record.ok());
      std::string result_str = (char*)buf;
      std::string expected_str;
      if (i == 0 && kResultSetValues[cursor].int_field != kNullInt) {
        expected_str = std::to_string(kResultSetValues[cursor].int_field);
      } else if (i == 1 &&
                 kResultSetValues[cursor].double_field != kNullDouble) {
        expected_str = std::to_string(kResultSetValues[cursor].double_field);
      } else if (i == 2 && kResultSetValues[cursor].str_field != kNullStr) {
        expected_str = kResultSetValues[cursor].str_field;
      }
      EXPECT_STREQ(result_str.data(), expected_str.data());
    }
    cursor++;
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
