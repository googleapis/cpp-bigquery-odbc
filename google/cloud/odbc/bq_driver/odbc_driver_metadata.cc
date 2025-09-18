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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::CreateResultSetForTableTypes;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::DSResults;
using google::cloud::odbc_bq_driver_internal::FetchBQSQLProceduresData;
using google::cloud::odbc_bq_driver_internal::FetchBQTablesData;
using google::cloud::odbc_bq_driver_internal::FetchForeignKeysFromDataSource;
using google::cloud::odbc_bq_driver_internal::GetResultSetForDatasets;
using google::cloud::odbc_bq_driver_internal::GetResultSetForProjects;
using google::cloud::odbc_bq_driver_internal::GetResultSetForTables;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc2;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc3;
using google::cloud::odbc_bq_driver_internal::kDriverOdbcVer;
using google::cloud::odbc_bq_driver_internal::kMatchAll;
using google::cloud::odbc_bq_driver_internal::kODBCColumnsMap;
using google::cloud::odbc_bq_driver_internal::kSchema;
using google::cloud::odbc_bq_driver_internal::kSqlApiAllFuncsSize;
using google::cloud::odbc_bq_driver_internal::LogAndReturnCode;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC2Functions;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC3Functions;
using google::cloud::odbc_bq_driver_internal::ProcessProcedures;
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
using google::cloud::odbc_bq_driver_internal::TableReference;
using google::cloud::odbc_bq_driver_internal::UnSupportedInfoType;
using google::cloud::odbc_bq_driver_internal::ValidateColumnParameters;
using google::cloud::odbc_bq_driver_internal::ValidateInputParameters;
using google::cloud::odbc_bq_driver_internal::ValidateProcedureColumnParameters;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

// Internal helper functions.
namespace {

StatusRecord InvalidType(char const* mesg, SQLUSMALLINT info_type) {
  std::string message = mesg;
  message.append(std::to_string(info_type));
  return StatusRecord{SQLStates::k_HY096(), message};
}

SQLRETURN HandleConnectionInformationTypes(
    SQLHDBC connection_handle, SQLUSMALLINT info_type,
    SQLPOINTER info_value_ptr, SQLSMALLINT in_buffer_len,
    SQLSMALLINT* str_len_ptr, bool check_is_connection_done = true) {
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle, check_is_connection_done);
  if (!handle_result) {
    LOG(ERROR)
        << "HandleConnectionInformationTypes::ValidateConnectionHandle:: "
        << handle_result.GetStatusRecord().message;
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
      LOG(ERROR) << "HandleConnectionInformationTypes::InvalidType:: "
                 << status_record.message;
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
  LOG(INFO) << "SQLGetFunctionsInternal:: Start";
  StatusRecordOr<ConnectionHandle*> handle_result =
      ValidateConnectionHandle(connection_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLGetFunctions::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }

  ConnectionHandle* handle = *handle_result;
  if (!supported_fn) {
    auto status_record = StatusRecord{SQLStates::k_HY024(),
                                      "Argument supported_fn cannot be null"};
    LOG(ERROR) << "SQLGetFunctions:: " << status_record.message;
    return LogAndReturnCode(*handle, status_record);
  }

  switch (function_id) {
    case SQL_API_ODBC3_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC3Functions(supported_fn);
      if (!status_record.ok()) {
        LOG(ERROR) << "SQLGetFunctions::PopulateSupportedODBC3Functions:: "
                   << status_record.message;
      }
      return LogAndReturnCode(*handle, status_record);
    }
    case SQL_API_ALL_FUNCTIONS: {
      StatusRecord status_record =
          PopulateSupportedODBC2Functions(supported_fn);
      if (!status_record.ok()) {
        LOG(ERROR) << "SQLGetFunctions::PopulateSupportedODBC2Functions:: "
                   << status_record.message;
      }
      return LogAndReturnCode(*handle, status_record);
    }
    default:
      break;
  }
  if (IsFunctionIdOdbc3(function_id)) {
    SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
    StatusRecord status_record = PopulateSupportedODBC3Functions(odbc3_fns);
    if (!status_record.ok()) {
      LOG(ERROR) << "SQLGetFunctions::PopulateSupportedODBC3Functions:: "
                 << status_record.message;
      return LogAndReturnCode(*handle, status_record);
    }
    *supported_fn = SQL_FUNC_EXISTS(odbc3_fns, function_id);
  } else if (IsFunctionIdOdbc2(function_id)) {
    SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];
    StatusRecord status_record = PopulateSupportedODBC2Functions(odbc2_fns);
    if (!status_record.ok()) {
      LOG(ERROR) << "SQLGetFunctions::PopulateSupportedODBC2Functions:: "
                 << status_record.message;
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
  LOG(INFO) << "SQLGetInfoInternal:: Start";
  // for SQL_DRIVER_ODBC_VER should go through even when connection is not
  // established
  StatusRecordOr<ConnectionHandle*> handle_result = ValidateConnectionHandle(
      connection_handle, info_type != SQL_DRIVER_ODBC_VER);

  if (!handle_result) {
    LOG(ERROR) << "SQLGetInfo::ValidateConnectionHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  ConnectionHandle* handle = *handle_result;

  if (in_buffer_len < 0) {
    std::string mesg = "Invalid Input BufferLength";
    auto status_record = StatusRecord{SQLStates::k_HY090(), mesg};
    LOG(ERROR) << "SQLGetInfo:: " << mesg;
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
  LOG(ERROR) << "SQLGetInfo::InvalidType:: " << status_record.message;
  return LogAndReturnCode(*handle, status_record);
}

SQLRETURN SQLPrimaryKeysInternal(SQLHSTMT stmt_handle,
                                 SQLCHAR const* catalog_name,
                                 SQLSMALLINT catalog_name_len,
                                 SQLCHAR const* schema_name,
                                 SQLSMALLINT schema_name_len,
                                 SQLCHAR const* table_name,
                                 SQLSMALLINT table_name_len) {
  LOG(INFO) << "SQLPrimaryKeysInternal:: Start";
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLPrimaryKeys::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
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
    LOG(ERROR) << "SQLPrimaryKeys::FetchPrimaryKeysFromDataSource:: "
               << ds_status_record_or.GetStatusRecord().message;
    return LogAndReturnCode(handle, ds_status_record_or);
  }
  // Process the DSResults and convert to ResultSet.
  StatusRecordOr<ResultSet> rs_status_record_or =
      ProcessQueryResults(*ds_status_record_or);
  if (!rs_status_record_or) {
    LOG(ERROR) << "SQLPrimaryKeys::ProcessQueryResults:: "
               << rs_status_record_or.GetStatusRecord().message;
    return LogAndReturnCode(handle, rs_status_record_or);
  }

  auto max_rows_status = handle.GetAttribute(SQL_ATTR_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "SQLPrimaryKeys::GetAttribute:: "
               << max_rows_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, max_rows_status);
  }
  SQLULEN max_rows = *max_rows_status;
  ResultSet& result_set = *rs_status_record_or;
  auto& rs_rows = result_set.rows;
  if (max_rows > 0 && max_rows < rs_rows.size()) {
    rs_rows.erase(rs_rows.begin() + max_rows, rs_rows.end());
  }

  // Store the resultset in statement handle.
  handle.SetResultSet(result_set);
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
  LOG(INFO) << "SQLForeignKeysInternal:: Start";
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLForeignKeys::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
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
    LOG(ERROR) << "SQLForeignKeys::FetchForeignKeysFromDataSource:: "
               << ds_status_record_or.GetStatusRecord().message;
    return LogAndReturnCode(handle, ds_status_record_or);
  }
  // Process the DSResults and convert to ResultSet.
  StatusRecordOr<ResultSet> rs_status_record_or =
      ProcessQueryResults(*ds_status_record_or);
  if (!rs_status_record_or) {
    LOG(ERROR) << "SQLForeignKeys::ProcessQueryResults:: "
               << rs_status_record_or.GetStatusRecord().message;
    return LogAndReturnCode(handle, rs_status_record_or);
  }
  auto max_rows_status = handle.GetAttribute(SQL_ATTR_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "SQLForeignKeys::GetAttribute:: "
               << max_rows_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, max_rows_status);
  }
  SQLULEN max_rows = *max_rows_status;
  ResultSet& result_set = *rs_status_record_or;
  auto& rs_rows = result_set.rows;
  if (max_rows > 0 && max_rows < rs_rows.size()) {
    rs_rows.erase(rs_rows.begin() + max_rows, rs_rows.end());
  }

  // Store the resultset in statement handle.
  handle.SetResultSet(result_set);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  return rc;
}

SQLRETURN SQLTablesInternal(SQLHSTMT stmt_handle, SQLCHAR* catalog_name,
                            SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
                            SQLSMALLINT schema_name_len, SQLCHAR* table_name,
                            SQLSMALLINT table_name_len, SQLCHAR* table_type,
                            SQLSMALLINT table_type_len) {
  LOG(INFO) << "SQLTablesInternal:: Start";
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLTables::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  StatusRecordOr<SQLULEN> attr_status =
      handle.GetAttribute(SQL_ATTR_METADATA_ID);
  if (!attr_status) {
    LOG(ERROR) << "SQLTables::GetAttribute:: "
               << attr_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, attr_status);
  }
  SQLULEN metadata_id = *attr_status;

  auto input_param_status = ValidateInputParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, table_name,
      table_name_len, table_type_len, metadata_id);
  if (!input_param_status.ok()) {
    LOG(ERROR) << "SQLTables::ValidateInputParameters:: "
               << input_param_status.message;
    return LogAndReturnCode(handle, input_param_status);
  }

  std::string project_filter = ToCharStr(catalog_name, kMatchAll);
  std::string dataset_filter = ToCharStr(schema_name, kMatchAll);
  std::string table_filter = ToCharStr(table_name, kMatchAll);
  std::string table_type_filter = ToCharStr(table_type, kMatchAll);

  if (handle.GetConnectionHandle() == nullptr) {
    LOG(ERROR) << "SQLTables:: Internal connection handle is null";
    return LogAndReturnCode(handle,
                            StatusRecord{SQLStates::k_HY013(),
                                         "Internal connection handle is null"});
  }
  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    LOG(ERROR) << "SQLTables:: Connection to the data source is broken";
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_08S01(),
                             "Connection to the data source is broken"});
  }
   auto start_time = std::chrono::high_resolution_clock::now();
  std::shared_ptr<ODBCBQClient> bq_client_ptr = conn_handle.GetClient();
  if (!bq_client_ptr) {
    LOG(ERROR) << "SQLTables:: Error establishing Datasource connection";
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_HY000(),
                             "Error establishing Datasource connection"});
  }
  ODBCBQClient& bq_client = *bq_client_ptr;
  StatusRecordOr<ResultSet> result_set_status;

  if (!metadata_id && project_filter == SQL_ALL_CATALOGS &&
      dataset_filter.empty() && table_filter.empty()) {
    result_set_status = GetResultSetForProjects(
        bq_client, metadata_id, conn_handle.GetDsn().additional_projects);
  } else if (!metadata_id && project_filter.empty() &&
             dataset_filter == SQL_ALL_SCHEMAS && table_filter.empty()) {
    result_set_status =
        GetResultSetForDatasets(bq_client, metadata_id, kMatchAll,
                                conn_handle.GetDsn().additional_projects);
  } else if (!metadata_id && project_filter.empty() && dataset_filter.empty() &&
             table_filter.empty() && table_type_filter == SQL_ALL_TABLE_TYPES) {
    result_set_status = CreateResultSetForTableTypes();
  } else {
    result_set_status =
        GetResultSetForTables(handle, bq_client, project_filter, dataset_filter,
                              table_filter, table_type_filter, metadata_id);
  }
  if (!result_set_status) {
    LOG(ERROR) << "SQLTables::ResultSet:: "
               << result_set_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, result_set_status);
  }
  auto end_time = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
  std::cout << "\n[PERF] result set generation time: " << elapsed.count() << " ms\n";

  auto max_rows_status = handle.GetAttribute(SQL_ATTR_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "SQLTables::GetAttribute:: "
               << max_rows_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, max_rows_status);
  }
  SQLULEN max_rows = *max_rows_status;
  ResultSet& result_set = *result_set_status;
  auto& rs_rows = result_set.rows;
  if (max_rows > 0 && max_rows < rs_rows.size()) {
    rs_rows.erase(rs_rows.begin() + max_rows, rs_rows.end());
  }

  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  ird.SetConnectionHandle(&conn_handle);
  auto table_schema =
      BuildTableSchemaFromRowSchema(result_set.row_schema, kSchema);
  if (!table_schema) {
    LOG(ERROR) << "SQLTables::BuildTableSchemaFromRowSchema:: "
               << table_schema.GetStatusRecord().message;
    return LogAndReturnCode(handle, table_schema);
  }

  TableReference table_fields;
  auto ird_status =
      StatementHandle::PopulateIrd(ird, *table_schema, table_fields);
  if (!ird_status.ok()) {
    LOG(ERROR) << "SQLTables::PopulateIrd:: " << ird_status.message;
    return LogAndReturnCode(handle, ird_status);
  }

  handle.SetResultSet(result_set);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  return SQL_SUCCESS;
}

SQLRETURN SQLColumnsInternal(SQLHSTMT stmt_handle, SQLCHAR* catalog_name,
                             SQLSMALLINT catalog_name_len, SQLCHAR* schema_name,
                             SQLSMALLINT schema_name_len, SQLCHAR* table_name,
                             SQLSMALLINT table_name_len, SQLCHAR* column_name,
                             SQLSMALLINT column_name_len) {
  LOG(INFO) << "SQLColumnsInternal:: Start";
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLColumns::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  StatusRecordOr<SQLULEN> attr_status =
      handle.GetAttribute(SQL_ATTR_METADATA_ID);
  if (!attr_status) {
    LOG(ERROR) << "SQLColumns::GetAttribute:: "
               << attr_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, attr_status);
  }
  SQLULEN metadata_id = *attr_status;

  if (handle.GetConnectionHandle() == nullptr) {
    LOG(ERROR) << "SQLColumns:: Internal connection handle is null";
    return LogAndReturnCode(handle,
                            StatusRecord{SQLStates::k_HY013(),
                                         "Internal connection handle is null"});
  }

  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    LOG(ERROR) << "SQLColumns:: Connection to the data source is broken";
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_08S01(),
                             "Connection to the data source is broken"});
  }

  std::string catalog_str;
  if (catalog_name_len == 0) {
    SQLINTEGER catalog_len = 0;
    SQLCHAR current_catalog[256] = {0};
    conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, current_catalog,
                             sizeof(current_catalog), &catalog_len);

    catalog_str.assign(reinterpret_cast<char*>(current_catalog), catalog_len);
    catalog_name = reinterpret_cast<SQLCHAR*>(catalog_str.data());
    catalog_name_len = static_cast<SQLSMALLINT>(catalog_str.size());
  }

  auto input_param_status = ValidateColumnParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, table_name,
      table_name_len, column_name, column_name_len, metadata_id);
  if (!input_param_status.ok()) {
    LOG(ERROR) << "SQLColumns::ValidateColumnParameters:: "
               << input_param_status.message;
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

  // Fetch BQ Table. This particular call fetches a single table.
  auto filtered_tables_data_status = FetchBQTablesData(
      handle, s_catalog_name, s_dataset_name, s_table_name, metadata_id);
  if (!filtered_tables_data_status) {
    LOG(ERROR) << "SQLColumns::FetchBQTablesData:: "
               << filtered_tables_data_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, filtered_tables_data_status);
  }

  // Process Table Results for each table returned from the list above.
  auto max_rows_status = handle.GetAttribute(SQL_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "SQLColumns::GetAttribute:: "
               << max_rows_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, max_rows_status);
  }
  SQLULEN max_rows = *max_rows_status;

  ResultSet final_result_set;
  for (auto const& bq_table : *filtered_tables_data_status) {
    StatusRecordOr<ResultSet> table_result_set_status =
        ProcessTableResults(conn_handle, bq_table, s_column_name, metadata_id);

    if (!table_result_set_status) {
      LOG(ERROR) << "SQLColumns::ProcessTableResults:: "
                 << table_result_set_status.GetStatusRecord().message;
      return LogAndReturnCode(handle, table_result_set_status);
    }
    auto const& new_rows = table_result_set_status->rows;
    if (!new_rows.empty()) {
      if (final_result_set.row_schema.empty()) {
        final_result_set.row_schema = table_result_set_status->row_schema;
      }

      for (auto const& row : new_rows) {
        if (max_rows != 0 && final_result_set.rows.size() >= max_rows) {
          break;
        }
        final_result_set.rows.push_back(row);
      }

      if (max_rows != 0 && final_result_set.rows.size() >= max_rows) {
        break;
      }
    }
  }

  DescriptorHandle& ird = handle.GetDescriptorHandle(DescriptorType::kIRD);
  auto table_schema = BuildTableSchemaFromRowSchema(final_result_set.row_schema,
                                                    kODBCColumnsMap);
  if (!table_schema) {
    LOG(ERROR) << "SQLColumns::BuildTableSchemaFromRowSchema:: "
               << table_schema.GetStatusRecord().message;
    return LogAndReturnCode(handle, table_schema);
  }

  TableReference table_fields;
  ird.SetConnectionHandle(&conn_handle);
  auto ird_status =
      StatementHandle::PopulateIrd(ird, *table_schema, table_fields);
  if (!ird_status.ok()) {
    LOG(ERROR) << "SQLColumns::PopulateIrd:: " << ird_status.message;
    return LogAndReturnCode(handle, ird_status);
  }

  if (!final_result_set.rows.empty()) {
    handle.SetResultSet(final_result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }

  return SQL_SUCCESS;
}

SQLRETURN SQLProcedureInternal(SQLHSTMT stmt_handle, SQLCHAR* catalog_name,
                               SQLSMALLINT catalog_name_len,
                               SQLCHAR* schema_name,
                               SQLSMALLINT schema_name_len, SQLCHAR* proc_name,
                               SQLSMALLINT proc_name_len) {
  LOG(INFO) << "SQLProcedureInternal:: Start";
  // Validate statement handle
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLProcedure::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  // Get metadata attribute
  StatusRecordOr<SQLULEN> attr_status =
      handle.GetAttribute(SQL_ATTR_METADATA_ID);
  if (!attr_status) {
    LOG(ERROR) << "SQLProcedure::GetAttribute:: "
               << attr_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, attr_status);
  }
  SQLULEN metadata_id = *attr_status;

  // Validate connection handle
  if (handle.GetConnectionHandle() == nullptr) {
    LOG(ERROR) << "SQLProcedure::Internal connection handle is null";
    return LogAndReturnCode(handle,
                            StatusRecord{SQLStates::k_HY013(),
                                         "Internal connection handle is null"});
  }
  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());

  if (!conn_handle.IsConnected()) {
    LOG(ERROR) << "SQLProcedure::Connection to the data source is broken";
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_08S01(),
                             "Connection to the data source is broken"});
  }

  std::string catalog_str;
  if (catalog_name_len == 0) {
    SQLINTEGER catalog_len = 0;
    SQLCHAR current_catalog[256] = {0};
    conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, current_catalog,
                             sizeof(current_catalog), &catalog_len);
    catalog_str.assign(reinterpret_cast<char*>(current_catalog), catalog_len);
    catalog_name = reinterpret_cast<SQLCHAR*>(catalog_str.data());
    catalog_name_len = static_cast<SQLSMALLINT>(catalog_str.size());
  }

  // Validate input parameters
  auto input_param_status = ValidateProcedureColumnParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, proc_name,
      proc_name_len, metadata_id);
  if (!input_param_status.Ok()) {
    LOG(ERROR) << "SQLProcedure::ValidateProcedureColumnParameters:: "
               << input_param_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, input_param_status);
  }

  // Convert parameters to strings
  std::string project_filter = ToCharStr(catalog_name, kMatchAll);
  std::string dataset_filter = ToCharStr(schema_name, kMatchAll);
  std::string proc_filter = ToCharStr(proc_name, kMatchAll);

  auto filtered_procedure_data_status = FetchBQSQLProceduresData(
      handle, project_filter, dataset_filter, proc_filter, metadata_id);
  if (!filtered_procedure_data_status) {
    LOG(ERROR) << "SQLProcedure::FetchBQSQLProceduresData:: "
               << filtered_procedure_data_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, filtered_procedure_data_status);
  }

  auto max_rows_status = handle.GetAttribute(SQL_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "SQLProcedure::GetAttribute:: "
               << max_rows_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, max_rows_status);
  }
  SQLULEN max_rows = *max_rows_status;

  ResultSet final_result_set;
  if (filtered_procedure_data_status.GetValue().empty()) {
    handle.SetResultSet(final_result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
    return SQL_SUCCESS;
  }

  StatusRecordOr<ResultSet> procedure_result_set_status =
      ProcessProcedures(filtered_procedure_data_status.GetValue());

  if (!procedure_result_set_status) {
    LOG(ERROR) << "SQLProcedure::ProcessProcedures:: "
               << procedure_result_set_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, procedure_result_set_status);
  }

  if (!procedure_result_set_status->rows.empty()) {
    if (final_result_set.row_schema.empty()) {
      final_result_set.row_schema = procedure_result_set_status->row_schema;
    }
    for (auto const& row : procedure_result_set_status->rows) {
      if (max_rows != 0 && final_result_set.rows.size() >= max_rows) {
        break;
      }
      final_result_set.rows.push_back(row);
    }
  }
  handle.SetResultSet(final_result_set);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  return SQL_SUCCESS;
}

SQLRETURN SQLProcedureColumnsInternal(
    SQLHSTMT stmt_handle, SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    SQLCHAR* schema_name, SQLSMALLINT schema_name_len, SQLCHAR* proc_name,
    SQLSMALLINT proc_name_len, SQLCHAR* column_name,
    SQLSMALLINT column_name_len) {
  LOG(INFO) << "SQLProcedureColumnsInternal:: Start";
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    LOG(ERROR) << "SQLProcedureColumns::ValidateStatementHandle:: "
               << handle_result.GetStatusRecord().message;
    return handle_result.GetCalculatedReturnCode();
  }
  StatementHandle& handle = *(*handle_result);

  StatusRecordOr<SQLULEN> attr_status =
      handle.GetAttribute(SQL_ATTR_METADATA_ID);
  if (!attr_status) {
    LOG(ERROR) << "SQLProcedureColumns::GetAttribute:: "
               << attr_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, attr_status);
  }
  SQLULEN metadata_id = *attr_status;

  if (handle.GetConnectionHandle() == nullptr) {
    LOG(ERROR) << "SQLProcedureColumns:: Internal connection handle is null";
    return LogAndReturnCode(handle,
                            StatusRecord{SQLStates::k_HY013(),
                                         "Internal connection handle is null"});
  }

  ConnectionHandle& conn_handle = *(handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    LOG(ERROR)
        << "SQLProcedureColumns::Connection to the data source is broken";
    return LogAndReturnCode(
        handle, StatusRecord{SQLStates::k_08S01(),
                             "Connection to the data source is broken"});
  }

  std::string catalog_str;
  if (catalog_name_len == 0) {
    SQLINTEGER catalog_len = 0;
    SQLCHAR current_catalog[256] = {0};
    conn_handle.GetAttribute(SQL_ATTR_CURRENT_CATALOG, current_catalog,
                             sizeof(current_catalog), &catalog_len);
    catalog_str.assign(reinterpret_cast<char*>(current_catalog), catalog_len);
    catalog_name = reinterpret_cast<SQLCHAR*>(catalog_str.data());
    catalog_name_len = static_cast<SQLSMALLINT>(catalog_str.size());
  }

  auto input_param_status = ValidateProcedureColumnParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, proc_name,
      proc_name_len, metadata_id);
  if (!input_param_status.Ok()) {
    LOG(ERROR) << "SQLProcedureColumns::ValidateProcedureColumnParameters:: "
               << input_param_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, input_param_status);
  }

  std::string s_catalog_name = ToCharStr(catalog_name);
  std::string s_dataset_name = ToCharStr(schema_name, kMatchAll);
  std::string s_proc_name = ToCharStr(proc_name, kMatchAll);

  std::string s_column_name;
  if (column_name_len != 0) {
    s_column_name = ToCharStr(column_name, kMatchAll);
  }

  if (metadata_id == SQL_TRUE) {
    SanitizeIdentifierArgument(s_dataset_name);
    SanitizeIdentifierArgument(s_proc_name);
    SanitizeIdentifierArgument(s_column_name);
  }

  auto filtered_procedure_data_status = FetchBQProceduresData(
      handle, s_catalog_name, s_dataset_name, s_proc_name, metadata_id);

  if (!filtered_procedure_data_status) {
    LOG(ERROR) << "SQLProcedureColumns::FetchBQProceduresData:: "
               << filtered_procedure_data_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, filtered_procedure_data_status);
  }
  auto max_rows_status = handle.GetAttribute(SQL_MAX_ROWS);
  if (!max_rows_status) {
    LOG(ERROR) << "SQLProcedureColumns::GetAttribute:: "
               << max_rows_status.GetStatusRecord().message;
    return LogAndReturnCode(handle, max_rows_status);
  }
  SQLULEN max_rows = *max_rows_status;

  ResultSet final_result_set;
  if (filtered_procedure_data_status.GetValue().empty()) {
    handle.SetResultSet(final_result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
    return SQL_SUCCESS;
  }

  for (auto const& bq_proc : *filtered_procedure_data_status) {
    StatusRecordOr<ResultSet> procedure_result_set_status =
        ProcessProcedureColumnResults(bq_proc, s_column_name, metadata_id);

    if (!procedure_result_set_status) {
      LOG(ERROR) << "SQLProcedureColumns::ProcessProcedureColumnResults:: "
                 << procedure_result_set_status.GetStatusRecord().message;
      return LogAndReturnCode(handle, procedure_result_set_status);
    }

    if (!procedure_result_set_status->rows.empty()) {
      if (final_result_set.row_schema.empty()) {
        final_result_set.row_schema = procedure_result_set_status->row_schema;
      }

      for (auto const& row : procedure_result_set_status->rows) {
        if (max_rows != 0 && final_result_set.rows.size() >= max_rows) {
          break;
        }
        final_result_set.rows.push_back(row);
      }
    }
  }

  handle.SetResultSet(final_result_set);
  handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  return SQL_SUCCESS;
}

}  // namespace google::cloud::odbc_bq_driver
