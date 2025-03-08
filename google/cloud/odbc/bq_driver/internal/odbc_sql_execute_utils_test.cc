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
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;
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
  strcpy((char*)cstr, value.c_str());
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
  strcpy((char*)cstr, value.c_str());
  SQLLEN str_len = value.size();
  PopulateDescriptors(apd, ipd, 1, SQL_C_CHAR, SQL_FLOAT, cstr, 50, &str_len);

  std::vector<QueryParameter> query_params = {str_param};
  StatusRecord status_record =
      ConstructPositionalQueryParams(apd, ipd, query_params);
  EXPECT_FALSE(status_record.ok());
  EXPECT_EQ(status_record.sql_state, SQLStates::k_HY000());
  EXPECT_EQ(status_record.message, "Conversion is unsupported");
}

TEST(ExecuteScript, Invalid_Statement_Handle) {
  PostQueryRequest req;
  StatementHandle stmt_handle;
  auto status_record_or = ExecuteScript(stmt_handle, req);
  EXPECT_FALSE(status_record_or.Ok());
  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY009(), "Invalid statement handle"));
}

TEST(ExecuteScript, Failure_Not_Connected) {
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

TEST(ExecuteScript, Failure_Null_BQClient) {
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

}  // namespace google::cloud::odbc_bq_driver_internal
