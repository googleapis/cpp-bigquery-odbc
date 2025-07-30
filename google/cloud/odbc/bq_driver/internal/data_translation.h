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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

constexpr int kTimeCharLength = 8;

// Checks if an arithmetic value can be converted to another accurately.
template <typename SrcType, typename DestType>
inline odbc_internal::StatusRecord CheckLimitsArithmetic(SrcType value) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  if (!std::is_arithmetic_v<SrcType> || !std::is_arithmetic_v<DestType>) {
    LOG(ERROR)
        << "CheckLimitsArithmetic::Invalid datatypes for conversion check!";
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
  bool status =
      static_cast<double>(value) >=
          static_cast<double>(std::numeric_limits<DestType>::lowest()) &&
      static_cast<double>(value) <=
          static_cast<double>(std::numeric_limits<DestType>::max());
  if (!status) {
    LOG(ERROR) << "CheckLimitsArithmetic::Numeric value out of range";
    return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
  }

  // Special case for floating point to integer
  if constexpr (std::is_floating_point_v<SrcType> &&
                std::is_integral_v<DestType>) {
    bool status =
        (value == static_cast<DestType>(value));  // Check for truncation
    if (!status) {
      LOG(WARNING) << "CheckLimitsArithmetic::Fractional truncation";
      return StatusRecord{SQLStates::k_01S07(), "Fractional truncation"};
    }
  }
  return StatusRecord::Ok();
}

template <typename SrcType>
inline odbc_internal::StatusRecord ConvertFromArithmeticDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  if (!std::is_arithmetic_v<SrcType>) {
    LOG(ERROR) << "ConvertFromArithmeticDSValue::Invalid datatypes for "
                  "conversion check!";
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid datatypes for conversion check!"};
  }

  SrcType src_val;
  std::memcpy(&src_val, src_dsval.data(),
              sizeof(SrcType));  // Get the src value

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN* res_len = dest_data.result_len;
  // Ref:
  // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/c-data-types?view=sql-server-ver16
  // to understand the ODBC C data types and their typedefs
  // TODO(b/343404637): Handle all arithmetic types
  switch (dest_type) {
    case SQL_C_FLOAT: {
      auto* dest_val = reinterpret_cast<SQLREAL*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SQLDOUBLE, SQLREAL>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLREAL>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLREAL);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_DOUBLE: {
      auto* dest_val = reinterpret_cast<SQLDOUBLE*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLDOUBLE>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLDOUBLE>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLDOUBLE);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_SBIGINT: {
      auto* dest_val = reinterpret_cast<SQLBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLBIGINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLBIGINT);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_UBIGINT: {
      auto* dest_val = reinterpret_cast<SQLUBIGINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLUBIGINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUBIGINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLUBIGINT);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_SSHORT: {
      auto* dest_val = reinterpret_cast<SQLSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLSMALLINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLSMALLINT);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_USHORT: {
      auto* dest_val = reinterpret_cast<SQLUSMALLINT*>(dest_buf);

      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLUSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUSMALLINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLUSMALLINT);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_LONG:
    case SQL_C_SLONG: {
      auto* dest_val = reinterpret_cast<SQLINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLINTEGER>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLINTEGER);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_ULONG: {
      auto* dest_val = reinterpret_cast<SQLUINTEGER*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLUINTEGER>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLUINTEGER>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLUINTEGER);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_C_CHAR: {
      std::string str = std::to_string(src_val);
      StatusRecord status_record =
          StringValueToOutputBufferResponse(str.c_str(), dest_data);
      if (status_record.sql_state == SQLStates::k_01004()) {
        LOG(ERROR)
            << "ConvertFromArithmeticDSValue::"
               "StringValueToOutputBufferResponse:: Numeric value out of range";
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      }
      return status_record;
    }
    case SQL_C_BIT: {
      auto* dest_val = reinterpret_cast<SQLCHAR*>(dest_buf);
      if (src_val == 0 || src_val == 1) {
        *dest_val = static_cast<SQLCHAR>(src_val);
        return StatusRecord::Ok();
      }
      LOG(ERROR) << "ConvertFromArithmeticDSValue::Numeric value out of range "
                    "for BIT type";
      return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
    }
    case SQL_C_SHORT: {
      auto* dest_val = reinterpret_cast<SQLSMALLINT*>(dest_buf);
      StatusRecord status_record =
          CheckLimitsArithmetic<SrcType, SQLSMALLINT>(src_val);
      // In case of 'Numeric value out of range'(22003), no need to populate the
      // buffer
      if (status_record.sql_state != SQLStates::k_22003()) {
        *dest_val = static_cast<SQLSMALLINT>(src_val);
        if (res_len) {
          *res_len = sizeof(SQLSMALLINT);
        }
      }
      if (!status_record.ok())
        LOG(ERROR) << "ConvertFromArithmeticDSValue::CheckLimitsArithmetic:: "
                   << status_record.message;
      return status_record;
    }
    case SQL_WCHAR: {
      std::string str = std::to_string(src_val);
      int src_len = str.length();
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(str);

      StatusRecord status_record = WStrToOutputBufferResponse(
          wstr.GetValue(), dest_data.buf, dest_data.buflen, src_len,
          dest_data.buflen, dest_data.result_len);
      if (status_record.sql_state == SQLStates::k_01004()) {
        LOG(ERROR) << "ConvertFromArithmeticDSValue::"
                      "WStrToOutputBufferResponse:: Numeric value out of range";
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      }
      return status_record;
    }
    case SQL_C_NUMERIC: {
      auto* dest_val = reinterpret_cast<SQL_NUMERIC_STRUCT*>(dest_buf);
      memset(dest_val, 0, sizeof(SQL_NUMERIC_STRUCT));
      dest_val->precision = 19;
      dest_val->scale = 0;
      dest_val->sign = src_val < 0 ? 0 : 1;
      auto abs_val = static_cast<uint64_t>(src_val < 0 ? -src_val : src_val);
      size_t i = 0;
      while (abs_val > 0 && i < sizeof(dest_val->val)) {
        dest_val->val[i++] = static_cast<unsigned char>(abs_val & 0xFF);
        abs_val >>= 8;
      }
      if (abs_val > 0) {
        LOG(ERROR) << "ConvertFromArithmeticDSValue::Numeric value out of "
                      "range for NUMERIC type";
        return StatusRecord{SQLStates::k_22003(), "Numeric value out of range"};
      }
      if (res_len) {
        *res_len = sizeof(SQL_NUMERIC_STRUCT);
      }
      return StatusRecord::Ok();
    }
    default: {
      LOG(WARNING) << "ConvertFromArithmeticDSValue::Conversion is unsupported "
                      "for C-type: "
                   << dest_type;
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

// Converts a string to SQLDOUBLE and returns StatusRecord if it failed
// TODO(sachinpro): Make sure double is a good enough container during
// translation wherever ConvertToDouble is used.
inline odbc_internal::StatusRecordOr<SQLDOUBLE> ConvertToDouble(
    std::string const& str) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;
  char* endptr;
  errno = 0;
  SQLDOUBLE result = std::strtod(str.c_str(), &endptr);

  if (endptr == str.c_str() || *endptr != '\0' || errno == ERANGE) {
    // Conversion failed or overflow/underflow occurred
    LOG(ERROR) << "ConvertToDouble::Invalid conversion from string: " << str;
    return StatusRecord{SQLStates::k_HY000(), "Invalid conversion"};
  }

  // Check for NaN or infinity
  if (!std::isfinite(result)) {
    LOG(ERROR) << "ConvertToDouble::Value is NaN or infinity for string: "
               << str;
    return StatusRecord{SQLStates::k_HY000(), "Value is NaN"};
  }
  return result;
}

odbc_internal::StatusRecord ConvertFromNumericDSValue(DSValue const& src_dsval,
                                                      DataBuffer& dest_data);
// Assuming that DSValue hosts string data, this converts it to the destination
// data type in the DataBuffer
odbc_internal::StatusRecord ConvertFromStringDSValue(DSValue const& src_dsval,
                                                     DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromTimeDSValue(DSValue const& src_dsval,
                                                   DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromTimestampDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromDateDSValue(DSValue const& src_dsval,
                                                   DataBuffer& dest_data);

StatusRecord ConvertFromJsonDSValue(DSValue const& src_dsval,
                                    DataBuffer& dest_data);

StatusRecord ConvertFromArrayDSValue(DSValue const& src_dsval,
                                     DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromIntervalDSValue(DSValue const& src_dsval,
                                                       DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromBooleanDSValue(DSValue const& src_dsval,
                                                      DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromGeographyDSValue(
    DSValue const& src_dsval, DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromBytesDSValue(DSValue const& src_dsval,
                                                    DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromRangeDSValue(DSValue const& src_dsval,
                                                    DataBuffer& dest_data);

odbc_internal::StatusRecord ConvertFromStructDSValue(DSValue const& src_dsval,
                                                     DataBuffer& dest_data);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
