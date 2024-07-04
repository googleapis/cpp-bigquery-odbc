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
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

// Checks if an arithmetic value can be converted to another accurately.
template <typename SrcType, typename DestType>
inline odbc_internal::StatusRecord CheckLimitsArithmetic(SrcType value) {
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
                 value <= (std::numeric_limits<DestType>::max)());
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
inline odbc_internal::StatusRecord ConvertFromArithmeticDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  if (!std::is_arithmetic_v<SrcType>) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid datatypes for conversion check!"};
  }

  SrcType src_val;
  std::memcpy(&src_val, src_dsval.data(),
              sizeof(SrcType));  // Get the src value

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  // Ref:
  // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16
  // to understand the ODBC C data types and their typedefs
  // TODO(b/343404637): Handle all arithmetic types
  switch (dest_type) {
    case SQL_C_FLOAT: {
      auto* dest_val = static_cast<SQLREAL*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLREAL>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLREAL>(src_val);
      }
      return status_record;
    }
    case SQL_C_DOUBLE: {
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
    case SQL_C_SBIGINT: {
      auto* dest_val = static_cast<SQLBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLBIGINT>(src_val);
      }
      return status_record;
    }
    case SQL_C_UBIGINT: {
      auto* dest_val = static_cast<SQLUBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLUBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUBIGINT>(src_val);
      }
      return status_record;
    }
    case SQL_C_SSHORT: {
      auto* dest_val = static_cast<SQLSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLSMALLINT>(src_val);
      }
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
      StatusRecord status_record =
          StringValueToOutputBufferResponse(str.c_str(), dest_data);
      return status_record;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

// Converts a string to SQLDOUBLE and returns StatusRecord if it failed
inline odbc_internal::StatusRecordOr<SQLDOUBLE> ConvertToDouble(
    std::string const& str) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  char* endptr;
  errno = 0;
  SQLDOUBLE result = std::strtod(str.c_str(), &endptr);

  if (endptr == str.c_str() || *endptr != '\0' || errno == ERANGE) {
    // Conversion failed or overflow/underflow occurred
    return StatusRecord{SQLStates::k_HY000(), "Invalid conversion"};
  }

  // Check for NaN or infinity
  if (!std::isfinite(result)) {
    return StatusRecord{SQLStates::k_HY000(), "Value is NaN"};
  }
  return result;
}

// Assuming that DSValue hosts string data, this converts it to the destination
// data type in the DataBuffer
inline odbc_internal::StatusRecord ConvertFromStringDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  using odbc_internal::StatusRecordOr;

  std::string src_str;
  DSValueToString(src_dsval, src_str);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;

  if (dest_type == SQL_C_CHAR) {
    StatusRecord status_record =
        StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
    return status_record;
  }

  // TODO(sachinpro): This assumes that SQLDOUBLE is a safe container for all
  // arithmetic types, which is not true for int64 which has a range(-2^63 to
  // +2^63 -1). Integer range of SQLDOUBLE is (-2^54 to +2^54 -1). Here we
  // should use SQLDOUBLE for floating point values and int64 for pure integers
  StatusRecordOr<SQLDOUBLE> conversion_status = ConvertToDouble(src_str);
  if (!conversion_status) {
    return conversion_status.GetStatusRecord();
  }
  SQLDOUBLE src_val = *conversion_status;

  // TODO(b/343404637): Handle all arithmetic types
  switch (dest_type) {
    case SQL_C_FLOAT: {
      auto* dest_val = static_cast<SQLREAL*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLREAL>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLREAL>(src_val);
      }
      return status_record;
    }
    case SQL_C_DOUBLE: {
      auto* dest_val = static_cast<SQLDOUBLE*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLDOUBLE>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLDOUBLE>(src_val);
      }
      return status_record;
    }
    case SQL_C_SBIGINT: {
      auto* dest_val = static_cast<SQLBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLBIGINT>(src_val);
      }
      return status_record;
    }
    case SQL_C_UBIGINT: {
      auto* dest_val = static_cast<SQLUBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLUBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUBIGINT>(src_val);
      }
      return status_record;
    }
    case SQL_C_SSHORT: {
      auto* dest_val = static_cast<SQLSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLSMALLINT>(src_val);
      }
      return status_record;
    }
    case SQL_C_USHORT: {
      auto* dest_val = static_cast<SQLUSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLUSMALLINT>(src_val);
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
          CheckLimitsArithmetic<SQLDOUBLE, SQLINTEGER>(src_val);
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
          CheckLimitsArithmetic<SQLDOUBLE, SQLUINTEGER>(src_val);
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
  return StatusRecord::Ok();
}
template <typename SrcType>
inline odbc_internal::StatusRecord ConvertDate(DSValue const& src_dsval,
                                               DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  std::string con_src_str;
  convertDateToAndFro(src_dsval, con_src_str);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;

  // Define length variables
  constexpr int kDateCharLength = 10;
  constexpr int kDateWcharLength = kDateCharLength;
  constexpr int kDateBinaryLength = sizeof(SQL_DATE_STRUCT);

  if (dest_type == SQL_C_TYPE_DATE || dest_type == SQL_TYPE_DATE ||
      dest_type == SQL_CODE_DATE) {
    return StringValueToOutputBufferResponse(con_src_str.c_str(), dest_data);
  }

  switch (dest_type) {
    case SQL_C_CHAR:
      if (buffer_length > kDateCharLength) {
        std::strncpy(static_cast<char*>(dest_buf), con_src_str.c_str(),
                     buffer_length);
        return StatusRecord{SQL_SUCCESS, "Success"};
      } else if (buffer_length <= kDateCharLength) {
        std::strncpy(static_cast<char*>(dest_buf), con_src_str.c_str(),
                     buffer_length);
        return StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        return StatusRecord{SQLStates::k_22003(),
                            "Buffer length is insufficient"};
      }

    case SQL_C_WCHAR:
      if (buffer_length > kDateWcharLength) {
        std::wcsncpy(
            static_cast<wchar_t*>(dest_buf),
            std::wstring(con_src_str.begin(), con_src_str.end()).c_str(),
            buffer_length);
        return StatusRecord{SQL_SUCCESS, "Success"};
      } else if (buffer_length <= kDateWcharLength) {
        std::wcsncpy(
            static_cast<wchar_t*>(dest_buf),
            std::wstring(con_src_str.begin(), con_src_str.end()).c_str(),
            buffer_length);
        return StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        return StatusRecord{SQLStates::k_22003(),
                            "Buffer length is insufficient"};
      }

    case SQL_C_BINARY:
      if (kDateBinaryLength <= buffer_length) {
        std::memcpy(dest_buf, con_src_str.c_str(), kDateBinaryLength);
        return StatusRecord{SQL_SUCCESS, "Success"};
      } else {
        return StatusRecord{SQLStates::k_22003(),
                            "Buffer length is insufficient"};
      }

    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
