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

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
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
std::cout << "check 4 " << src_data.type<< std::endl;
std::cout << "check 4.1 " << sql_type<< std::endl;
  std::string src_str;
  switch (src_data.type) {
    case SQL_C_CHAR: {
      src_str = std::string(static_cast<char*>(src_buf), result_len);
      break;
    }
    // TODO(b/345194139): Support SQL_C_WCHAR here

    // Need to check this 
    case SQL_C_WCHAR:{
      // std::wstring src_str = 
      SQLWCHAR* wchar_buf = static_cast<SQLWCHAR*>(src_buf);
      std::cout << " result len "<< result_len<<std::endl;
      
      size_t wchar_count;
      if (result_len == SQL_NTS) {
          wchar_count = wcslen(reinterpret_cast<const wchar_t*>(wchar_buf));  // Null-terminated string case
      } else if (result_len > 0) {
          wchar_count = result_len / sizeof(SQLWCHAR);  // Convert bytes to wchar count
      } else {
          return StatusRecord{SQLStates::k_HY000(), "Invalid buffer length"};
      }

      std::wstring wstr(reinterpret_cast<const wchar_t*>(wchar_buf), wchar_count);
      auto utf8_res = Utf16ToUtf8(wstr);
      src_str = utf8_res->c_str();
      break;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  std::cout << "check 5 "<< sql_type <<std::endl; 
  switch (sql_type) {
    case SQL_VARCHAR:
    case SQL_WLONGVARCHAR:
    case SQL_LONGVARCHAR:
    case SQL_CHAR:
    case SQL_WCHAR: {
  std::cout << "check 6 "<< src_str <<std::endl;

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

std::string Base64Encode(const uint8_t* data, int len){
      std::string encoded;
    int val = 0, valb = -6;
    for (size_t i = 0; i < len; i++) {
        val = (val << 8) + data[i];
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (encoded.size() % 4) encoded.push_back('=');
    return encoded;
}

StatusRecordOr<std::string> ConvertFromBuffer(DataBuffer& src_data,
                                              SQLSMALLINT sql_type) {
  SQLPOINTER src_buf = src_data.buf;
  SQLLEN* res_len = src_data.result_len;
    std::cout << "check 2 "<< src_data.type<<std::endl;

  switch (src_data.type) {
    case SQL_C_WCHAR:
    case SQL_C_CHAR: {
    std::cout << "check 3 "<< std::endl;

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
      auto src_val = *reinterpret_cast<SQLCHAR*>(src_buf);
      if(src_val != 0 && src_val != 1){
        return StatusRecord{SQLStates::k_22003(), "Invalid BIT value (must be 0 or 1)"};
      }
      std::string result = (src_val == 0) ?   "false" : "true";
      return result;
    }
    case SQL_C_BINARY: {
    std::cout << "binary  conversion   " << std::endl;
      auto src_val = reinterpret_cast<uint8_t*>(src_buf);
      if(!src_val || !res_len || *res_len <= 0){
        return StatusRecord{SQLStates::k_HY000(), "Invalid binary data"};
      }
      std::string bas64_encoded = Base64Encode(src_val, *res_len);
      return bas64_encoded;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
