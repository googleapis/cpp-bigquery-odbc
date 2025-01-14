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
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;

void HeaderRecord::CopyHeaderRecordsFrom(HeaderRecord const& header_record) {
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
    return {SQLStates::k_HY092(), "Invalid attribute/option identifier"};
  }
  num_prec_radix = value;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetParameterType(SQLSMALLINT value) {
  if (value != SQL_PARAM_INPUT && value != SQL_PARAM_INPUT_OUTPUT &&
      value != SQL_PARAM_OUTPUT) {
    return {SQLStates::k_HY105(), "Invalid parameter type"};
  }
  parameter_type = value;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetUnnamed(SQLSMALLINT value) {
  if (value != SQL_UNNAMED) {
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
    length = 2;
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
  scale = precision;
  if (IsDescriptorTypeApplication(desc_type)) {
    datetime_interval_precision = 0;
    length = 0;
  } else {
    datetime_interval_precision = length;
    length = GetLengthForDatetimeCode(entry.datetime_interval_code);
  }
}

StatusRecord DescriptorRecord::SetOtherCType(SQLSMALLINT const value,
                                             std::string const& error_message) {
  switch (value) {
    case SQL_C_CHAR:
    case SQL_C_BINARY:
      // case SQL_C_VARBOOKMARK: (this macro has same value as SQL_C_BINARY)
      type = concise_type = value;
      datetime_interval_precision = precision = length = 1;
      break;
    case SQL_C_NUMERIC:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 38;
      break;
    case SQL_C_FLOAT:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 24;
      break;
    case SQL_C_DOUBLE:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 53;
      break;
    case SQL_C_BIT:
    case SQL_C_WCHAR:
    case SQL_C_SSHORT:
    case SQL_C_USHORT:
    case SQL_C_SLONG:
    case SQL_C_ULONG:
    case SQL_C_LONG:
    case SQL_C_STINYINT:
    case SQL_C_UTINYINT:
    case SQL_C_SBIGINT:
    case SQL_C_UBIGINT:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 0;
      break;
    case SQL_C_GUID:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 16;
      break;
    default:
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
      datetime_interval_precision = precision = length = 1;
      break;
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
      type = concise_type = value;
      datetime_interval_precision = precision = length;
      break;
    case SQL_NUMERIC:
    case SQL_DECIMAL:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 38;
      scale = 0;
      break;
    case SQL_SMALLINT:
      type = concise_type = value;
      datetime_interval_precision = precision = length;
      length = 5;
      break;
    case SQL_INTEGER:
      type = concise_type = value;
      datetime_interval_precision = precision = length;
      length = 10;
      break;
    case SQL_REAL:
      type = concise_type = value;
      datetime_interval_precision = 14;
      precision = 24;
      length = 7;
      break;
    case SQL_FLOAT:
    case SQL_DOUBLE:
      type = concise_type = value;
      datetime_interval_precision = 24;
      precision = 53;
      length = 15;
      break;
    case SQL_BIT:
      type = concise_type = value;
      datetime_interval_precision = precision = length;
      length = 1;
      break;
    case SQL_TINYINT:
      type = concise_type = value;
      datetime_interval_precision = precision = length;
      length = 3;
      break;
    case SQL_BIGINT:
      type = concise_type = value;
      datetime_interval_precision = precision = length;
      length = 19;
      break;
    case SQL_GUID:
      type = concise_type = value;
      datetime_interval_precision = precision = length = 36;
      break;
    default:
      return StatusRecord{SQLStates::k_HY021(), error_message};
  }
  datetime_interval_code = 0;
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
  return StatusRecord{SQLStates::k_HY021(),
                      "Inconsistent descriptor information"};
}

StatusRecord DescriptorRecord::SetDataPointer(SQLPOINTER ptr,
                                              DescriptorType const& desc_type) {
  StatusRecord status_record = ConsistencyCheck();
  if (!status_record.ok()) {
    return status_record;
  }

  if (desc_type != DescriptorType::kIPD) {
    data_ptr = ptr;
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
