
// Copyright 2023 Google LLC
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_CATALOG_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_CATALOG_H

#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/internal/getenv.h"

namespace google::cloud::odbc_tests {

std::string const kCatalogName = "bigquery-devtools-drivers";
// Dataset for catalogn functions.
std::string const kCatalogFnsDataset = "ODBC_TEST_DATASET_CATALOG_FNS";
// Tables for SQLPrimaryKeys.
std::string const kCatalogDatasetTableWithPK =
    "ODBC_SQLPrimaryKeys_TABLE_WITH_PK";
std::string const kCatalogDatasetTableWithoutPK =
    "ODBC_SQLPrimaryKeys_TABLE_WITHOUT_PK";
// Tables for SQLForeignKeys.
std::string const kTableOrders = "ODBC_SQLForeignKeys_TABLE_ORDERS";
std::string const kTableLines = "ODBC_SQLForeignKeys_TABLE_LINES";
std::string const kTableCustomer = "ODBC_SQLForeignKeys_TABLE_CUSTOMER";
class Catalog {
 public:
  ~Catalog();

  SQLSMALLINT target_type;
  SQLPOINTER target_value;
  SQLINTEGER buffer_length;
  SQLLEN str_len;

  // Uses the SQLTables API to fetch tables in a dataset.
  static std::shared_ptr<Results> GetTables(std::shared_ptr<ODBCHandles> conn,
                                            std::string dataset = "",
                                            bool use_ansi = false);

  // Uses the SQLPrimaryKeys API to fetch primary keys in a dataset.
  static RowWiseResults GetPrimaryKeys(std::shared_ptr<ODBCHandles> conn,
                                       std::string dataset = "",
                                       std::string table = "",
                                       bool use_ansi = false);

  // Uses the SQLForeignKeys API to fetch foreign keys in a dataset.
  static RowWiseResults GetForeignKeys(std::shared_ptr<ODBCHandles> conn,
                                       std::string dataset = "",
                                       std::string pk_table = "",
                                       std::string fk_table = "",
                                       bool use_ansi = false);
};

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_CATALOG_H
