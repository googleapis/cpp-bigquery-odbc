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
  SQLSMALLINT src_type = src_data.type;
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN src_buflen = src_data.buflen;
  SQLLEN* src_result_len = src_data.result_len;
  SQLLEN result_len = src_buflen;
  if (src_result_len != nullptr) {
    result_len = *src_result_len;
  }

  std::string src_str;
  // Ref:
  // https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/sql-data-types?view=sql-server-ver16
  // to understand the ODBC SQL data types
  // TODO(b/343404637): Handle all arithmetic types
  switch (src_type) {
    case SQL_C_CHAR: {
      src_str = std::string(static_cast<char*>(src_buf), result_len);
      break;
    }
    // TODO(b/345194139): Support SQL_C_WCHAR here
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  switch (sql_type) {
    case SQL_VARCHAR:
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

StatusRecordOr<std::string> ConvertFromBuffer(DataBuffer& src_data,
                                              SQLSMALLINT sql_type) {
  SQLSMALLINT src_type = src_data.type;
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN* res_len = src_data.result_len;

  switch (src_type) {
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
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
