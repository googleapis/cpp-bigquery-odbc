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
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_foreign_keys.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_info.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_primary_keys.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_tables.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::CreateResultSetForTableTypes;
using google::cloud::odbc_bq_driver_internal::DSResults;
using google::cloud::odbc_bq_driver_internal::FetchForeignKeysFromDataSource;
using google::cloud::odbc_bq_driver_internal::FetchPrimaryKeysFromDataSource;
using google::cloud::odbc_bq_driver_internal::GetResultSetForDatasets;
using google::cloud::odbc_bq_driver_internal::GetResultSetForProjects;
using google::cloud::odbc_bq_driver_internal::GetResultSetForTables;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc2;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc3;
using google::cloud::odbc_bq_driver_internal::kMatchAll;
using google::cloud::odbc_bq_driver_internal::kSqlApiAllFuncsSize;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC2Functions;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC3Functions;
using google::cloud::odbc_bq_driver_internal::ProcessQueryResults;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoBitmask;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlChar;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlUInt;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlUSmallInt;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::SupportedInfoType;
using google::cloud::odbc_bq_driver_internal::TraceOptions;
using google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using google::cloud::odbc_bq_driver_internal::UnSupportedInfoType;
using google::cloud::odbc_bq_driver_internal::ValidateInputParameters;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

TraceOptions& opts = *(*kTraceOption);

// Internal helper functions.
namespace {

template <typename T>
SQLRETURN LogAndReturnCode(StatementHandle& handle,
                           StatusRecordOr<T> status_record_or) {
  auto status_record = status_record_or.GetStatusRecord();
  handle.GetDiagnostics().AddStatusRecord(status_record);
  TracePrintInternal(opts, status_record.message);
  return status_record_or.GetCalculatedReturnCode();
}

StatusRecord InvalidType(char const* mesg, SQLUSMALLINT info_type) {
  std::string message = mesg;
  message.append(std::to_string(info_type));
  TracePrintInternal(opts, message);
  return StatusRecord{SQLStates::k_HY096(), message};
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
      auto status_record = InvalidType(
          "HandleConnectionInformationTypes - Invalid infoType: ", info_type);
      handle->GetDiagnostics().AddStatusRecord(status_record);
      return status_record.CalculateReturnCode();
    }
  }

  auto len = info_type_value.length();
  if (info_val_char.info_val == nullptr) {
    info_val_char.info_val = new SQLCHAR[len + 1];
  }
  strcpy(reinterpret_cast<char*>(info_val_char.info_val),
         info_type_value.c_str());

  SQLRETURN rc = info_val_char.InfoValToResponse(handle, info_value_ptr,
                                                 in_buffer_len, str_len_ptr);
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
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  ConnectionHandle* handle = *handle_result;
  if (!supported_fn) {
    auto status_record = StatusRecord{SQLStates::k_HY024(),
                                      "Argument supported_fn cannot be null"};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    TracePrintInternal(opts, status_record.message);
    return status_record.CalculateReturnCode();
  }

  switch (function_id) {
    case SQL_API_ODBC3_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC3Functions(supported_fn);
      if (!status_record.ok()) {
        handle->GetDiagnostics().AddStatusRecord(status_record);
        TracePrintInternal(opts, status_record.message);
        return status_record.CalculateReturnCode();
      }
      return rc;
    }
    case SQL_API_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC2Functions(supported_fn);
      if (!status_record.ok()) {
        handle->GetDiagnostics().AddStatusRecord(status_record);
        TracePrintInternal(opts, status_record.message);
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
      handle->GetDiagnostics().AddStatusRecord(status_record);
      TracePrintInternal(opts, status_record.message);
      return status_record.CalculateReturnCode();
    }
    *supported_fn = SQL_FUNC_EXISTS(odbc3_fns, function_id);
  } else if (IsFunctionIdOdbc2(function_id)) {
    SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];
    StatusRecord status_record = PopulateSupportedODBC2Functions(odbc2_fns);
    if (!status_record.ok()) {
      handle->GetDiagnostics().AddStatusRecord(status_record);
      TracePrintInternal(opts, status_record.message);
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
  ConnectionHandle* handle = *handle_result;

  if (in_buffer_len < 0) {
    std::string mesg = "Invalid Input BufferLength";
    auto status_record = StatusRecord{SQLStates::k_HY090(), mesg};
    handle->GetDiagnostics().AddStatusRecord(status_record);
    TracePrintInternal(opts, status_record.message);
    return status_record.CalculateReturnCode();
  }

  // Handle information types dependent on connection handle.
  if (info_type == SQL_DATA_SOURCE_NAME || info_type == SQL_DATABASE_NAME) {
    return HandleConnectionInformationTypes(connection_handle, info_type,
                                            info_value_ptr, in_buffer_len,
                                            str_len_ptr);
  }
  // Handle rest of the information types not dependent on the connection
  // handle.
  if (auto r = SupportedInfoType<SQLGetInfoSqlChar>(info_type); r.Ok()) {
    return r->InfoValToResponse(handle, info_value_ptr, in_buffer_len,
                                str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoSqlChar>(info_type); r.Ok()) {
    return r->InfoValToResponse(handle, info_value_ptr, in_buffer_len,
                                str_len_ptr);
  }
  if (auto r = SupportedInfoType<SQLGetInfoSqlUInt>(info_type); r.Ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoSqlUInt>(info_type); r.Ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = SupportedInfoType<SQLGetInfoSqlUSmallInt>(info_type); r.Ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoSqlUSmallInt>(info_type); r.Ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = SupportedInfoType<SQLGetInfoBitmask>(info_type); r.Ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }
  if (auto r = UnSupportedInfoType<SQLGetInfoBitmask>(info_type); r.Ok()) {
    return r->InfoValToResponse(info_value_ptr, str_len_ptr);
  }

  auto status_record =
      InvalidType("SQLGetInfoInternal - Invalid infoType: ", info_type);
  handle->GetDiagnostics().AddStatusRecord(status_record);
  return status_record.CalculateReturnCode();
}

SQLRETURN SQLPrimaryKeysInternal(SQLHSTMT stmt_handle,
                                 SQLCHAR const* catalog_name,
                                 SQLSMALLINT catalog_name_len,
                                 SQLCHAR const* schema_name,
                                 SQLSMALLINT schema_name_len,
                                 SQLCHAR const* table_name,
                                 SQLSMALLINT table_name_len) {
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  StatementHandle& handle = *(*handle_result);

  // First fetch the primary keys from data source.
  StatusRecordOr<DSResults> ds_status_record_or =
      FetchPrimaryKeysFromDataSource(handle, ToCharStr(catalog_name),
                                     catalog_name_len, ToCharStr(schema_name),
                                     schema_name_len, ToCharStr(table_name),
                                     table_name_len);
  if (!ds_status_record_or) {
    auto status_record = ds_status_record_or.GetStatusRecord();
    TracePrintInternal(opts, status_record.message);
    handle.GetDiagnostics().AddStatusRecord(status_record);
    return ds_status_record_or.GetCalculatedReturnCode();
  }
  // Process the DSResults and convert to ResultSet.
  StatusRecordOr<ResultSet> rs_status_record_or =
      ProcessQueryResults(*ds_status_record_or);
  if (!rs_status_record_or) {
    auto status_record = rs_status_record_or.GetStatusRecord();
    TracePrintInternal(opts, status_record.message);
    handle.GetDiagnostics().AddStatusRecord(status_record);
    return rs_status_record_or.GetCalculatedReturnCode();
  }

  if (!rs_status_record_or->rows.empty()) {
    // Store the resultset in statement handle.
    handle.SetResultSet(*rs_status_record_or);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }
  return rc;
}

SQLRETURN SQLForeignKeysInternal(
    SQLHSTMT stmt_handle, SQLCHAR const* pk_catalog_name,
    SQLSMALLINT pk_catalog_name_len, SQLCHAR const* pk_schema_name,
    SQLSMALLINT pk_schema_name_len, SQLCHAR const* pk_table_name,
    SQLSMALLINT pk_table_name_len, SQLCHAR const* fk_catalog_name,
    SQLSMALLINT fk_catalog_name_len, SQLCHAR const* fk_schema_name,
    SQLSMALLINT fk_schema_name_len, SQLCHAR const* fk_table_name,
    SQLSMALLINT fk_table_name_len) {
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  // First fetch the foreign keys from data source.
  StatusRecordOr<DSResults> ds_status_record_or =
      FetchForeignKeysFromDataSource(
          handle, ToCharStr(pk_catalog_name), pk_catalog_name_len,
          ToCharStr(pk_schema_name), pk_schema_name_len,
          ToCharStr(pk_table_name), pk_table_name_len,
          ToCharStr(fk_catalog_name), fk_catalog_name_len,
          ToCharStr(fk_schema_name), fk_schema_name_len,
          ToCharStr(fk_table_name), fk_table_name_len);
  if (!ds_status_record_or) {
    auto status_record = ds_status_record_or.GetStatusRecord();
    TracePrintInternal(opts, status_record.message);
    handle.GetDiagnostics().AddStatusRecord(status_record);
    return ds_status_record_or.GetCalculatedReturnCode();
  }
  // Process the DSResults and convert to ResultSet.
  StatusRecordOr<ResultSet> rs_status_record_or =
      ProcessQueryResults(*ds_status_record_or);
  if (!rs_status_record_or) {
    auto status_record = rs_status_record_or.GetStatusRecord();
    TracePrintInternal(opts, status_record.message);
    handle.GetDiagnostics().AddStatusRecord(status_record);
    return rs_status_record_or.GetCalculatedReturnCode();
  }

  if (!rs_status_record_or->rows.empty()) {
    // Store the resultset in statement handle.
    handle.SetResultSet(*rs_status_record_or);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }
  return rc;
}

SQLRETURN SQLTablesInternal(SQLHSTMT stmt_handle, SQLCHAR* catalog_name,
                            SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
                            SQLSMALLINT schema_name_len, SQLCHAR* table_name,
                            SQLSMALLINT table_name_len, SQLCHAR* table_type,
                            SQLSMALLINT table_type_len) {
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  StatusRecordOr<SQLULEN> attr_status =
      handle.GetAttribute(SQL_ATTR_METADATA_ID);
  if (!attr_status) {
    return LogAndReturnCode(handle, attr_status);
  }
  SQLULEN metadata_id = *attr_status;

  auto input_param_status = ValidateInputParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, table_name,
      table_name_len, table_type_len, metadata_id);
  if (!input_param_status.ok()) {
    handle.GetDiagnostics().AddStatusRecord(input_param_status);
    TracePrintInternal(opts, input_param_status.message);
    return input_param_status.CalculateReturnCode();
  }

  std::string project_filter = ToCharStr(catalog_name, kMatchAll);
  std::string dataset_filter = ToCharStr(schema_name, kMatchAll);
  std::string table_filter = ToCharStr(table_name, kMatchAll);
  std::string table_type_filter = ToCharStr(table_type, kMatchAll);

  if (handle.GetConnectionHandle() == nullptr) {
    auto status_record = StatusRecord{SQLStates::k_HY013(),
                                      "Internal connection handle is null"};
    handle.GetDiagnostics().AddStatusRecord(status_record);
    TracePrintInternal(opts, status_record.message);
    return status_record.CalculateReturnCode();
  }
  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    auto status_record = StatusRecord{
        SQLStates::k_08S01(), "Connection to the data source is broken"};
    handle.GetDiagnostics().AddStatusRecord(status_record);
    TracePrintInternal(opts, status_record.message);
    return status_record.CalculateReturnCode();
  }
  std::shared_ptr<ODBCBQClient> bq_client_ptr = conn_handle.GetClient();
  if (!bq_client_ptr) {
    auto status_record = StatusRecord{
        SQLStates::k_HY000(), "Error establishing Datasource connection"};
    handle.GetDiagnostics().AddStatusRecord(status_record);
    TracePrintInternal(opts, status_record.message);
    return status_record.CalculateReturnCode();
  }
  ODBCBQClient& bq_client = *bq_client_ptr;
  StatusRecordOr<ResultSet> result_set_status;

  if (!metadata_id && project_filter == SQL_ALL_CATALOGS &&
      dataset_filter.empty() && table_filter.empty()) {
    result_set_status = GetResultSetForProjects(bq_client, metadata_id);
  } else if (!metadata_id && project_filter.empty() &&
             dataset_filter == SQL_ALL_SCHEMAS && table_filter.empty()) {
    result_set_status = GetResultSetForDatasets(bq_client, metadata_id);
  } else if (!metadata_id && project_filter.empty() && dataset_filter.empty() &&
             table_filter.empty() && table_type_filter == SQL_ALL_TABLE_TYPES) {
    result_set_status = CreateResultSetForTableTypes();
  } else {
    result_set_status = GetResultSetForTables(
        conn_handle, bq_client, project_filter, dataset_filter, table_filter,
        table_type_filter, metadata_id);
  }
  if (!result_set_status) {
    return LogAndReturnCode(handle, result_set_status);
  }

  if (!result_set_status->rows.empty()) {
    handle.SetResultSet(*result_set_status);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
