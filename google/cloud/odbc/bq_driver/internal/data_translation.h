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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <charconv>

namespace google::cloud::odbc_bq_driver_internal {

struct DataBuffer {
  // C data type of the data the application expects
  SQLSMALLINT type;

  // Pointer to the buffer provided by the application
  SQLPOINTER buf;

  // Length of the buffer provided by the application
  SQLLEN buflen;

  // Length of the result populated by the driver
  SQLLEN* result_len;
};

inline odbc_internal::StatusRecord StrToDataBuffer(char const* src,
                                                   DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  auto src_len = strlen(src);
  SQLLEN* str_len_ptr = dest_data.result_len;
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLLEN>(src_len);
  }

  SQLPOINTER buffer_ptr = dest_data.buf;
  SQLLEN buffer_len = dest_data.buflen;
  if (!buffer_ptr) {
    return StatusRecord::Ok();
  }
  if (buffer_len < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  char* dest = reinterpret_cast<char*>(buffer_ptr);
  auto status_record = StatusRecord::Ok();

  if (src_len == 0 || buffer_len == 0) {
    *dest = '\0';
  } else if (src_len < buffer_len) {
    strncpy(dest, src, src_len);
    dest[src_len] = '\0';
  } else {
    strncpy(dest, src, (buffer_len - 1));
    dest[buffer_len - 1] = '\0';
    status_record =
        StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
  }
  // Update the str_len_ptr to be that of the destination buffer
  // as per the spec.
  auto dest_len = strlen(dest);
  if (str_len_ptr) {
    *str_len_ptr = static_cast<SQLLEN>(dest_len);
  }

  return status_record;
}

// Checks if an arithmetic value can be converted to another accurately.
template <typename SrcType, typename DestType>
odbc_internal::StatusRecord CheckLimitsArithmetic(SrcType value) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  if (!std::is_arithmetic_v<SrcType> || !std::is_arithmetic_v<DestType>) {
    return odbc_internal::StatusRecord{
        SQLStates::k_HY000(), "Invalid datatypes for conversion check!"};
  }

  // Special case for same type
  if constexpr (std::is_same_v<SrcType, DestType>) {
    return StatusRecord::Ok();
  }

  // Special case for boolean to numeric
  if constexpr (std::is_same_v<SrcType, bool>) {
    return StatusRecord::Ok();  // bool can always be converted to a number (0
                                // or 1)
  }

  bool status = (value >= std::numeric_limits<DestType>::lowest() &&
                 value <= std::numeric_limits<DestType>::max());
  if (!status) {
    return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
  }

  // Special case for floating point to integer
  if constexpr (std::is_floating_point_v<SrcType> &&
                std::is_integral_v<DestType>) {
    bool status =
        (value == static_cast<DestType>(value));  // Check for truncation
    if (!status) {
      return StatusRecord{SQLStates::k_01S07(), "Fractional truncation"};
    }
  }
  return StatusRecord::Ok();
}

// Assuming that DSValue hosts fixed-length arithmetic data, this converts it to
// the destination data type in the DataBuffer
template <typename SrcType>
odbc_internal::StatusRecord ConvertFromArithmeticDSValue(
    DSValue& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  std::cout << "CP1:: " << std::endl;
  if (!std::is_arithmetic_v<SrcType>) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid datatypes for conversion check!"};
  }

  std::cout << "CP2:: " << std::endl;
  SrcType src_val;
  std::memcpy(&src_val, src_dsval.data(),
              sizeof(SrcType));  // Get the src value

  std::cout << "CP3:: " << std::endl;
  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  switch (dest_type) {
    case SQL_C_FLOAT: {
      std::cout << "CP4:: " << std::endl;
      auto* dest_val = static_cast<SQLREAL*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<double, SQLREAL>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLREAL>(src_val);
      }
      std::cout << "CP4.2:: " << *dest_val << std::endl;
      return status_record;
    }
    case SQL_C_DOUBLE: {
      std::cout << "CP5:: " << std::endl;
      auto* dest_val = static_cast<SQLDOUBLE*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLDOUBLE>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLDOUBLE>(src_val);
      }
      return status_record;
    }
    case SQL_C_SSHORT: {
      std::cout << "CP6:: " << std::endl;
      auto* dest_val = static_cast<SQLSMALLINT*>(dest_buf);
      std::cout << "CP6.1:: " << src_val << std::endl;
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLSMALLINT>(src_val);
      }
      std::cout << "CP6.2:: " << *dest_val << std::endl;
      return status_record;
    }
    case SQL_C_USHORT: {
      auto* dest_val = static_cast<SQLUSMALLINT*>(dest_buf);

      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLUSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUSMALLINT>(src_val);
      }
      return status_record;
    }
    case SQL_C_SLONG: {
      auto* dest_val = static_cast<SQLINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLINTEGER>(src_val);
      }
      return status_record;
    }
    case SQL_C_ULONG: {
      auto* dest_val = static_cast<SQLUINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLUINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUINTEGER>(src_val);
      }
      return status_record;
    }
    case SQL_C_CHAR: {
      std::string str = std::to_string(src_val);
      StatusRecord status_record = StrToDataBuffer(str.c_str(), dest_data);
      return status_record;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();  // Success!
}

inline odbc_internal::StatusRecordOr<double> ConvertToDouble(
    std::string const& str) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  char* endptr;
  errno = 0;
  double result = std::strtod(str.c_str(), &endptr);

  if (endptr == str.c_str() || *endptr != '\0' || errno == ERANGE) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid conversion"};
    ;  // Conversion failed or overflow/underflow occurred
  }

  // Check for NaN or infinity
  if (!std::isfinite(result)) {
    return StatusRecord{SQLStates::k_HY000(), "Value is NaN"};
    ;
  }
  return result;
}

// Assuming that DSValue hosts string data, this converts it to the destination
// data type in the DataBuffer
inline odbc_internal::StatusRecord ConvertFromStringDSValue(
    DSValue& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  using odbc_internal::StatusRecordOr;
  std::cout << "CP1:: " << std::endl;

  std::string src_str;
  DSValueToString(src_dsval, src_str);

  std::cout << "CP3:: " << std::endl;
  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;

  if (dest_type == SQL_C_CHAR) {
    StatusRecord status_record = StrToDataBuffer(src_str.c_str(), dest_data);
    return status_record;
  }

  // TODO(): this assumes that double is a safe container for all arithmetic
  // types, which is not true for int64 which has a range(-2^63 to +2^63 -1).
  // Integer range of double is (-2^54 to +2^54 -1).
  // Here we should use double for floating point values and int64 for pure
  // integers
  StatusRecordOr<double> conversion_status = ConvertToDouble(src_str);
  if (!conversion_status) {
    return conversion_status.GetStatusRecord();
  }
  double src_val = *conversion_status;

  switch (dest_type) {
    case SQL_C_FLOAT: {
      std::cout << "CP4:: " << std::endl;
      auto* dest_val = static_cast<SQLREAL*>(dest_buf);
      std::cout << "CP4.1:: " << src_val << std::endl;
      StatusRecord status_record =
          CheckLimitsArithmetic<double, SQLREAL>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLREAL>(src_val);
      }
      std::cout << "CP4.2:: " << *dest_val << std::endl;
      return status_record;
    }
    case SQL_C_DOUBLE: {
      std::cout << "CP5:: " << std::endl;
      auto* dest_val = static_cast<SQLDOUBLE*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<double, SQLDOUBLE>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLDOUBLE>(src_val);
      }
      return status_record;
    }
    case SQL_C_SSHORT: {
      std::cout << "CP6:: " << std::endl;
      auto* dest_val = static_cast<SQLSMALLINT*>(dest_buf);
      std::cout << "CP6.1:: " << src_str << std::endl;
      StatusRecord status_record =
          CheckLimitsArithmetic<double, SQLSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLSMALLINT>(src_val);
      }
      std::cout << "CP6.2:: " << *dest_val << std::endl;
      return status_record;
    }
    case SQL_C_USHORT: {
      auto* dest_val = static_cast<SQLUSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<double, SQLUSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUSMALLINT>(src_val);
      }
      return status_record;
    }
    case SQL_C_SLONG: {
      auto* dest_val = static_cast<SQLINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<double, SQLINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLINTEGER>(src_val);
      }
      return status_record;
    }
    case SQL_C_ULONG: {
      auto* dest_val = static_cast<SQLUINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<double, SQLUINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUINTEGER>(src_val);
      }
      return status_record;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();  // Success!
}

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
