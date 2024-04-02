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

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

static std::vector<Interval> const kDatetimeTypes = {
    {SQL_TYPE_DATE, SQL_C_TYPE_DATE, SQL_CODE_DATE},
    {SQL_TYPE_TIME, SQL_C_TYPE_TIME, SQL_CODE_TIME},
    {SQL_TYPE_TIMESTAMP, SQL_C_TYPE_TIMESTAMP, SQL_CODE_TIMESTAMP},
};

static std::vector<Interval> const kIntervalTypes = {
    {SQL_INTERVAL_MONTH, SQL_C_INTERVAL_MONTH, SQL_CODE_MONTH},
    {SQL_INTERVAL_YEAR, SQL_C_INTERVAL_YEAR, SQL_CODE_YEAR},
    {SQL_INTERVAL_YEAR_TO_MONTH, SQL_C_INTERVAL_YEAR_TO_MONTH,
     SQL_CODE_YEAR_TO_MONTH},
    {SQL_INTERVAL_DAY, SQL_C_INTERVAL_DAY, SQL_CODE_DAY},
    {SQL_INTERVAL_HOUR, SQL_C_INTERVAL_HOUR, SQL_CODE_HOUR},
    {SQL_INTERVAL_MINUTE, SQL_C_INTERVAL_MINUTE, SQL_CODE_MINUTE},
    {SQL_INTERVAL_SECOND, SQL_C_INTERVAL_SECOND, SQL_CODE_SECOND},
    {SQL_INTERVAL_DAY_TO_HOUR, SQL_C_INTERVAL_DAY_TO_HOUR,
     SQL_CODE_DAY_TO_HOUR},
    {SQL_INTERVAL_DAY_TO_MINUTE, SQL_C_INTERVAL_DAY_TO_MINUTE,
     SQL_CODE_DAY_TO_MINUTE},
    {SQL_INTERVAL_DAY_TO_SECOND, SQL_C_INTERVAL_DAY_TO_SECOND,
     SQL_CODE_DAY_TO_SECOND},
    {SQL_INTERVAL_HOUR_TO_MINUTE, SQL_C_INTERVAL_HOUR_TO_MINUTE,
     SQL_CODE_HOUR_TO_MINUTE},
    {SQL_INTERVAL_HOUR_TO_SECOND, SQL_C_INTERVAL_HOUR_TO_SECOND,
     SQL_CODE_HOUR_TO_SECOND},
    {SQL_INTERVAL_MINUTE_TO_SECOND, SQL_C_INTERVAL_MINUTE_TO_SECOND,
     SQL_CODE_MINUTE_TO_SECOND},
};

static std::vector<int> const kOtherSupportedTypes = {
    SQL_CHAR, SQL_C_CHAR,  SQL_BINARY, SQL_NUMERIC,  SQL_C_NUMERIC,
    SQL_REAL, SQL_C_FLOAT, SQL_DOUBLE, SQL_SMALLINT, SQL_INTEGER,
    SQL_BIT,  SQL_TINYINT, SQL_GUID};

void DescriptorHandle::BindNewDescriptorRecord(
    SQLSMALLINT index, DescriptorRecord descriptor_record) {
  descriptor_records_[index] = std::move(descriptor_record);
  header_record_.count = descriptor_records_.rbegin()->first;
}

StatusRecordOr<DescriptorRecord> DescriptorHandle::UnbindDescriptorRecord(
    int index) {
  if (descriptor_records_.count(index)) {
    DescriptorRecord erased = descriptor_records_[index];
    descriptor_records_.erase(index);
    header_record_.count =
        descriptor_records_.empty() ? 0 : descriptor_records_.rbegin()->first;
    return erased;
  }
  return StatusRecord{SQLStates::k_HY000(),
                      "Trying to unbind non-existent descriptor record"};
}

StatusRecord DescriptorHandle::UnbindAllDescriptorRecordsFrom(int index) {
  if (index < 0) {
    return {SQLStates::k_07009(), "Invalid descriptor index"};
  }
  int old_val = header_record_.count;
  for (int i = index + 1; i <= old_val; i++) {
    descriptor_records_.erase(i);
  }
  header_record_.count =
      descriptor_records_.empty() ? 0 : descriptor_records_.rbegin()->first;
  return StatusRecord::Ok();
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

void DescriptorRecord::SetIntervalType(Interval const& entry,
                                       DescriptorType desc_type) {
  type = SQL_INTERVAL;
  concise_type = (desc_type == DescriptorType::kApplication)
                     ? entry.concise_c_type
                     : entry.concise_sql_type;
  datetime_interval_precision = 2;
  datetime_interval_code = entry.datetime_interval_code;
  precision = GetPrecisionForIntervalCode(datetime_interval_code);
  scale = precision;
  length = 2;
}

void DescriptorRecord::SetDatetimeType(Interval const& entry,
                                       DescriptorType desc_type) {
  type = SQL_DATETIME;
  concise_type = (desc_type == DescriptorType::kApplication)
                     ? entry.concise_c_type
                     : entry.concise_sql_type;
  datetime_interval_precision = 0;
  datetime_interval_code = entry.datetime_interval_code;
  precision = GetPrecisionForDatetimeCode(datetime_interval_code);
  scale = precision;
  length = 0;
}

StatusRecord DescriptorRecord::SetOtherType(SQLSMALLINT const value,
                                            std::string const& error_message) {
  if (value == SQL_CHAR || value == SQL_C_CHAR || value == SQL_BINARY) {
    type = concise_type = value;
    datetime_interval_precision = precision = length = 1;
    datetime_interval_code = scale = 0;
  } else if (value == SQL_NUMERIC || value == SQL_C_NUMERIC) {
    type = concise_type = value;
    datetime_interval_precision = precision = length = 38;
    datetime_interval_code = scale = 0;
  } else if (value == SQL_REAL || value == SQL_C_FLOAT) {
    type = concise_type = value;
    datetime_interval_precision = precision = length = 24;
    datetime_interval_code = scale = 0;
  } else if (value == SQL_DOUBLE) {
    type = concise_type = value;
    datetime_interval_precision = precision = length = 53;
    datetime_interval_code = scale = 0;
  } else if (value == SQL_SMALLINT || value == SQL_INTEGER ||
             value == SQL_BIT || value == SQL_TINYINT) {
    type = concise_type = value;
    datetime_interval_precision = precision = length = 0;
    datetime_interval_code = scale = 0;
  } else if (value == SQL_GUID) {
    type = concise_type = value;
    datetime_interval_precision = precision = length = 16;
    datetime_interval_code = scale = 0;
  } else if (value == SQL_TIMESTAMP) {
    type = SQL_DATETIME;
    concise_type = value;
    datetime_interval_precision = 0;
    datetime_interval_code = SQL_CODE_TIMESTAMP;
    scale = 6;
    precision = 6;
    length = 0;
  } else {
    // Not supported: SQL_DECIMAL, SQL_VARCHAR, SQL_FLOAT, SQL_BIGINT,
    // SQL_VARBINARY, SQL_LONGVARCHAR, SQL_VARBINARY
    return StatusRecord{SQLStates::k_HY021(), error_message};
  }
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetType(SQLSMALLINT value,
                                       DescriptorType desc_type) {
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
  return SetOtherType(value, "Illegal descriptor type");
}

StatusRecord DescriptorRecord::SetConciseType(SQLSMALLINT value) {
  for (auto const& entry : kIntervalTypes) {
    if (entry.concise_sql_type == value || entry.concise_c_type == value) {
      DescriptorType desc_type = entry.concise_sql_type == value
                                     ? DescriptorType::kApplication
                                     : DescriptorType::kIPD;
      SetIntervalType(entry, desc_type);
      return StatusRecord::Ok();
    }
  }
  for (auto const& entry : kDatetimeTypes) {
    if (entry.concise_sql_type == value || entry.concise_c_type == value) {
      DescriptorType desc_type = entry.concise_sql_type == value
                                     ? DescriptorType::kApplication
                                     : DescriptorType::kIPD;
      SetDatetimeType(entry, desc_type);
      return StatusRecord::Ok();
    }
  }

  if (value == SQL_DATE || value == SQL_TIME) {
    type = SQL_DATETIME;
    concise_type = value;
    datetime_interval_precision = 0;
    datetime_interval_code = value == SQL_DATE ? SQL_CODE_DATE : SQL_CODE_TIME;
    precision = 0;
    scale = 0;
    length = 0;
    return StatusRecord::Ok();
  }
  return SetOtherType(value, "Illegal descriptor concise type");
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

  if (std::find(kOtherSupportedTypes.begin(), kOtherSupportedTypes.end(),
                type) != kOtherSupportedTypes.end() &&
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
