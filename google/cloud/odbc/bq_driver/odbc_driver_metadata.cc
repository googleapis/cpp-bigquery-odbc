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
#include "google/cloud/odbc/bq_driver/internal/odbc_procedure_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
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
using google::cloud::odbc_bq_driver_internal::FetchBQProceduresData;
using google::cloud::odbc_bq_driver_internal::FetchBQTablesData;
using google::cloud::odbc_bq_driver_internal::FetchForeignKeysFromDataSource;
using google::cloud::odbc_bq_driver_internal::GetResultSetForDatasets;
using google::cloud::odbc_bq_driver_internal::GetResultSetForProjects;
using google::cloud::odbc_bq_driver_internal::GetResultSetForTables;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc2;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc3;
using google::cloud::odbc_bq_driver_internal::kDriverOdbcVer;
using google::cloud::odbc_bq_driver_internal::kMatchAll;
using google::cloud::odbc_bq_driver_internal::kSqlApiAllFuncsSize;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC2Functions;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC3Functions;
using google::cloud::odbc_bq_driver_internal::ProcessQueryResults;
using google::cloud::odbc_bq_driver_internal::ProcessTableResults;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::SanitizeIdentifierArgument;
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
using google::cloud::odbc_bq_driver_internal::ValidateColumnParameters;
using google::cloud::odbc_bq_driver_internal::ValidateInputParameters;
using google::cloud::odbc_bq_driver_internal::ValidateProcedureColumnParameters;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

TraceOptions& opts = *(*kTraceOption);

// Internal helper functions.
namespace {

StatusRecord InvalidType(char const* mesg, SQLUSMALLINT info_type) {
  std::string message = mesg;
  message.append(std::to_string(info_type));
  TracePrintInternal(opts, message);
  return StatusRecord{SQLStates::k_HY096(), message};
}

SQLRETURN HandleConnectionInformationTypes(
    SQLHDBC connection_handle, SQLUSMALLINT info_type,
    SQLPOINTER info_value_ptr, SQLSMALLINT in_buffer_len,
    SQLSMALLINT* str_len_ptr, bool check_is_connection_done = true) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, check_is_connection_done);
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
    case SQL_DRIVER_ODBC_VER: {
      info_type_value = kDriverOdbcVer;
      break;
    }
    default: {
      auto status_record = InvalidType(
          "HandleConnectionInformationTypes - Invalid infoType: ", info_type);
      return LogAndReturnCode(*handle, status_record);
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
    return LogAndReturnCode(*handle, status_record);
  }

  switch (function_id) {
    case SQL_API_ODBC3_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC3Functions(supported_fn);
      return LogAndReturnCode(*handle, status_record);
    }
    case SQL_API_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC2Functions(supported_fn);
      return LogAndReturnCode(*handle, status_record);
    }
    default:
      break;
  }
  if (IsFunctionIdOdbc3(function_id)) {
    SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
    StatusRecord status_record = PopulateSupportedODBC3Functions(odbc3_fns);
    if (!status_record.ok()) {
      return LogAndReturnCode(*handle, status_record);
    }
    *supported_fn = SQL_FUNC_EXISTS(odbc3_fns, function_id);
  } else if (IsFunctionIdOdbc2(function_id)) {
    SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];
    StatusRecord status_record = PopulateSupportedODBC2Functions(odbc2_fns);
    if (!status_record.ok()) {
      return LogAndReturnCode(*handle, status_record);
    }
    *supported_fn = odbc2_fns[function_id];
  }
  return SQL_SUCCESS;
}

SQLRETURN SQLGetInfoInternal(SQLHDBC connection_handle, SQLUSMALLINT info_type,
                             SQLPOINTER info_value_ptr,
                             SQLSMALLINT in_buffer_len,
                             SQLSMALLINT* str_len_ptr) {
  // for SQL_DRIVER_ODBC_VER should go through even when connection is not
  // established
  StatusRecordOr<ConnectionHandle*> handle_result = ValidateConnectionHandle(
      connection_handle, info_type != SQL_DRIVER_ODBC_VER);

  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }
  ConnectionHandle* handle = *handle_result;

  if (in_buffer_len < 0) {
    std::string mesg = "Invalid Input BufferLength";
    auto status_record = StatusRecord{SQLStates::k_HY090(), mesg};
    return LogAndReturnCode(*handle, status_record);
  }

  // Handle information types dependent on connection handle.
  // SQL_DRIVER_ODBC_VER should be returned when connection is not even made
  if (info_type == SQL_DATA_SOURCE_NAME || info_type == SQL_DATABASE_NAME ||
      info_type == SQL_DRIVER_ODBC_VER) {
    return HandleConnectionInformationTypes(
        connection_handle, info_type, info_value_ptr, in_buffer_len,
        str_len_ptr, info_type != SQL_DRIVER_ODBC_VER);
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
  return LogAndReturnCode(*handle, status_record);
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
    return LogAndReturnCode(handle, ds_status_record_or);
  }
  // Process the DSResults and convert to ResultSet.
  StatusRecordOr<ResultSet> rs_status_record_or =
      ProcessQueryResults(*ds_status_record_or);
  if (!rs_status_record_or) {
    return LogAndReturnCode(handle, rs_status_record_or);
  }

  // Store the resultset in statement handle.
  handle.SetResultSet(*rs_status_record_or);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
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
    return LogAndReturnCode(handle, ds_status_record_or);
  }
  // Process the DSResults and convert to ResultSet.
  StatusRecordOr<ResultSet> rs_status_record_or =
      ProcessQueryResults(*ds_status_record_or);
  if (!rs_status_record_or) {
    return LogAndReturnCode(handle, rs_status_record_or);
  }

  // Store the resultset in statement handle.
  handle.SetResultSet(*rs_status_record_or);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
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
    return LogAndReturnCode(handle, input_param_status);
  }

  std::string project_filter = ToCharStr(catalog_name, kMatchAll);
  std::string dataset_filter = ToCharStr(schema_name, kMatchAll);
  std::string table_filter = ToCharStr(table_name, kMatchAll);
  std::string table_type_filter = ToCharStr(table_type, kMatchAll);

  if (handle.GetConnectionHandle() == nullptr) {
    return LogAndReturnCode(handle,
                            StatusRecord{SQLStates::k_HY013(),
                                         "Internal connection handle is null"});
  }
  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_08S01(),
                             "Connection to the data source is broken"});
  }
  std::shared_ptr<ODBCBQClient> bq_client_ptr = conn_handle.GetClient();
  if (!bq_client_ptr) {
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_HY000(),
                             "Error establishing Datasource connection"});
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

  handle.SetResultSet(*result_set_status);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  return SQL_SUCCESS;
}

SQLRETURN SQLColumnsInternal(SQLHSTMT stmt_handle, SQLCHAR* catalog_name,
                             SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
                             SQLSMALLINT schema_name_len, SQLCHAR* table_name,
                             SQLSMALLINT table_name_len, SQLCHAR* column_name,
                             SQLSMALLINT column_name_len) {
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

  auto input_param_status = ValidateColumnParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, table_name,
      table_name_len, column_name, column_name_len, metadata_id);
  if (!input_param_status.ok()) {
    return LogAndReturnCode(handle, input_param_status);
  }
  // catalog_name cannot be search pattern.
  std::string s_catalog_name = ToCharStr(catalog_name);
  // Rest of the arguments can have search pattern characters.
  std::string s_dataset_name = ToCharStr(schema_name, kMatchAll);
  std::string s_table_name = ToCharStr(table_name, kMatchAll);
  std::string s_column_name = ToCharStr(column_name, kMatchAll);

  // For metadata_id == SQL_TRUE, all parameters are ID Arguments.
  // Sanitize the ID arguments before fetching data from BQ.
  // For sanitization rules see:
  // https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/identifier-arguments?view=sql-server-ver16
  if (metadata_id == SQL_TRUE) {
    // We don't sanitize catalog name to uppercase because BigQquery doesn't
    // allow it.
    SanitizeIdentifierArgument(s_dataset_name);
    SanitizeIdentifierArgument(s_table_name);
    SanitizeIdentifierArgument(s_column_name);
  }

  if (handle.GetConnectionHandle() == nullptr) {
    return LogAndReturnCode(handle,
                            StatusRecord{SQLStates::k_HY013(),
                                         "Internal connection handle is null"});
  }
  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_08S01(),
                             "Connection to the data source is broken"});
  }

  // Fetch BQ Table. This particular call fetches a single table.
  auto filtered_tables_data_status = FetchBQTablesData(
      conn_handle, s_catalog_name, s_dataset_name, s_table_name, metadata_id);
  if (!filtered_tables_data_status) {
    return LogAndReturnCode(handle, filtered_tables_data_status);
  }

  // Process Table Results for each table returned from the list above.
  ResultSet final_result_set;
  for (auto const& bq_table : *filtered_tables_data_status) {
    StatusRecordOr<ResultSet> table_result_set_status =
        ProcessTableResults(bq_table, s_column_name, metadata_id);

    if (!table_result_set_status) {
      return LogAndReturnCode(handle, table_result_set_status);
    }
    if (!table_result_set_status->rows.empty()) {
      // Set the row schema
      final_result_set.row_schema = table_result_set_status->row_schema;
      // Append the result set rows to the final results.
      final_result_set.rows.insert(final_result_set.rows.end(),
                                   table_result_set_status->rows.begin(),
                                   table_result_set_status->rows.end());
    }
  }

  if (!final_result_set.rows.empty()) {
    handle.SetResultSet(final_result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }

  return SQL_SUCCESS;
}

SQLRETURN SQLProcedureColumnsInternal(
    SQLHSTMT stmt_handle, SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    SQLCHAR* schema_name, SQLSMALLINT schema_name_len, SQLCHAR* proc_name,
    SQLSMALLINT proc_name_len, SQLCHAR* column_name,
    SQLSMALLINT column_name_len) {
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

  if (handle.GetConnectionHandle() == nullptr) {
    return LogAndReturnCode(handle,
                            StatusRecord{SQLStates::k_HY013(),
                                         "Internal connection handle is null"});
  }

  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_08S01(),
                             "Connection to the data source is broken"});
  }

  SQLINTEGER catalog_len = 0;
  SQLCHAR current_catalog[256] = {0};
  conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, current_catalog,
                           sizeof(current_catalog), &catalog_len);

  if (catalog_name == nullptr) {
    catalog_name = current_catalog;
    catalog_name_len = catalog_len;
  }

  auto input_param_status = ValidateProcedureColumnParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, proc_name,
      proc_name_len, column_name, column_name_len, metadata_id);
  if (!input_param_status.ok()) {
    return LogAndReturnCode(handle, input_param_status);
  }

  // Convert input parameters to strings
  std::string s_catalog_name = ToCharStr(catalog_name);
  std::string s_dataset_name = ToCharStr(schema_name, kMatchAll);
  std::string s_proc_name = ToCharStr(proc_name, kMatchAll);
  std::string s_column_name = ToCharStr(column_name, kMatchAll);

  if (metadata_id == SQL_TRUE) {
    SanitizeIdentifierArgument(s_dataset_name);
    SanitizeIdentifierArgument(s_proc_name);
    SanitizeIdentifierArgument(s_column_name);
  }

  auto filtered_procedure_data_status = FetchBQProceduresData(
      conn_handle, s_catalog_name, s_dataset_name, s_proc_name, metadata_id);

  ResultSet final_result_set;
  if (!filtered_procedure_data_status) {
    handle.SetResultSet(final_result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
    return SQL_SUCCESS;
  }

  for (auto const& bq_proc : *filtered_procedure_data_status) {
    StatusRecordOr<ResultSet> procedure_result_set_status =
        ProcessProcedureResults(bq_proc, s_column_name, metadata_id);

    if (!procedure_result_set_status) {
      return LogAndReturnCode(handle, procedure_result_set_status);
    }

    if (!procedure_result_set_status->rows.empty()) {
      final_result_set.row_schema = procedure_result_set_status->row_schema;
      final_result_set.rows.insert(final_result_set.rows.end(),
                                   procedure_result_set_status->rows.begin(),
                                   procedure_result_set_status->rows.end());
    }
  }
  handle.SetResultSet(final_result_set);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
