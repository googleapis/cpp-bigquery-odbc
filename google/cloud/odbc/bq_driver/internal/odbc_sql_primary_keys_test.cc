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

using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

std::string const kCatalog = "test-catalog";
std::string const kDataset = "test-schema";
std::string const kTable = "test-table";

int const kCatalogLen = kCatalog.length();
int const kDatasetLen = kDataset.length();
int const kTableLen = kTable.length();

TEST(FetchPrimaryKeys, failureEmptyCatalogName) {
  StatementHandle handle;
  auto status_record_or = FetchPrimaryKeysFromDataSource(
      handle, "", kCatalogLen, kDataset, kDatasetLen, kTable, kTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Parameter catalog_name cannot be empty")));
}
TEST(FetchPrimaryKeys, failureEmptyCatalogLen) {
  StatementHandle handle;
  auto status_record_or = FetchPrimaryKeysFromDataSource(
      handle, kCatalog, 0, kDataset, kDatasetLen, kTable, kTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Parameter catalog_name cannot be empty")));
}

TEST(FetchPrimaryKeys, failureEmptySchemaName) {
  StatementHandle handle;
  auto status_record_or = FetchPrimaryKeysFromDataSource(
      handle, kCatalog, kCatalogLen, "", kDatasetLen, kTable, kTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Parameter schema_name cannot be empty")));
}
TEST(FetchPrimaryKeys, failureEmptySchemaLen) {
  StatementHandle handle;
  auto status_record_or = FetchPrimaryKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, 0, kTable, kTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Parameter schema_name cannot be empty")));
}

TEST(FetchPrimaryKeys, failureEmptyTableName) {
  StatementHandle handle;
  auto status_record_or = FetchPrimaryKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, "", kTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Parameter table_name cannot be empty")));
}
TEST(FetchPrimaryKeys, failureEmptyTableLen) {
  StatementHandle handle;
  auto status_record_or = FetchPrimaryKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, kTable, 0);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Parameter table_name cannot be empty")));
}

TEST(FetchPrimaryKeys, FailureNullConnectionhandle) {
  StatementHandle handle;
  auto status_record_or = FetchPrimaryKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, kTable, kTableLen);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY013(),
                             HasSubstr("Internal connection handle is null")));
}
}  // namespace google::cloud::odbc_bq_driver_internal
