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

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

StatusRecord ConvertFromJsonDSValue(DSValue const& src_dsval,
                                    DataBuffer& dest_data) {
  std::string src_str;
  DSValueToString(src_dsval, src_str);
  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  SQLLEN* res_len = dest_data.result_len;

  switch (dest_type) {
    case SQL_C_CHAR: {
      StatusRecord status_record =
          StringValueToOutputBufferResponse(src_str.c_str(), dest_data);
      return status_record;
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wide_string = Utf8ToUtf16(src_str);
      if (!wide_string.Ok()) {
        StatusRecord status_record =
            StatusRecord{SQLStates::k_HY000(), "Conversion Failed"};
        break;
      }
      return WStrToOutputBufferResponse(
          wide_string.GetValue(), dest_buf, buffer_length, src_str.length(),
          src_str.length(), reinterpret_cast<SQLLEN*>(dest_data.result_len));
      break;
    }
    // TODO(b\367841053): SQL_C_BINARY to be done later
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

template <typename T>
inline void HandleIntervalArthmeticConversion(
    T* dest_buf, SQL_INTERVAL_STRUCT interval, SQLLEN* res_len,
    odbc_internal::StatusRecord& status_record) {
  SQLUINTEGER value = 0;
  GetSinglePrecisionInterval(interval, value);
  // Get the maximum value for the type T.
  if (value <= static_cast<SQLUINTEGER>(std::numeric_limits<T>::max())) {
    *dest_buf = static_cast<T>(value);
    if (res_len) {
      *res_len = sizeof(T);
    }
  } else {
    status_record = odbc_internal::StatusRecord{
        odbc_internal::SQLStates::k_22003(), "Data truncated"};
  }
}

odbc_internal::StatusRecord ConvertFromIntervalDSValue(DSValue const& src_dsval,
                                                       DataBuffer& dest_data) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  SQL_INTERVAL_STRUCT conn_interval = {};
  std::string interval_src_str;

  DSValueToString(src_dsval, interval_src_str);
  ConvertStringToIntervalStruct(interval_src_str, conn_interval);

  SQLSMALLINT dest_type = dest_data.type;
  SQLPOINTER dest_buf = dest_data.buf;
  SQLLEN buffer_length = dest_data.buflen;
  auto* res_len = reinterpret_cast<SQLLEN*>(dest_data.result_len);

  constexpr int kIntervalCharLength = 30;
  int interval_src_len = interval_src_str.length();

  if (!dest_buf) {
    return StatusRecord::Ok();
  }
  if (buffer_length < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  StatusRecord status_record = StatusRecord::Ok();
  switch (dest_type) {
    case SQL_C_CHAR: {
      char* dest = reinterpret_cast<char*>(dest_buf);
      auto whole_digit_count = GetWholeDigitCount(interval_src_str);
      if (buffer_length > kIntervalCharLength) {
        if (res_len) {
          *res_len = interval_src_str.length();
        }
        std::strncpy(dest, interval_src_str.c_str(), interval_src_len);
        dest[interval_src_len] = '\0';
      } else if (buffer_length > whole_digit_count) {
        if (res_len) {
          *res_len = interval_src_str.length();
        }
        std::strncpy(dest, interval_src_str.c_str(), interval_src_len);
        dest[interval_src_len] = '\0';
        status_record = StatusRecord{SQLStates::k_01004(), "Data truncated"};
      } else {
        strncpy(dest, "Y-M D H:M:S", buffer_length - 1);
        status_record =
            StatusRecord{SQLStates::k_22003(), "Buffer length is insufficient"};
      }
      break;
    }
    case SQL_C_WCHAR: {
      StatusRecordOr<std::wstring> wstr = Utf8ToUtf16(interval_src_str);
      int interval_char_length = wstr.GetValue().length();
      auto whole_digit_count = GetWholeDigitCount(interval_src_str);
      if (!wstr) {
        status_record =
            StatusRecord{SQLStates::k_HY000(), wstr.GetStatusRecord().message};
        break;
      }
      return WStrIntervalBufferResponse(
          wstr.GetValue(), dest_buf, buffer_length, interval_char_length,
          whole_digit_count, reinterpret_cast<SQLLEN*>(dest_data.result_len));
      break;
    }
    case SQL_C_STINYINT: {
      auto* dest = reinterpret_cast<SQLSCHAR*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLSCHAR>(dest, conn_interval, res_len,
                                                  status_record);
      break;
    }
    case SQL_C_UTINYINT: {
      auto* dest = reinterpret_cast<SQLCHAR*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLCHAR>(dest, conn_interval, res_len,
                                                 status_record);
      break;
    }
    case SQL_C_SSHORT: {
      auto* dest = reinterpret_cast<SQLSMALLINT*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLSMALLINT>(dest, conn_interval,
                                                     res_len, status_record);
      break;
    }
    case SQL_C_USHORT: {
      auto* dest = reinterpret_cast<SQLUSMALLINT*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLUSMALLINT>(dest, conn_interval,
                                                      res_len, status_record);
      break;
    }
    case SQL_C_ULONG: {
      auto* dest = reinterpret_cast<SQLUINTEGER*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLUINTEGER>(dest, conn_interval,
                                                     res_len, status_record);
      break;
    }
    case SQL_C_SBIGINT: {
      auto* dest = reinterpret_cast<SQLBIGINT*>(dest_buf);
      HandleIntervalArthmeticConversion<SQLBIGINT>(dest, conn_interval, res_len,
                                                   status_record);
      break;
    }
    case SQL_C_NUMERIC: {
      auto* dest = reinterpret_cast<SQL_NUMERIC_STRUCT*>(dest_buf);
      SQLUINTEGER value = 0;
      GetSinglePrecisionInterval(conn_interval, value);
      std::memset(dest, 0, sizeof(SQL_NUMERIC_STRUCT));
      dest->sign = 1;

      int byte_index = 0;
      while (value > 0 && byte_index < SQL_MAX_NUMERIC_LEN) {
        dest->val[byte_index++] = static_cast<SQLCHAR>(value & 0xFF);
        value >>= 8;
      }
      if (value > 0) {
        status_record =
            odbc_internal::StatusRecord{odbc_internal::SQLStates::k_22003(),
                                        "Value out of range for SQL_C_NUMERIC"};
      } else {
        dest->precision = 10;
        dest->scale = 0;
        if (res_len) {
          *res_len = sizeof(SQL_NUMERIC_STRUCT);
        }
      }
      break;
    }
    case SQL_C_INTERVAL_YEAR:
    case SQL_C_INTERVAL_MONTH:
    case SQL_C_INTERVAL_DAY:
    case SQL_C_INTERVAL_HOUR:
    case SQL_C_INTERVAL_MINUTE:
    case SQL_C_INTERVAL_SECOND:
    case SQL_C_INTERVAL_YEAR_TO_MONTH:
    case SQL_C_INTERVAL_DAY_TO_HOUR:
    case SQL_C_INTERVAL_DAY_TO_MINUTE:
    case SQL_C_INTERVAL_DAY_TO_SECOND:
    case SQL_C_INTERVAL_HOUR_TO_MINUTE:
    case SQL_C_INTERVAL_HOUR_TO_SECOND:
    case SQL_C_INTERVAL_MINUTE_TO_SECOND: {
      if (kIntervalCharLength < buffer_length) {
        return IntervalToOutputBufferResponse(conn_interval, dest_buf,
                                              buffer_length, res_len);
      }
      status_record = StatusRecord{SQLStates::k_01S07(), "Data truncated"};
      break;
    }
    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }
  return status_record;
}

StatusRecord ConvertFromBooleanDSValue(DSValue const& src_dsval,
                                       DataBuffer& dest_data) {
  bool conn_bool = false;

  if (!dest_data.buf) {
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }

  if (dest_data.buflen < 0) {
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  DSValueToBoolean(src_dsval, conn_bool);
  StatusRecord status_record = StatusRecord::Ok();
  switch (dest_data.type) {
    case SQL_C_CHAR: {
      auto* dest = reinterpret_cast<char*>(dest_data.buf);
      if (dest_data.buflen < 2) {
        if (dest_data.buflen > 0) {
          dest[0] = '\0';
        }
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
      } else {
        dest[0] = conn_bool ? '1' : '0';
        dest[1] = '\0';
      }
      break;
    }

    case SQL_C_WCHAR: {
      auto* dest = reinterpret_cast<wchar_t*>(dest_data.buf);
      size_t wchar_len = dest_data.buflen / sizeof(wchar_t);
      if (wchar_len < 2) {
        if (wchar_len > 0) {
          dest[0] = L'\0';
        }
        status_record =
            StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
      } else {
        std::wstring value = conn_bool ? L"1" : L"0";
        std::wcsncpy(dest, value.c_str(), wchar_len - 1);
        dest[wchar_len - 1] = L'\0';
      }
      break;
    }

    case SQL_C_BINARY: {
      if (dest_data.buflen < sizeof(bool)) {
        std::memcpy(dest_data.buf, &conn_bool, dest_data.buflen);
        status_record =
            StatusRecord{SQLStates::k_01004(), "Binary data, right truncated"};
      } else {
        std::memcpy(dest_data.buf, &conn_bool, sizeof(bool));
      }
      break;
    }

    case SQL_C_LONG: {
      if (dest_data.buflen < sizeof(SQLINTEGER)) {
        status_record = StatusRecord{SQLStates::k_01004(),
                                     "Long integer data, right truncated"};
      } else {
        *reinterpret_cast<SQLINTEGER*>(dest_data.buf) =
            static_cast<SQLINTEGER>(conn_bool);
      }
      break;
    }

    case SQL_C_BIT: {
      if (dest_data.buflen < sizeof(SQLCHAR)) {
        status_record =
            StatusRecord{SQLStates::k_01004(), "Bit data, right truncated"};
      } else {
        *reinterpret_cast<SQLCHAR*>(dest_data.buf) =
            conn_bool ? static_cast<SQLCHAR>(1) : static_cast<SQLCHAR>(0);
      }
      break;
    }

    case SQL_C_DOUBLE: {
      if (dest_data.buflen < sizeof(SQLDOUBLE)) {
        status_record =
            StatusRecord{SQLStates::k_01004(), "Double data, right truncated"};
      } else {
        *reinterpret_cast<SQLDOUBLE*>(dest_data.buf) = conn_bool ? 1.0 : 0.0;
      }
      break;
    }

    default:
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
  }

  return status_record;
}

// This func converts a vector of SQLCHAR bytes (hex-encoded) to binary data,
// handling truncation if needed.
StatusRecord ConvertBytesToBinary(std::vector<SQLCHAR> const& conn_val,
                                  DataBuffer& dest_data) {
  std::cout << "ConvertBytesToBinary: Starting conversion of " << conn_val.size() 
            << " bytes" << std::endl;
  
  std::vector<uint8_t> binary_data;
  binary_data.reserve(conn_val.size() / 2);

  StatusRecord status_record = StatusRecord::Ok();
  // Convert each hex pair into a byte
  for (size_t i = 0; i < conn_val.size(); i += 2) {
    uint8_t byte = (conn_val[i] - '0') * 16 + (conn_val[i + 1] - '0');
    binary_data.push_back(byte);
  }

  std::cout << "ConvertBytesToBinary: Converted " << binary_data.size() 
            << " bytes, buffer length = " << dest_data.buflen << std::endl;

  // Handle buffer truncation scenario
  if (dest_data.buflen < binary_data.size()) {
    std::memcpy(dest_data.buf, binary_data.data(), dest_data.buflen);
    if (dest_data.result_len) {
      *dest_data.result_len = dest_data.buflen;
    }
    status_record =
        StatusRecord{SQLStates::k_01004(), "Binary data, right truncated"};
  } else {
    std::memcpy(dest_data.buf, binary_data.data(), binary_data.size());
    if (dest_data.result_len) {
      *dest_data.result_len = binary_data.size();
    }
  }
  return status_record;
}

// This func converts a vector of SQLCHAR bytes to a standard char string,
// ensuring null termination and handling truncation.
StatusRecord ConvertBytesToChar(std::vector<SQLCHAR> const& conn_val,
                                DataBuffer& dest_data) {
  std::cout << "ConvertBytesToChar: Starting conversion of " << conn_val.size() 
            << " bytes to char" << std::endl;
  std::cout << "ConvertBytesToChar: Buffer length = " << dest_data.buflen << std::endl;
  
  auto* dest = reinterpret_cast<char*>(dest_data.buf);
  StatusRecord status_record = StatusRecord::Ok();

  // Check for truncation and copy data accordingly
  if (dest_data.buflen < static_cast<SQLLEN>(conn_val.size()) + 1) {
    std::memcpy(dest, conn_val.data(), dest_data.buflen - 1);
    dest[dest_data.buflen - 1] = '\0';
    if (dest_data.result_len) {
      *dest_data.result_len = dest_data.buflen;
    }
    status_record =
        StatusRecord{SQLStates::k_01004(), "String data, right truncated"};
  } else {
    std::memcpy(dest, conn_val.data(), conn_val.size());
    if (dest_data.result_len) {
      *dest_data.result_len = conn_val.size();
    }
  }

  std::cout << "ConvertBytesToChar: Converted data length = " 
            << (dest_data.result_len ? *dest_data.result_len : 0) << std::endl;
            
  return status_record;
}

// This func converts a vector of SQLCHAR bytes to a UTF-16 wchar_t string,
// ensuring proper truncation handling.
StatusRecord ConvertBytesToWChar(std::vector<SQLCHAR> const& conn_val,
                                 DataBuffer& dest_data) {

    StatusRecord status_record = StatusRecord::Ok();
    
    std::string utf8_str(conn_val.begin(), conn_val.end());

      StatusRecordOr<std::wstring> utf16_str = Utf8ToUtf16(utf8_str);
   
    const size_t required_size = utf16_str.GetValue().length() * sizeof(SQLWCHAR);
    

    SQLWCHAR* buffer = reinterpret_cast<SQLWCHAR*>(dest_data.buf);

    // Convert UTF-16 to 4-byte SQLWCHAR format with zero-padding
    for (size_t i = 0; i < utf16_str.GetValue().size(); ++i) {
        buffer[i] = static_cast<SQLWCHAR>(utf16_str.GetValue()[i]);
    }

    // Set output length
    if (dest_data.result_len) {
        *dest_data.result_len = utf16_str.GetValue().size() * sizeof(SQLWCHAR);
    }

 return status_record;
}


StatusRecord ConvertFromBytesDSValue(DSValue const& src_dsval,
                                     DataBuffer& dest_data) {
  std::cout << "ConvertFromBytesDSValue: Starting conversion" << std::endl;
  
  // Print source DSValue contents
  std::cout << "ConvertFromBytesDSValue: Source DSValue contents: ";
  for(size_t i = 0; i < src_dsval.size(); i++) {
    std::cout << src_dsval[i] << " ";
  }
  
  std::vector<SQLCHAR> conn_val = DSValueToBytes(src_dsval);
  SQLLEN src_length = static_cast<SQLLEN>(conn_val.size());
  
  std::cout << "ConvertFromBytesDSValue: Source length = " << src_length << std::endl;
  std::cout << "ConvertFromBytesDSValue: First few bytes: ";
  for(size_t i = 0; i < conn_val.size(); i++) {
    std::cout << std::hex << (int)conn_val[i] << " ";
  }
  std::cout << std::dec << std::endl;

  if (!dest_data.buf) {
    std::cout << "ConvertFromBytesDSValue: Error - Destination buffer is null" << std::endl;
    return StatusRecord{SQLStates::k_HY090(), "Destination buffer is null"};
  }
  if (dest_data.buflen < 0) {
    std::cout << "ConvertFromBytesDSValue: Error - Buffer length is negative (" 
              << dest_data.buflen << ")" << std::endl;
    return StatusRecord{SQLStates::k_HY090(), "Buffer length is negative"};
  }

  std::cout << "ConvertFromBytesDSValue: Destination type = " << dest_data.type 
            << ", buffer length = " << dest_data.buflen << std::endl;

  StatusRecord result;
  switch (dest_data.type) {
    case SQL_C_BINARY:
      std::cout << "ConvertFromBytesDSValue: Converting to binary" << std::endl;
      result = ConvertBytesToBinary(conn_val, dest_data);
      break;
    case SQL_C_CHAR:
      std::cout << "ConvertFromBytesDSValue: Converting to char" << std::endl;
      result = ConvertBytesToChar(conn_val, dest_data);
      break;
    case SQL_C_WCHAR:
      std::cout << "ConvertFromBytesDSValue: Converting to wchar" << std::endl;
      result = ConvertBytesToWChar(conn_val, dest_data);
      break;
    default:
      std::cout << "ConvertFromBytesDSValue: Error - Unsupported conversion type" << std::endl;
      return StatusRecord{SQLStates::k_HY000(), "Unsupported conversion type"};
  }
  
  return result;
}

}  // namespace google::cloud::odbc_bq_driver_internal
