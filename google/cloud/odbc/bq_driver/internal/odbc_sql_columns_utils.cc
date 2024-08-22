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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

StatusRecordOr<FixedColumnMetadata> GetFixedColumnMetadata(
    TableFieldSchema const& field_schema) {
  FixedColumnMetadata fixed_column_metadata;
  auto ds_type_status = ConvertDSType(field_schema.type);
  if (!ds_type_status) {
    return ds_type_status.GetStatusRecord();
  }
  switch (*ds_type_status) {
    case BQDataType::kString:
    case BQDataType::kBytes:
    case BQDataType::kInterval: {
      fixed_column_metadata.precision = 16384;
      fixed_column_metadata.buf_len = 16384;
      fixed_column_metadata.scale = SQL_NULL_DATA;
      fixed_column_metadata.char_octet_len = 16384;
      break;
    }
    case BQDataType::kInt64: {
      fixed_column_metadata.precision = 19;
      fixed_column_metadata.buf_len = 20;
      fixed_column_metadata.scale = 0;
      fixed_column_metadata.char_octet_len = SQL_NULL_DATA;
      break;
    }
    case BQDataType::kBool: {
      fixed_column_metadata.precision = 1;
      fixed_column_metadata.buf_len = 1;
      fixed_column_metadata.scale = SQL_NULL_DATA;
      fixed_column_metadata.char_octet_len = SQL_NULL_DATA;
      break;
    }
    case BQDataType::kTime: {
      fixed_column_metadata.precision = 15;
      fixed_column_metadata.buf_len = 6;
      fixed_column_metadata.scale = 6;
      fixed_column_metadata.char_octet_len = SQL_NULL_DATA;
      break;
    }
    case BQDataType::kDate: {
      fixed_column_metadata.precision = 10;
      fixed_column_metadata.buf_len = 6;
      fixed_column_metadata.scale = SQL_NULL_DATA;
      fixed_column_metadata.char_octet_len = SQL_NULL_DATA;
      break;
    }
    case BQDataType::kTimeStamp:
    case BQDataType::kDatetime: {
      fixed_column_metadata.precision = 26;
      fixed_column_metadata.buf_len = 16;
      fixed_column_metadata.scale = 6;
      fixed_column_metadata.char_octet_len = SQL_NULL_DATA;
      break;
    }
    case BQDataType::kNumeric:
    case BQDataType::kBigNumeric: {
      fixed_column_metadata.precision = 38;
      fixed_column_metadata.buf_len = 40;
      fixed_column_metadata.scale = 9;
      fixed_column_metadata.char_octet_len = SQL_NULL_DATA;
      break;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(),
                          "Unsupported BQ Data Type: " + *ds_type_status};
    }
  }
  return fixed_column_metadata;
}

StatusRecordOr<SQLINTEGER> GetColSize(TableFieldSchema const& field_schema) {
  SQLINTEGER result;
  if (field_schema.precision > 0) {
    result = static_cast<SQLINTEGER>(field_schema.precision);
  } else {
    auto fixed_col_status = GetFixedColumnMetadata(field_schema);
    if (!fixed_col_status) {
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    result = static_cast<SQLINTEGER>(fixed_column_metadata.precision);
  }
  return result;
}

StatusRecordOr<SQLINTEGER> GetBufferLen(TableFieldSchema const& field_schema) {
  SQLINTEGER result;
  if (field_schema.max_length > 0) {
    result = static_cast<SQLINTEGER>(field_schema.max_length);
  } else {
    auto fixed_col_status = GetFixedColumnMetadata(field_schema);
    if (!fixed_col_status) {
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    result = static_cast<SQLINTEGER>(fixed_column_metadata.buf_len);
  }
  return result;
}

StatusRecordOr<SQLINTEGER> GetCharOctetLen(
    TableFieldSchema const& field_schema) {
  SQLINTEGER result;
  if (field_schema.max_length > 0) {
    result = static_cast<SQLINTEGER>(field_schema.max_length);
  } else {
    auto fixed_col_status = GetFixedColumnMetadata(field_schema);
    if (!fixed_col_status) {
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    result = static_cast<SQLINTEGER>(fixed_column_metadata.char_octet_len);
  }
  return result;
}

StatusRecordOr<SQLSMALLINT> GetDecimalDigits(
    TableFieldSchema const& field_schema) {
  SQLSMALLINT result;
  if (field_schema.scale > 0) {
    result = static_cast<SQLSMALLINT>(field_schema.scale);
  } else {
    auto fixed_col_status = GetFixedColumnMetadata(field_schema);
    if (!fixed_col_status) {
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    result = static_cast<SQLSMALLINT>(fixed_column_metadata.scale);
  }
  return result;
}

StatusRecordOr<SQLSMALLINT> GetRadix(TableFieldSchema const& field_schema) {
  SQLSMALLINT radix = (field_schema.scale >= 0 && field_schema.precision >= 0)
                          ? 10
                          : ((field_schema.precision >= 0) ? 2 : SQL_NULL_DATA);
  return static_cast<SQLSMALLINT>(radix);
}

StatusRecordOr<SQLSMALLINT> GetSQLDateTimeSub(SQLSMALLINT sql_data_type,
                                              SQLSMALLINT data_type) {
  SQLSMALLINT sql_datetime_sub = SQL_NULL_DATA;
  if (sql_data_type == SQL_DATETIME) {
    switch (data_type) {
      case SQL_TYPE_DATE: {
        sql_datetime_sub = SQL_CODE_DATE;
        break;
      }
      case SQL_TYPE_TIME: {
        sql_datetime_sub = SQL_CODE_TIME;
        break;
      }
      case SQL_TYPE_TIMESTAMP: {
        sql_datetime_sub = SQL_CODE_TIMESTAMP;
        break;
      }
      default:
        return StatusRecord{
            SQLStates::k_HY000(),
            "Invalid data_type for SQL_DATETIME. Expecting one of "
            "{SQL_TYPE_DATE, SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP}: "};
    }
  }
  return sql_datetime_sub;
}

StatusRecordOr<SQLSMALLINT> GetSQLDataType(SQLSMALLINT data_type) {
  SQLSMALLINT sql_data_type = data_type;
  if (sql_data_type == SQL_TYPE_DATE || sql_data_type == SQL_TYPE_TIME ||
      sql_data_type == SQL_TYPE_TIMESTAMP) {
    sql_data_type = SQL_DATETIME;
  }
  return sql_data_type;
}

odbc_internal::StatusRecord ValidateColumnParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* table_name, SQLSMALLINT table_name_len,
    const SQLCHAR* /*column_name*/, SQLSMALLINT column_name_len,
    SQLULEN metadata_id) {
  // Validate table and table related parameters.
  auto status_record = ValidateTableParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, table_name,
      table_name_len, metadata_id);
  if (!status_record.ok()) {
    return status_record;
  }
  if (column_name_len < 0 && column_name_len != SQL_NTS) {
    return StatusRecord{
        SQLStates::k_HY090(),
        "Invalid buffer length - column name length is invalid"};
  }
  // Validate SQLColumns specific parameters.

  if (IsSearchPatternArgument(reinterpret_cast<char const*>(catalog_name))) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Catalog name cannot be a search pattern"};
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
