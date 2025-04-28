// Copyright 2025 Google LLC
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

//////////////////////////////////////////////////////////////////
// This file has the utilities to translate from ODBC C data type
//  to SQL Data type. ConvertFromBuffer is supposed to return a
// std::string because QueryParameter::parameter_value is a string
//////////////////////////////////////////////////////////////////

#include "google/cloud/odbc/bq_driver/internal/data_translation_inv.h"

namespace google::cloud::odbc_bq_driver_internal {

using odbc_internal::SQLStates;
using odbc_internal::StatusRecord;
using odbc_internal::StatusRecordOr;

// Assuming src_data has c_type of SQL_C_CHAR or SQL_C_WCHAR, this function
// returns a string which should be set to QueryParameter::parameter_value.
StatusRecordOr<std::string> ConvertFromCharBuffer(DataBuffer& src_data,
                                                  SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN src_buflen = src_data.buflen;
  SQLLEN* src_result_len = src_data.result_len;
  SQLLEN result_len = src_buflen;
  if (src_result_len != nullptr) {
    result_len = *src_result_len;
  }

  std::string src_str;
  switch (src_data.type) {
    case SQL_C_CHAR: {
      if (src_buf == nullptr || result_len <= 0) {
        src_str = "";
        break;
      }
      src_str = std::string(static_cast<char*>(src_buf), result_len);
      break;
    }
    case SQL_C_WCHAR: {
      auto* wchar_buf = static_cast<SQLWCHAR*>(src_buf);
      if ((result_len > 0) || (result_len == SQL_NTS)) {
        auto utf8_res =
            ConvertSQLWCHARToString(wchar_buf, result_len / sizeof(wchar_t));
        if (!utf8_res) {
          return StatusRecord{SQLStates::k_HY000(), "UTF-8 conversion failed"};
        }
        src_str = *utf8_res;
        break;
      }
      return StatusRecord{SQLStates::k_HY000(), "Invalid buffer length"};
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  switch (sql_type) {
    case SQL_VARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_CHAR:
    case SQL_WCHAR: {
      return src_str;
    }
    // TODO(b/345194139): Add conversion to complex data types
    default: {
      // Assume that sql_type can possibly be an arithmetic data type
      StatusRecordOr<SQLDOUBLE> double_status = ConvertToDouble(src_str);
      if (!double_status) {
        // If `src_str` cannot be converted to double, it is not an arithmetic
        // value
        return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
      }
      // Try to convert the SQLDOUBLE value to the sql_type's not handled
      // explicitly by the switch-case.
      auto conv_status =
          ConvertFromArithmeticValue<SQLDOUBLE>(*double_status, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
  }
  return StatusRecord::Ok();
}

StatusRecordOr<std::string> ConvertFromBinaryBuffer(DataBuffer& src_data,
                                                    SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN* src_result_len = src_data.result_len;

  auto* src_val = static_cast<uint8_t*>(src_buf);
  if (!src_val || !src_result_len || *src_result_len < 0) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid binary data"};
  }

  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_WCHAR:
    case SQL_WVARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_BINARY:
    case SQL_VARBINARY:
    case SQL_LONGVARBINARY: {
      return Base64Encode(src_val, *src_result_len);
    }
    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromBitBuffer(DataBuffer src_data,
                                                 SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLCHAR src_val = *reinterpret_cast<SQLCHAR*>(src_buf);

  if (src_val != 0 && src_val != 1) {
    return StatusRecord{SQLStates::k_22003(),
                        "Invalid BIT value (must be 0 or 1)"};
  }
  switch (sql_type) {
    case SQL_CHAR:
    case SQL_BIT:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR: {
      return std::string((src_val == 0) ? "false" : "true");
    }
    case SQL_INTEGER: {
      return std::to_string(static_cast<SQLINTEGER>(src_val));
    }
    case SQL_SMALLINT: {
      return std::to_string(static_cast<SQLSMALLINT>(src_val));
    }
    case SQL_TINYINT: {
      return std::to_string(static_cast<SQLCHAR>(src_val));
    }
    case SQL_FLOAT:
    case SQL_REAL: {
      return std::to_string(static_cast<SQLREAL>(src_val));
    }
    case SQL_DOUBLE: {
      return std::to_string(static_cast<SQLDOUBLE>(src_val));
    }
    case SQL_BIGINT: {
      return std::to_string(static_cast<SQLBIGINT>(src_val));
    }
    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
}

StatusRecordOr<std::string> ConvertFromBuffer(DataBuffer& src_data,
                                              SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN* res_len = src_data.result_len;

  switch (src_data.type) {
    case SQL_C_WCHAR:
    case SQL_C_CHAR: {
      auto conv_status = ConvertFromCharBuffer(src_data, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_FLOAT: {
      auto src_val = *reinterpret_cast<SQLREAL*>(src_buf);
      auto conv_status = ConvertFromArithmeticValue<SQLREAL>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_DOUBLE: {
      auto src_val = *reinterpret_cast<SQLDOUBLE*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLDOUBLE>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_SBIGINT: {
      auto src_val = *reinterpret_cast<SQLBIGINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLBIGINT>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_UBIGINT: {
      auto src_val = *reinterpret_cast<SQLUBIGINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLUBIGINT>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_SSHORT: {
      auto src_val = *reinterpret_cast<SQLSMALLINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLSMALLINT>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_USHORT: {
      auto src_val = *reinterpret_cast<SQLUSMALLINT*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLUSMALLINT>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_SLONG: {
      auto src_val = *reinterpret_cast<SQLINTEGER*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLINTEGER>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_ULONG: {
      auto src_val = *reinterpret_cast<SQLUINTEGER*>(src_buf);
      auto conv_status =
          ConvertFromArithmeticValue<SQLUINTEGER>(src_val, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_BIT: {
      auto conv_status = ConvertFromBitBuffer(src_data, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    case SQL_C_BINARY: {
      auto conv_status = ConvertFromBinaryBuffer(src_data, sql_type);
      if (!conv_status) {
        return conv_status.GetStatusRecord();
      }
      return *conv_status;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
