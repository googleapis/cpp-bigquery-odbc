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

#include "google/cloud/odbc/bq_driver/internal/odbc_statement_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;

SQLRETURN StatementHandle::BindColumn(SQLUSMALLINT col_idx,
                                      SQLSMALLINT data_type, SQLPOINTER buf,
                                      SQLLEN buf_len, const SQLLEN* res_len) {
  if (!buf) {
    StatusRecord status_record = {SQLStates::k_HY001(),
                                  "TargetValuePtr should not be null"};
    GetDiagnostics().AddStatusRecord(status_record);
    return SQL_ERROR;
  }

  if (buf_len < 0) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "BufferLength should not be less than zero"};
    GetDiagnostics().AddStatusRecord(status_record);
    return SQL_ERROR;
  }

  if (!res_len) {
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "TargetValueStrLen should not be null"};
    GetDiagnostics().AddStatusRecord(status_record);
    return SQL_ERROR;
  }

  DataBuffer data_buffer = {data_type, buf, buf_len, res_len};
  column_bindings_[col_idx] = data_buffer;
  return SQL_SUCCESS;
}

SQLRETURN StatementHandle::ExecuteTypeInfoQuery(SQLSMALLINT data_type) {
  auto type_info_query = std::make_shared<TypeInfoQuery>();
  query_ = type_info_query;
  type_info_query->Execute(data_type);
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver_internal
