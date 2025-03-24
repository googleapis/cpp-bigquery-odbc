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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_tables.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::optional;
using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Table;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableMetadataView;
using ::google::cloud::odbc_bigquery_client_interface::TableFilter;
using google::cloud::odbc_bq_driver_internal::GetSQLDataType;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

std::string const kBaseTableType = "BASE TABLE";
std::string const kBaseProcedureType = "BASE PROCEDURE";

StatusRecord CreateResultSetRowSchema(ResultSet& result_set) {
  for (auto const& entry : kODBCColumnsMap) {
    auto col_schema_status = GetColumnSchema(entry.first);
    if (!col_schema_status) {
      return col_schema_status.GetStatusRecord();
    }
    result_set.row_schema.emplace_back(*col_schema_status);
  }
  return StatusRecord::Ok();
}

StatusRecordOr<DSRow> CreateResultSetDSRow(std::string const& catalog,
                                           std::string const& dataset,
                                           std::string const& table,
                                           TableFieldSchema const& field_schema,
                                           SQLSMALLINT field_pos) {
  DSRow ds_row;

  // TABLE_CAT
  DSValue ds_table_cat = kNullValue;
  if (!catalog.empty()) {
    StringToDSValue(catalog, ds_table_cat);
  }
  ds_row.emplace_back(ds_table_cat);

  // TABLE_SCHEMA
  DSValue ds_table_schema = kNullValue;
  if (!dataset.empty()) {
    StringToDSValue(dataset, ds_table_schema);
  }
  ds_row.emplace_back(ds_table_schema);

  // TABLE_NAME
  DSValue ds_table_name = kNullValue;
  if (!table.empty()) {
    StringToDSValue(table, ds_table_name);
  }
  ds_row.emplace_back(ds_table_name);

  // COLUMN_NAME
  DSValue ds_column_name = kNullValue;
  if (!field_schema.name.empty()) {
    StringToDSValue(field_schema.name, ds_column_name);
  }
  ds_row.emplace_back(ds_column_name);

  // DATA_TYPE
  DSValue ds_data_type = kNullValue;
  auto data_type_status = GetSQLDataType(field_schema.type, false);
  if (!data_type_status) {
    return data_type_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> data_type = *data_type_status;
  if (data_type.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*data_type),
                                   ds_data_type);
  }
  ds_row.emplace_back(ds_data_type);

  // TYPE_NAME
  DSValue ds_type_name = kNullValue;
  auto type_status = GetTypeDescription(field_schema.type);
  if (!type_status) {
    return type_status.GetStatusRecord();
  }
  std::string type_name = *type_status;
  if (!type_name.empty()) {
    StringToDSValue(type_name, ds_type_name);
  }
  ds_row.emplace_back(ds_type_name);

  // COLUMN_SIZE
  DSValue ds_col_size = kNullValue;
  auto col_size_status = GetColSize(field_schema);
  if (!col_size_status) {
    return col_size_status.GetStatusRecord();
  }
  optional<SQLINTEGER> col_size = *col_size_status;
  if (col_size.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*col_size),
                                   ds_col_size);
  }
  ds_row.emplace_back(ds_col_size);

  // BUFFER_LENGTH
  DSValue ds_buf_len = kNullValue;
  auto buf_len_status = GetBufferLen(field_schema);
  if (!buf_len_status) {
    return buf_len_status.GetStatusRecord();
  }
  optional<SQLINTEGER> buf_len = *buf_len_status;
  if (buf_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*buf_len),
                                   ds_buf_len);
  }
  ds_row.emplace_back(ds_buf_len);

  // DECIMAL_DIGITS
  DSValue ds_dec_digits = kNullValue;
  auto dec_digits_status = GetDecimalDigits(field_schema);
  if (!dec_digits_status) {
    return dec_digits_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> dec_digits = *dec_digits_status;
  if (dec_digits.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*dec_digits),
                                   ds_dec_digits);
  }
  ds_row.emplace_back(ds_dec_digits);

  // NUM_PREC_RADIX
  DSValue ds_radix = kNullValue;
  auto radix_status = GetRadix(field_schema);
  if (!radix_status) {
    return radix_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> radix = *radix_status;
  if (radix.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*radix), ds_radix);
  }
  ds_row.emplace_back(ds_radix);

  // NULLABLE
  DSValue ds_nullable;
  SQLSMALLINT nullable =
      (field_schema.mode == "REQUIRED") ? SQL_NO_NULLS : SQL_NULLABLE;
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(nullable), ds_nullable);
  ds_row.emplace_back(ds_nullable);

  // REMARKS
  DSValue ds_description = kNullValue;
  if (!field_schema.type.empty()) {
    StringToDSValue(field_schema.type, ds_description);
  }
  ds_row.emplace_back(ds_description);

  // COLUMN_DEF
  DSValue column_def = kNullValue;
  if (!field_schema.default_value_expression.empty()) {
    StringToDSValue(field_schema.default_value_expression, column_def);
  }
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
  auto char_octet_len_status = GetCharOctetLen(field_schema);
  if (!char_octet_len_status) {
    return char_octet_len_status.GetStatusRecord();
  }
  optional<SQLINTEGER> char_octet_len = *char_octet_len_status;
  if (char_octet_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*char_octet_len),
                                   ds_char_octet_len);
  }
  ds_row.emplace_back(ds_char_octet_len);

  // ORDINAL_POSITION
  DSValue ds_ord_pos = kNullValue;
  // field_pos is always >= 0 any other value is error.
  if (field_pos < 0) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid ordinal position"};
  }
  ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(field_pos), ds_ord_pos);
  ds_row.emplace_back(ds_ord_pos);

  // IS_NULLABLE
  DSValue ds_is_nullable;
  std::string is_nullable = (nullable) ? "YES" : "NO";
  StringToDSValue(is_nullable, ds_is_nullable);
  ds_row.emplace_back(ds_is_nullable);

  return ds_row;
}

StatusRecordOr<FixedColumnMetadata> GetFixedColumnMetadataa(std::string type) {
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
      fixed_column_metadata.radix = 2;
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

  // // COLUMN_TYPE
  // std::cout<<"data"<<proc_column.column_type<<std::endl;
  // DSValue ds_column_type;
  // ArithmeticToDSValue<SQLSMALLINT>(proc_column.column_type, ds_column_type);
  // ds_row.emplace_back(ds_column_type);
  // SQLBIGINT a = DSValueToArithmetic<SQLSMALLINT>(ds_column_type);
  // std::cout<<"data"<<a<<std::endl;

  // DATA_TYPE
  DSValue ds_data_type = kNullValue;
  auto data_type_status = GetSQLDataType(proc_column.type_name, false);
  if (!data_type_status) {
    return data_type_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> data_type = data_type_status.GetValue();
  std::cout << "DATATYPE:" << data_type.value() << std::endl;
  if (data_type.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*data_type),
                                   ds_data_type);
  }
  ds_row.emplace_back(ds_data_type);

  // TYPE_NAME
  // DSValue ds_type_name = kNullValue;
  // if (!proc_column.type_name.empty()) {
  //   StringToDSValue(proc_column.type_name, ds_type_name);
  // }
  // ds_row.emplace_back(ds_type_name);
  // TYPE_NAME
  std::cout << "type_name" << proc_column.type_name << std::endl;
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
  // DSValue ds_col_size = kNullValue;
  // optional<SQLINTEGER> col_size = *fixed_column_metadata.precision;
  // if (col_size.has_value()) {
  //   ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*col_size),
  //                                  ds_col_size);
  // }
  // ds_row.emplace_back(ds_col_size);
  // // COLUMN_SIZE
  // DSValue ds_col_size = kNullValue;
  // if (fixed_column_metadata.precision.has_value()) {
  //   ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(fixed_column_metadata.precision.value_or(0)),
  //   ds_col_size);
  // }
  // ds_row.emplace_back(ds_col_size);

  // // BUFFER_LENGTH
  // DSValue ds_buf_len = kNullValue;
  // if (fixed_column_metadata.buf_len.has_value()) {
  //   ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(fixed_column_metadata.buf_len.value_or(0)),
  //   ds_buf_len);
  // }
  // ds_row.emplace_back(ds_buf_len);

  // // DECIMAL_DIGITS
  // DSValue ds_dec_digits = kNullValue;
  // if (fixed_column_metadata.scale.has_value()) {
  //   ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(fixed_column_metadata.scale.value_or(0)),
  //   ds_dec_digits);
  // }
  // ds_row.emplace_back(ds_dec_digits);

  // // NUM_PREC_RADIX
  // DSValue ds_radix = kNullValue;
  // if (fixed_column_metadata.radix.has_value()) {
  //   ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(fixed_column_metadata.radix.value_or(0)),
  //   ds_radix);
  // }
  // ds_row.emplace_back(ds_radix);

  // // NULLABLE
  // DSValue ds_nullable;
  // ArithmeticToDSValue<SQLBIGINT>(std::stoll(proc_column.nullable),
  // ds_nullable); ds_row.emplace_back(ds_nullable);

  // // // REMARKS
  // // DSValue ds_remarks = kNullValue;
  // // if (!proc_column.remarks.empty()) {
  // //   StringToDSValue(proc_column.remarks, ds_remarks);
  // // }
  // // ds_row.emplace_back(ds_remarks);

  // // COLUMN_DEF
  // DSValue column_def = kNullValue;
  // if (!proc_column.) {
  //   StringToDSValue(proc_column.column_def, column_def);
  // }
  // ds_row.emplace_back(column_def);

  // // SQL_DATA_TYPE
  // DSValue ds_sql_data_type = kNullValue;
  // optional<SQLSMALLINT> sql_data_type;
  // if (data_type.has_value()) {
  //   auto sql_data_type_status = GetSQLDataType(*data_type);
  //   if (!sql_data_type_status) {
  //     return sql_data_type_status.GetStatusRecord();
  //   }
  //   sql_data_type = *sql_data_type_status;
  //   if (sql_data_type.has_value()) {
  //     ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_data_type),
  //     ds_sql_data_type);
  //   }
  // }
  // ds_row.emplace_back(ds_sql_data_type);

  // // SQL_DATETIME_SUB
  // DSValue ds_sql_datetime_sub = kNullValue;
  // if (sql_data_type.has_value() && data_type.has_value()) {
  //   auto sql_data_time_sub_status = GetSQLDateTimeSub(*sql_data_type,
  //   *data_type); if (!sql_data_time_sub_status) {
  //     return sql_data_time_sub_status.GetStatusRecord();
  //   }
  //   optional<SQLSMALLINT> sql_date_time_sub = *sql_data_time_sub_status;
  //   if (sql_date_time_sub.has_value()) {
  //     ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_date_time_sub),
  //     ds_sql_datetime_sub);
  //   }
  // }
  // ds_row.emplace_back(ds_sql_datetime_sub);

  // // CHAR_OCTET_LENGTH
  // DSValue ds_char_octet_len = kNullValue;
  // if (fixed_column_metadata.char_octet_len.has_value()) {
  //   ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(fixed_column_metadata.char_octet_len.value_or(0)),
  //   ds_char_octet_len);
  // }
  // ds_row.emplace_back(ds_char_octet_len);

  // ORDINAL_POSITION
  // DSValue ds_ord_pos = kNullValue;
  // if (std::stoll(proc_column.ordinal_number) < 0) {
  //   return StatusRecord{SQLStates::k_HY000(), "Invalid ordinal position"};
  // }
  // ArithmeticToDSValue<SQLBIGINT>(std::stoll(proc_column.ordinal_number),
  // ds_ord_pos); ds_row.emplace_back(ds_ord_pos);

  // IS_NULLABLE
  // DSValue ds_is_nullable;
  // std::string is_nullable = proc_column.nullable ;
  // StringToDSValue(is_nullable, ds_is_nullable);
  // ds_row.emplace_back(ds_is_nullable);

  return ds_row;
}

StatusRecordOr<Table> FetchBQTableData(ConnectionHandle& conn_handle,
                                       std::string const& catalog,
                                       std::string const& dataset,
                                       std::string const& table) {
  StatusRecordOr<Table> result;
  // Validate the data source parameters for the BQ call.
  if (catalog.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Catalog cannot be empty for BQ Data source"};
  }
  if (dataset.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Dataset cannot be empty for BQ Data source"};
  }
  if (table.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Table cannot be empty for BQ Data source"};
  }
  // Validate the  connection handle.
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
  Options options;
  TableFilter filter{{}, TableMetadataView::Full()};
  return bq_client->GetTable(catalog, dataset, table, filter, options);
}

// void FetchBQProcedureData(
//   ConnectionHandle& conn_handle, std::string const& catalog, std::string
//   const& dataset, std::string const& proc_name) {

// auto bq_client = conn_handle.GetClient();
// if (!bq_client) {
//   std::cout<<"CLIENT EMPTY"<<std::endl;
// }

// std::string query = "SELECT * "
//                     "FROM `" + catalog + "." + dataset +
//                     ".INFORMATION_SCHEMA.PARAMETERS` " "WHERE specific_name =
//                     '" + proc_name + "'";

// std::cout<<"QUERY:"<<query<<std::endl;
// QueryRequest query_request;
// query_request.set_query(query);

// // Assuming project_id is available in conn_handle
// auto query_result = bq_client->Query(catalog, query_request, {});

// if (!query_result.Ok()) {
//   std::cout<<"query result EMPTY"<<std::endl;

// }
// std::cout<<"QUERY DATA"<<query_result.GetValue().DebugString("")<<std::endl;
// std::cout<<"DONE"<<std::endl;
// auto response = query_result.GetValue();

// std::vector<SQLProcedureColumn> procedureColumns;
// int ordinal_counter = 1;
// for (const auto& row : response.rows) {
//   const auto& columns = row.columns;
//   SQLProcedureColumn column;

//   column.procedure_catalog = columns[0].value;
//   column.procedure_schema = columns[1].value;
//   column.procedure_name =  columns[2].value;
//   column.column_name = columns[6].value;
//   column.type_name = columns[7].value;
//   auto res = GetSQLDataType(column.type_name,false);
//   column.data_type= res.GetValue();
//   column.sql_data_type = GetSQLDataType(column.data_type).GetValue().value();
//   column.nullable=columns[0].is_null? 0 : 1;
//   column.is_nullable=columns[0].is_null? "NO" : "YES";
//   if (columns[2].value == "OUT") {
//     column.column_type = 4;
// } else {
//     column.column_type = 1;
// }
//    auto fixed_col_status = GetFixedColumnMetadataa(column.type_name);
//    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
//      int64_t value = fixed_column_metadata.radix.value();
//       column.num_prec_radix = static_cast<SQLINTEGER>(value);
// if (fixed_column_metadata.char_octet_len.has_value()) {
//   int64_t value = fixed_column_metadata.char_octet_len.value();
//   column.char_octet_length = static_cast<int>(value);
// } else {
//   column.char_octet_length = 0;
// }
// if (fixed_column_metadata.buf_len.has_value()) {
//   int64_t value = fixed_column_metadata.buf_len.value();
//   column.buffer_length = static_cast<int>(value);
// } else {
//   column.buffer_length = 0;
// }
// if (fixed_column_metadata.precision.has_value()) {
//   int64_t value = fixed_column_metadata.precision.value();
//   column.column_size = static_cast<int>(value);
// } else {
//   column.column_size = 0;
// }
// if (fixed_column_metadata.scale.has_value()) {
//   column.decimal_digits =
//   static_cast<SQLSMALLINT>(fixed_column_metadata.scale.value());
// }
// auto sql_datetime_sub_result = GetSQLDateTimeSub(column.sql_data_type,
// column.data_type); if (!sql_datetime_sub_result.Ok()) {
//     column.sql_datetime_sub = 0;
// } else {
//     auto sql_datetime_sub_optional = sql_datetime_sub_result.GetValue();
//     column.sql_datetime_sub = sql_datetime_sub_optional.value_or(0);
// }
// column.ordinal_position = std::stoi(columns[3].value);
// procedureColumns.push_back(column);
// }

// // Output the parsed result
// std::cout << "SQLProcedureColumns Output:\n";
// for (const auto& column : procedureColumns) {
//   std::cout << "Procedure Catalog: " << column.procedure_catalog << "\n"
//             << "Procedure Schema: " << column.procedure_schema << "\n"
//             << "Procedure Name: " << column.procedure_name << "\n";
//             std::cout << "  Column Name: " << column.column_name << "\n";
//             std::cout << "  Column Type: " << column.column_type<< "\n";
//             std::cout << "  Data Type: " << column.data_type<< "\n";
//             std::cout << "  Type Name: " << column.type_name << "\n";
//             std::cout << "  Column Size: " << column.column_size << "\n";
//             std::cout << "  Buffer Length: " << column.buffer_length << "\n";
//             std::cout << "  Decimal Digits: " << column.decimal_digits <<
//             "\n"; std::cout << "  Numeric Precision Radix: " <<
//             column.num_prec_radix << "\n"; std::cout << "  Nullable: " <<
//             column.nullable << "\n"; std::cout << "  Remarks: " <<
//             column.remarks << "\n"; std::cout << "  Column Default: " <<
//             column.column_def<< "\n"; std::cout << "  SQL Data Type: " <<
//             column.sql_data_type << "\n"; std::cout << "  SQL DateTime Sub: "
//             << column.sql_datetime_sub << "\n"; std::cout << "  Character
//             Octet Length: " << column.char_octet_length << "\n"; std::cout <<
//             "  Ordinal Position: " << column.ordinal_position << "\n";
//             std::cout << "  Is Nullable: " << column.is_nullable<< "\n";
//             std::cout << "------------------------------------\n";
// }
// //return procedureColumns;
// }

std::string SQLProcedureColumn::DebugString(absl::string_view name,
                                            TracingOptions const& options,
                                            int indent) const {
  nlohmann::json json_obj = {{"procedure_catalog", procedure_catalog},
                             {"procedure_schema", procedure_schema},
                             {"procedure_name", procedure_name},
                             {"column_name", column_name},
                             {"column_type", column_type},
                             {"data_type", data_type},
                             {"type_name", type_name},
                             {"column_size", column_size},
                             {"buffer_length", buffer_length},
                             {"decimal_digits", decimal_digits},
                             {"num_prec_radix", num_prec_radix},
                             {"nullable", nullable},
                             {"remarks", remarks},
                             {"column_def", column_def},
                             {"sql_data_type", sql_data_type},
                             {"sql_datetime_sub", sql_datetime_sub},
                             {"char_octet_length", char_octet_length},
                             {"ordinal_position", ordinal_position},
                             {"is_nullable", is_nullable}};

  return json_obj.dump(indent);  // Pretty-print JSON with indentation
}

// StatusRecordOr<std::vector<SQLProcedureColumn>> FetchBQProcedureData(
//   ConnectionHandle& conn_handle, std::string const& catalog,
//   std::string const& dataset, std::string const& proc_name) {

// // Validate input parameters
// if (catalog.empty()) {
//   return StatusRecord{SQLStates::k_HY000(), "Catalog cannot be empty for BQ
//   Data source"};
// }
// if (dataset.empty()) {
//   return StatusRecord{SQLStates::k_HY000(), "Dataset cannot be empty for BQ
//   Data source"};
// }
// if (proc_name.empty()) {
//   return StatusRecord{SQLStates::k_HY000(), "Procedure name cannot be empty
//   for BQ Data source"};
// }

// // Validate connection
// if (!conn_handle.IsConnected()) {
//   return StatusRecord{SQLStates::k_08S01(), "Connection to the data source is
//   broken"};
// }

// auto bq_client = conn_handle.GetClient();
// if (!bq_client) {
//   return StatusRecord{SQLStates::k_HY000(), "Invalid or null BQ Client within
//   the connection handle"};
// }

// // Construct the query
// std::string query =
//     "SELECT * FROM `" + catalog + "." + dataset +
//     ".INFORMATION_SCHEMA.PARAMETERS` " "WHERE specific_name = '" + proc_name
//     + "'";

// QueryRequest query_request;
// query_request.set_query(query);

// auto query_result = bq_client->Query(catalog, query_request, {});
// std::cout<<"QUERY
// RESULT:"<<query_result.GetValue().DebugString("")<<std::endl; if
// (!query_result.Ok()) {
//   return StatusRecord{SQLStates::k_HY000(), "Failed to fetch procedure
//   data"};
// }

// auto response = query_result.GetValue();
// if (response.rows.empty()) {
//   return StatusRecord{SQLStates::k_HY000(), "No procedure data found"};
// }

// std::vector<SQLProcedureColumn> procedureColumns;

// for (const auto& row : response.rows) {
//   const auto& columns = row.columns;

//   if (columns.size() < 8) {
//     return StatusRecord{SQLStates::k_HY000(), "Unexpected column count in the
//     response"};
//   }

//   SQLProcedureColumn column;
//   column.procedure_catalog = columns[0].value;
//   column.procedure_schema = columns[1].value;
//   column.procedure_name = columns[2].value;
//   column.column_name = columns[6].value;
//   column.type_name = columns[7].value;

//   auto sql_data_res = GetSQLDataType(column.type_name, false);
//   if (!sql_data_res.Ok()) {
//     return StatusRecord{SQLStates::k_HY000(), "Invalid SQL data type
//     detected"};
//   }
//   column.data_type = sql_data_res.GetValue();

//   auto sql_type_res = GetSQLDataType(column.data_type);
//   if (!sql_type_res.Ok()) {
//     return StatusRecord{SQLStates::k_HY000(), "Failed to fetch SQL data
//     type"};
//   }
//   column.sql_data_type = sql_type_res.GetValue().value();

//   column.nullable = columns[5].is_null ? 0 : 1;
//   column.is_nullable = columns[5].is_null ? "NO" : "YES";

//   column.column_type = (columns[4].value == "OUT") ? 4 : 1;

//   auto fixed_col_status = GetFixedColumnMetadataa(column.type_name);
//   if (!fixed_col_status.Ok()) {
//     return StatusRecord{SQLStates::k_HY000(), "Failed to retrieve fixed
//     column metadata"};
//   }

//   FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
//   column.num_prec_radix =
//   static_cast<SQLINTEGER>(fixed_column_metadata.radix.value_or(0));
//   column.char_octet_length =
//   fixed_column_metadata.char_octet_len.value_or(0); column.buffer_length =
//   fixed_column_metadata.buf_len.value_or(0); column.column_size =
//   fixed_column_metadata.precision.value_or(0); column.decimal_digits =
//   fixed_column_metadata.scale.value_or(0);

//   auto sql_datetime_sub_result = GetSQLDateTimeSub(column.sql_data_type,
//   column.data_type); column.sql_datetime_sub = sql_datetime_sub_result.Ok() ?
//   sql_datetime_sub_result.GetValue().value_or(0) : 0;

//   try {
//     column.ordinal_position = std::stoi(columns[3].value);
//   } catch (const std::exception&) {
//     return StatusRecord{SQLStates::k_HY000(), "Failed to parse ordinal
//     position"};
//   }

//   procedureColumns.push_back(column);
// }
// std::cout << "DAATA: " << std::endl;
// for (const auto& col : procedureColumns) {
//   std::cout << col.DebugString("") << std::endl;
// }
// return procedureColumns;
// }

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
        columns[5].is_null ? "YES" : "NO",  // nullable
        (columns[4].value == "OUT")
            ? static_cast<SQLSMALLINT>(4)
            : static_cast<SQLSMALLINT>(1)  // column_type
    });
  }
  return procedure;
}

StatusRecordOr<ResultSet> ProcessTableResults(
    Table const& bq_table, std::string const& bq_table_column,
    SQLULEN metadata_id) {
  ResultSet result_set;
  // Populate Row Schema for the ResultSet.
  auto row_schema_status = CreateResultSetRowSchema(result_set);
  if (!row_schema_status.ok()) {
    return row_schema_status;
  }
  // Now populate data for the resultset from the BQ Table.
  if (!metadata_id && (bq_table_column.empty() || bq_table_column == "%")) {
    // Puts all columns in the result set.
    // Each table_field_schema entry below represents
    // a ResultSetRow that has all the ODBC fields as mentioned in
    // CreateResultSetRowSchema. In this usecase, number of resultset rows =
    // number of table_field_schema entries.
    int ord_pos = 1;
    for (TableFieldSchema const& table_field_schema : bq_table.schema.fields) {
      auto ds_row_status = CreateResultSetDSRow(
          bq_table.table_reference.project_id,
          bq_table.table_reference.dataset_id,
          bq_table.table_reference.table_id, table_field_schema, ord_pos++);
      if (!ds_row_status) {
        return ds_row_status.GetStatusRecord();
      }
      result_set.rows.emplace_back(*ds_row_status);
    }
  } else {
    // Put only the specific column metadata in the resultset. In this
    // usecase, number of rows in the resultset = 1.
    int ord_pos = 1;
    for (TableFieldSchema const& table_field_schema : bq_table.schema.fields) {
      // bq_table_column could contain a search pattern character so do a regex
      // match.
      std::regex column_pattern = BuildRegex(bq_table_column, metadata_id);
      if (std::regex_match(table_field_schema.name, column_pattern)) {
        auto ds_row_status = CreateResultSetDSRow(
            bq_table.table_reference.project_id,
            bq_table.table_reference.dataset_id,
            bq_table.table_reference.table_id, table_field_schema, ord_pos);
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

StatusRecordOr<ResultSet> ProcessProcedureResults(
    Procedure const& bq_procedure, std::string const& bq_procedure_column,
    SQLULEN metadata_id) {
  ResultSet result_set;

  auto row_schema_status = CreateResultSetRowSchema(result_set);
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

StatusRecordOr<std::vector<Table>> FetchBQTablesData(
    ConnectionHandle& conn_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& table_pattern,
    SQLULEN metadata_id) {
  std::vector<Table> result;
  if (catalog.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Catalog cannot be empty for BQ Data source"};
  }
  if (dataset_pattern.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Dataset pattern cannot be empty for BQ Data source"};
  }
  if (table_pattern.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Table pattern cannot be empty for BQ Data source"};
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
  // Get Datasets based on search pattern in the dataset argument
  StatusRecordOr<std::vector<std::string>> datasets_status =
      GetFilteredDatasetIds(*bq_client, catalog, dataset_pattern, metadata_id);
  if (!datasets_status) {
    return datasets_status.GetStatusRecord();
  }

  for (auto const& dataset : *datasets_status) {
    // Get all tables matching the table pattern and dataset.
    StatusRecordOr<std::vector<FilteredTableResponse>> tables_status =
        GetFilteredTables(conn_handle, catalog, dataset, table_pattern,
                          kBaseTableType, metadata_id);
    if (!tables_status) {
      return tables_status.GetStatusRecord();
    }
    // Get detailed information from BQ for each table returned.
    for (auto const& filtered_table : *tables_status) {
      StatusRecordOr<Table> bq_table_status = FetchBQTableData(
          conn_handle, catalog, dataset, filtered_table.table_name);
      if (!bq_table_status) {
        return bq_table_status.GetStatusRecord();
      }
      result.push_back(*bq_table_status);
    }
  }
  return result;
}

// StatusRecordOr<std::vector<SQLProcedureColumn>> FetchBQProceduresData(
//     ConnectionHandle& conn_handle, std::string const& catalog,
//     std::string const& dataset_pattern, std::string const& procedure_pattern,
//     SQLULEN metadata_id) {
//   std::cout << "Entering FetchBQProceduresData" << std::endl;
//   std::cout << "Catalog: " << catalog << ", Dataset Pattern: " <<
//   dataset_pattern
//             << ", Procedure Pattern: " << procedure_pattern << std::endl;

//   auto bq_client = conn_handle.GetClient();
//   if (!bq_client) {
//     std::cout << "Bq Client not found" << std::endl;
//     return;
//   }
//   std::vector<SQLProcedureColumn> result;
//   std::cout << "Fetching dataset IDs matching pattern: " << dataset_pattern
//   << std::endl;

//   StatusRecordOr<std::vector<std::string>> datasets_status =
//       GetFilteredDatasetIds(*bq_client, catalog, dataset_pattern,
//       metadata_id);

//   if (!datasets_status) {
//     std::cout << "DATASET not found" << std::endl;
//     return;
//   }

//   std::cout << "Found " << datasets_status->size() << " datasets." <<
//   std::endl; for (auto const& dataset : *datasets_status) {
//     std::cout << "Processing dataset: " << dataset << std::endl;

//     StatusRecordOr<std::vector<FilteredProcedureResponse>> procedure_status =
//         GetFilteredProcedures(conn_handle, catalog, dataset,
//         procedure_pattern,
//                               kBaseProcedureType, metadata_id);

//     if (!procedure_status) {
//       std::cout << "No procedures found for dataset: " << dataset <<
//       std::endl; continue;
//     }

//     std::cout << "Found " << procedure_status->size() << " procedures in
//     dataset: "
//               << dataset << std::endl;

//     for (auto const& filtered_proc : *procedure_status) {
//       std::cout << "Fetching procedure: " << filtered_proc.proc_name <<
//       std::endl; StatusRecordOr<SQLProcedureColumn> bq_procedure_status =
//       FetchBQProcedureData(
//             conn_handle, catalog,dataset,filtered_proc.proc_name);
//     if (!bq_procedure_status) {
//       //return bq_table_status.GetStatusRecord();
//       std::cout<<"error in FetchBQProcedureData "<<std::endl;
//     }
//    result.push_back(*bq_procedure_status);
//     }
//   }
//  return result;
// }

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
        GetFilteredProcedures(conn_handle, catalog, dataset, procedure_pattern,
                              kBaseProcedureType, metadata_id);
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

}  // namespace google::cloud::odbc_bq_driver_internal
