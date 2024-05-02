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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_primary_keys.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Struct;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using ::google::cloud::bigquery_v2_minimal_internal::Value;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

namespace {
TableSchema CreateTableSchema() {
  TableSchema schema;
  TableFieldSchema f1, f2, f3, f4, f5, f6;
  f1.type = "STRING";
  f2.type = "STRING";
  f3.type = "STRING";
  f4.type = "STRING";
  f5.type = "INTEGER";
  f6.type = "STRING";
  schema.fields.emplace_back(f1);
  schema.fields.emplace_back(f2);
  schema.fields.emplace_back(f3);
  schema.fields.emplace_back(f4);
  schema.fields.emplace_back(f5);
  schema.fields.emplace_back(f6);
  return schema;
}

std::vector<Struct> CreateTableRows() {
  std::vector<Struct> rows;
  Struct row1, row2;
  Value v1, v2, v3, v4, v5, v6;
  v1.value_kind = std::string("table-catalog-1");
  v2.value_kind = std::string("table-schema-1");
  v3.value_kind = std::string("table-name-1");
  v4.value_kind = std::string("col-name-1");
  v5.value_kind = std::string("1");
  v6.value_kind = std::string("pk-constraint-1");
  row1.fields.insert({"k1", v1});
  row1.fields.insert({"k2", v2});
  row1.fields.insert({"k3", v3});
  row1.fields.insert({"k4", v4});
  row1.fields.insert({"k5", v5});
  row1.fields.insert({"k6", v6});
  v1.value_kind = std::string("table-catalog-2");
  v2.value_kind = std::string("table-schema-2");
  v3.value_kind = std::string("table-name-2");
  v4.value_kind = std::string("col-name-2");
  v5.value_kind = std::string("2");
  v6.value_kind = std::string("pk-constraint-2");
  row2.fields.insert({"k1", v1});
  row2.fields.insert({"k2", v2});
  row2.fields.insert({"k3", v3});
  row2.fields.insert({"k4", v4});
  row2.fields.insert({"k5", v5});
  row2.fields.insert({"k6", v6});
  rows.emplace_back(row1);
  rows.emplace_back(row2);
  return rows;
}

PostQueryResults CreatePostQueryResults() {
  PostQueryResults results;
  results.job_complete = true;
  results.schema = CreateTableSchema();
  results.rows = CreateTableRows();
  return results;
}

GetQueryResults CreateGetQueryResults() {
  GetQueryResults results;
  results.job_complete = true;
  results.schema = CreateTableSchema();
  results.rows = CreateTableRows();
  return results;
}

void AssertResults(StatusRecordOr<ResultSet> status_record_or) {
  EXPECT_EQ(status_record_or->rows.size(), 2);
  EXPECT_EQ(status_record_or->row_schema.size(), 6);
  EXPECT_EQ(status_record_or->row_schema[0].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[1].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[2].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[3].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[4].col_type, BQDataType::kInt64);
  EXPECT_EQ(status_record_or->row_schema[5].col_type, BQDataType::kString);
  std::string data;
  DSValueToString(status_record_or->rows[0][0], data);
  EXPECT_EQ(data, "table-catalog-1");
  DSValueToString(status_record_or->rows[0][1], data);
  EXPECT_EQ(data, "table-schema-1");
  DSValueToString(status_record_or->rows[0][2], data);
  EXPECT_EQ(data, "table-name-1");
  DSValueToString(status_record_or->rows[0][3], data);
  EXPECT_EQ(data, "col-name-1");
  DSValueToString(status_record_or->rows[0][4], data);
  EXPECT_EQ(data, "1");
  DSValueToString(status_record_or->rows[0][5], data);
  EXPECT_EQ(data, "pk-constraint-1");
  DSValueToString(status_record_or->rows[1][0], data);
  EXPECT_EQ(data, "table-catalog-2");
  DSValueToString(status_record_or->rows[1][1], data);
  EXPECT_EQ(data, "table-schema-2");
  DSValueToString(status_record_or->rows[1][2], data);
  EXPECT_EQ(data, "table-name-2");
  DSValueToString(status_record_or->rows[1][3], data);
  EXPECT_EQ(data, "col-name-2");
  DSValueToString(status_record_or->rows[1][4], data);
  EXPECT_EQ(data, "2");
  DSValueToString(status_record_or->rows[1][5], data);
  EXPECT_EQ(data, "pk-constraint-2");
}

}  // namespace

TEST(ProcessPKResults, PostQueryResults_Success) {
  PostQueryResults results = CreatePostQueryResults();
  auto status_record_or = ProcessPKPostQueryResults(results);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  AssertResults(status_record_or);
}

TEST(ProcessPKResults, GetQueryResults_Success) {
  GetQueryResults results = CreateGetQueryResults();
  auto status_record_or = ProcessPKGetQueryResults(results);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  AssertResults(status_record_or);
}

TEST(ProcessPKResults, PostQueryResults_Error_InvalidDataType) {
  PostQueryResults results;
  results.job_complete = true;
  TableSchema schema;
  TableFieldSchema f1;
  f1.type = "INVALID";
  schema.fields.emplace_back(f1);
  results.schema = schema;
  auto status_record_or = ProcessPKPostQueryResults(results);

  EXPECT_THAT(status_record_or, StatusRecordIs(SQLStates::k_HY000(),
                                               HasSubstr("Invalid Data Type")));
}

TEST(ProcessPKResults, GetQueryResults_Error_InvalidDataType) {
  GetQueryResults results;
  results.job_complete = true;
  TableSchema schema;
  TableFieldSchema f1;
  f1.type = "INVALID";
  schema.fields.emplace_back(f1);
  results.schema = schema;
  auto status_record_or = ProcessPKGetQueryResults(results);

  EXPECT_THAT(status_record_or, StatusRecordIs(SQLStates::k_HY000(),
                                               HasSubstr("Invalid Data Type")));
}

TEST(ProcessPKResults, PostQueryResults_Error_JobComplete) {
  PostQueryResults results;
  results.job_complete = false;
  auto status_record_or = ProcessPKPostQueryResults(results);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Internal Error: Unexpected value for "
                                       "job_complete")));
}

TEST(ProcessPKResults, GetQueryResults_Error_JobComplete) {
  GetQueryResults results;
  results.job_complete = false;
  auto status_record_or = ProcessPKGetQueryResults(results);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Internal Error: Unexpected value for "
                                       "job_complete")));
}

}  // namespace google::cloud::odbc_bq_driver_internal
