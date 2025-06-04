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
using google::cloud::odbc_bq_driver_internal::GetMissingAttributesStr;
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
  TableFieldSchema f1, f2, f3, f4, f5, f6, f7;
  f1.type = "STRING";
  f2.type = "STRING";
  f3.type = "STRING";
  f4.type = "STRING";
  f5.type = "INTEGER";
  f6.type = "STRING";
  f7.type = "INTEGER";
  f7.mode = "REPEATED";
  schema.fields.emplace_back(f1);
  schema.fields.emplace_back(f2);
  schema.fields.emplace_back(f3);
  schema.fields.emplace_back(f4);
  schema.fields.emplace_back(f5);
  schema.fields.emplace_back(f6);
  schema.fields.emplace_back(f7);
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
  EXPECT_EQ(status_record_or->row_schema.size(), 7);
  EXPECT_EQ(status_record_or->row_schema[0].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[1].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[2].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[3].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[4].col_type, BQDataType::kInt64);
  EXPECT_EQ(status_record_or->row_schema[5].col_type, BQDataType::kString);
  EXPECT_EQ(status_record_or->row_schema[6].col_type, BQDataType::kInt64);
  EXPECT_TRUE(status_record_or->row_schema[6].is_mode_repeated);
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
  dsn_section["CATALOG"] = kTestCatalog;
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
  dsn_section["SQLDIALECT"] = "0";
  conn_handle.SetUp(dsn_section, "name");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_TRUE(returned.query_request().use_legacy_sql());
}

TEST(ConstructBasicPostQueryRequest, Basic_withJobCreationModeRequired) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["JOBCREATIONMODE"] = "1";
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
  dsn_section["CATALOG"] = kTestCatalog;
  dsn_section["DEFAULTDATASET"] = kDefaultDataset;
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
  dsn_section["ENABLESESSION"] = "1";
  conn_handle.SetUp(dsn_section, "name");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  EXPECT_TRUE(returned.query_request().create_session());
  EXPECT_TRUE(returned.query_request().connection_properties().empty());
}

TEST(ConstructBasicPostQueryRequest, IncludesConnectionProperties) {
  std::string query_str = "SELECT 1";
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["CATALOG"] = kTestCatalog;
  dsn_section["QUERYPROPERTIES"] = "key1=value1, key2=value2";
  conn_handle.SetUp(dsn_section, "TestDSN");

  PostQueryRequest returned =
      ConstructBasicPostQueryRequest(conn_handle, query_str);

  auto const& connection_properties =
      returned.query_request().connection_properties();
  ASSERT_EQ(connection_properties.size(), 2);

  EXPECT_EQ(connection_properties[0].key, "key1");
  EXPECT_EQ(connection_properties[0].value, "value1");

  EXPECT_EQ(connection_properties[1].key, "key2");
  EXPECT_EQ(connection_properties[1].value, "value2");
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
  auto result =
      ConvertStringToIntervalStruct(interval_invalid_str, interval_struct);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.sql_state, SQLStates::k_HY000());
  EXPECT_THAT(result.message,
              ::testing::HasSubstr("Invalid interval string format"));
}

TEST(ConvertStringToIntervalStruct, Empty_str) {
  SQL_INTERVAL_STRUCT interval_struct;
  std::string empty_str = "";
  auto result = ConvertStringToIntervalStruct(empty_str, interval_struct);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.sql_state, SQLStates::k_HY000());
  EXPECT_THAT(result.message,
              ::testing::HasSubstr("Interval string can't be empty"));
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

  auto result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result.GetValue(), expected));
}

TEST(ConvertStringToTimestampStruct, ValidDateWithFraction) {
  std::string date_str = "2024-10-04 12:30:45.123456";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 123456};

  auto result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result.GetValue(), expected));
}

TEST(ConvertStringToTimestampStruct, ValidDateWithShortFraction) {
  std::string date_str = "2024-10-04 12:30:45.123";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 123000};

  auto result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result.GetValue(), expected));
}

TEST(ConvertStringToTimestampStruct, ValidDateNoFraction) {
  std::string date_str = "2024-10-04 12:30:45";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 0};

  auto result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result.GetValue(), expected));
}

TEST(ConvertStringToTimestampStruct, EmptyDateString) {
  std::string date_str = "";
  auto result = ConvertStringToTimestampStruct(date_str);
  EXPECT_FALSE(result.Ok());
}

TEST(ConvertStringToTimestampStruct, TooManyFractionalDigits) {
  std::string date_str = "2024-10-04 12:30:45.1234567";
  SQL_TIMESTAMP_STRUCT expected = {2024, 10, 4, 12, 30, 45, 123456};

  auto result = ConvertStringToTimestampStruct(date_str);
  EXPECT_TRUE(CompareTimestampStruct(result.GetValue(), expected));
}

TEST(BooleanToDSValue, CheckBooleanTrue) {
  bool bool_val = true;
  DSValue val;
  BooleanToDSValue(bool_val, val);

  bool returned = false;
  DSValueToBoolean(val, returned);

  EXPECT_EQ(bool_val, returned);
}

TEST(BooleanToDSValue, CheckBooleanFalse) {
  bool bool_val = false;
  DSValue val;
  BooleanToDSValue(bool_val, val);

  bool returned = true;
  DSValueToBoolean(val, returned);

  EXPECT_EQ(bool_val, returned);
}

// On linux bool value only accepts true and false,else it throws out of scope
// error. But on windows it supports TRUE and FALSE as well.
#ifdef _WIN32
TEST(BooleanToDSValue, CheckCaseSensitive) {
  bool bool_val = TRUE;
  DSValue val;
  BooleanToDSValue(bool_val, val);

  bool returned = false;
  DSValueToBoolean(val, returned);

  EXPECT_EQ(bool_val, returned);
}
#endif  //_WIN32

TEST(GetMissingAttributesStr, Success_AllRequiredKeywordsPresent) {
  ConnectionHandle conn_handle;
  Section section;
  section["CATALOG"] = "BigQueryCatalog";
  section["OAUTHMECHANISM"] = "1";
  section["KEYFILEPATH"] = "/path/to/keyfile";

  conn_handle.SetUp(section, "");
  auto result = GetMissingAttributesStr(&conn_handle);
  EXPECT_FALSE(result.Ok());
}

TEST(GetMissingAttributesStr, Failure_MissingSomeKeywords) {
  ConnectionHandle conn_handle;
  Section dsn_section;
  dsn_section["CATALOG"] = "BigQueryCatalog";

  conn_handle.SetUp(dsn_section, "");
  auto result = GetMissingAttributesStr(&conn_handle);

  EXPECT_TRUE(result.Ok());
  EXPECT_EQ(result.GetValue(), "OAuthMechanism:OAuthMechanism=?;");
}

TEST(GetMissingAttributesStr, Failure_AllKeywordsMissing) {
  ConnectionHandle conn_handle;
  SQLCHAR out_conn_str[1024] = {0};
  SQLSMALLINT out_conn_str_len;

  auto result = GetMissingAttributesStr(&conn_handle);

  EXPECT_TRUE(result.Ok());
  EXPECT_EQ(result.GetValue(),
            "Catalog:Catalog=?;OAuthMechanism:OAuthMechanism=?;");
}

TEST(GetMissingAttributesStr, Failure_PartialMissingEmptyInput) {
  ConnectionHandle conn_handle;
  Section section;
  section["CATALOG"] = "BigQueryCatalog";
  section["OAUTHMECHANISM"] = "1";

  conn_handle.SetUp(section, "");
  auto result = GetMissingAttributesStr(&conn_handle);

  EXPECT_TRUE(result.Ok());
  EXPECT_EQ(result.GetValue(), "KeyFilePath:KeyFilePath=?;");
}

TEST(ValidateAllowedAttribute, Success) {
  ConnectionHandle conn_handle;
  Section section = {{"CATALOG", "TestVal"}, {"OAUTHMECHANISM", "TestVal"}};

  conn_handle.SetUp(section, "");
  StatusRecord status_record = ValidateAllowedAttributes(&conn_handle, section);
  EXPECT_FALSE(status_record.ok());
}

TEST(ValidateAllowedAttribute, Fail_NonRequestedAttribute) {
  ConnectionHandle conn_handle;
  Section section = {{"CATALOG", ""}, {"ExtraAttribute", ""}};

  StatusRecord status_record = ValidateAllowedAttributes(&conn_handle, section);

  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.message,
            "Connection Error: Non Requested connection attribute "
            "'ExtraAttribute' in ConnectionString");
}

TEST(ValidateAllowedAttribute, Fail_AlreadyFoundAttribute) {
  ConnectionHandle conn_handle;
  Section section = {{"DRIVER", "DriverName"}};

  conn_handle.SetUp(section, "");
  StatusRecord status_record =
      ValidateAllowedAttributes(&conn_handle, {{"DRIVER", "DriverName"}});

  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.message,
            "Connection Error: Connection Attribute 'DRIVER' already found!");
}

TEST(ValidateAllowedAttributes, Success_EmptyRequestedAttributes) {
  ConnectionHandle conn_handle;
  Section section = {{"OAUTHMECHANISM", ""}};

  StatusRecord status_record = ValidateAllowedAttributes(&conn_handle, section);
  EXPECT_TRUE(status_record.ok());
}

TEST(ArrayJsonToDSValue, String_ArrayType) {
  char buf[100];
  std::string src_val = R"([{"v":"apple"},{"v":"banana"},{"v":"peach"}])";
  std::string expected_val =
      "{\"v\":[{\"v\":\"apple\"},{\"v\":\"banana\"},{\"v\":\"peach\"}]}";
  DSValue value;
  ArrayJsonToDSValue(src_val, value, BQDataType::kString);

  std::string returned;

  DSValueToString(value, returned);

  EXPECT_EQ(expected_val, returned);
}

TEST(ArrayJsonToDSValue, Int_ArrayType) {
  char buf[100];
  std::string src_val = R"([{"v":"121"},{"v":"123"},{"v":"1212"}])";
  std::string expected_val =
      "{\"v\":[{\"v\":\"121\"},{\"v\":\"123\"},{\"v\":\"1212\"}]}";
  DSValue value;
  ArrayJsonToDSValue(src_val, value, BQDataType::kInt64);

  std::string returned;

  DSValueToString(value, returned);

  EXPECT_EQ(expected_val, returned);
}

TEST(ArrayJsonToDSValue, Bytes_ArrayType) {
  char buf[100];
  std::string src_val = R"([{"v":"YQ=="},{"v":"Yg=="},{"v":"Yw=="}])";
  std::string expected_val =
      "{\"v\":[{\"v\":\"0x61\"},{\"v\":\"0x62\"},{\"v\":\"0x63\"}]}";
  DSValue value;
  ArrayJsonToDSValue(src_val, value, BQDataType::kBytes);

  std::string returned;

  DSValueToString(value, returned);

  EXPECT_EQ(expected_val, returned);
}

TEST(ValidatingBinaryValues, Base64_To_Hex) {
  std::string src_val = "YQ==";
  std::string expected_val = "0x61";
  std::vector<uint8_t> decoded_data;
  Base64Decode(src_val, decoded_data);
  std::string returned;
  BytesToHex(decoded_data, returned);

  EXPECT_EQ(expected_val, returned);
}

TEST(ConvertUnixTimestampToTimestampStructTest, ValidUnixTimestamp) {
  SQL_TIMESTAMP_STRUCT timestamp_struct;
  double unix_timestamp =
      1609459200.0;  // Timestamp for 2021-01-01 00:00:00 UTC

  StatusRecord status =
      ConvertUnixTimestampToTimestampStruct(unix_timestamp, timestamp_struct);

  ASSERT_TRUE(status.ok());
  ASSERT_EQ(timestamp_struct.year, 2021);
  ASSERT_EQ(timestamp_struct.month, 1);
  ASSERT_EQ(timestamp_struct.day, 1);
  ASSERT_EQ(timestamp_struct.hour, 0);
  ASSERT_EQ(timestamp_struct.minute, 0);
  ASSERT_EQ(timestamp_struct.second, 0);
  ASSERT_EQ(timestamp_struct.fraction, 0);  // Since it's a whole second
}

TEST(ConvertUnixTimestampToTimestampStructTest, InvalidUnixTimestampNegative) {
  SQL_TIMESTAMP_STRUCT timestamp_struct;
  double unix_timestamp = -1609459200.0;  // Invalid negative timestamp

  StatusRecord status =
      ConvertUnixTimestampToTimestampStruct(unix_timestamp, timestamp_struct);

  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.sql_state, SQLStates::k_01004());
  ASSERT_EQ(status.message, "Invalid Unix timestamp");
}

TEST(ConvertUnixTimestampToTimestampStructTest, InvalidUnixTimestampNaN) {
  SQL_TIMESTAMP_STRUCT timestamp_struct;
  double unix_timestamp = std::nan("");  // NaN value

  StatusRecord status =
      ConvertUnixTimestampToTimestampStruct(unix_timestamp, timestamp_struct);

  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.sql_state, SQLStates::k_01004());
  ASSERT_EQ(status.message, "Invalid Unix timestamp");
}

TEST(ConvertUnixTimestampToTimestampStructTest, EdgeCaseUnixTimestampEpoch) {
  SQL_TIMESTAMP_STRUCT timestamp_struct;
  double unix_timestamp = 0.0;  // Unix epoch 1970-01-01 00:00:00 UTC

  StatusRecord status =
      ConvertUnixTimestampToTimestampStruct(unix_timestamp, timestamp_struct);

  ASSERT_TRUE(status.ok());
  ASSERT_EQ(timestamp_struct.year, 1970);
  ASSERT_EQ(timestamp_struct.month, 1);
  ASSERT_EQ(timestamp_struct.day, 1);
  ASSERT_EQ(timestamp_struct.hour, 0);
  ASSERT_EQ(timestamp_struct.minute, 0);
  ASSERT_EQ(timestamp_struct.second, 0);
  ASSERT_EQ(timestamp_struct.fraction, 0);  // No fractional part
}

TEST(ConvertUnixTimestampToTimestampStructTest, FutureTimestamp) {
  SQL_TIMESTAMP_STRUCT timestamp_struct;
  double unix_timestamp = 32503680000.0;  // 3000-01-01 00:00:00 UTC

  StatusRecord status =
      ConvertUnixTimestampToTimestampStruct(unix_timestamp, timestamp_struct);

  ASSERT_TRUE(status.ok());
  ASSERT_EQ(timestamp_struct.year, 3000);
  ASSERT_EQ(timestamp_struct.month, 1);
  ASSERT_EQ(timestamp_struct.day, 1);
  ASSERT_EQ(timestamp_struct.hour, 0);
  ASSERT_EQ(timestamp_struct.minute, 0);
  ASSERT_EQ(timestamp_struct.second, 0);
  ASSERT_EQ(timestamp_struct.fraction, 0);
}

TEST(ConvertUnixTimestampToTimestampStructTest, FractionalTimestamp) {
  SQL_TIMESTAMP_STRUCT timestamp_struct;
  double unix_timestamp = 1609459200.123456;  // Timestamp with fractional part

  StatusRecord status =
      ConvertUnixTimestampToTimestampStruct(unix_timestamp, timestamp_struct);

  ASSERT_TRUE(status.ok());
  ASSERT_EQ(timestamp_struct.year, 2021);
  ASSERT_EQ(timestamp_struct.month, 1);
  ASSERT_EQ(timestamp_struct.day, 1);
  ASSERT_EQ(timestamp_struct.hour, 0);
  ASSERT_EQ(timestamp_struct.minute, 0);
  ASSERT_EQ(timestamp_struct.second, 0);
  ASSERT_EQ(timestamp_struct.fraction, 123456);  // Microseconds (fraction part)
}

#ifdef _WIN32
TEST(EncryptPassword, EncryptAndDecryptPassword) {
  std::string pass = "abc";
  std::string encrypted_pass = EncryptPassword(pass);
  std::string decrypted_pass = DecryptPassword(encrypted_pass);
  EXPECT_EQ(pass, decrypted_pass);
}

TEST(EncryptPassword, DecryptionFailsForEmptyString) {
  std::string empty_encrypted_pass = "";
  std::string decrypted_pass = DecryptPassword(empty_encrypted_pass);
  EXPECT_EQ(decrypted_pass, "");
}

TEST(EncryptPassword, DecryptionFailsForModifiedData) {
  std::string pass = "abc";
  std::string encrypted_pass = EncryptPassword(pass);
  if (!encrypted_pass.empty()) {
    encrypted_pass[0] =
        (encrypted_pass[0] == '0') ? '1' : '0';  // Flip a hex character
  }

  std::string decrypted_pass = DecryptPassword(encrypted_pass);
  EXPECT_NE(pass, decrypted_pass);
  EXPECT_EQ(decrypted_pass, "");
}
#endif  //_WIN32

}  // namespace google::cloud::odbc_bq_driver_internal
