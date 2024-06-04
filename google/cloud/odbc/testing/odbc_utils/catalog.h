
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
