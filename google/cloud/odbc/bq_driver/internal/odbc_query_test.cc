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
  SQL_TIMESTAMP_STRUCT timestamp_field;
  SQL_DATE_STRUCT date_field;
  SQL_TIME_STRUCT time_field;
  nlohmann::json json_field;
};

// It is hard to define null values in these data types
// To keep the tests simple, we define these magic numbers to assume null values
const SQLBIGINT kNullInt = 31415926;
const SQLDOUBLE kNullDouble = 3141.5926;
std::string const kNullStr = "3.1415926";

std::vector<ResultSetRow> const kResultSetValues = {
    {9223372036854775807, /* highest int64 */
     7.123,
     "12a",
     {2024, 01, 20, 10, 20, 30, 123112},
     {2024, 2, 20},
     {11, 9, 20},
     {{"age", 30}, {"name", "Sita"}}},
    {12,
     1.2,
     "c12b",
     {2024, 01, 20, 11, 2, 33, 1212},
     {2024, 3, 12},
     {22, 45, 54},
     {{"age", 30}, {"name", "Alice"}}},
    {13,
     kNullDouble,
     "12c",
     {2024, 01, 20, 2, 20, 22, 123123},
     {2024, 4, 20},
     {2, 36, 29},
     {{"age", 90}, {"name", "Ram"}}},
    {14,
     1.4,
     kNullStr,
     {2024, 07, 20, 2, 20, 22, 123123},
     {2024, 4, 29},
     {9, 07, 20},
     {{"age", 26}, {"name", "Bob"}}},
    {15,
     1.5,
     kNullStr,
     {2024, 01, 20, 00, 00, 00, 000000},
     {2024, 7, 20},
     {04, 06, 07},
     {{"age", 32}, {"name", "Kapoor"}}},
    {kNullInt,
     kNullDouble,
     kNullStr,
     {2024, 10, 20, 00, 00, 00, 000000},
     {2024, 04, 20},
     {04, 06, 07},
     {{"age", 30}, {"name", "robin"}}}};

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
                           },
                           {
                               3,
                               BQDataType::kTimeStamp,
                           },
                           {
                               4,
                               BQDataType::kDate,
                           },
                           {
                               5,
                               BQDataType::kTime,
                           },
                           {
                               6,
                               BQDataType::kJson,
                           }};
  for (ResultSetRow rs_row : kResultSetValues) {
    DSValue int_val, double_val, str_val, timestamp_val, date_val, time_val,
        json_val;
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
    TimestampToDSValue(rs_row.timestamp_field, timestamp_val);

    DateToDSValue(rs_row.date_field, date_val);
    TimeToDSValue(rs_row.time_field, time_val);
    StringToDSValue(rs_row.json_field.dump(), json_val);

    DSRow ds_row{int_val,  double_val, str_val, timestamp_val,
                 date_val, time_val,   json_val};
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
      SQLCHAR buf[20] = {0};
      SQLLEN target_str_len;

      StatusRecord status_record = GetColumnData(
          ds_val, bq_data_type, SQL_C_CHAR, buf, 1024, &target_str_len);
      EXPECT_TRUE(status_record.ok());
      std::string result_str = reinterpret_cast<char*>(buf);
      std::string expected_str;
      if (i == 0) {
        if (kResultSetValues[cursor].int_field != kNullInt) {
          expected_str = std::to_string(kResultSetValues[cursor].int_field);
          EXPECT_STREQ(result_str.data(), expected_str.data());
        } else {
          EXPECT_EQ(target_str_len, SQL_NULL_DATA);
        }
      } else if (i == 1) {
        if (kResultSetValues[cursor].double_field != kNullDouble) {
          expected_str = std::to_string(kResultSetValues[cursor].double_field);
          EXPECT_STREQ(result_str.data(), expected_str.data());
        } else {
          EXPECT_EQ(target_str_len, SQL_NULL_DATA);
        }
      } else if (i == 2) {
        if (kResultSetValues[cursor].str_field != kNullStr) {
          expected_str = kResultSetValues[cursor].str_field;
          EXPECT_STREQ(result_str.data(), expected_str.data());
        } else {
          EXPECT_EQ(target_str_len, SQL_NULL_DATA);
        }
      } else if (i == 3) {
        expected_str =
            FormatTimestampToString(kResultSetValues[cursor].timestamp_field);
        EXPECT_STREQ(result_str.data(), expected_str.data());

      } else if (i == 4) {
        snprintf(expected_str.data(), 10, "%04d-%02d-%02d",
                 kResultSetValues[cursor].date_field.year,
                 kResultSetValues[cursor].date_field.month,
                 kResultSetValues[cursor].date_field.day);
        // expected_str = kResultSetValues[cursor].date_field;
        EXPECT_STREQ(result_str.data(), expected_str.data());
      } else if (i == 5) {
        expected_str = FormatTimetoString(kResultSetValues[cursor].time_field);
        EXPECT_STREQ(result_str.data(), expected_str.data());
      } else if (i == 6) {
        expected_str = kResultSetValues[cursor].json_field.dump();
        EXPECT_STREQ(result_str.data(), expected_str.data());
      }
    }
    cursor++;
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
   // namespace google::cloud::odbc_bq_driver_internal
