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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_HANDLE_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_query.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_attr.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <map>
#include <memory>

namespace google::cloud::odbc_bq_driver_internal {

class ConnectionHandle;

class StatementHandle : public Handle {
 public:
  // This constructor is used only for tests
  explicit StatementHandle(ConnectionHandle* conn_handle = nullptr)
      : conn_handle_(conn_handle){};
  explicit StatementHandle(ConnectionHandle* conn_handle,
                           Descriptors descriptors)
      : conn_handle_(conn_handle),
        descriptors_(std::move(descriptors)),
        attributes_(kDefaultAttributes){};

  ~StatementHandle() = default;

  StatementHandle(StatementHandle const&) = default;
  StatementHandle& operator=(StatementHandle const&) = default;
  StatementHandle(StatementHandle&&) = default;
  StatementHandle& operator=(StatementHandle&&) = default;

  SQLRETURN BindColumn(SQLUSMALLINT col_idx, SQLSMALLINT data_type,
                       SQLPOINTER buf, SQLLEN buf_len, const SQLLEN* res_len);

  [[nodiscard]] DescriptorHandle& GetDescriptorHandle(
      DescriptorType type) const;
  odbc_internal::StatusRecord SetDescriptorHandle(
      DescriptorType type, DescriptorHandle* descriptor_handle);

  odbc_internal::StatusRecord SetAttribute(int attribute, SQLULEN value);
  odbc_internal::StatusRecordOr<SQLULEN> GetAttribute(int attribute);

  HandleType kType = HandleType::kStmtHandle;

  inline ConnectionHandle* GetConnectionHandle() { return conn_handle_; };

 private:
  std::map<int, DataBuffer> column_bindings_;
  std::shared_ptr<Query> query_;
  Descriptors descriptors_;
  std::map<int, SQLULEN> attributes_;
  ConnectionHandle* conn_handle_{nullptr};
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_HANDLE_H
