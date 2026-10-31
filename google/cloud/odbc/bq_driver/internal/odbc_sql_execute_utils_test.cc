// Copyright 2025 Google LLC
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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using ::google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using ::google::cloud::odbc_testing_utils::StatusRecordIs;
using odbc_internal::SQLStates;
using ::testing::HasSubstr;

void PopulateDescriptors(DescriptorHandle& apd, DescriptorHandle& ipd,
                         int col_ind, SQLSMALLINT c_type, SQLSMALLINT sql_type,
                         SQLPOINTER value_ptr, SQLLEN buf_len,
                         SQLLEN* ind_ptr) {
  DescriptorRecord apd_record;
  DescriptorRecord ipd_record;
  StatusRecord status_record;

  status_record = apd_record.SetConciseType(c_type, apd.GetType());
  EXPECT_TRUE(status_record.ok());

  status_record = ipd_record.SetConciseType(sql_type, ipd.GetType());
  EXPECT_TRUE(status_record.ok());

  apd_record.data_ptr = value_ptr;
  apd_record.octet_length = buf_len;
  apd_record.octet_length_ptr = ind_ptr;
  apd_record.indicator_ptr = ind_ptr;

  apd.BindNewDescriptorRecord(col_ind, apd_record);
  ipd.BindNewDescriptorRecord(col_ind, ipd_record);
}

TEST(ConstructPositionalQueryParams, Basic) {
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);

  QueryParameter float_param;
  float_param.parameter_type.type = "FLOAT64";
  SQLREAL float_value = 12345.67;
  PopulateDescriptors(apd, ipd, 1, SQL_C_FLOAT, SQL_CHAR, &float_value, 0,
                      nullptr);

  QueryParameter int_param;
  int_param.parameter_type.type = "INT64";
  PopulateDescriptors(apd, ipd, 2, SQL_C_FLOAT, SQL_SMALLINT, &float_value, 0,
                      nullptr);

  QueryParameter str_param;
  str_param.parameter_type.type = "STRING";
  std::string value = "Testing String";
  SQLCHAR cstr[50];
  strcpy(reinterpret_cast<char*>(cstr), value.c_str());
  SQLLEN str_len = value.size();
  PopulateDescriptors(apd, ipd, 3, SQL_C_CHAR, SQL_CHAR, cstr, 50, &str_len);

  std::vector<QueryParameter> query_params = {float_param, int_param,
                                              str_param};
  StatusRecord status_record =
      ConstructPositionalQueryParams(apd, ipd, query_params);

  EXPECT_NEAR(std::stod(query_params[0].parameter_value.value), 12345.67, 1e-3);
  EXPECT_EQ(query_params[1].parameter_value.value, "12345");
  EXPECT_EQ(query_params[2].parameter_value.value, "Testing String");
}

TEST(ConstructPositionalQueryParams, ParameterArrayColumnWise) {
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);

  std::vector<SQLBIGINT> ids = {101, 102, 103};
  std::vector<SQLLEN> id_inds = {sizeof(SQLBIGINT), sizeof(SQLBIGINT),
                                 sizeof(SQLBIGINT)};

  std::vector<std::array<char, 16>> names(3);
  std::strcpy(names[0].data(), "Alice");
  std::strcpy(names[1].data(), "Bob");
  std::strcpy(names[2].data(), "Charlie");
  std::vector<SQLLEN> name_inds = {SQL_NTS, SQL_NTS, SQL_NTS};

  PopulateDescriptors(apd, ipd, 1, SQL_C_SBIGINT, SQL_BIGINT, ids.data(),
                      sizeof(SQLBIGINT), id_inds.data());
  PopulateDescriptors(apd, ipd, 2, SQL_C_CHAR, SQL_VARCHAR, names.data(), 16,
                      name_inds.data());

  QueryParameter id_param;
  id_param.parameter_type.type = "INT64";
  QueryParameter name_param;
  name_param.parameter_type.type = "STRING";

  // Test row 0
  std::vector<QueryParameter> params0 = {id_param, name_param};
  EXPECT_TRUE(ConstructPositionalQueryParams(apd, ipd, params0, false, 0).ok());
  EXPECT_EQ(params0[0].parameter_value.value, "101");
  EXPECT_EQ(params0[1].parameter_value.value, "Alice");

  // Test row 1
  std::vector<QueryParameter> params1 = {id_param, name_param};
  EXPECT_TRUE(ConstructPositionalQueryParams(apd, ipd, params1, false, 1).ok());
  EXPECT_EQ(params1[0].parameter_value.value, "102");
  EXPECT_EQ(params1[1].parameter_value.value, "Bob");

  // Test row 2
  std::vector<QueryParameter> params2 = {id_param, name_param};
  EXPECT_TRUE(ConstructPositionalQueryParams(apd, ipd, params2, false, 2).ok());
  EXPECT_EQ(params2[0].parameter_value.value, "103");
  EXPECT_EQ(params2[1].parameter_value.value, "Charlie");
}

TEST(ConstructPositionalQueryParams, ParameterArrayRowWise) {
  struct RowData {
    SQLBIGINT id;
    SQLLEN id_ind;
    char name[16];
    SQLLEN name_ind;
  };

  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);
  apd.GetHeaderRecord().bind_type = sizeof(RowData);

  std::vector<RowData> rows = {
      {201, sizeof(SQLBIGINT), "Dave", SQL_NTS},
      {202, sizeof(SQLBIGINT), "Eve", SQL_NTS},
  };

  PopulateDescriptors(apd, ipd, 1, SQL_C_SBIGINT, SQL_BIGINT, &rows[0].id,
                      sizeof(SQLBIGINT), &rows[0].id_ind);
  PopulateDescriptors(apd, ipd, 2, SQL_C_CHAR, SQL_VARCHAR, rows[0].name, 16,
                      &rows[0].name_ind);

  QueryParameter id_param;
  id_param.parameter_type.type = "INT64";
  QueryParameter name_param;
  name_param.parameter_type.type = "STRING";

  // Test row 0
  std::vector<QueryParameter> params0 = {id_param, name_param};
  EXPECT_TRUE(ConstructPositionalQueryParams(apd, ipd, params0, false, 0).ok());
  EXPECT_EQ(params0[0].parameter_value.value, "201");
  EXPECT_EQ(params0[1].parameter_value.value, "Dave");

  // Test row 1
  std::vector<QueryParameter> params1 = {id_param, name_param};
  EXPECT_TRUE(ConstructPositionalQueryParams(apd, ipd, params1, false, 1).ok());
  EXPECT_EQ(params1[0].parameter_value.value, "202");
  EXPECT_EQ(params1[1].parameter_value.value, "Eve");
}

TEST(ConstructPositionalQueryParams, ParameterArrayNullData) {
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);

  std::vector<SQLBIGINT> ids = {301, 302};
  std::vector<SQLLEN> id_inds = {sizeof(SQLBIGINT), SQL_NULL_DATA};

  PopulateDescriptors(apd, ipd, 1, SQL_C_SBIGINT, SQL_BIGINT, ids.data(),
                      sizeof(SQLBIGINT), id_inds.data());

  QueryParameter id_param;
  id_param.parameter_type.type = "INT64";

  // Row 0 has value
  std::vector<QueryParameter> params0 = {id_param};
  EXPECT_TRUE(ConstructPositionalQueryParams(apd, ipd, params0, false, 0).ok());
  EXPECT_EQ(params0[0].parameter_value.value, "301");

  // Row 1 is null
  std::vector<QueryParameter> params1 = {id_param};
  EXPECT_TRUE(ConstructPositionalQueryParams(apd, ipd, params1, false, 1).ok());
  EXPECT_EQ(params1[0].parameter_value.value, "");
}

TEST(ConstructPositionalQueryParams, DescRecNotExists) {
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);

  QueryParameter float_param;
  float_param.parameter_type.type = "FLOAT64";

  std::vector<QueryParameter> query_params = {float_param};
  StatusRecord status_record =
      ConstructPositionalQueryParams(apd, ipd, query_params);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_07002());
  EXPECT_EQ(
      status_record.message,
      "Expected descriptor record does not exist during query execution.");
}

TEST(ConstructPositionalQueryParams, NullDataPtr) {
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);

  QueryParameter float_param;
  float_param.parameter_type.type = "FLOAT64";
  PopulateDescriptors(apd, ipd, 1, SQL_C_FLOAT, SQL_CHAR, nullptr, 0, nullptr);

  std::vector<QueryParameter> query_params = {float_param};
  StatusRecord status_record =
      ConstructPositionalQueryParams(apd, ipd, query_params);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY009());
  EXPECT_EQ(status_record.message, "The bound param buffer was null");
}

TEST(ConstructPositionalQueryParams, InvalidConversion) {
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);

  QueryParameter str_param;
  str_param.parameter_type.type = "STRING";
  std::string value = "Testing String";
  SQLCHAR cstr[50];
  strcpy(reinterpret_cast<char*>(cstr), value.c_str());
  SQLLEN str_len = value.size();
  PopulateDescriptors(apd, ipd, 1, SQL_C_CHAR, SQL_FLOAT, cstr, 50, &str_len);

  std::vector<QueryParameter> query_params = {str_param};
  StatusRecord status_record =
      ConstructPositionalQueryParams(apd, ipd, query_params);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY000());
  EXPECT_EQ(status_record.message, "Conversion is unsupported");
}

TEST(ExecuteScript, InvalidStatementHandle) {
  PostQueryRequest req;
  StatementHandle stmt_handle;
  auto status_record_or = ExecuteScript(stmt_handle, req);
  EXPECT_FALSE(status_record_or.Ok());
  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY009(), "Invalid statement handle"));
}

TEST(ExecuteScript, FailureNotConnected) {
  PostQueryRequest req;

  // Create a valid connection handle but mark it as disconnected
  auto conn_handle = CreateConnectionHandle(false);

  // Create a statement handle associated with this connection
  StatementHandle stmt_handle(&conn_handle);

  // Ensure the connection handle exists but is not connected
  ASSERT_NE(stmt_handle.GetConnectionHandle(), nullptr);
  ASSERT_FALSE(stmt_handle.GetConnectionHandle()->IsConnected());

  // Execute and validate failure due to broken connection
  auto status_record_or = ExecuteScript(stmt_handle, req);

  EXPECT_FALSE(status_record_or.Ok());
  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_08S01(),
                     HasSubstr("Connection to the data source is broken")));
}

TEST(ExecuteScript, FailureNullBQClient) {
  PostQueryRequest req;

  // Create a valid connection handle
  auto conn_handle = CreateConnectionHandle();

  // Create a statement handle associated with this connection
  StatementHandle stmt_handle(&conn_handle);

  // Ensure the connection handle is valid
  ASSERT_NE(stmt_handle.GetConnectionHandle(), nullptr);

  // Execute and validate failure due to null BQ Client
  auto status_record_or = ExecuteScript(stmt_handle, req);

  EXPECT_FALSE(status_record_or.Ok());
  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HY000(),
          HasSubstr("Invalid or null BQ Client within the connection handle")));
}

TEST(FetchBQResults, FailureNotConnected) {
  PostQueryRequest req;
  ConnectionHandle conn_handle;
  StatementHandle handle(&conn_handle);
  auto status_record_or = FetchBQData(handle, req);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_08S01(),
                     HasSubstr("Connection to the data source is broken")));
}

TEST(FetchBQResults, FailureNullBqclient) {
  PostQueryRequest req;
  auto conn_handle = CreateConnectionHandle(true);
  StatementHandle handle(&conn_handle);
  auto status_record_or = FetchBQData(handle, req);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HY000(),
          HasSubstr("Invalid or null BQ Client within the connection handle")));
}

TEST(FetchNextPageResultSet, FailureSQLNODATA) {
  auto conn_handle = CreateConnectionHandle(true);
  StatementHandle handle(&conn_handle);
  auto status_record_or = FetchNextPageResultSet(handle);
  EXPECT_EQ(status_record_or.sql_state, SQLStates::k_SQL_NO_DATA());
  EXPECT_EQ(status_record_or.message, "No more data to return.");
}

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
TEST(IsTransientStreamError, IdentifiesTransientAndPermanentErrors) {
  using google::cloud::Status;
  using google::cloud::StatusCode;

  EXPECT_TRUE(
      IsTransientStreamError(Status(StatusCode::kUnavailable, "unavailable")));
  EXPECT_TRUE(IsTransientStreamError(
      Status(StatusCode::kDeadlineExceeded, "deadline exceeded")));
  EXPECT_TRUE(IsTransientStreamError(Status(StatusCode::kAborted, "aborted")));
  EXPECT_TRUE(IsTransientStreamError(
      Status(StatusCode::kResourceExhausted, "quota exceeded")));

  EXPECT_TRUE(IsTransientStreamError(
      Status(StatusCode::kInternal, "Received RST_STREAM with error code 2")));
  EXPECT_TRUE(
      IsTransientStreamError(Status(StatusCode::kUnknown, "Stream removed")));
  EXPECT_TRUE(IsTransientStreamError(
      Status(StatusCode::kInternal, "Connection reset by peer")));
  EXPECT_TRUE(IsTransientStreamError(
      Status(StatusCode::kUnknown, "transport: lost connection")));

  EXPECT_FALSE(IsTransientStreamError(
      Status(StatusCode::kInternal, "Permanent internal parser error")));
  EXPECT_FALSE(IsTransientStreamError(
      Status(StatusCode::kInvalidArgument, "Invalid offset")));
  EXPECT_FALSE(IsTransientStreamError(
      Status(StatusCode::kPermissionDenied, "Access denied")));
}

TEST(HtapiStreamRetryPolicy, RetriesTransientErrorsUpToMaxRetries) {
  using google::cloud::Status;
  using google::cloud::StatusCode;

  HtapiStreamRetryPolicy policy(/*maximum_failures=*/2);
  EXPECT_FALSE(policy.IsExhausted());
  EXPECT_FALSE(policy.IsPermanentFailure(
      Status(StatusCode::kUnknown, "Stream removed")));
  EXPECT_TRUE(policy.IsPermanentFailure(
      Status(StatusCode::kPermissionDenied, "Access denied")));

  EXPECT_TRUE(policy.OnFailure(Status(StatusCode::kUnknown, "Stream removed")));
  EXPECT_FALSE(policy.IsExhausted());
  EXPECT_TRUE(policy.OnFailure(
      Status(StatusCode::kInternal, "Received RST_STREAM with error code 2")));
  EXPECT_FALSE(policy.IsExhausted());
  EXPECT_FALSE(
      policy.OnFailure(Status(StatusCode::kUnknown, "Stream removed")));
  EXPECT_TRUE(policy.IsExhausted());

  auto cloned = policy.clone();
  EXPECT_FALSE(cloned->IsExhausted());
  EXPECT_FALSE(cloned->OnFailure(
      Status(StatusCode::kInvalidArgument, "Invalid offset")));
  EXPECT_TRUE(cloned->IsExhausted());
}
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

}  // namespace google::cloud::odbc_bq_driver_internal
