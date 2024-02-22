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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STATEMENT_HANDLE_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STATEMENT_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_query.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <memory>

namespace google::cloud::odbc_bq_driver_internal {

class StatementHandle : public Handle {
 public:
  explicit StatementHandle() = default;
  ~StatementHandle() = default;

  StatementHandle(StatementHandle const&) = default;
  StatementHandle& operator=(StatementHandle const&) = default;
  StatementHandle(StatementHandle&&) = default;
  StatementHandle& operator=(StatementHandle&&) = default;

  SQLRETURN GetAttribute(SQLINTEGER attribute, void* value, void* length);

  SQLRETURN SetAttribute(SQLINTEGER attribute, void* value, void* length);

  SQLRETURN BindColumn(SQLUSMALLINT col_idx, SQLSMALLINT data_type,
                       SQLPOINTER buf, SQLLEN buf_len, SQLLEN* res_len);

 private:
  std::map<int, DataBuffer> column_bindings_;
  std::shared_ptr<Query> query_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STATEMENT_HANDLE_H
