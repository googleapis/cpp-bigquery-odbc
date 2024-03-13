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

#include "google/cloud/odbc/bq_driver/odbc_driver_metadata.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_info.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc2;
using ::google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc3;
using ::google::cloud::odbc_bq_driver_internal::kSqlApiAllFuncsSize;
using ::google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using ::google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC2Functions;
using ::google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC3Functions;
using ::google::cloud::odbc_bq_driver_internal::SQLGetInfoBitmask;
using ::google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlChar;
using ::google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlUInt;
using ::google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlUSmallInt;
using ::google::cloud::odbc_bq_driver_internal::SupportedInfoType;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using ::google::cloud::odbc_bq_driver_internal::UnSupportedInfoType;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

TraceOptions& opts = *(*kTraceOptsConsole);

// Internal helper functions.
namespace {

SQLRETURN InvalidType(char const* mesg, SQLUSMALLINT info_type) {
  std::string message = mesg;
  message.append(std::to_string(info_type));
  TracePrintInternal(opts, message);
  return SQL_ERROR;
}

SQLRETURN HandleConnectionInformationTypes(SQLHDBC connection_handle,
                                           SQLUSMALLINT info_type,
                                           SQLPOINTER info_value_ptr,
                                           SQLSMALLINT in_buffer_len,
                                           SQLSMALLINT* str_len_ptr) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  auto* handle = *handle_result;

  SQLGetInfoSqlChar info_val_char;
  std::string info_type_value;
  switch (info_type) {
    case SQL_DATA_SOURCE_NAME: {
      info_type_value = handle->GetDsn().dsn_name;
      break;
    }
    case SQL_DATABASE_NAME: {
      info_type_value = handle->GetDsn().catalog;
      break;
    }
    default: {
      return InvalidType(
          "HandleConnectionInformationTypes - Invalid infoType: ", info_type);
    }
  }

  auto len = info_type_value.length();
  if (info_val_char.info_val == nullptr) {
    info_val_char.info_val = new SQLCHAR[len + 1];
  }
  strcpy(reinterpret_cast<char*>(info_val_char.info_val),
         info_type_value.c_str());

  SQLRETURN rc = info_val_char.InfoValToResponse(info_value_ptr, in_buffer_len,
                                                 str_len_ptr);
  delete[] info_val_char.info_val;
  return rc;
}

}  // namespace

SQLRETURN SQLGetFunctionsInternal(SQLHDBC connection_handle,
                                  SQLUSMALLINT function_id,
                                  SQLUSMALLINT* supported_fn) {
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle);
  if (!handle_result) {
    return handle_result.GetCalculatedReturnCode();
  }

  if (!supported_fn) {
    auto status_record = StatusRecord{SQLStates::k_HY024(),
                                      "Argument supported_fn cannot be null"};
    return status_record.CalculateReturnCode();
  }

  switch (function_id) {
    case SQL_API_ODBC3_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC3Functions(supported_fn);
      if (!status_record.ok()) {
        return status_record.CalculateReturnCode();
      }
      return rc;
    }
    case SQL_API_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC2Functions(supported_fn);
      if (!status_record.ok()) {
        return status_record.CalculateReturnCode();
      }
      return rc;
    }
    default:
      break;
  }
  if (IsFunctionIdOdbc3(function_id)) {
    SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
    StatusRecord status_record = PopulateSupportedODBC3Functions(odbc3_fns);
    if (!status_record.ok()) {
      return status_record.CalculateReturnCode();
    }
    *supported_fn = SQL_FUNC_EXISTS(odbc3_fns, function_id);
  } else if (IsFunctionIdOdbc2(function_id)) {
    SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];
    StatusRecord status_record = PopulateSupportedODBC2Functions(odbc2_fns);
    if (!status_record.ok()) {
      return status_record.CalculateReturnCode();
    }
    *supported_fn = odbc2_fns[function_id];
  }
  return rc;
}

SQLRETURN SQLGetInfoInternal(SQLHDBC connection_handle, SQLUSMALLINT info_type,
                             SQLPOINTER info_value_ptr,
                             SQLSMALLINT in_buffer_len,
                             SQLSMALLINT* str_len_ptr) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  if (!info_value_ptr) {
    TracePrintInternal(opts, "Invalid InfoValuePtr");
    // TODO(#158): SQLGetDiagRec should handle this
    return SQL_ERROR;
  }
  // Handle information types dependent on connection handle.
  if (info_type == SQL_DATA_SOURCE_NAME || info_type == SQL_DATABASE_NAME) {
    return HandleConnectionInformationTypes(connection_handle, info_type,
                                            info_value_ptr, in_buffer_len,
                                            str_len_ptr);
  }
  // Handle rest of the information types not dependent on the connection
  // handle.
  if (auto r = SupportedInfoType<SQLGetInfoSqlChar>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, in_buffer_len, str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoSqlChar>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, in_buffer_len, str_len_ptr);
  }
  if (auto r = SupportedInfoType<SQLGetInfoSqlUInt>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoSqlUInt>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = SupportedInfoType<SQLGetInfoSqlUSmallInt>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoSqlUSmallInt>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = SupportedInfoType<SQLGetInfoBitmask>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoBitmask>(info_type); r.ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }

  return InvalidType("SQLGetInfoInternal - Invalid infoType: ", info_type);
}

// NOLINTBEGIN(misc-unused-parameters)

SQLRETURN SQLGetTypeInfoInternal(SQLHSTMT statementHandle,
                                 SQLSMALLINT dataType) {
  return SQL_SUCCESS;
}

// NOLINTEND(misc-unused-parameters)

}  // namespace google::cloud::odbc_bq_driver
