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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::optional;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

StatusRecordOr<FixedColumnMetadata> GetFixedColumnMetadata(
    std::string const& type, std::uint32_t column_size) {
  FixedColumnMetadata fixed_column_metadata;
  auto ds_type_status = ConvertDSType(type);
  if (!ds_type_status) {
    LOG(ERROR) << "GetFixedColumnMetadata::ConvertDSType:: " << ds_type_status.GetStatusRecord().message;
    return ds_type_status.GetStatusRecord();
  }
  switch (*ds_type_status) {
    case BQDataType::kString:
    case BQDataType::kBytes:
    case BQDataType::kInterval:
    case BQDataType::kStruct:
    case BQDataType::kJson:
    case BQDataType::kGeography: {
      fixed_column_metadata.precision = column_size;
      fixed_column_metadata.buf_len = column_size;
      fixed_column_metadata.char_octet_len = column_size;
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
      break;
    }
    case BQDataType::kRange: {
      fixed_column_metadata.precision = 256;
      fixed_column_metadata.buf_len = 256, fixed_column_metadata.radix = 0;
      fixed_column_metadata.scale = 0;
      fixed_column_metadata.char_octet_len = 256;
      break;
    }
    default: {
      LOG(ERROR) << "GetFixedColumnMetadata:: Unsupported BQ Data Type: " << type;
      return StatusRecord{SQLStates::k_HY000(),
                          "Unsupported BQ Data Type: " + *ds_type_status};
    }
  }
  return fixed_column_metadata;
}

StatusRecordOr<optional<SQLINTEGER>> GetColSize(
    TableFieldSchema const& field_schema, std::uint32_t column_size) {
  optional<SQLINTEGER> result;
  if (field_schema.precision > 0) {
    result = static_cast<SQLINTEGER>(field_schema.precision);
  } else if (field_schema.max_length > 0) {
    result = static_cast<SQLINTEGER>(field_schema.max_length);
  } else {
    auto fixed_col_status =
        GetFixedColumnMetadata(field_schema.type, column_size);
    if (!fixed_col_status) {
      LOG(ERROR) << "GetColSize::GetFixedColumnMetadata:: " << fixed_col_status.GetStatusRecord().message;
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    if (fixed_column_metadata.precision.has_value()) {
      result = static_cast<SQLINTEGER>(fixed_column_metadata.precision.value());
    }
  }
  return result;
}

StatusRecordOr<optional<SQLINTEGER>> GetBufferLen(
    TableFieldSchema const& field_schema, std::uint32_t column_size) {
  optional<SQLINTEGER> result;
  if (field_schema.max_length > 0) {
    result = static_cast<SQLINTEGER>(field_schema.max_length);
  } else if (field_schema.precision > 0) {
    result = static_cast<SQLINTEGER>(field_schema.precision + 2);
  } else {
    auto fixed_col_status =
        GetFixedColumnMetadata(field_schema.type, column_size);
    if (!fixed_col_status) {
      LOG(ERROR) << "GetBufferLen::GetFixedColumnMetadata:: " << fixed_col_status.GetStatusRecord().message;
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    if (fixed_column_metadata.buf_len.has_value()) {
      result = static_cast<SQLINTEGER>(fixed_column_metadata.buf_len.value());
    }
  }
  return result;
}

StatusRecordOr<optional<SQLINTEGER>> GetCharOctetLen(
    TableFieldSchema const& field_schema, std::uint32_t column_size) {
  optional<SQLINTEGER> result;
  if (field_schema.max_length > 0) {
    result = static_cast<SQLINTEGER>(field_schema.max_length);
  } else {
    auto fixed_col_status =
        GetFixedColumnMetadata(field_schema.type, column_size);
    if (!fixed_col_status) {
      LOG(ERROR) << "GetCharOctetLen::GetFixedColumnMetadata:: " << fixed_col_status.GetStatusRecord().message;
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    if (fixed_column_metadata.char_octet_len.has_value()) {
      result =
          static_cast<SQLINTEGER>(fixed_column_metadata.char_octet_len.value());
    }
  }
  return result;
}

StatusRecordOr<optional<SQLSMALLINT>> GetDecimalDigits(
    TableFieldSchema const& field_schema, std::uint32_t column_size) {
  optional<SQLSMALLINT> result;
  if (field_schema.scale > 0) {
    result = static_cast<SQLSMALLINT>(field_schema.scale);
  } else {
    auto fixed_col_status =
        GetFixedColumnMetadata(field_schema.type, column_size);
    if (!fixed_col_status) {
      LOG(ERROR) << "GetDecimalDigits::GetFixedColumnMetadata:: " << fixed_col_status.GetStatusRecord().message;
      return fixed_col_status.GetStatusRecord();
    }
    FixedColumnMetadata fixed_column_metadata = *fixed_col_status;
    if (fixed_column_metadata.scale.has_value()) {
      result = static_cast<SQLSMALLINT>(fixed_column_metadata.scale.value());
    }
  }
  return result;
}

StatusRecordOr<optional<SQLSMALLINT>> GetRadix(
    TableFieldSchema const& field_schema, std::uint32_t column_size) {
  auto fixed_metadata_status =
      GetFixedColumnMetadata(field_schema.type, column_size);
  if (!fixed_metadata_status) {
    LOG(ERROR) << "GetRadix::GetFixedColumnMetadata:: " << fixed_metadata_status.GetStatusRecord().message;
    return fixed_metadata_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> fixed_radix;
  if (fixed_metadata_status->radix.has_value()) {
    fixed_radix = *(fixed_metadata_status->radix);
  }
  optional<SQLSMALLINT> radix =
      (field_schema.scale > 0 && field_schema.precision > 0) ? 10
      : (field_schema.precision > 0)                         ? 2
                                                             : fixed_radix;
  return radix;
}

StatusRecordOr<optional<SQLSMALLINT>> GetSQLDateTimeSub(
    SQLSMALLINT sql_data_type, SQLSMALLINT data_type) {
  optional<SQLSMALLINT> sql_datetime_sub;
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
        LOG(ERROR) << "GetSQLDateTimeSub:: Invalid data_type for SQL_DATETIME: " << data_type;
        return StatusRecord{
            SQLStates::k_HY000(),
            "Invalid data_type for SQL_DATETIME. Expecting one of "
            "{SQL_TYPE_DATE, SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP}: "};
    }
  }
  return sql_datetime_sub;
}

StatusRecordOr<optional<SQLSMALLINT>> GetSQLDataType(SQLSMALLINT data_type) {
  optional<SQLSMALLINT> sql_data_type;
  if (data_type == SQL_TYPE_DATE || data_type == SQL_TYPE_TIME ||
      data_type == SQL_TYPE_TIMESTAMP) {
    sql_data_type = SQL_DATETIME;
  } else {
    sql_data_type = data_type;
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
    LOG(ERROR) << "ValidateColumnParameters::ValidateTableParameters:: " << status_record.message;
    return status_record;
  }
  if (column_name_len < 0 && column_name_len != SQL_NTS) {
    LOG(ERROR) << "ValidateColumnParameters:: Invalid buffer length for column name.";
    return StatusRecord{
        SQLStates::k_HY090(),
        "Invalid buffer length - column name length is invalid"};
  }
  // Validate SQLColumns specific parameters.

  if (IsSearchPatternArgument(reinterpret_cast<char const*>(catalog_name))) {
    LOG(ERROR) << "ValidateColumnParameters:: Catalog name cannot be a search pattern.";
    return StatusRecord{SQLStates::k_HY090(),
                        "Catalog name cannot be a search pattern"};
  }
  return StatusRecord::Ok();
}

StatusRecordOr<std::string> GetTypeDescription(
    std::string const& field_schema_type) {
  auto type_status = ConvertDSType(field_schema_type);
  if (!type_status) {
    return type_status.GetStatusRecord();
  }
  switch (*type_status) {
    case BQDataType::kInt64: {
      return std::string("INT64");
    }
    case BQDataType::kBool: {
      return std::string("BOOL");
    }
    default: {
      return field_schema_type;
    }
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
