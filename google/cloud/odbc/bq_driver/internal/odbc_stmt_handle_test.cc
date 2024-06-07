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

#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/testing/bq_driver_utils/status_utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_bq_driver_utils::CreateExplicitDescriptor;
using ::google::cloud::odbc_testing_bq_driver_utils::GetLastStatusRecord;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TableSchema CreateTableSchema() {
  TableSchema schema;
  TableFieldSchema f1, f2, f3;

  f1.type = "STRING";
  f2.type = "INTEGER";
  f3.type = "STRING";

  f1.precision = 20;
  f2.precision = 10;
  f3.precision = 14;

  f1.max_length = 16384;
  f2.max_length = 20;
  f3.max_length = 16384;

  f1.mode = "NULLABLE";
  f2.mode = "NULLABLE";
  f3.mode = "NOTNull";

  f1.name = "Name";
  f2.name = "Id";
  f3.name = "Address";

  schema.fields.emplace_back(f1);
  schema.fields.emplace_back(f2);
  schema.fields.emplace_back(f3);

  return schema;
}

PostQueryResults CreatePostQueryResults() {
  PostQueryResults results;
  results.job_complete = true;
  results.schema = CreateTableSchema();
  return results;
}

TEST(GetDescriptorHandle, GetARD_impl) {
  DescriptorHandle ard(DescriptorType::kARD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle apd;
  DescriptorHandle ird;
  DescriptorHandle ipd;
  StatementHandle handle(nullptr, {ard, apd, ird, ipd});

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kARD);
  EXPECT_EQ(DescriptorType::kARD, desc_handle.GetType());
}

TEST(GetDescriptorHandle, GetAPD_impl) {
  DescriptorHandle ard;
  DescriptorHandle apd(DescriptorType::kAPD, SQL_DESC_ALLOC_AUTO);
  DescriptorHandle ird;
  DescriptorHandle ipd;
  StatementHandle handle(nullptr, {ard, apd, ird, ipd});

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kAPD);

  EXPECT_EQ(DescriptorType::kAPD, desc_handle.GetType());
}

TEST(GetDescriptorHandle, GetIRD_impl) {
  DescriptorHandle ard;
  DescriptorHandle apd;
  DescriptorHandle ird(DescriptorType::kIRD, SQL_DESC_ALLOC_AUTO);

  DescriptorHandle ipd;
  StatementHandle handle(nullptr, {ard, apd, ird, ipd});

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kIRD);

  EXPECT_EQ(DescriptorType::kIRD, desc_handle.GetType());
}

TEST(GetDescriptorHandle, GetIPD_impl) {
  DescriptorHandle ard;
  DescriptorHandle apd;
  DescriptorHandle ird;
  DescriptorHandle ipd(DescriptorType::kIPD, SQL_DESC_ALLOC_AUTO);

  StatementHandle handle(nullptr, {ard, apd, ird, ipd});

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kIPD);

  EXPECT_EQ(DescriptorType::kIPD, desc_handle.GetType());
}

TEST(SetDescriptorHandle, SetAndGetARD) {
  DescriptorHandle desc_impl;
  StatementHandle handle(nullptr, {desc_impl, desc_impl, desc_impl, desc_impl});
  DescriptorHandle desc = CreateExplicitDescriptor();

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kARD, &desc);

  EXPECT_TRUE(status_record.ok());

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kARD);

  EXPECT_EQ(desc.GetType(), desc_handle.GetType());
}

TEST(SetDescriptorHandle, SetAndGetAPD) {
  DescriptorHandle desc_impl;
  StatementHandle handle(nullptr, {desc_impl, desc_impl, desc_impl, desc_impl});
  DescriptorHandle desc = CreateExplicitDescriptor();

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kAPD, &desc);

  EXPECT_TRUE(status_record.ok());

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kAPD);

  EXPECT_EQ(desc.GetType(), desc_handle.GetType());
}

TEST(SetDescriptorHandle, Fails_InvalidAllocType) {
  StatementHandle handle;
  DescriptorHandle desc(DescriptorType::kApplication, SQL_DESC_ALLOC_AUTO);

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kAPD, &desc);

  EXPECT_EQ(SQLStates::k_HY017(), status_record.sql_state);
}

TEST(SetDescriptorHandle, Fails_InvalidType_IRD) {
  StatementHandle handle;
  DescriptorHandle desc = CreateExplicitDescriptor();

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kIRD, &desc);

  EXPECT_EQ(SQLStates::k_HY017(), status_record.sql_state);
}

TEST(SetDescriptorHandle, Fails_InvalidType_IPD) {
  StatementHandle handle;
  DescriptorHandle desc = CreateExplicitDescriptor();

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kIPD, &desc);

  EXPECT_EQ(SQLStates::k_HY017(), status_record.sql_state);
}

TEST(SetDescriptorHandle, SetExplicitDescAndThenSetNull) {
  DescriptorHandle desc_impl;
  StatementHandle handle(nullptr, {desc_impl, desc_impl, desc_impl, desc_impl});
  DescriptorHandle desc = CreateExplicitDescriptor();

  StatusRecord status_record =
      handle.SetDescriptorHandle(DescriptorType::kAPD, &desc);

  EXPECT_TRUE(status_record.ok());
  auto pair = *desc.GetAssociatedStatementHandles().begin();
  EXPECT_EQ(&handle, pair.first);
  EXPECT_EQ(DescriptorType::kAPD, pair.second);

  DescriptorHandle& get_desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kAPD);

  EXPECT_EQ(desc.GetHeaderRecord().GetAllocType(),
            get_desc_handle.GetHeaderRecord().GetAllocType());

  status_record = handle.SetDescriptorHandle(DescriptorType::kAPD, nullptr);

  EXPECT_TRUE(status_record.ok());
  EXPECT_EQ(0, desc.GetAssociatedStatementHandles().size());

  DescriptorHandle& get_desc_handle_new =
      handle.GetDescriptorHandle(DescriptorType::kAPD);

  EXPECT_EQ(desc_impl.GetHeaderRecord().GetAllocType(),
            get_desc_handle_new.GetHeaderRecord().GetAllocType());
  EXPECT_NE(desc.GetHeaderRecord().GetAllocType(),
            get_desc_handle_new.GetHeaderRecord().GetAllocType());
}

TEST(SetAttribute, Fails_InvalidAttribute) {
  StatementHandle handle;

  StatusRecord status_record = handle.SetAttribute(1111, 1111);

  EXPECT_EQ(SQLStates::k_HY092(), status_record.sql_state);
}

TEST(SetAttribute, Fails_InvalidAttributeValue) {
  StatementHandle handle;

  StatusRecord status_record = handle.SetAttribute(SQL_ATTR_ASYNC_ENABLE, 1111);

  EXPECT_EQ(SQLStates::k_HY024(), status_record.sql_state);
}

TEST(SetAttribute, SetAttribute_SQL_ATTR_ASYNC_ENABLE) {
  StatementHandle handle;

  StatusRecord status_record =
      handle.SetAttribute(SQL_ATTR_ASYNC_ENABLE, SQL_ASYNC_ENABLE_ON);

  EXPECT_TRUE(status_record.ok());

  StatusRecordOr<SQLULEN> val = handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE);

  EXPECT_EQ(SQL_ASYNC_ENABLE_ON, *val);
}

TEST(SetAttribute, SetAttribute_SQL_ATTR_ROW_NUMBER) {
  StatementHandle handle;

  StatusRecord status_record = handle.SetAttribute(SQL_ATTR_ROW_NUMBER, 1111);

  EXPECT_EQ(SQLStates::k_HY092(), status_record.sql_state);
}

TEST(GetAttribute, GetDefaultAttribute) {
  StatementHandle handle;

  StatusRecordOr<SQLULEN> val = handle.GetAttribute(SQL_ATTR_ASYNC_ENABLE);

  EXPECT_EQ(SQL_ASYNC_ENABLE_OFF, *val);
}

TEST(Populat_IRD_Descriptor, Invalid_Descriptor_Handle) {
  DescriptorHandle ard;
  DescriptorHandle apd;
  DescriptorHandle ird(DescriptorType::kIRD, SQL_DESC_ALLOC_AUTO);

  DescriptorHandle ipd;
  StatementHandle handle(nullptr, {ard, apd, ird, ipd});

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kIPD);

  PostQueryResults post_results = CreatePostQueryResults();

  StatusRecord ird_response =
      handle.PopulateIrd(desc_handle, post_results.schema);
  EXPECT_TRUE(!ird_response.ok());
  EXPECT_EQ(ird_response.sql_state, SQLStates::k_HY024());
}

TEST(Populat_IRD_Descriptor, PopulateIrdDescriptorHandle) {
  DescriptorHandle ard;
  DescriptorHandle apd;
  DescriptorHandle ird(DescriptorType::kIRD, SQL_DESC_ALLOC_AUTO);

  DescriptorHandle ipd;
  StatementHandle handle(nullptr, {ard, apd, ird, ipd});

  DescriptorHandle& desc_handle =
      handle.GetDescriptorHandle(DescriptorType::kIRD);

  PostQueryResults post_results = CreatePostQueryResults();

  StatusRecord ird_response =
      handle.PopulateIrd(desc_handle, post_results.schema);
  EXPECT_TRUE(ird_response.ok());

  DescriptorRecord descriptor_record;
  for (int i = 0; i < post_results.schema.fields.size(); ++i) {
    auto const& res = post_results.schema.fields[i];

    EXPECT_EQ(desc_handle.GetDescriptorRecord(i + 1).name, res.name);
    StatusRecordOr<SQLSMALLINT> type_status_record = GetSQLDataType(res.type);
    EXPECT_EQ(desc_handle.GetDescriptorRecord(i + 1).concise_type,
              *type_status_record);
    EXPECT_EQ(desc_handle.GetDescriptorRecord(i + 1).length, res.max_length);
    if(res.type == "STRING")
    EXPECT_EQ(desc_handle.GetDescriptorRecord(i + 1).precision, res.max_length);
    else
    EXPECT_EQ(desc_handle.GetDescriptorRecord(i + 1).precision, res.precision);
    SQLSMALLINT nullable = res.mode == "NULLABLE" ? SQL_NULLABLE : SQL_NO_NULLS;
    EXPECT_EQ(desc_handle.GetDescriptorRecord(i + 1).nullable, nullable);
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
