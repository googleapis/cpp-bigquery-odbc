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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_foreign_keys.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

std::string const kCatalog = "test-catalog";
std::string const kDataset = "test-schema";
std::string const kPKTable = "pk-test-table";
std::string const kFKTable = "fk-test-table";

int const kCatalogLen = kCatalog.length();
int const kDatasetLen = kDataset.length();
int const kPKTableLen = kPKTable.length();
int const kFKTableLen = kFKTable.length();

TEST(FetchForeignKeys, failure_empty_catalog_name) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, "", kCatalogLen, kDataset, kDatasetLen, kPKTable, kPKTableLen, "",
      kCatalogLen, kDataset, kDatasetLen, kFKTable, kFKTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Catalog name for both primary and foreign keys "
                               "cannot be empty")));
}
TEST(FetchForeignKeys, failure_empty_catalog_name_len) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, 0, kDataset, kDatasetLen, kPKTable, kPKTableLen,
      kCatalog, 0, kDataset, kDatasetLen, kFKTable, kFKTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Catalog name for both primary and foreign keys "
                               "cannot be empty")));
}

TEST(FetchForeignKeys, failure_empty_schema_name) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, kCatalogLen, "", kDatasetLen, kPKTable, kPKTableLen,
      kCatalog, kCatalogLen, "", kDatasetLen, kFKTable, kFKTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Schema name for both primary and foreign keys "
                               "cannot be empty")));
}
TEST(FetchForeignKeys, failure_empty_schema_name_len) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, 0, kPKTable, kPKTableLen,
      kCatalog, kCatalogLen, kDataset, 0, kFKTable, kFKTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(SQLStates::k_HY090(),
                     HasSubstr("Schema name for both primary and foreign keys "
                               "cannot be empty")));
}

TEST(FetchForeignKeys, failure_different_primary_foreign_catalog) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, kPKTable,
      kPKTableLen, "fk-catalog", 10 /* fk catalog len*/, kDataset, kDatasetLen,
      kFKTable, kFKTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HYC00(),
          HasSubstr("Optional feature not supported by the data source: PK "
                    "and FK catalog needs to be the same")));
}

TEST(FetchForeignKeys, failure_different_primary_foreign_schema) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, kPKTable,
      kPKTableLen, kCatalog, kCatalogLen, "fk-dataset", 10 /* fk dataset len*/,
      kFKTable, kFKTableLen);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HYC00(),
          HasSubstr("Optional feature not supported by the data source: PK "
                    "and FK schema needs to be the same")));
}

TEST(FetchForeignKeys, failure_empty_table_name) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, "", 0, kCatalog,
      kCatalogLen, kDataset, kDatasetLen, "", 0);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HY009(),
          HasSubstr(
              "Both Primary and Foreign key table names cannot be empty")));
}
TEST(FetchForeignKeys, failure_empty_table_name_len) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, kPKTable, 0,
      kCatalog, kCatalogLen, kDataset, kDatasetLen, kFKTable, 0);

  EXPECT_THAT(
      status_record_or,
      StatusRecordIs(
          SQLStates::k_HY009(),
          HasSubstr(
              "Both Primary and Foreign key table names cannot be empty")));
}

TEST(FetchForeignKeys, Failure_Null_ConnectionHandle) {
  StatementHandle handle;
  auto status_record_or = FetchForeignKeysFromDataSource(
      handle, kCatalog, kCatalogLen, kDataset, kDatasetLen, kPKTable,
      kPKTableLen, kCatalog, kCatalogLen, kDataset, kDatasetLen, kFKTable,
      kFKTableLen);

  EXPECT_THAT(status_record_or,
              StatusRecordIs(SQLStates::k_HY013(),
                             HasSubstr("Internal connection handle is null")));
}

}  // namespace google::cloud::odbc_bq_driver_internal
