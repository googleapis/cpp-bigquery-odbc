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

#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using ::google::cloud::odbc_bq_driver_internal::DescriptorType;
using ::google::cloud::odbc_bq_driver_internal::IsDateCType;
using ::google::cloud::odbc_bq_driver_internal::IsIntervalCType;
using ::google::cloud::odbc_bq_driver_internal::kToDateTimeIntervalCode;
using ::google::cloud::odbc_bq_driver_internal::kTraceOption;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

SQLRETURN SQLBindColInternal(SQLHSTMT statement_handle,
                             SQLUSMALLINT column_number,
                             SQLSMALLINT target_c_type, SQLPOINTER target_value,
                             SQLLEN target_value_buffer_len,
                             SQLLEN* target_value_str_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(statement_handle);
  if (!handle_result) {
    TracePrintInternal(**kTraceOption, handle_result.GetStatusRecord().message);
    std::cout << "CP 3:: " << std::endl;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle* handle = *handle_result;
  handle->GetDiagnostics().ClearDiagnostics();

  // ----- Validations ---------

  if (column_number < 0) {
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "ColumnNumber should not < 0"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    std::cout << "CP 4:: " << std::endl;
    return SQL_ERROR;
  }

  StatusRecordOr<SQLULEN> use_bookmarks_status =
      handle->GetAttribute(SQL_ATTR_USE_BOOKMARKS);
  if (!use_bookmarks_status) {
    handle->GetDiagnostics().AddStatusRecord(
        use_bookmarks_status.GetStatusRecord());
    std::cout << "CP 5:: " << std::endl;
    return use_bookmarks_status.GetCalculatedReturnCode();
  }
  if (*use_bookmarks_status == SQL_UB_OFF && column_number == 0) {
    StatusRecord status_record = {SQLStates::k_07006(),
                                  "ColumnNumber should not < 0"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    std::cout << "CP 6:: " << std::endl;
    return SQL_ERROR;
  }

  if (target_value_buffer_len < 0) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "BufferLength should not < 0"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    std::cout << "CP 7:: " << std::endl;
    return SQL_ERROR;
  }

  std::cout << "CP 8:: " << std::endl;

  // ----- Set appropriate SQL_DESC_COUNT for new binding ---------

  DescriptorHandle& ard = handle->GetDescriptorHandle(DescriptorType::kARD);
  // Unbinding column
  if (target_value == nullptr) {
    // Here we don't care about the status record returned by
    // UnbindDescriptorRecord because SQL_SUCCESS should be returned in case of
    // unbound column number
    // too.(https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlbindcol-function?view=sql-server-ver16#unbinding-columns)
    ard.UnbindDescriptorRecord(column_number);
    return SQL_SUCCESS;
  }

  SQLSMALLINT desc_count = 0;
  SQLRETURN status =
      GetDescField(&ard, 0, SQL_DESC_COUNT, &desc_count, 0, nullptr);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "CP 9:: " << std::endl;
    return status;
  }

  if (column_number > desc_count) {
    desc_count = column_number;
    status = SetDescField(&ard, 0, SQL_DESC_COUNT,
                          reinterpret_cast<SQLPOINTER>(desc_count), 0);
    if (!SQL_SUCCEEDED(status)) {
      std::cout << "CP 10:: " << std::endl;
      return status;
    }
  }
  SQLSMALLINT desc_type;
  if (IsDateCType(target_c_type)) {
    std::cout << "CP 11:: " << std::endl;
    SQLSMALLINT subcode = kToDateTimeIntervalCode.at(target_c_type);
    status = SetDescField(&ard, column_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                          reinterpret_cast<SQLPOINTER>(subcode), 0);
    if (!SQL_SUCCEEDED(status)) {
      std::cout << "CP 12:: " << std::endl;
      return status;
    }

    desc_type = SQL_DATETIME;
    status = SetDescField(&ard, column_number, SQL_DESC_TYPE,
                          reinterpret_cast<SQLPOINTER>(desc_type), 0);
    if (!SQL_SUCCEEDED(status)) {
      std::cout << "CP 12.1:: " << std::endl;
      return status;
    }
  } else if (IsIntervalCType(target_c_type)) {
    std::cout << "CP 13.0:: " << std::endl;
    SQLSMALLINT subcode = kToDateTimeIntervalCode.at(target_c_type);
    status = SetDescField(&ard, column_number, SQL_DESC_DATETIME_INTERVAL_CODE,
                          reinterpret_cast<SQLPOINTER>(subcode), 0);
    if (!SQL_SUCCEEDED(status)) {
      std::cout << "CP 14:: " << std::endl;
      return status;
    }

    std::cout << "CP 13.1:: " << subcode << std::endl;
    desc_type = SQL_INTERVAL;
    status = SetDescField(&ard, column_number, SQL_DESC_TYPE,
                          reinterpret_cast<SQLPOINTER>(desc_type), 0);
    if (!SQL_SUCCEEDED(status)) {
      std::cout << "CP 13.1:: " << std::endl;
      return status;
    }
  } else {
    desc_type = target_c_type;
    status = SetDescField(&ard, column_number, SQL_DESC_TYPE,
                          reinterpret_cast<SQLPOINTER>(desc_type), 0);
    if (!SQL_SUCCEEDED(status)) {
      std::cout << "CP 15:: " << std::endl;
      return status;
    }
  }

  status = SetDescField(&ard, column_number, SQL_DESC_CONCISE_TYPE,
                        reinterpret_cast<SQLPOINTER>(desc_type), 0);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "CP 16:: " << std::endl;
    return status;
  }

  status =
      SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH,
                   reinterpret_cast<SQLPOINTER>(target_value_buffer_len), 0);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "CP 17:: " << std::endl;
    return status;
  }

  status = SetDescField(&ard, column_number, SQL_DESC_DATA_PTR,
                        reinterpret_cast<SQLPOINTER>(target_value), 0);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "CP 18:: " << std::endl;
    return status;
  }

  status = SetDescField(&ard, column_number, SQL_DESC_INDICATOR_PTR,
                        reinterpret_cast<SQLPOINTER>(target_value_str_len), 0);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "CP 19:: " << std::endl;
    return status;
  }

  status = SetDescField(&ard, column_number, SQL_DESC_OCTET_LENGTH_PTR,
                        reinterpret_cast<SQLPOINTER>(target_value_str_len), 0);
  if (!SQL_SUCCEEDED(status)) {
    std::cout << "CP 20:: " << std::endl;
    return status;
  }
  return SQL_SUCCESS;
}

// NOLINTBEGIN(misc-unused-parameters)

SQLRETURN SQLFetchInternal(SQLHSTMT statement_handle) { return SQL_SUCCESS; }

// NOLINTEND(misc-unused-parameters)

}  // namespace google::cloud::odbc_bq_driver
