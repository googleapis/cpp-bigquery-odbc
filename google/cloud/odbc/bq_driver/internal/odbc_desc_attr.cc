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

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

void HeaderRecord::CopyHeaderRecordsFrom(HeaderRecord const& header_record) {
  LOG(INFO) << "HeaderRecord::CopyHeaderRecordsFrom:: Copying header records.";
  array_size = header_record.array_size;
  array_status_ptr = header_record.array_status_ptr;
  bind_offset_ptr = header_record.bind_offset_ptr;
  bind_type = header_record.bind_type;
  rows_processed_ptr = header_record.rows_processed_ptr;
}

void DescriptorRecord::SetName(std::string const& val,
                               SQLINTEGER const buffer_len) {
  if (val.empty() || buffer_len == 0) {
    name = "";
    unnamed = SQL_UNNAMED;
  } else {
    if (buffer_len == SQL_NTS ||
        buffer_len >= static_cast<SQLINTEGER>(val.size())) {
      name = val;
    } else {
      name = val.substr(0, buffer_len);
    }
    unnamed = SQL_NAMED;
  }
}

StatusRecord DescriptorRecord::SetNumPrecRadix(SQLINTEGER value) {
  if (value != kNumPrecRadixForNonNumeric &&
      value != kNumPrecRadixForApproximateNumeric &&
      value != kNumPrecRadixForExactNumeric) {
    LOG(ERROR) << "DescriptorRecord::SetNumPrecRadix:: Invalid "
                  "attribute/option identifier: "
               << value;
    return {SQLStates::k_HY092(), "Invalid attribute/option identifier"};
  }
  num_prec_radix = value;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetParameterType(SQLSMALLINT value) {
  if (value != SQL_PARAM_INPUT && value != SQL_PARAM_INPUT_OUTPUT &&
      value != SQL_PARAM_OUTPUT) {
    LOG(ERROR)
        << "DescriptorRecord::SetParameterType:: Invalid parameter type: "
        << value;
    return {SQLStates::k_HY105(), "Invalid parameter type"};
  }
  parameter_type = value;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetUnnamed(SQLSMALLINT value) {
  if (value != SQL_UNNAMED) {
    LOG(ERROR) << "DescriptorRecord::SetUnnamed:: Invalid descriptor field "
                  "identifier: "
               << value;
    return {SQLStates::k_HY091(), "Invalid descriptor field identifier"};
  }
  unnamed = value;
  return StatusRecord::Ok();
}

SQLSMALLINT GetPrecisionForIntervalCode(SQLSMALLINT datetime_interval_code) {
  return (datetime_interval_code == SQL_CODE_SECOND ||
          datetime_interval_code == SQL_CODE_DAY_TO_SECOND ||
          datetime_interval_code == SQL_CODE_HOUR_TO_SECOND ||
          datetime_interval_code == SQL_CODE_MINUTE_TO_SECOND)
             ? kDefaultIntervalSecondsPrecision
             : kDefaultIntervalPrecision;
}

SQLSMALLINT GetPrecisionForDatetimeCode(SQLSMALLINT datetime_interval_code) {
  return (datetime_interval_code == SQL_CODE_TIMESTAMP)
             ? kDefaultIntervalSecondsPrecision
             : kDefaultIntervalPrecision;
}

SQLSMALLINT GetLengthForDatetimeCode(SQLSMALLINT datetime_interval_code) {
  switch (datetime_interval_code) {
    case SQL_CODE_DATE:
      return 10;
    case SQL_CODE_TIME:
      return 8;
    case SQL_CODE_TIMESTAMP:
      return 26;
    default:
      return 0;
  }
}

SQLSMALLINT GetLengthForIntervalCode(SQLSMALLINT datetime_interval_code) {
  switch (datetime_interval_code) {
    case SQL_CODE_MONTH:
    case SQL_CODE_YEAR:
    case SQL_CODE_DAY:
    case SQL_CODE_HOUR:
    case SQL_CODE_MINUTE:
      return 2;
    case SQL_CODE_YEAR_TO_MONTH:
    case SQL_CODE_DAY_TO_HOUR:
    case SQL_CODE_HOUR_TO_MINUTE:
      return 5;
    case SQL_CODE_DAY_TO_MINUTE:
      return 8;
    case SQL_CODE_SECOND:
      return 9;
    case SQL_CODE_DAY_TO_SECOND:
      return 18;
    case SQL_CODE_HOUR_TO_SECOND:
      return 15;
    case SQL_CODE_MINUTE_TO_SECOND:
      return 12;
    default:
      return 0;
  }
}

void DescriptorRecord::SetIntervalType(Interval const& entry,
                                       DescriptorType desc_type) {
  type = SQL_INTERVAL;
  concise_type = (IsDescriptorTypeApplication(desc_type))
                     ? entry.concise_c_type
                     : entry.concise_sql_type;
  datetime_interval_precision = 2;
  datetime_interval_code = entry.datetime_interval_code;
  precision = GetPrecisionForIntervalCode(datetime_interval_code);
  scale = precision;
  if (IsDescriptorTypeApplication(desc_type)) {
    length = 0;
  } else {
    length = GetLengthForIntervalCode(entry.datetime_interval_code);
  }
}

void DescriptorRecord::SetDatetimeType(Interval const& entry,
                                       DescriptorType desc_type) {
  type = SQL_DATETIME;
  concise_type = (IsDescriptorTypeApplication(desc_type))
                     ? entry.concise_c_type
                     : entry.concise_sql_type;
  datetime_interval_code = entry.datetime_interval_code;
  precision = GetPrecisionForDatetimeCode(entry.datetime_interval_code);
  scale = (entry.datetime_interval_code == SQL_CODE_TIMESTAMP &&
           desc_type == DescriptorType::kIPD)
              ? 0
              : precision;
  if (IsDescriptorTypeApplication(desc_type)) {
    length = 0;
  } else {
    length = GetLengthForDatetimeCode(entry.datetime_interval_code);
  }
  datetime_interval_precision = 0;
}

StatusRecord DescriptorRecord::SetOtherCType(SQLSMALLINT const value,
                                             std::string const& error_message) {
  switch (value) {
    case SQL_C_CHAR:
    case SQL_C_BINARY:
      // case SQL_C_VARBOOKMARK: (this macro has same value as SQL_C_BINARY)
      type = concise_type = value;
      precision = length = 1;
      datetime_interval_precision = 0;
      break;
    case SQL_C_NUMERIC:
      type = concise_type = value;
      precision = length = 38;
      datetime_interval_precision = 0;
      break;
    case SQL_C_FLOAT:
      type = concise_type = value;
      precision = length = 24;
      datetime_interval_precision = 0;
      break;
    case SQL_C_DOUBLE:
      type = concise_type = value;
      precision = length = 53;
      datetime_interval_precision = 0;
      break;
    case SQL_C_BIT:
    case SQL_C_WCHAR:
    case SQL_C_SSHORT:
    case SQL_C_USHORT:
    case SQL_C_SHORT:
    case SQL_C_SLONG:
    case SQL_C_ULONG:
    case SQL_C_LONG:
    case SQL_C_STINYINT:
    case SQL_C_UTINYINT:
    case SQL_C_TINYINT:
    case SQL_C_SBIGINT:
    case SQL_C_UBIGINT:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 0;
      break;
    case SQL_C_GUID:
      type = concise_type = value;
      precision = length = 16;
      datetime_interval_precision = 0;
      break;
    default:
      LOG(ERROR) << "DescriptorRecord::SetOtherCType:: " << error_message
                 << ": " << value;
      return StatusRecord{SQLStates::k_HY021(), error_message};
  }
  datetime_interval_code = scale = 0;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetOtherSQLType(
    SQLSMALLINT const value, std::string const& error_message) {
  switch (value) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
      type = concise_type = value;
      length = 1;
      precision = 0;
      datetime_interval_precision = 0;
      break;
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
      type = concise_type = value;
      datetime_interval_precision = 0;
      break;
    case SQL_NUMERIC:
    case SQL_DECIMAL:
      type = concise_type = value;
      datetime_interval_precision = 0;
      precision = length = 38;
      scale = 0;
      break;
    case SQL_SMALLINT:
      type = concise_type = value;
      datetime_interval_precision = 0;
      length = 5;
      break;
    case SQL_INTEGER:
      type = concise_type = value;
      datetime_interval_precision = 0;
      length = 10;
      break;
    case SQL_REAL:
      type = concise_type = value;
      datetime_interval_precision = 0;
      precision = 24;
      length = 7;
      break;
    case SQL_FLOAT:
    case SQL_DOUBLE:
      type = concise_type = value;
      datetime_interval_precision = 0;
      precision = 53;
      length = 15;
      break;
    case SQL_BIT:
      type = concise_type = value;
      datetime_interval_precision = 0;
      length = 1;
      break;
    case SQL_TINYINT:
      type = concise_type = value;
      datetime_interval_precision = 0;
      length = 3;
      break;
    case SQL_BIGINT:
      type = concise_type = value;
      datetime_interval_precision = 0;
      length = 19;
      break;
    case SQL_GUID:
      type = concise_type = value;
      datetime_interval_precision = 0;
      precision = 0;
      length = 36;
      break;
    default:
      LOG(ERROR) << "DescriptorRecord::SetOtherSQLType:: " << error_message
                 << ": " << value;
      return StatusRecord{SQLStates::k_HY021(), error_message};
  }
  datetime_interval_code = 0;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetDisplaySize(SQLSMALLINT type,
                                              SQLINTEGER value,
                                              SQLINTEGER precision) {
  if (!type) {
    LOG(ERROR) << "DescriptorRecord::SetDisplaySize:: Invalid attribute/option "
                  "identifier (type is null).";
    return StatusRecord{SQLStates::k_HY092(),
                        "Invalid attribute/option identifier"};
  }
  switch (type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
      display_size = value;
      break;
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
      display_size = 2 * value;
      break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
      display_size = precision + 2;
      break;
    case SQL_SMALLINT:
      display_size = 6;
      break;
    case SQL_INTEGER:
      display_size = 11;
      break;
    case SQL_BIGINT:
      display_size = 20;
      break;
    case SQL_REAL:
    case SQL_FLOAT:
    case SQL_DOUBLE:
      display_size = 24;
      break;
    case SQL_DATE:
      display_size = 10;
      break;
    case SQL_TIME:
      display_size = 8;
      break;
    case SQL_TIMESTAMP:
      display_size = 19;
      break;
    default:
      display_size = value;
      break;
  }
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetOctetLength(SQLSMALLINT type,
                                              SQLINTEGER value,
                                              SQLINTEGER precision) {
  if (!type) {
    LOG(ERROR) << "DescriptorRecord::SetOctetLength:: Invalid attribute/option "
                  "identifier (type is null).";
    return StatusRecord{SQLStates::k_HY092(),
                        "Invalid attribute/option identifier"};
  }
  switch (type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
      octet_length = 4 * precision;
      break;
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY:
      octet_length = value;
      break;
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
      octet_length = value * sizeof(SQLWCHAR);
      break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
      octet_length = (precision + 2);
      break;
    case SQL_SMALLINT:
    case SQL_INTEGER:
    case SQL_BIGINT:
      octet_length = 20;
      break;
    case SQL_REAL:
      octet_length = sizeof(SQLREAL);
      break;
    case SQL_FLOAT:
    case SQL_DOUBLE:
      octet_length = sizeof(SQLDOUBLE);
      break;
    case SQL_GUID:
      octet_length = 16;
      break;
    case SQL_TYPE_TIME:
    case SQL_TYPE_DATE:
      octet_length = 6;
      break;
    case SQL_TYPE_TIMESTAMP:
      octet_length = 16;
      break;
    default:
      octet_length = value;
      break;
  }
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetType(SQLSMALLINT value,
                                       DescriptorType const& desc_type) {
  if (value == SQL_INTERVAL) {
    for (auto const& entry : kIntervalTypes) {
      if (datetime_interval_code == entry.datetime_interval_code) {
        SetIntervalType(entry, desc_type);
        return StatusRecord::Ok();
      }
    }
    LOG(ERROR) << "DescriptorRecord::SetType:: Interval code "
               << datetime_interval_code << " invalid or not supported.";
    return StatusRecord{SQLStates::k_HY021(),
                        "Interval code invalid or not supported"};
  }
  if (value == SQL_DATETIME) {
    for (auto const& entry : kDatetimeTypes) {
      if (datetime_interval_code == entry.datetime_interval_code) {
        SetDatetimeType(entry, desc_type);
        return StatusRecord::Ok();
      }
    }
    LOG(ERROR) << "DescriptorRecord::SetType:: Datetime interval code "
               << datetime_interval_code << " invalid or not supported.";
    return StatusRecord{SQLStates::k_HY021(),
                        "Datetime interval code invalid or not supported"};
  }
  if (IsDescriptorTypeApplication(desc_type)) {
    return SetOtherCType(value, "Illegal descriptor type");
  }
  return SetOtherSQLType(value, "Illegal descriptor type");
}

StatusRecord DescriptorRecord::SetConciseType(SQLSMALLINT value,
                                              DescriptorType const& desc_type) {
  for (auto const& entry : kIntervalTypes) {
    if (entry.concise_sql_type == value || entry.concise_c_type == value) {
      SetIntervalType(entry, desc_type);
      return StatusRecord::Ok();
    }
  }
  for (auto const& entry : kDatetimeTypes) {
    if (entry.concise_sql_type == value || entry.concise_c_type == value) {
      SetDatetimeType(entry, desc_type);
      return StatusRecord::Ok();
    }
  }
  std::string error_message =
      "Illegal descriptor concise type: " + std::to_string(value);
  if (IsDescriptorTypeApplication(desc_type)) {
    return SetOtherCType(value, error_message);
  }
  return SetOtherSQLType(value, error_message);
}

bool DescriptorRecord::IsTypeValid(SQLSMALLINT valid_type,
                                   SQLSMALLINT valid_concise_type,
                                   SQLSMALLINT valid_code) const {
  return type == valid_type && concise_type == valid_concise_type &&
         datetime_interval_code == valid_code;
}

bool DescriptorRecord::IsTypeValid(SQLSMALLINT valid_type,
                                   Interval const& interval) const {
  return IsTypeValid(valid_type, interval.concise_sql_type,
                     interval.datetime_interval_code) ||
         IsTypeValid(valid_type, interval.concise_c_type,
                     interval.datetime_interval_code);
}

StatusRecord DescriptorRecord::ConsistencyCheck() const {
  if (type == SQL_C_DEFAULT && concise_type == SQL_C_DEFAULT) {
    return StatusRecord::Ok();
  }
  for (auto const& entry : kIntervalTypes) {
    if (IsTypeValid(SQL_INTERVAL, entry) &&
        precision == GetPrecisionForIntervalCode(datetime_interval_code)) {
      return StatusRecord::Ok();
    }
  }
  for (auto const& entry : kDatetimeTypes) {
    if (IsTypeValid(SQL_DATETIME, entry) &&
        precision == GetPrecisionForDatetimeCode(datetime_interval_code)) {
      return StatusRecord::Ok();
    }
  }
  if (IsTypeValid(SQL_DATETIME, SQL_DATE, SQL_CODE_DATE)) {
    return StatusRecord::Ok();
  }
  if (IsTypeValid(SQL_DATETIME, SQL_TIME, SQL_CODE_TIME)) {
    return StatusRecord::Ok();
  }
  if (IsTypeValid(SQL_DATETIME, SQL_TIMESTAMP, SQL_CODE_TIMESTAMP)) {
    return StatusRecord::Ok();
  }

  if (std::find(kOtherSQLSupportedTypes.begin(), kOtherSQLSupportedTypes.end(),
                type) != kOtherSQLSupportedTypes.end() &&
      type == concise_type) {
    return StatusRecord::Ok();
  }
  if (std::find(kOtherCSupportedTypes.begin(), kOtherCSupportedTypes.end(),
                type) != kOtherCSupportedTypes.end() &&
      type == concise_type) {
    return StatusRecord::Ok();
  }
  LOG(ERROR) << "DescriptorRecord::ConsistencyCheck:: Inconsistent descriptor "
                "information. Type: "
             << type << ", ConciseType: " << concise_type;
  return StatusRecord{SQLStates::k_HY021(),
                      "Inconsistent descriptor information"};
}

StatusRecord DescriptorRecord::SetDataPointer(SQLPOINTER ptr,
                                              DescriptorType const& desc_type) {
  StatusRecord status_record = ConsistencyCheck();
  if (!status_record.ok()) {
    LOG(ERROR) << "DescriptorRecord::SetDataPointer::ConsistencyCheck:: "
               << status_record.message;
    return status_record;
  }

  if (desc_type != DescriptorType::kIPD) {
    data_ptr = ptr;
  }
  return StatusRecord::Ok();
}

void DescriptorRecord::ApplyMetadataIrdOverrides(std::string const& col_name) {
  const bool is_short_wvarchar =
      col_name == "TABLE_CAT" || col_name == "COLUMN_NAME" ||
      col_name == "PKCOLUMN_NAME" || col_name == "PKTABLE_CAT" ||
      col_name == "FKTABLE_CAT" || col_name == "FKCOLUMN_NAME" ||
      col_name == "FK_NAME" || col_name == "PK_NAME" ||
      col_name == "TYPE_NAME";

  const bool is_long_wvarchar =
      col_name == "TABLE_SCHEM" || col_name == "TABLE_NAME" ||
      col_name == "PKTABLE_SCHEM" || col_name == "PKTABLE_NAME" ||
      col_name == "FKTABLE_SCHEM" || col_name == "FKTABLE_NAME";

  const bool is_smallint =
      col_name == "DATA_TYPE" || col_name == "DECIMAL_DIGITS" ||
      col_name == "NULLABLE" || col_name == "SQL_DATA_TYPE" ||
      col_name == "SQL_DATETIME_SUB" || col_name == "KEY_SEQ" ||
      col_name == "UPDATE_RULE" || col_name == "DELETE_RULE" ||
      col_name == "DEFERRABILITY";

  if (is_short_wvarchar || is_long_wvarchar) {
    type_name = "WVARCHAR";
    local_type_name = "WVARCHAR";
    SetConciseType(SQL_WVARCHAR, DescriptorType::kIRD);

    length = is_short_wvarchar ? 128 : 1024;
    precision = static_cast<SQLSMALLINT>(length);
    case_sensitive = 0;
    searchable = 0;
    scale = 0;

    SetDisplaySize(SQL_WVARCHAR, length, precision);
    SetOctetLength(SQL_WVARCHAR, length, precision);
  }
  else if (is_smallint) {
    type_name = "SMALLINT";
    local_type_name = "SMALLINT";
    SetConciseType(SQL_SMALLINT, DescriptorType::kIRD);

    searchable = 0;
    precision = 5;
    scale = 0;

    SetDisplaySize(SQL_SMALLINT, 5, 5);
    SetOctetLength(SQL_SMALLINT, 5, 5);
  }

  if (col_name == "TABLE_NAME" || col_name == "COLUMN_NAME" ||
      col_name == "PKTABLE_NAME" || col_name == "PKCOLUMN_NAME" ||
      col_name == "FKTABLE_NAME" || col_name == "FKCOLUMN_NAME" ||
      col_name == "KEY_SEQ") {
    nullable = SQL_NO_NULLS;
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal
