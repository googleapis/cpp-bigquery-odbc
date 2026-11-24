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
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
#include <arrow/ipc/writer.h>
#include <arrow/util/decimal.h>
#endif
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>

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

namespace {

::google::cloud::bigquery::storage::v1::ArrowRecordBatch SerializeBatchToProto(
    std::shared_ptr<arrow::RecordBatch> const& batch) {
  auto serialized = arrow::ipc::SerializeRecordBatch(
      *batch, arrow::ipc::IpcWriteOptions::Defaults());
  EXPECT_TRUE(serialized.ok()) << serialized.status().ToString();
  ::google::cloud::bigquery::storage::v1::ArrowRecordBatch proto;
  if (serialized.ok() && *serialized) {
    proto.set_serialized_record_batch((*serialized)->ToString());
  }
  return proto;
}

}  // namespace

TEST(HtapiArrowValidationTest, ValidRecordBatchRoundTrips) {
  auto schema = arrow::schema({
      arrow::field("i", arrow::int64()),
      arrow::field("s", arrow::utf8()),
      arrow::field("b", arrow::binary()),
      arrow::field("flag", arrow::boolean()),
  });

  arrow::Int64Builder int_builder;
  ASSERT_TRUE(int_builder.Append(42).ok());
  ASSERT_TRUE(int_builder.AppendNull().ok());

  arrow::StringBuilder str_builder;
  ASSERT_TRUE(str_builder.Append("hello").ok());
  ASSERT_TRUE(str_builder.Append("world").ok());

  arrow::BinaryBuilder bin_builder;
  ASSERT_TRUE(bin_builder.Append("ab", 2).ok());
  ASSERT_TRUE(bin_builder.AppendNull().ok());

  arrow::BooleanBuilder bool_builder;
  ASSERT_TRUE(bool_builder.Append(true).ok());
  ASSERT_TRUE(bool_builder.Append(false).ok());

  std::shared_ptr<arrow::Array> int_arr;
  std::shared_ptr<arrow::Array> str_arr;
  std::shared_ptr<arrow::Array> bin_arr;
  std::shared_ptr<arrow::Array> bool_arr;
  ASSERT_TRUE(int_builder.Finish(&int_arr).ok());
  ASSERT_TRUE(str_builder.Finish(&str_arr).ok());
  ASSERT_TRUE(bin_builder.Finish(&bin_arr).ok());
  ASSERT_TRUE(bool_builder.Finish(&bool_arr).ok());

  auto batch = arrow::RecordBatch::Make(schema, 2,
                                        {int_arr, str_arr, bin_arr, bool_arr});
  auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
  ASSERT_TRUE(batch_or.Ok()) << batch_or.GetStatusRecord().message;

  ResultSet rs;
  StatusRecord status = ProcessRecordBatch(schema, *batch_or, rs);
  ASSERT_TRUE(status.ok()) << status.message;
  ASSERT_EQ(rs.rows.size(), 2U);
  ASSERT_EQ(rs.rows[0].size(), 4U);
  EXPECT_EQ(DSValueToInt(rs.rows[0][0]), 42);
  EXPECT_TRUE(IsDSValueNull(rs.rows[1][0]));
  std::string s0;
  DSValueToString(rs.rows[0][1], s0);
  EXPECT_EQ(s0, "hello");
  std::string b0;
  DSValueToString(rs.rows[0][2], b0);
  EXPECT_EQ(b0, "ab");
  EXPECT_TRUE(IsDSValueNull(rs.rows[1][2]));
}

TEST(HtapiArrowValidationTest, RejectsOutOfBoundsOffsetsAndTruncatedBuffers) {
  // 1. BinaryArray with offsets [0, 65536] over a 1-byte data buffer.
  {
    auto schema = arrow::schema({arrow::field("b", arrow::binary())});
    std::vector<int32_t> bad_offsets = {0, 65536};
    auto offsets_buf = arrow::Buffer::Wrap(bad_offsets);
    std::string one_byte = "x";
    auto values_buf = arrow::Buffer::FromString(one_byte);
    auto col_data = arrow::ArrayData::Make(arrow::binary(), /*length=*/1,
                                           {nullptr, offsets_buf, values_buf},
                                           /*null_count=*/0);
    auto batch = arrow::RecordBatch::Make(schema, 1, {col_data});

    auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
    EXPECT_FALSE(batch_or.Ok());
    EXPECT_EQ(batch_or.GetStatusRecord().sql_state, SQLStates::k_HY000());
  }

  // 2. StringArray with non-monotonic interior offsets [0, 4, 2] over 4 bytes.
  {
    auto schema = arrow::schema({arrow::field("s", arrow::utf8())});
    std::vector<int32_t> bad_offsets = {0, 4, 2};
    auto offsets_buf = arrow::Buffer::Wrap(bad_offsets);
    auto values_buf = arrow::Buffer::FromString("abcd");
    auto col_data = arrow::ArrayData::Make(arrow::utf8(), /*length=*/2,
                                           {nullptr, offsets_buf, values_buf},
                                           /*null_count=*/0);
    auto batch = arrow::RecordBatch::Make(schema, 2, {col_data});

    auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
    EXPECT_FALSE(batch_or.Ok());
    EXPECT_EQ(batch_or.GetStatusRecord().sql_state, SQLStates::k_HY000());
  }

  // 3. Int64Array declaring 2 rows over a 4-byte buffer.
  {
    auto schema = arrow::schema({arrow::field("i", arrow::int64())});
    auto short_buf = arrow::Buffer::FromString("1234");
    auto col_data = arrow::ArrayData::Make(arrow::int64(), /*length=*/2,
                                           {nullptr, short_buf},
                                           /*null_count=*/0);
    auto batch = arrow::RecordBatch::Make(schema, 2, {col_data});

    auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
    EXPECT_FALSE(batch_or.Ok());
    EXPECT_EQ(batch_or.GetStatusRecord().sql_state, SQLStates::k_HY000());
  }

  // 4. List<Decimal256> with out-of-bounds list offsets [0, 5] over 1 item.
  {
    auto dec_type = arrow::decimal256(76, 38);
    auto list_type = arrow::list(dec_type);
    auto schema = arrow::schema({arrow::field("arr", list_type)});

    arrow::Decimal256Builder dec_builder(dec_type);
    ASSERT_TRUE(dec_builder.Append(arrow::Decimal256(1)).ok());
    std::shared_ptr<arrow::Array> dec_arr;
    ASSERT_TRUE(dec_builder.Finish(&dec_arr).ok());

    std::vector<int32_t> bad_list_offsets = {0, 5};
    auto offsets_buf = arrow::Buffer::Wrap(bad_list_offsets);
    auto list_data = arrow::ArrayData::Make(
        list_type, /*length=*/1, {nullptr, offsets_buf}, {dec_arr->data()},
        /*null_count=*/0);
    auto batch = arrow::RecordBatch::Make(schema, 1, {list_data});

    auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
    EXPECT_FALSE(batch_or.Ok());
    EXPECT_EQ(batch_or.GetStatusRecord().sql_state, SQLStates::k_HY000());
  }

  // 5. List<Decimal256> with interior out-of-bounds offset [0, 5, 1] over 1
  // item (passes O(1) Validate() extent check, caught by ValidateFull() on the
  // decimal-stripped view).
  {
    auto dec_type = arrow::decimal256(76, 38);
    auto list_type = arrow::list(dec_type);
    auto schema = arrow::schema({arrow::field("arr", list_type)});

    arrow::Decimal256Builder dec_builder(dec_type);
    ASSERT_TRUE(dec_builder.Append(arrow::Decimal256(1)).ok());
    std::shared_ptr<arrow::Array> dec_arr;
    ASSERT_TRUE(dec_builder.Finish(&dec_arr).ok());

    std::vector<int32_t> bad_interior_offsets = {0, 5, 1};
    auto offsets_buf = arrow::Buffer::Wrap(bad_interior_offsets);
    auto list_data = arrow::ArrayData::Make(
        list_type, /*length=*/2, {nullptr, offsets_buf}, {dec_arr->data()},
        /*null_count=*/0);
    auto batch = arrow::RecordBatch::Make(schema, 2, {list_data});
    ASSERT_TRUE(batch->Validate().ok());

    auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
    EXPECT_FALSE(batch_or.Ok());
    EXPECT_EQ(batch_or.GetStatusRecord().sql_state, SQLStates::k_HY000());
  }

  // 6. Struct<Decimal256, String> with invalid UTF-8 in sibling String field
  // (passes O(1) Validate(), caught by ValidateFull() on decimal-stripped
  // view).
  {
    auto dec_type = arrow::decimal256(76, 38);
    auto struct_type = arrow::struct_({
        arrow::field("d", dec_type),
        arrow::field("s", arrow::utf8()),
    });
    auto schema = arrow::schema({arrow::field("st", struct_type)});

    arrow::Decimal256Builder dec_builder(dec_type);
    ASSERT_TRUE(dec_builder.Append(arrow::Decimal256(1)).ok());
    std::shared_ptr<arrow::Array> dec_arr;
    ASSERT_TRUE(dec_builder.Finish(&dec_arr).ok());

    std::vector<int32_t> str_offsets = {0, 2};
    auto bad_utf8_buf = arrow::Buffer::FromString("\xFF\xFE");
    auto bad_str_data = arrow::ArrayData::Make(
        arrow::utf8(), /*length=*/1,
        {nullptr, arrow::Buffer::Wrap(str_offsets), bad_utf8_buf},
        /*null_count=*/0);
    auto st_data = arrow::ArrayData::Make(
        struct_type, /*length=*/1,
        std::vector<std::shared_ptr<arrow::Buffer>>{nullptr},
        {dec_arr->data(), bad_str_data}, /*null_count=*/0);
    auto batch = arrow::RecordBatch::Make(schema, 1, {st_data});
    ASSERT_TRUE(batch->Validate().ok());

    auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
    EXPECT_FALSE(batch_or.Ok());
    EXPECT_EQ(batch_or.GetStatusRecord().sql_state, SQLStates::k_HY000());
  }
}

TEST(HtapiArrowValidationTest, Accepts77DigitBigNumericTopLevelAndNested) {
  auto dec_type = arrow::decimal256(76, 38);
  auto list_type = arrow::list(dec_type);
  auto struct_type = arrow::struct_({arrow::field("d", dec_type)});
  auto schema = arrow::schema({
      arrow::field("top_dec", dec_type),
      arrow::field("arr_dec", list_type),
      arrow::field("st_dec", struct_type),
  });

  // Max BigQuery BIGNUMERIC value has 77 decimal digits, exceeding precision 76
  std::string const max_big_numeric =
      "578960446186580977117854925043439539266."
      "34992332820282019728792003956564819967";
  arrow::Decimal256 wide_val;
  int32_t parsed_precision = 0;
  int32_t parsed_scale = 0;
  ASSERT_TRUE(arrow::Decimal256::FromString(max_big_numeric, &wide_val,
                                            &parsed_precision, &parsed_scale)
                  .ok());
  ASSERT_EQ(parsed_precision, 77);
  ASSERT_EQ(parsed_scale, 38);

  arrow::Decimal256Builder dec_builder(dec_type);
  ASSERT_TRUE(dec_builder.Append(wide_val).ok());
  std::shared_ptr<arrow::Array> dec_arr;
  ASSERT_TRUE(dec_builder.Finish(&dec_arr).ok());
  // Confirm upstream Arrow ValidateFull() rejects 77-digit values directly.
  EXPECT_FALSE(dec_arr->ValidateFull().ok());

  std::vector<int32_t> list_offsets = {0, 1};
  auto list_arr = std::make_shared<arrow::ListArray>(
      list_type, /*length=*/1, arrow::Buffer::Wrap(list_offsets), dec_arr);
  auto struct_arr = std::make_shared<arrow::StructArray>(
      struct_type, /*length=*/1,
      std::vector<std::shared_ptr<arrow::Array>>{dec_arr});

  auto batch =
      arrow::RecordBatch::Make(schema, 1, {dec_arr, list_arr, struct_arr});
  auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
  ASSERT_TRUE(batch_or.Ok()) << batch_or.GetStatusRecord().message;

  ResultSet rs;
  StatusRecord status = ProcessRecordBatch(schema, *batch_or, rs);
  ASSERT_TRUE(status.ok()) << status.message;
  ASSERT_EQ(rs.rows.size(), 1U);
  std::string formatted;
  DSValueToString(rs.rows[0][0], formatted);
  EXPECT_EQ(formatted, max_big_numeric);
}

TEST(HtapiArrowValidationTest, RejectsRowCountOutsideIntRange) {
  auto empty_schema = arrow::schema({});

  auto over_int_max = arrow::RecordBatch::Make(
      empty_schema, static_cast<int64_t>(std::numeric_limits<int>::max()) + 1,
      std::vector<std::shared_ptr<arrow::Array>>{});
  ResultSet rs;
  StatusRecord status = ProcessRecordBatch(empty_schema, over_int_max, rs);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_TRUE(rs.rows.empty());

  auto negative_rows = arrow::RecordBatch::Make(
      empty_schema, -1, std::vector<std::shared_ptr<arrow::Array>>{});
  status = ProcessRecordBatch(empty_schema, negative_rows, rs);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
  EXPECT_TRUE(rs.rows.empty());
}

TEST(HtapiArrowValidationTest, RejectsUnbackedOrAliasedBuffers) {
  ResultSet rs;

  // 1. Zero-column batch claiming millions of rows with 0-byte backing buffers.
  {
    auto empty_schema = arrow::schema({});
    auto unbacked_batch = arrow::RecordBatch::Make(
        empty_schema, 1000000, std::vector<std::shared_ptr<arrow::Array>>{});
    StatusRecord status = ProcessRecordBatch(empty_schema, unbacked_batch, rs);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
    EXPECT_TRUE(rs.rows.empty());
  }

  // 2. Empty-struct column with null_count = 0 and null validity buffer (0
  // bytes backing 10,000 rows).
  {
    auto st_type = arrow::struct_({});
    auto schema = arrow::schema({arrow::field("st", st_type)});
    auto st_data = arrow::ArrayData::Make(
        st_type, /*length=*/10000,
        std::vector<std::shared_ptr<arrow::Buffer>>{nullptr},
        /*child_data=*/{}, /*null_count=*/0);
    auto batch = arrow::RecordBatch::Make(schema, 10000, {st_data});
    StatusRecord status = ProcessRecordBatch(schema, batch, rs);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
    EXPECT_TRUE(rs.rows.empty());
  }

  // 3. Two 8-row BOOL columns aliasing the same 1-byte buffer (16 cells > 8
  // backing bits).
  {
    auto schema = arrow::schema({
        arrow::field("b1", arrow::boolean()),
        arrow::field("b2", arrow::boolean()),
    });
    auto shared_one_byte = arrow::Buffer::FromString("\xAA");
    auto c1 = arrow::ArrayData::Make(arrow::boolean(), 8,
                                     {nullptr, shared_one_byte}, 0);
    auto c2 = arrow::ArrayData::Make(arrow::boolean(), 8,
                                     {nullptr, shared_one_byte}, 0);
    auto batch = arrow::RecordBatch::Make(schema, 8, {c1, c2});
    StatusRecord status = ProcessRecordBatch(schema, batch, rs);
    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.sql_state, SQLStates::k_HY000());
    EXPECT_TRUE(rs.rows.empty());
  }
}

TEST(HtapiArrowValidationTest, AcceptsBitPackedBoolAtTightBoundAndEmptyBatch) {
  auto schema = arrow::schema({arrow::field("b", arrow::boolean())});

  // 8 BOOL rows backed by exactly 1 byte (8 bits == 8 cells).
  arrow::BooleanBuilder builder;
  for (int i = 0; i < 8; ++i) {
    ASSERT_TRUE(builder.Append((i % 2) == 0).ok());
  }
  std::shared_ptr<arrow::Array> bool_arr;
  ASSERT_TRUE(builder.Finish(&bool_arr).ok());
  auto batch = arrow::RecordBatch::Make(schema, 8, {bool_arr});

  auto batch_or = GetArrowRecordBatch(SerializeBatchToProto(batch), schema);
  ASSERT_TRUE(batch_or.Ok()) << batch_or.GetStatusRecord().message;

  ResultSet rs;
  StatusRecord status = ProcessRecordBatch(schema, *batch_or, rs);
  ASSERT_TRUE(status.ok()) << status.message;
  ASSERT_EQ(rs.rows.size(), 8U);
  bool val0 = false;
  bool val1 = true;
  DSValueToBoolean(rs.rows[0][0], val0);
  DSValueToBoolean(rs.rows[1][0], val1);
  EXPECT_TRUE(val0);
  EXPECT_FALSE(val1);

  // Empty 0-row batch resets rows cleanly.
  auto empty_batch_or = arrow::RecordBatch::MakeEmpty(schema);
  ASSERT_TRUE(empty_batch_or.ok());
  status = ProcessRecordBatch(schema, *empty_batch_or, rs);
  ASSERT_TRUE(status.ok()) << status.message;
  EXPECT_TRUE(rs.rows.empty());
}
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

}  // namespace google::cloud::odbc_bq_driver_internal
