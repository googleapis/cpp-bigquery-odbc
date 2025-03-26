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

#include "google/cloud/odbc/bq_driver/internal/odbc_procedure_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using google::cloud::odbc_bq_driver_internal::GetSQLDataType;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

std::string const kProcedureNameParam = "procedure_name";
std::string const kSchemaParam = "schema_name";
std::string const kProcedureQuery =
    "SELECT procedure_name, schema_name FROM INFORMATION_SCHEMA.ROUTINES";

StatusRecord ValidateProcedureColumnParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* procedure_name, SQLSMALLINT procedure_name_len,
    const SQLCHAR* /*column_name*/, SQLSMALLINT column_name_len,
    SQLULEN metadata_id) {
  // Validate procedure and related parameters.
  auto status_record = ValidateProcedureParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len,
      procedure_name, procedure_name_len, metadata_id);
  if (!status_record.ok()) {
    return status_record;
  }

  if (column_name_len < 0 && column_name_len != SQL_NTS) {
    return StatusRecord{
        SQLStates::k_HY090(),
        "Invalid buffer length - procedure column name length is invalid"};
  }

  // Validate SQLProcedureColumns specific parameters.
  if (IsSearchPatternArgument(reinterpret_cast<char const*>(catalog_name))) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Catalog name cannot be a search pattern"};
  }

  return StatusRecord::Ok();
}

StatusRecordOr<FixedColumnMetadata> GetFixedColumnMetadataa(
    std::string const& type) {
  FixedColumnMetadata fixed_column_metadata;
  auto ds_type_status = ConvertDSType(type);
  if (!ds_type_status) {
    return ds_type_status.GetStatusRecord();
  }
  switch (*ds_type_status) {
    case BQDataType::kString:
    case BQDataType::kBytes:
    case BQDataType::kInterval: {
      fixed_column_metadata.precision = 16384;
      fixed_column_metadata.buf_len = 16384;
      fixed_column_metadata.char_octet_len = 16384;
      fixed_column_metadata.radix = 10;
      break;
    }
    case BQDataType::kInt64: {
      fixed_column_metadata.precision = 19;
      fixed_column_metadata.buf_len = 20;
      fixed_column_metadata.scale = 0;
      fixed_column_metadata.radix = 10;
      break;
    }
    case BQDataType::kBool: {
      fixed_column_metadata.precision = 1;
      fixed_column_metadata.buf_len = 1;
      fixed_column_metadata.char_octet_len = 16384;
      break;
    }
    case BQDataType::kTime: {
      fixed_column_metadata.precision = 15;
      fixed_column_metadata.buf_len = 6;
      fixed_column_metadata.scale = 6;
      break;
    }
    case BQDataType::kDate: {
      fixed_column_metadata.precision = 10;
      fixed_column_metadata.buf_len = 6;
      break;
    }
    case BQDataType::kTimeStamp:
    case BQDataType::kDatetime: {
      fixed_column_metadata.precision = 26;
      fixed_column_metadata.buf_len = 16;
      fixed_column_metadata.scale = 6;
      fixed_column_metadata.radix = 2;
      fixed_column_metadata.char_octet_len = 16384;
      break;
    }
    case BQDataType::kNumeric: {
      fixed_column_metadata.precision = 38;
      fixed_column_metadata.buf_len = 40;
      fixed_column_metadata.scale = 9;
      fixed_column_metadata.radix = 10;
      break;
    }
    case BQDataType::kBigNumeric: {
      fixed_column_metadata.precision = 77;
      fixed_column_metadata.buf_len = 79;
      fixed_column_metadata.scale = 38;
      fixed_column_metadata.radix = 10;
      break;
    }
    case BQDataType::kFloat64: {
      fixed_column_metadata.precision = 53;
      fixed_column_metadata.buf_len = 8;
      fixed_column_metadata.scale = 9;
      fixed_column_metadata.radix = 2;
      fixed_column_metadata.char_octet_len = 16384;
      break;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(),
                          "Unsupported BQ Data Type: " + *ds_type_status};
    }
  }
  return fixed_column_metadata;
}

StatusRecordOr<DSRow> CreateProcedureResultSetDSRow(
    ProcedureFieldSchema const& proc_column) {
  DSRow ds_row;

  // PROCEDURE_CAT
  DSValue ds_procedure_cat = kNullValue;
  if (!proc_column.catalog.empty()) {
    StringToDSValue(proc_column.catalog, ds_procedure_cat);
  }
  ds_row.emplace_back(ds_procedure_cat);

  // PROCEDURE_SCHEMA
  DSValue ds_procedure_schema = kNullValue;
  if (!proc_column.dataset.empty()) {
    StringToDSValue(proc_column.dataset, ds_procedure_schema);
  }
  ds_row.emplace_back(ds_procedure_schema);

  // PROCEDURE_NAME
  DSValue ds_procedure_name = kNullValue;
  if (!proc_column.procedure.empty()) {
    StringToDSValue(proc_column.procedure, ds_procedure_name);
  }
  ds_row.emplace_back(ds_procedure_name);

  // COLUMN_NAME
  DSValue ds_column_name = kNullValue;
  if (!proc_column.name.empty()) {
    StringToDSValue(proc_column.name, ds_column_name);
  }
  ds_row.emplace_back(ds_column_name);

  // COLUMN_TYPE
  DSValue ds_column_type;
  std::string value;
  if (proc_column.column_type == "OUT") {
    value = "4";
  } else if (proc_column.column_type == "INOUT") {
    value = "2";
  } else {
    value = "1";
  }
  SQLBIGINT bigint_value = std::stoll(value);
  ArithmeticToDSValue<SQLBIGINT>(bigint_value, ds_column_type);
  ds_row.emplace_back(ds_column_type);

  // DATA_TYPE
  DSValue ds_data_type = kNullValue;
  auto data_type_status = GetSQLDataType(proc_column.type_name, false);
  if (!data_type_status) {
    return data_type_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> data_type = data_type_status.GetValue();
  if (data_type.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*data_type),
                                   ds_data_type);
  }
  ds_row.emplace_back(ds_data_type);

  // TYPE_NAME
  DSValue ds_type_name = kNullValue;
  auto type_status = GetTypeDescription(proc_column.type_name);
  if (!type_status) {
    return type_status.GetStatusRecord();
  }
  std::string type_name = *type_status;
  if (!type_name.empty()) {
    StringToDSValue(type_name, ds_type_name);
  }
  ds_row.emplace_back(ds_type_name);

  auto fixed_col_status = GetFixedColumnMetadataa(proc_column.type_name);
  if (!fixed_col_status.Ok()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to retrieve fixed column metadata"};
  }
  FixedColumnMetadata fixed_column_metadata = *fixed_col_status;

  // COLUMN_SIZE
  DSValue ds_col_size = kNullValue;
  if (fixed_column_metadata.precision.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.precision.value_or(0)),
        ds_col_size);
  }
  ds_row.emplace_back(ds_col_size);

  // BUFFER_LENGTH
  DSValue ds_buf_len = kNullValue;
  if (fixed_column_metadata.buf_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.buf_len.value_or(0)),
        ds_buf_len);
  }
  ds_row.emplace_back(ds_buf_len);

  // DECIMAL_DIGITS
  DSValue ds_dec_digits = kNullValue;
  if (fixed_column_metadata.scale.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.scale.value_or(0)),
        ds_dec_digits);
  }
  ds_row.emplace_back(ds_dec_digits);

  // NUM_PREC_RADIX
  DSValue ds_radix = kNullValue;
  if (fixed_column_metadata.radix.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.radix.value_or(0)),
        ds_radix);
  }
  ds_row.emplace_back(ds_radix);

  // NULLABLE
  DSValue ds_nullable;
  if (proc_column.nullable == "YES") {
    ArithmeticToDSValue<SQLBIGINT>(1, ds_nullable);
  } else {
    ArithmeticToDSValue<SQLBIGINT>(0, ds_nullable);
  }
  ds_row.emplace_back(ds_nullable);

  // REMARKS
  DSValue ds_remarks = kNullValue;
  ds_row.emplace_back(ds_remarks);

  // COLUMN_DEF
  DSValue column_def = kNullValue;
  ds_row.emplace_back(column_def);

  // SQL_DATA_TYPE
  DSValue ds_sql_data_type = kNullValue;
  optional<SQLSMALLINT> sql_data_type;
  if (data_type.has_value()) {
    auto sql_data_type_status = GetSQLDataType(*data_type);
    if (!sql_data_type_status) {
      return sql_data_type_status.GetStatusRecord();
    }
    sql_data_type = *sql_data_type_status;
    if (sql_data_type.has_value()) {
      ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_data_type),
                                     ds_sql_data_type);
    }
  }
  ds_row.emplace_back(ds_sql_data_type);

  // SQL_DATETIME_SUB
  DSValue ds_sql_datetime_sub = kNullValue;
  if (sql_data_type.has_value() && data_type.has_value()) {
    auto sql_data_time_sub_status =
        GetSQLDateTimeSub(*sql_data_type, *data_type);
    if (!sql_data_time_sub_status) {
      return sql_data_time_sub_status.GetStatusRecord();
    }
    optional<SQLSMALLINT> sql_date_time_sub = *sql_data_time_sub_status;
    if (sql_date_time_sub.has_value()) {
      ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_date_time_sub),
                                     ds_sql_datetime_sub);
    }
  }
  ds_row.emplace_back(ds_sql_datetime_sub);

  // CHAR_OCTET_LENGTH
  DSValue ds_char_octet_len = kNullValue;
  if (proc_column.column_type == "OUT") {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(16384),
                                   ds_char_octet_len);
  } else if (fixed_column_metadata.char_octet_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(
            fixed_column_metadata.char_octet_len.value_or(0)),
        ds_char_octet_len);
  }
  ds_row.emplace_back(ds_char_octet_len);

  // ORDINAL_POSITION
  DSValue ds_ord_pos = kNullValue;
  if (std::stoll(proc_column.ordinal_number) < 0) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid ordinal position"};
  }
  ArithmeticToDSValue<SQLBIGINT>(std::stoll(proc_column.ordinal_number),
                                 ds_ord_pos);
  ds_row.emplace_back(ds_ord_pos);

  // IS_NULLABLE
  DSValue ds_is_nullable;
  std::string is_nullable = proc_column.nullable;
  StringToDSValue(is_nullable, ds_is_nullable);
  ds_row.emplace_back(ds_is_nullable);

  return ds_row;
}

StatusRecordOr<Procedure> FetchBQProcedureData(ConnectionHandle& conn_handle,
                                               std::string const& catalog,
                                               std::string const& dataset,
                                               std::string const& proc_name) {
  // Validate input parameters
  if (catalog.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Catalog cannot be empty for BQ Data source"};
  }
  if (dataset.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Dataset cannot be empty for BQ Data source"};
  }
  if (proc_name.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Procedure name cannot be empty for BQ Data source"};
  }

  // Validate connection
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  // Construct the query
  std::string query = "SELECT * FROM `" + catalog + "." + dataset +
                      ".INFORMATION_SCHEMA.PARAMETERS` "
                      "WHERE specific_name = '" +
                      proc_name + "'";

  QueryRequest query_request;
  query_request.set_query(query);

  auto query_result = bq_client->Query(catalog, query_request, {});
  if (!query_result.Ok()) {
    return StatusRecord{SQLStates::k_HY000(), "Failed to fetch procedure data"};
  }

  auto response = query_result.GetValue();
  if (response.rows.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "No procedure data found"};
  }

  Procedure procedure;

  procedure.catalog = catalog;
  procedure.dataset = dataset;
  procedure.procedure_name = proc_name;
  for (auto const& row : response.rows) {
    auto const& columns = row.columns;

    if (columns.size() < 8) {
      return StatusRecord{SQLStates::k_HY000(),
                          "Unexpected column count in the response"};
    }
    procedure.schema.fields.emplace_back(ProcedureFieldSchema{
        columns[0].value,                   // catalog
        columns[1].value,                   // dataset
        columns[2].value,                   // procedure
        columns[3].value,                   // ordinal_number
        columns[6].value,                   // name
        columns[7].value,                   // type_name
        columns[5].is_null ? "NO" : "YES",  // nullable
        columns[4].value                    // column_type
    });
  }
  return procedure;
}

static std::map<std::string, ColumnSchema> const kODBCProcedureColumnsMap = {
    {"PROCEDURE_CAT",
     ColumnSchema{0, BQDataType::kString}},  // Procedure catalog
    {"PROCEDURE_SCHEMA",
     ColumnSchema{1, BQDataType::kString}},  // Procedure schema
    {"PROCEDURE_NAME", ColumnSchema{2, BQDataType::kString}},  // Procedure name
    {"COLUMN_NAME", ColumnSchema{3, BQDataType::kString}},     // Column name
    {"COLUMN_TYPE",
     ColumnSchema{4, BQDataType::kInt64}},  // Column type (input, output, etc.)
    {"DATA_TYPE", ColumnSchema{5, BQDataType::kInt64}},   // SQL data type
    {"TYPE_NAME", ColumnSchema{6, BQDataType::kString}},  // Type name
    {"COLUMN_SIZE",
     ColumnSchema{7, BQDataType::kInt64}},  // Column size (precision)
    {"BUFFER_LENGTH", ColumnSchema{8, BQDataType::kInt64}},   // Buffer length
    {"DECIMAL_DIGITS", ColumnSchema{9, BQDataType::kInt64}},  // Decimal digits
    {"NUM_PREC_RADIX",
     ColumnSchema{10, BQDataType::kInt64}},  // Numeric precision radix
    {"NULLABLE", ColumnSchema{11, BQDataType::kInt64}},  // Nullable flag
    {"REMARKS", ColumnSchema{12, BQDataType::kString}},  // Remarks/comments
    {"COLUMN_DEF",
     ColumnSchema{13, BQDataType::kString}},  // Column default value
    {"SQL_DATA_TYPE",
     ColumnSchema{14,
                  BQDataType::kInt64}},  // SQL data type (same as DATA_TYPE)
    {"SQL_DATETIME_SUB",
     ColumnSchema{15, BQDataType::kInt64}},  // Date/time subtype
    {"CHAR_OCTET_LENGTH",
     ColumnSchema{16, BQDataType::kInt64}},  // Character octet length
    {"ORDINAL_POSITION",
     ColumnSchema{17, BQDataType::kInt64}},  // Ordinal position
    {"IS_NULLABLE",
     ColumnSchema{18, BQDataType::kString}}  // "YES", "NO", or "UNKNOWN"
};

StatusRecordOr<ColumnSchema> GetProcedureColumnSchema(
    std::string const& col_name) {
  auto map_item = kODBCProcedureColumnsMap.find(col_name);
  if (map_item != kODBCProcedureColumnsMap.end()) {
    return map_item->second;
  }
  return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                     "Invalid column name: " + col_name};
}

StatusRecord CreateProcedureResultSetRowSchema(ResultSet& result_set) {
  for (auto const& entry : kODBCProcedureColumnsMap) {
    auto col_schema_status = GetProcedureColumnSchema(entry.first);
    if (!col_schema_status) {
      return col_schema_status.GetStatusRecord();
    }
    result_set.row_schema.emplace_back(*col_schema_status);
  }
  return StatusRecord::Ok();
}

StatusRecordOr<ResultSet> ProcessProcedureResults(
    Procedure const& bq_procedure, std::string const& bq_procedure_column,
    SQLULEN metadata_id) {
  ResultSet result_set;

  auto row_schema_status = CreateProcedureResultSetRowSchema(result_set);
  if (!row_schema_status.ok()) {
    return row_schema_status;
  }

  if (!metadata_id &&
      (bq_procedure_column.empty() || bq_procedure_column == "%")) {
    int ord_pos = 1;
    for (auto const& procedure_field : bq_procedure.schema.fields) {
      auto ds_row_status = CreateProcedureResultSetDSRow(procedure_field);
      if (!ds_row_status) {
        return ds_row_status.GetStatusRecord();
      }
      result_set.rows.emplace_back(*ds_row_status);
    }
  } else {
    int ord_pos = 1;
    for (auto const& procedure_field : bq_procedure.schema.fields) {
      std::regex column_pattern = BuildRegex(bq_procedure_column, metadata_id);
      if (std::regex_match(procedure_field.name, column_pattern)) {
        auto ds_row_status = CreateProcedureResultSetDSRow(procedure_field);
        if (!ds_row_status) {
          return ds_row_status.GetStatusRecord();
        }
        result_set.rows.emplace_back(*ds_row_status);
        break;
      }
      ord_pos++;
    }
  }
  return result_set;
}

StatusRecordOr<std::vector<Procedure>> FetchBQProceduresData(
    ConnectionHandle& conn_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id) {
  std::vector<Procedure> result;
  if (catalog.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Catalog cannot be empty for BQ Data source"};
  }
  if (dataset_pattern.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Dataset pattern cannot be empty for BQ Data source"};
  }
  if (procedure_pattern.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Procedure pattern cannot be empty for BQ Data source"};
  }
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  StatusRecordOr<std::vector<std::string>> datasets_status =
      GetFilteredDatasetIds(*bq_client, catalog, dataset_pattern, metadata_id);
  if (!datasets_status) {
    return datasets_status.GetStatusRecord();
  }

  for (auto const& dataset : *datasets_status) {
    StatusRecordOr<std::vector<FilteredProcedureResponse>> procedure_status =
        GetFilteredProcedures(conn_handle, catalog, dataset, procedure_pattern);
    if (!procedure_status) {
      return procedure_status.GetStatusRecord();
    }

    for (auto const& filtered_proc : *procedure_status) {
      StatusRecordOr<Procedure> bq_procedure_status = FetchBQProcedureData(
          conn_handle, catalog, dataset, filtered_proc.proc_name);
      if (!bq_procedure_status) {
        return bq_procedure_status.GetStatusRecord();
      }
      result.push_back(*bq_procedure_status);
    }
  }
  return result;
}

std::string ConstructProcedureNameWhereClause(
    std::string const& procedures_filter, SQLULEN metadata_id) {
  if (metadata_id == SQL_TRUE) {
    return "LOWER(routine_name) = LOWER(@" + kProcedureNameParam + ")";
  }
  if (procedures_filter != "%") {
    return "routine_name LIKE @" + kProcedureNameParam;
  }
  return "";
}

std::string ConstructProcedureTypeWhereClause(
    std::string procedure_types_filter) {
  Trim(procedure_types_filter);
  if (procedure_types_filter != "%") {
    return "procedure_type IN UNNEST(@{" + kSchemaParam + ")";
  }
  return "";
}

StatusRecordOr<std::vector<FilteredProcedureResponse>> GetFilteredProcedures(
    ConnectionHandle& conn_handle, std::string const& project_id,
    std::string const& dataset_id, std::string const& procedures_filter) {
  std::vector<QueryParameter> named_query_params;
  QueryParameter param;
  param.name = "procedure_name";
  param.parameter_type.type = "STRING";  // Ensure correct type if required
  param.parameter_value.value = procedures_filter;  // Assign value

  named_query_params.push_back(param);

  std::string query = R"(
  SELECT routine_name, routine_schema
  FROM `)" + project_id +
                      "." + dataset_id + R"(.INFORMATION_SCHEMA.ROUTINES`
  WHERE routine_name LIKE @procedure_name
  AND routine_type = 'PROCEDURE'
)";

  // Construct Post Query Request
  auto post_query_request_status = ConstructNamedParametersPostQueryRequest(
      project_id, dataset_id, query, named_query_params);

  if (!post_query_request_status) {
    return post_query_request_status.GetStatusRecord();
  }

  // Ensure Connection Handle is Valid
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(), "Connection lost"};
  }

  // Fetch Data
  auto fetch_status_record_or =
      FetchBQData(conn_handle, *post_query_request_status);
  if (!fetch_status_record_or) {
    return fetch_status_record_or.GetStatusRecord();
  }

  StatusRecordOr<std::vector<RowData>> rows =
      GetRowsResults(*fetch_status_record_or);
  if (!rows) {
    return rows.GetStatusRecord();
  }
  std::vector<FilteredProcedureResponse> procedure_response;
  for (auto const& row : *rows) {
    procedure_response.push_back({row.columns[0].value, row.columns[1].value});
  }

  return procedure_response;
}

StatusRecord ValidateProcedureParameters(const SQLCHAR* catalog_name,
                                         SQLSMALLINT catalog_name_len,
                                         const SQLCHAR* schema_name,
                                         SQLSMALLINT schema_name_len,
                                         const SQLCHAR* procedure_name,
                                         SQLSMALLINT procedure_name_len,
                                         SQLULEN metadata_id) {
  if (catalog_name_len < 0 && catalog_name_len != SQL_NTS) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - catalog length is invalid"};
  }
  if (schema_name_len < 0 && schema_name_len != SQL_NTS) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - schema length is invalid"};
  }
  if (procedure_name_len < 0 && procedure_name_len != SQL_NTS) {
    return StatusRecord{
        SQLStates::k_HY090(),
        "Invalid buffer length - procedure name length is invalid"};
  }
  if (metadata_id == SQL_TRUE) {
    if (!catalog_name) {
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for catalog name"};
    }
    if (!schema_name) {
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for schema name"};
    }
    if (!procedure_name) {
      return StatusRecord{SQLStates::k_HY009(),
                          "Invalid use of NULL pointer for procedure name"};
    }
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
