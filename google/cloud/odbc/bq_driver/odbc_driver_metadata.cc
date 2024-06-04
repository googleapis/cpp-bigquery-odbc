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
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_primary_keys.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ArithmeticToDSValue;
using google::cloud::odbc_bq_driver_internal::BQDataType;
using google::cloud::odbc_bq_driver_internal::ColumnSchema;
using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DSResults;
using google::cloud::odbc_bq_driver_internal::DSRow;
using google::cloud::odbc_bq_driver_internal::DSValue;
using google::cloud::odbc_bq_driver_internal::FetchPrimaryKeysFromDataSource;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc2;
using google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc3;
using google::cloud::odbc_bq_driver_internal::kSqlApiAllFuncsSize;
using google::cloud::odbc_bq_driver_internal::kSqlToBqDataTypes;
using google::cloud::odbc_bq_driver_internal::kTraceOption;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC2Functions;
using google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC3Functions;
using google::cloud::odbc_bq_driver_internal::ProcessQueryResults;
using google::cloud::odbc_bq_driver_internal::ResultSet;
using google::cloud::odbc_bq_driver_internal::RowSchema;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoBitmask;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlChar;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlUInt;
using google::cloud::odbc_bq_driver_internal::SQLGetInfoSqlUSmallInt;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_bq_driver_internal::StmtStates;
using google::cloud::odbc_bq_driver_internal::StringToDSValue;
using google::cloud::odbc_bq_driver_internal::SupportedInfoType;
using google::cloud::odbc_bq_driver_internal::TraceOptions;
using google::cloud::odbc_bq_driver_internal::TracePrintInternal;
using google::cloud::odbc_bq_driver_internal::TypeInfoRow;
using google::cloud::odbc_bq_driver_internal::UnSupportedInfoType;
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

char* to_char_str(SQLCHAR const* sql_str) {
  return reinterpret_cast<char*>(const_cast<SQLCHAR*>(sql_str));
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

DSRow CreateDSRowFromTypeInfo(TypeInfoRow& type_info) {
  DSRow ds_row;

  DSValue type_name;
  StringToDSValue(type_info.type_name, type_name);
  ds_row.emplace_back(type_name);

  DSValue data_type;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(type_info.data_type),
                                 data_type);
  ds_row.emplace_back(data_type);

  DSValue col_size;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(type_info.col_size),
                                 col_size);
  ds_row.emplace_back(col_size);

  if (type_info.literal_prefix) {
    DSValue literal_prefix;
    StringToDSValue(type_info.literal_prefix, literal_prefix);
    ds_row.emplace_back(literal_prefix);
  } else {
    ds_row.emplace_back();
  }

  if (type_info.literal_suffix) {
    DSValue literal_suffix;
    StringToDSValue(type_info.literal_suffix, literal_suffix);
    ds_row.emplace_back(literal_suffix);
  } else {
    ds_row.emplace_back();
  }

  if (type_info.create_params) {
    DSValue create_params;
    StringToDSValue(type_info.create_params, create_params);
    ds_row.emplace_back(create_params);
  } else {
    ds_row.emplace_back();
  }

  DSValue nullable;
  ArithmeticToDSValue<SQLBIGINT>(type_info.nullable, nullable);
  ds_row.emplace_back(nullable);

  DSValue case_sensitive;
  ArithmeticToDSValue<SQLBIGINT>(type_info.case_sensitive, case_sensitive);
  ds_row.emplace_back(case_sensitive);

  DSValue searchable;
  ArithmeticToDSValue<SQLBIGINT>(type_info.searchable, searchable);
  ds_row.emplace_back(searchable);

  DSValue unsigned_attribute;
  ArithmeticToDSValue<SQLBIGINT>(type_info.unsigned_attribute,
                                 unsigned_attribute);
  ds_row.emplace_back(unsigned_attribute);

  DSValue fixed_prec_scale;
  ArithmeticToDSValue<SQLBIGINT>(type_info.fixed_prec_scale, fixed_prec_scale);
  ds_row.emplace_back(fixed_prec_scale);

  DSValue auto_unique_value;
  ArithmeticToDSValue<SQLBIGINT>(type_info.auto_unique_value,
                                 auto_unique_value);
  ds_row.emplace_back(auto_unique_value);

  DSValue local_type_name;
  StringToDSValue(type_info.local_type_name, local_type_name);
  ds_row.emplace_back(local_type_name);

  DSValue minimum_scale;
  ArithmeticToDSValue<SQLBIGINT>(type_info.minimum_scale, minimum_scale);
  ds_row.emplace_back(minimum_scale);

  DSValue maximum_scale;
  ArithmeticToDSValue<SQLBIGINT>(type_info.maximum_scale, maximum_scale);
  ds_row.emplace_back(maximum_scale);

  DSValue sql_data_type;
  ArithmeticToDSValue<SQLBIGINT>(type_info.sql_data_type, sql_data_type);
  ds_row.emplace_back(sql_data_type);

  DSValue sql_datetime_sub;
  ArithmeticToDSValue<SQLBIGINT>(type_info.sql_datetime_sub, sql_datetime_sub);
  ds_row.emplace_back(sql_datetime_sub);

  DSValue num_prec_radix;
  ArithmeticToDSValue<SQLBIGINT>(type_info.num_prec_radix, num_prec_radix);
  ds_row.emplace_back(num_prec_radix);

  DSValue interval_precision;
  ArithmeticToDSValue<SQLBIGINT>(type_info.interval_precision,
                                 interval_precision);
  ds_row.emplace_back(interval_precision);

  return ds_row;
}

void CreateTypeInfoRowSchema(ResultSet& result_set) {
  RowSchema& row_schema = result_set.row_schema;
  ColumnSchema col_schema;

  // Schema for type_name
  col_schema.col_index = 0;
  col_schema.col_type = BQDataType::kString;
  row_schema.push_back(col_schema);

  // Schema for data_type
  col_schema.col_index++;
  col_schema.col_type = BQDataType::kInt64;
  row_schema.push_back(col_schema);

  // Schema for col_size
  col_schema.col_index++;
  col_schema.col_type = BQDataType::kInt64;
  row_schema.push_back(col_schema);

  // Schema for literal_prefix
  col_schema.col_index++;
  col_schema.col_type = BQDataType::kString;
  row_schema.push_back(col_schema);

  // Schema for literal_suffix
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for create_params
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for nullable
  col_schema.col_index++;
  col_schema.col_type = BQDataType::kInt64;
  row_schema.push_back(col_schema);

  // Schema for case_sensitive
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for searchable
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for unsigned_attribute
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for fixed_prec_scale
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for auto_unique_value
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for local_type_name
  col_schema.col_index++;
  col_schema.col_type = BQDataType::kString;
  row_schema.push_back(col_schema);

  // Schema for minimum_scale
  col_schema.col_index++;
  col_schema.col_type = BQDataType::kInt64;
  row_schema.push_back(col_schema);

  // Schema for maximum_scale
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for sql_data_type
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for sql_datetime_sub
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for num_prec_radix
  col_schema.col_index++;
  row_schema.push_back(col_schema);

  // Schema for interval_precision
  col_schema.col_index++;
  row_schema.push_back(col_schema);
}

SQLRETURN SQLGetTypeInfoInternal(SQLHSTMT stmt_handle, SQLSMALLINT data_type) {
  SQLRETURN rc = SQL_SUCCESS;
  StatusRecordOr<StatementHandle*> handle_result =
      ValidateStatementHandle(stmt_handle);
  if (!handle_result) {
    TracePrintInternal(opts, handle_result.GetStatusRecord().message);
    return handle_result.GetCalculatedReturnCode();
  }

  StatementHandle& handle = *(*handle_result);

  ResultSet& result_set = handle.GetResultSet();
  if (data_type == SQL_ALL_TYPES) {
    for (auto [sql_data_type, bq_data_type_info] : kSqlToBqDataTypes) {
      for (auto [bq_data_type, type_info] : bq_data_type_info) {
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
      }
    }
  } else {
    if (kSqlToBqDataTypes.count(data_type)) {
      for (auto [bq_data_type, type_info] : kSqlToBqDataTypes.at(data_type)) {
        result_set.rows.push_back(CreateDSRowFromTypeInfo(type_info));
      }
    }
  }

  if (!result_set.rows.empty()) {
    CreateTypeInfoRowSchema(result_set);
    handle.SetStmtState(StmtStates::kStatementExecutedWithRs);
  } else {
    handle.SetStmtState(StmtStates::kStatementExecutedWithoutRs);
  }

  return SQL_SUCCESS;
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
      FetchPrimaryKeysFromDataSource(handle, to_char_str(catalog_name),
                                     catalog_name_len, to_char_str(schema_name),
                                     schema_name_len, to_char_str(table_name),
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

}  // namespace google::cloud::odbc_bq_driver
