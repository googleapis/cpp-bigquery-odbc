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
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;
using ::google::cloud::odbc_bq_driver_internal::SupportedInfoType;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using ::google::cloud::odbc_bq_driver_internal::UnSupportedInfoType;

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
  StatusOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle);
  if (!handle_result.ok()) {
    TracePrintInternal(
        opts, "Invalid Connection handle: " + handle_result.status().message());
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField.
    return SQL_INVALID_HANDLE;
  }

  auto* handle = *handle_result;

  SQLGetInfoSqlChar info_val_char;
  switch (info_type) {
    case SQL_DATA_SOURCE_NAME: {
      SQLCHAR* dsn_name = reinterpret_cast<SQLCHAR*>(
          const_cast<char*>(handle->GetDsn().dsn_name.c_str()));
      info_val_char.info_val = dsn_name;
      return info_val_char.InfoValToResponse(info_value_ptr, in_buffer_len,
                                             str_len_ptr);
    }
    case SQL_DATABASE_NAME: {
      SQLCHAR* database_name = reinterpret_cast<SQLCHAR*>(
          const_cast<char*>(handle->GetDsn().catalog.c_str()));
      info_val_char.info_val = database_name;
      return info_val_char.InfoValToResponse(info_value_ptr, in_buffer_len,
                                             str_len_ptr);
    }
  }

  return InvalidType("HandleConnectionInformationTypes - Invalid infoType: ",
                     info_type);
}

}  // namespace

SQLRETURN SQLGetFunctionsInternal(ConnectionHandle*  /*connection_handle*/,
                                  SQLUSMALLINT function_id,
                                  SQLUSMALLINT* supported_fn) {
  SQLRETURN rc = SQL_SUCCESS;
  // Assumption here is memory for output is managed/owned by the caller.
  if (!supported_fn) {
    TracePrintInternal(opts, "Argument supported_fn cannot be null");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField.
    return SQL_ERROR;
  }
  switch (function_id) {
    case SQL_API_ODBC3_ALL_FUNCTIONS: {
      Status status = PopulateSupportedODBC3Functions(opts, supported_fn);
      if (!status.ok()) {
        TracePrintInternal(opts,
                           "Internal Error: PopulateSupportedODBCFunctions() "
                           "failed with status: " +
                               status.message());
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField.
        return SQL_ERROR;
      }
      return rc;
    }
    case SQL_API_ALL_FUNCTIONS: {
      Status status = PopulateSupportedODBC2Functions(opts, supported_fn);
      if (!status.ok()) {
        TracePrintInternal(opts,
                           "Internal Error: PopulateSupportedODBCFunctions() "
                           "failed with status: " +
                               status.message());
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField.
        return SQL_ERROR;
      }
      return rc;
    }
    default:
      break;
  }
  if (IsFunctionIdOdbc3(function_id)) {
    SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
    Status status = PopulateSupportedODBC3Functions(opts, odbc3_fns);
    if (!status.ok()) {
      TracePrintInternal(opts,
                         "Internal Error: PopulateSupportedODBCFunctions() "
                         "failed with status: " +
                             status.message());
      // TODO(b/308656768,b/308656826): Record error or diagnostic info for
      // SQLDiagRec and/or SQLDiagField.
      return SQL_ERROR;
    }
    *supported_fn = SQL_FUNC_EXISTS(odbc3_fns, function_id);
  } else if (IsFunctionIdOdbc2(function_id)) {
    SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];
    Status status = PopulateSupportedODBC2Functions(opts, odbc2_fns);
    if (!status.ok()) {
      TracePrintInternal(opts,
                         "Internal Error: PopulateSupportedODBCFunctions() "
                         "failed with status: " +
                             status.message());
      // TODO(b/308656768,b/308656826): Record error or diagnostic info for
      // SQLDiagRec and/or SQLDiagField.
      return SQL_ERROR;
    }
    *supported_fn = odbc2_fns[function_id];
  }
  return rc;
}

SQLRETURN SQLGetInfoInternal(ConnectionHandle* connection_handle,
                             SQLUSMALLINT info_type, SQLPOINTER info_value_ptr,
                             SQLSMALLINT in_buffer_len,
                             SQLSMALLINT* str_len_ptr) {
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

SQLRETURN SQLGetTypeInfoInternal(StatementHandle* statement_handle,
                                 SQLSMALLINT dataType) {
  return SQL_SUCCESS;
}

// NOLINTEND(misc-unused-parameters)

}  // namespace google::cloud::odbc_bq_driver
