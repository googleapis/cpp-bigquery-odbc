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
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::ColumnData;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::JobCreationMode;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using google::cloud::odbc_bq_driver_internal::ValidateConnAttribute;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using ::google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using ::google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

namespace {

std::string const kTestCatalog = "test-catalog";
std::string const kTestSchema = "test-schema";
std::string const kDefaultDataset = "default-dataset";

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

std::vector<RowData> CreateTableRows() {
  std::vector<RowData> rows;
  RowData row1, row2;

  row1.columns.push_back(ColumnData{"table-catalog-1"});
  row1.columns.push_back(ColumnData{"table-schema-1"});
  row1.columns.push_back(ColumnData{"table-name-1"});
  row1.columns.push_back(ColumnData{"col-name-1"});
  row1.columns.push_back(ColumnData{"1"});

  row1.columns.push_back(ColumnData{"pk-constraint-1"});
  row2.columns.push_back(ColumnData{"table-catalog-2"});
  row2.columns.push_back(ColumnData{"table-schema-2"});
  row2.columns.push_back(ColumnData{"table-name-2"});
  row2.columns.push_back(ColumnData{"col-name-2"});
  row2.columns.push_back(ColumnData{"2"});
  row2.columns.push_back(ColumnData{"pk-constraint-2"});

  rows.push_back(row1);
  rows.push_back(row2);
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
  auto i_data = DSValueToInt(status_record_or->rows[0][4]);
  EXPECT_EQ(i_data, 1);
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
  i_data = DSValueToInt(status_record_or->rows[1][4]);
  EXPECT_EQ(i_data, 2);
  DSValueToString(status_record_or->rows[1][5], data);
  EXPECT_EQ(data, "pk-constraint-2");
}

}  // namespace

TEST(IsDSValueNull, Basic) { EXPECT_TRUE(IsDSValueNull(kNullValue)); }

TEST(IsDSValueNull, NotEmpty) {
  DSValue val{1};
  EXPECT_FALSE(IsDSValueNull(val));
}

TEST(IsDSValueNull, Empty) {
  DSValue empty_val;
  EXPECT_FALSE(IsDSValueNull(empty_val));
}

TEST(DSValue, Basic_String) {
  std::string expected = "Some string which should be converted to DSValue";
  DSValue value;
  StringToDSValue(expected, value);

  std::string returned;

  DSValueToString(value, returned);
  EXPECT_EQ(expected, returned);
}

TEST(DSValue, EmptyString) {
  std::string str;
  DSValue ds_val;
  StringToDSValue(str, ds_val);
  EXPECT_EQ(ds_val.size(), 0);
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

TEST(DSValue, Timestamp) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2020;
  Timestamp.month = 1;
  Timestamp.day = 10;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 123456;
  DSValue src_dsval;
  TimestampToDSValue(Timestamp, src_dsval);
  SQL_TIMESTAMP_STRUCT actual;
  DSValueToTimestamp(src_dsval, actual);
  EXPECT_EQ(actual.year, Timestamp.year);
  EXPECT_EQ(actual.month, Timestamp.month);
  EXPECT_EQ(actual.day, Timestamp.day);
  EXPECT_EQ(actual.hour, Timestamp.hour);
  EXPECT_EQ(actual.minute, Timestamp.minute);
  EXPECT_EQ(actual.second, Timestamp.second);
}

TEST(FormatTimestampToString, Timestamp_String) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2020;
  Timestamp.month = 1;
  Timestamp.day = 10;
  Timestamp.hour = 01;
  Timestamp.minute = 59;
  Timestamp.second = 43;
  Timestamp.fraction = 123456;

  std::string timestampString = FormatTimestampToString(Timestamp);

  std::string expectedString = "2020-01-10 01:59:43.123456";
  EXPECT_EQ(timestampString, expectedString);
}

TEST(FormatTimestampToString, Timestamp_String_with_zeros) {
  SQL_TIMESTAMP_STRUCT Timestamp;
  Timestamp.year = 2020;
  Timestamp.month = 1;
  Timestamp.day = 10;
  Timestamp.hour = 0;
  Timestamp.minute = 5;
  Timestamp.second = 3;
  Timestamp.fraction = 0;

  std::string timestampString = FormatTimestampToString(Timestamp);

  std::string expectedString = "2020-01-10 00:05:03.000000";
  EXPECT_EQ(timestampString, expectedString);
}

TEST(StringToDSValue, SQLCHAR_String) {
  const SQLCHAR expected[10] = "Hello";
  DSValue value;
  StringToDSValue(expected, value);

  std::string dsvalue_converted;
  DSValueToString(value, dsvalue_converted);
  EXPECT_STREQ(dsvalue_converted.c_str(), (char*)expected);
}

TEST(ArithmeticToDSValue, Success_SQLBIGINT) {
  SQLBIGINT expected = 404;
  DSValue value;
  ArithmeticToDSValue<SQLBIGINT>(expected, value);

  EXPECT_EQ(DSValueToArithmetic<SQLBIGINT>(value), expected);
}

TEST(ArithmeticToDSValue, Success_SQLDOUBLE) {
  SQLDOUBLE expected = 3.14;
  DSValue value;
  ArithmeticToDSValue<SQLDOUBLE>(expected, value);

  EXPECT_EQ(DSValueToArithmetic<SQLDOUBLE>(value), expected);
}

TEST(StringToDSValue, Std_String) {
  std::string expected = "Hello";
  DSValue value;
  StringToDSValue(expected, value);

  std::string dsvalue_converted;
  DSValueToString(value, dsvalue_converted);
  EXPECT_EQ(dsvalue_converted, expected);
}

TEST(ProcessBQResults, ProcessPostQueryResults_Success) {
  PostQueryResults results = CreatePostQueryResults();
  auto status_record_or = ProcessPostQueryResults(results);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  AssertResults(status_record_or);
}

TEST(ProcessBQResults, ProcessGetQueryResults_Success) {
  GetQueryResults results = CreateGetQueryResults();
  auto status_record_or = ProcessGetQueryResults(results);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  AssertResults(status_record_or);
}

TEST(ProcessBQResults, ProcessQueryResults_PostQueryResults_Success) {
  DSResults results;
  PostQueryResults post_results = CreatePostQueryResults();
  results.data_source_results = post_results;
  auto status_record_or = ProcessQueryResults(results);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  AssertResults(status_record_or);
}

TEST(ProcessBQResults, ProcessQueryResults_GetQueryResults_Success) {
  DSResults results;
  GetQueryResults get_results = CreateGetQueryResults();
  results.data_source_results = get_results;
  auto status_record_or = ProcessQueryResults(results);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  AssertResults(status_record_or);
}

TEST(ProcessBQResults, ProcessQueryResults_Failure) {
  DSResults results;
  auto status_record_or = ProcessQueryResults(results);
  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Invalid query results object")));
}

TEST(ProcessBQResults, PostQueryResults_Error_InvalidDataType) {
  PostQueryResults results;
  results.job_complete = true;
  TableSchema schema;
  TableFieldSchema f1;
  f1.type = "INVALID";
  schema.fields.emplace_back(f1);
  results.schema = schema;
  auto status_record_or = ProcessPostQueryResults(results);

  EXPECT_THAT(status_record_or, StatusRecordIs(SQLStates::k_HY000(),
                                               HasSubstr("Invalid Data Type")));
}

TEST(ProcessBQResults, GetQueryResults_Error_InvalidDataType) {
  GetQueryResults results;
  results.job_complete = true;
  TableSchema schema;
  TableFieldSchema f1;
  f1.type = "INVALID";
  schema.fields.emplace_back(f1);
  results.schema = schema;
  auto status_record_or = ProcessGetQueryResults(results);

  EXPECT_THAT(status_record_or, StatusRecordIs(SQLStates::k_HY000(),
                                               HasSubstr("Invalid Data Type")));
}

TEST(ProcessBQResults, PostQueryResults_Error_JobComplete) {
  PostQueryResults results;
  results.job_complete = false;
  auto status_record_or = ProcessPostQueryResults(results);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Internal Error: Unexpected value for "
                                       "job_complete")));
}

TEST(ProcessBQResults, GetQueryResults_Error_JobComplete) {
  GetQueryResults results;
  results.job_complete = false;
  auto status_record_or = ProcessGetQueryResults(results);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Internal Error: Unexpected value for "
                                       "job_complete")));
}

TEST(GetRowsResults, GetQueryResults_Success) {
  DSResults results;
  GetQueryResults get_results = CreateGetQueryResults();
  results.data_source_results = get_results;

  auto status_record_or = GetRowsResults(results);

  ASSERT_STATUS_RECORD_OK(status_record_or);
  EXPECT_EQ(status_record_or->size(), CreateTableRows().size());
}

TEST(GetRowsResults, GetQueryResults_Success_Error_JobComplete) {
  DSResults results;
  GetQueryResults get_results;
  get_results.job_complete = false;
  results.data_source_results = get_results;

  auto status_record_or = GetRowsResults(results);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Internal Error: Unexpected value for "
                                       "job_complete")));
}

TEST(GetRowsResults, PostQueryResults_Success) {
  DSResults results;
  PostQueryResults get_results = CreatePostQueryResults();
  results.data_source_results = get_results;

  auto status_record_or = GetRowsResults(results);

  ASSERT_STATUS_RECORD_OK(status_record_or);
  EXPECT_EQ(status_record_or->size(), CreateTableRows().size());
}

TEST(GetRowsResults, PostQueryResults_Error_JobComplete) {
  DSResults results;
  PostQueryResults get_results = CreatePostQueryResults();
  get_results.job_complete = false;
  results.data_source_results = get_results;

  auto status_record_or = GetRowsResults(results);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Internal Error: Unexpected value for "
                                       "job_complete")));
}

TEST(GetRowsResults, Failure_NoResults) {
  DSResults results;

  auto status_record_or = GetRowsResults(results);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Invalid query results object")));
}

TEST(FetchBQResults, Failure_Not_Connected) {
  PostQueryRequest req;
  ConnectionHandle handle;
  auto status_record_or = FetchBQData(handle, req);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_08S01(),
                     HasSubstr("Connection to the data source is broken")));
}

TEST(FetchBQResults, Failure_Null_BQClient) {
  PostQueryRequest req;
  auto handle = CreateConnectionHandle(true);
  auto status_record_or = FetchBQData(handle, req);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HY000(),
          HasSubstr("Invalid or null BQ Client within the connection handle")));
}

TEST(CancelBQJob, Failure_Not_Connected) {
  ConnectionHandle handle;
  auto status_record_or = CancelBQJob(handle, "1234");

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_08S01(),
                     HasSubstr("Connection to the data source is broken")));
}

TEST(CancelBQJob, Failure_Null_BQClient) {
  auto handle = CreateConnectionHandle(true);
  auto status_record_or = CancelBQJob(handle, "1234");

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HY000(),
          HasSubstr("Invalid or null BQ Client within the connection handle")));
}

TEST(CancelBQJob, Failure_Empty_Job) {
  ConnectionHandle handle;
  auto status_record_or = CancelBQJob(handle, "");

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Invalid or empty job id")));
}

TEST(ConstructStringArrayQueryParameter, Success) {
  auto status_record_or = ConstructStringArrayQueryParameter(
      "param-name-1", {"param-val-1", "param-val-2"});

  ASSERT_STATUS_RECORD_OK(status_record_or);
  QueryParameter param = *status_record_or;
  EXPECT_EQ(param.name, "param-name-1");
  EXPECT_EQ(param.parameter_type.type, "ARRAY");
  EXPECT_EQ(param.parameter_type.array_type->type, "STRING");
  EXPECT_EQ(param.parameter_value.array_values[0].value, "param-val-1");
  EXPECT_EQ(param.parameter_value.array_values[1].value, "param-val-2");
}

TEST(ConstructStringArrayQueryParameter, Failure_EmptyParamName) {
  auto status_record_or =
      ConstructStringArrayQueryParameter("", {"param-val-1"});

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Invalid parameter name")));
}

TEST(ConstructStringArrayQueryParameter, Failure_EmptyParamVector) {
  auto status_record_or =
      ConstructStringArrayQueryParameter("param-name-1", {});

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Empty parameter values")));
}

TEST(ConstructStringQueryParameter, Success) {
  auto status_record_or =
      ConstructStringQueryParameter("param-name-1", "param-val-1");

  ASSERT_STATUS_RECORD_OK(status_record_or);
  EXPECT_EQ((*status_record_or).name, "param-name-1");
  EXPECT_EQ((*status_record_or).parameter_type.type, "STRING");
  EXPECT_EQ((*status_record_or).parameter_value.value, "param-val-1");
}

TEST(ConstructStringQueryParameter, Success_EmptyParamValue) {
  auto status_record_or = ConstructStringQueryParameter("param-name-1", "");

  ASSERT_STATUS_RECORD_OK(status_record_or);
  EXPECT_EQ((*status_record_or).name, "param-name-1");
  EXPECT_EQ((*status_record_or).parameter_type.type, "STRING");
  EXPECT_EQ((*status_record_or).parameter_value.value, "");
}

TEST(ConstructStringQueryParameter, Failure_EmptyParamName) {
  auto status_record_or = ConstructStringQueryParameter("", "param-val-1");

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Invalid parameter name")));
}

TEST(ConstructStringQueryParameters, Success) {
  std::map<std::string, std::string> named_query_params;
  named_query_params.insert({"param-name-1", "param-val-1"});
  named_query_params.insert({"param-name-2", "param-val-2"});
  named_query_params.insert({"param-name-3", "param-val-3"});

  auto status_record_or = ConstructStringQueryParameters(named_query_params);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  EXPECT_FALSE(status_record_or->empty());
  EXPECT_EQ(status_record_or->size(), 3);
  // Verify results.
  std::vector<QueryParameter> query_params = *status_record_or;
  EXPECT_EQ(query_params[0].name, "param-name-1");
  EXPECT_EQ(query_params[1].name, "param-name-2");
  EXPECT_EQ(query_params[2].name, "param-name-3");

  EXPECT_EQ(query_params[0].parameter_type.type, "STRING");
  EXPECT_EQ(query_params[1].parameter_type.type, "STRING");
  EXPECT_EQ(query_params[2].parameter_type.type, "STRING");

  EXPECT_EQ(query_params[0].parameter_value.value, "param-val-1");
  EXPECT_EQ(query_params[1].parameter_value.value, "param-val-2");
  EXPECT_EQ(query_params[2].parameter_value.value, "param-val-3");
}

TEST(ConstructStringQueryParameters, Failure_Empty_Param_name) {
  std::map<std::string, std::string> named_query_params;
  named_query_params.insert({"", "param-val-1"});

  auto status_record_or = ConstructStringQueryParameters(named_query_params);
  EXPECT_FALSE(status_record_or.Ok());

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY000(),
                             HasSubstr("Invalid parameter name")));
}

TEST(ConstructBasicPostQueryRequest, Basic) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["Catalog"] = kTestCatalog;
  conn_handle.SetUp(dsn_section, "name");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_EQ(returned.project_id(), kTestCatalog);
  EXPECT_EQ(returned.query_request().query(), query_str);
  EXPECT_FALSE(returned.query_request().dry_run());
  EXPECT_FALSE(returned.query_request().use_legacy_sql());
  EXPECT_TRUE(returned.query_request().default_dataset().project_id.empty());
  EXPECT_TRUE(returned.query_request().default_dataset().dataset_id.empty());
  EXPECT_FALSE(returned.query_request().create_session());
  EXPECT_TRUE(returned.query_request().connection_properties().empty());
  EXPECT_TRUE(returned.query_request().job_creation_mode().value.empty());
}

TEST(ConstructBasicPostQueryRequest, Basic_withLegacySql) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["SQLDialect"] = "0";
  conn_handle.SetUp(dsn_section, "name");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_TRUE(returned.query_request().use_legacy_sql());
}

TEST(ConstructBasicPostQueryRequest, Basic_withJobCreationModeRequired) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JobCreationMode"] = "1";
  conn_handle.SetUp(dsn_section, "name");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_EQ(returned.query_request().job_creation_mode().value,
            JobCreationMode::Required().value);
}

TEST(ConstructBasicPostQueryRequest, Basic_withDefaultDataset) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["Catalog"] = kTestCatalog;
  dsn_section["DefaultDataset"] = kDefaultDataset;
  conn_handle.SetUp(dsn_section, "name");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_EQ(returned.query_request().default_dataset().project_id,
            kTestCatalog);
  EXPECT_EQ(returned.query_request().default_dataset().dataset_id,
            kDefaultDataset);
}

TEST(ConstructBasicPostQueryRequest, Basic_CreateSession) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["EnableSession"] = "1";
  conn_handle.SetUp(dsn_section, "name");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_TRUE(returned.query_request().create_session());
  EXPECT_TRUE(returned.query_request().connection_properties().empty());
}

TEST(ConstructBasicPostQueryRequest, Basic_SetTimeout) {
  ConnectionHandle conn_handle;
  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, "SELECT 1", 2);
  EXPECT_EQ(returned.query_request().timeout(),
            std::chrono::milliseconds(2000));
}

TEST(ConstructBasicPostQueryRequest, Basic_UseSession) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  conn_handle.SetSessionId("sessionId");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_FALSE(returned.query_request().create_session());
  EXPECT_FALSE(returned.query_request().connection_properties().empty());
  EXPECT_EQ("session_id",
            returned.query_request().connection_properties()[0].key);
  EXPECT_EQ(conn_handle.GetSessionId(),
            returned.query_request().connection_properties()[0].value);
}

TEST(ConstructnamedPostQueryRequestTest, Success) {
  PostQueryRequest expected;
  std::string named_query =
      "select * from test_table where test_col1 = @param1 and test_col2 = "
      "@param2 and test_col3 = "
      "@param3";
  std::vector<QueryParameter> named_query_params;
  named_query_params.emplace_back(
      QueryParameter{"param1", {"STRING"}, {"param-val-1"}});
  named_query_params.emplace_back(
      QueryParameter{"param2", {"STRING"}, {"param-val-2"}});
  named_query_params.emplace_back(
      QueryParameter{"param3", {"STRING"}, {"param-val-3"}});

  auto status_record_or = ConstructNamedParametersPostQueryRequest(
      kTestCatalog, kTestSchema, named_query, named_query_params);
  ASSERT_STATUS_RECORD_OK(status_record_or);
  // Verify results pertaining to query request.
  PostQueryRequest actual = *status_record_or;
  EXPECT_EQ(actual.project_id(), kTestCatalog);
  EXPECT_EQ(actual.query_request().parameter_mode(), "NAMED");
  EXPECT_EQ(actual.query_request().query(), named_query);
  EXPECT_FALSE(actual.query_request().dry_run());
  EXPECT_FALSE(actual.query_request().use_legacy_sql());
  // Verify query params
  auto query_params = actual.query_request().query_parameters();
  EXPECT_FALSE(query_params.empty());
  EXPECT_EQ(query_params.size(), 3);
  EXPECT_EQ(query_params[0].name, "param1");
  EXPECT_EQ(query_params[1].name, "param2");
  EXPECT_EQ(query_params[2].name, "param3");

  EXPECT_EQ(query_params[0].parameter_type.type, "STRING");
  EXPECT_EQ(query_params[1].parameter_type.type, "STRING");
  EXPECT_EQ(query_params[2].parameter_type.type, "STRING");

  EXPECT_EQ(query_params[0].parameter_value.value, "param-val-1");
  EXPECT_EQ(query_params[1].parameter_value.value, "param-val-2");
  EXPECT_EQ(query_params[2].parameter_value.value, "param-val-3");
}

TEST(ConstructQueryParamsTest, Failure_Empty_Catalog_Name) {
  std::string named_query = "select * from table where col = @param1";
  std::vector<QueryParameter> named_query_params;
  named_query_params.emplace_back(
      QueryParameter{"param1", {"STRING"}, {"param-val-1"}});
  auto status_record_or = ConstructNamedParametersPostQueryRequest(
      "", kTestSchema, named_query, named_query_params);
  EXPECT_FALSE(status_record_or.Ok());

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY090(),
                             HasSubstr("catalog name is required")));
}

TEST(ConstructQueryParamsTest, Failure_Empty_Schema_Name) {
  std::string named_query = "select * from table where col = @param1";
  std::vector<QueryParameter> named_query_params;
  named_query_params.emplace_back(
      QueryParameter{"param1", {"STRING"}, {"param-val-1"}});
  auto status_record_or = ConstructNamedParametersPostQueryRequest(
      kTestCatalog, "", named_query, named_query_params);
  EXPECT_FALSE(status_record_or.Ok());

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY090(),
                             HasSubstr("dataset name is required")));
}

TEST(ConstructQueryParamsTest, Failure_Empty_Query) {
  std::string named_query = "";
  std::vector<QueryParameter> named_query_params;
  named_query_params.emplace_back(
      QueryParameter{"param1", {"STRING"}, {"param-val-1"}});
  auto status_record_or = ConstructNamedParametersPostQueryRequest(
      kTestCatalog, kTestSchema, "", named_query_params);
  EXPECT_FALSE(status_record_or.Ok());

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY090(),
                             HasSubstr("parameterized query is required")));
}

TEST(ProcessResultSetRows, Success_Basic) {
  TableSchema table_schema = CreateTableSchema();
  std::vector<RowData> rows = CreateTableRows();
  StatusRecordOr<ResultSet> results_status =
      ProcessResultSetRows(table_schema, rows);
  ASSERT_STATUS_RECORD_OK(results_status);
  AssertResults(results_status);
}

TEST(GetSQLDataType, GetInvalidDataType) {
  std::string f1 = "INT65";
  auto res = GetSQLDataType(f1);
  EXPECT_FALSE(res.Ok());
  EXPECT_EQ("Invalid Data Type: " + f1, res.GetStatusRecord().message);
}

TEST(GetSQLDataType, GetValidDataType) {
  std::string const& f1 = "FLOAT64";
  std::string const& f2 = "DATE";
  std::string const& f3 = "ARRAY";
  std::string const& f4 = "TIMESTAMP";
  std::string const& f5 = "INTEGER";

  auto first_res = GetSQLDataType(f1);
  auto second_res = GetSQLDataType(f2);
  auto third_res = GetSQLDataType(f3);
  auto fourth_res = GetSQLDataType(f4);
  auto fifth_res = GetSQLDataType(f5);

  EXPECT_EQ(SQL_DOUBLE, *first_res);
  EXPECT_EQ(SQL_TYPE_DATE, *second_res);
  EXPECT_EQ(SQL_VARCHAR, *third_res);
  EXPECT_EQ(SQL_TYPE_TIMESTAMP, *fourth_res);
  EXPECT_EQ(SQL_BIGINT, *fifth_res);
}

TEST(DateToDSValue, checkdate) {
  SQL_DATE_STRUCT d;
  d.year = 2020;
  d.month = 10;
  d.day = 10;

  DSValue val;
  DateToDSValue(d, val);

  SQL_DATE_STRUCT returned = DSValueToDate(val, returned);
  EXPECT_EQ(d.year, returned.year);
  EXPECT_EQ(d.month, returned.month);
  EXPECT_EQ(d.day, returned.day);
}

TEST(DSValueToDate, EmptyDateString) {
  SQL_DATE_STRUCT d = {};
  DSValue val;
  DateToDSValue(d, val);

  SQL_DATE_STRUCT returned = {};
  DSValueToDate(val, returned);

  EXPECT_EQ(returned.year, 0);
  EXPECT_EQ(returned.month, 0);
  EXPECT_EQ(returned.day, 0);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_YEAR) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "1-0 0 0:0:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_YEAR);
  EXPECT_EQ(interval_struct.intval.year_month.year, 1);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_MONTH) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-4 0 0:0:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_MONTH);
  EXPECT_EQ(interval_struct.intval.year_month.month, 4);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_DAY) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 9 0:0:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_DAY);
  EXPECT_EQ(interval_struct.intval.day_second.day, 9);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_HOUR) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 0 7:0:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_HOUR);
  EXPECT_EQ(interval_struct.intval.day_second.hour, 7);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_MINUTE) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 0 0:6:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_MINUTE);
  EXPECT_EQ(interval_struct.intval.day_second.minute, 6);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_SECOND) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 0 0:0:10";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_SECOND);
  EXPECT_EQ(interval_struct.intval.day_second.second, 10);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_YEAR_TO_MONTH) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "1-2 0 0:0:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_YEAR_TO_MONTH);
  EXPECT_EQ(interval_struct.intval.year_month.year, 1);
  EXPECT_EQ(interval_struct.intval.year_month.month, 2);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_DAY_TO_HOUR) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 1 2:0:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_DAY_TO_HOUR);
  EXPECT_EQ(interval_struct.intval.day_second.day, 1);
  EXPECT_EQ(interval_struct.intval.day_second.hour, 2);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_DAY_TO_MINUTE) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 1 2:3:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_DAY_TO_MINUTE);
  EXPECT_EQ(interval_struct.intval.day_second.day, 1);
  EXPECT_EQ(interval_struct.intval.day_second.hour, 2);
  EXPECT_EQ(interval_struct.intval.day_second.minute, 3);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_DAY_TO_SECOND) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 1 2:3:20";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_DAY_TO_SECOND);
  EXPECT_EQ(interval_struct.intval.day_second.day, 1);
  EXPECT_EQ(interval_struct.intval.day_second.hour, 2);
  EXPECT_EQ(interval_struct.intval.day_second.minute, 3);
  EXPECT_EQ(interval_struct.intval.day_second.second, 20);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_HOUR_TO_MINUTE) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 0 2:33:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_HOUR_TO_MINUTE);
  EXPECT_EQ(interval_struct.intval.day_second.hour, 2);
  EXPECT_EQ(interval_struct.intval.day_second.minute, 33);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_HOUR_TO_SECOND) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 0 2:3:7";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_HOUR_TO_SECOND);
  EXPECT_EQ(interval_struct.intval.day_second.hour, 2);
  EXPECT_EQ(interval_struct.intval.day_second.minute, 3);
  EXPECT_EQ(interval_struct.intval.day_second.second, 7);
}

TEST(ConvertStringToIntervalStruct, SQL_IS_MINUTE_TO_SECOND) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 0 0:3:17";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, 1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_MINUTE_TO_SECOND);
  EXPECT_EQ(interval_struct.intval.day_second.minute, 3);
  EXPECT_EQ(interval_struct.intval.day_second.second, 17);
}

TEST(ConvertStringToIntervalStruct, neg_interval_str) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_str = "0-0 -15 2:0:0";
  ConvertStringToIntervalStruct(interval_str, interval_struct);
  EXPECT_EQ(interval_struct.interval_sign, -1);
  EXPECT_EQ(interval_struct.interval_type, SQL_IS_DAY_TO_HOUR);
  EXPECT_EQ(interval_struct.intval.day_second.day, -15);
  EXPECT_EQ(interval_struct.intval.day_second.hour, 2);
}

TEST(ConvertStringToIntervalStruct, Invalid_interval_str) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string interval_invalid_str = "0-1";
  EXPECT_THROW(
      ConvertStringToIntervalStruct(interval_invalid_str, interval_struct),
      std::invalid_argument);
}

TEST(ConvertStringToIntervalStruct, Empty_str) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string empty_str = "";
  EXPECT_THROW(ConvertStringToIntervalStruct(empty_str, interval_struct),
               std::invalid_argument);
}

bool CompareTimestampStruct(const SQL_TIMESTAMP_STRUCT& ts1,
                            const SQL_TIMESTAMP_STRUCT& ts2) {
  return ts1.year == ts2.year && ts1.month == ts2.month && ts1.day == ts2.day &&
         ts1.hour == ts2.hour && ts1.minute == ts2.minute &&
         ts1.second == ts2.second && ts1.fraction == ts2.fraction;
}

TEST(ConvertStringToTimestampStruct, ValidDateWithoutFraction) {
  std::string date_str = "2024-10-04 12:30:45";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 0};

  SQL_TIMESTAMP_STRUCT result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result, expected));
}

TEST(ConvertStringToTimestampStruct, ValidDateWithFraction) {
  std::string date_str = "2024-10-04 12:30:45.123456";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 123456};

  SQL_TIMESTAMP_STRUCT result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result, expected));
}

TEST(ConvertStringToTimestampStruct, ValidDateWithShortFraction) {
  std::string date_str = "2024-10-04 12:30:45.123";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 123000};

  SQL_TIMESTAMP_STRUCT result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result, expected));
}

TEST(ConvertStringToTimestampStruct, ValidDateNoFraction) {
  std::string date_str = "2024-10-04 12:30:45";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 0};

  SQL_TIMESTAMP_STRUCT result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result, expected));
}

TEST(ConvertStringToTimestampStruct, EmptyDateString) {
  std::string date_str = "";
  EXPECT_THROW(ConvertStringToTimestampStruct(date_str), std::invalid_argument);
}

TEST(ConvertStringToTimestampStruct, TooManyFractionalDigits) {
  std::string date_str = "2024-10-04 12:30:45.1234567";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 123456};

  SQL_TIMESTAMP_STRUCT result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result, expected));
}

TEST(ValidateConnAttribute, Success_AllRequiredKeywordsPresent) {
  ConnectionHandle conn_handle;
  Section section;
  section["Catalog"] = "BigQueryCatalog";
  section["OAuthMechanism"] = "1";
  section["KeyFilePath"] = "/path/to/keyfile";

  conn_handle.SetUp(section, "");

  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  auto result =
      ValidateConnAttribute(&conn_handle, out_conn_str, &out_conn_str_len);
  EXPECT_TRUE(result);
  EXPECT_STREQ(reinterpret_cast<char const*>(out_conn_str), "");
  EXPECT_EQ(out_conn_str_len, 0);
}

TEST(ValidateConnAttribute, Failure_MissingSomeKeywords) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["Catalog"] = "BigQueryCatalog";

  conn_handle.SetUp(dsn_section, "");

  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  SQLRETURN result =
      ValidateConnAttribute(&conn_handle, out_conn_str, &out_conn_str_len);
  std::string const expected_out_conn_str = "OAuthMechanism:OAuthMechanism=?;";

  EXPECT_FALSE(result);
  EXPECT_STREQ(reinterpret_cast<char const*>(out_conn_str),
               expected_out_conn_str.c_str());
  EXPECT_EQ(out_conn_str_len, expected_out_conn_str.length());
}

TEST(ValidateConnAttribute, Failure_AllKeywordsMissing) {
  ConnectionHandle conn_handle;
  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  SQLRETURN result =
      ValidateConnAttribute(&conn_handle, out_conn_str, &out_conn_str_len);
  std::string const expected_out_conn_str =
      "Catalog:Catalog=?;OAuthMechanism:OAuthMechanism=?;";

  EXPECT_FALSE(result);
  EXPECT_STREQ(reinterpret_cast<char const*>(out_conn_str),
               expected_out_conn_str.c_str());
  EXPECT_EQ(out_conn_str_len, expected_out_conn_str.length());
}

TEST(ValidateConnAttribute, Failure_PartialMissingEmptyInput) {
  ConnectionHandle conn_handle;
  Section section;
  section["Catalog"] = "BigQueryCatalog";
  section["OAuthMechanism"] = "1";

  conn_handle.SetUp(section, "");
  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  SQLRETURN result =
      ValidateConnAttribute(&conn_handle, out_conn_str, &out_conn_str_len);
  std::string const expected_out_conn_str = "KeyFilePath:KeyFilePath=?;";

  EXPECT_FALSE(result);
  EXPECT_STREQ(reinterpret_cast<char const*>(out_conn_str),
               expected_out_conn_str.c_str());
  EXPECT_EQ(out_conn_str_len, expected_out_conn_str.length());
}
}  // namespace google::cloud::odbc_bq_driver_internal
