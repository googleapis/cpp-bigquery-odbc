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
//  to SQL Data type. Every function is supposed to return a
// std::string because QueryParameter::parameter_value is a string
//////////////////////////////////////////////////////////////////

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_INV_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_INV_H

#include "google/cloud/odbc/bq_driver/internal/data_translation.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

// Assuming SrcType is an arithmetic type, this function converts src_val into
// sql_type before returning it as string.
template <typename SrcType>
StatusRecordOr<std::string> ConvertFromArithmeticValue(SrcType src_val,
                                                       SQLSMALLINT sql_type) {
  using odbc_internal::SQLStates;
  using odbc_internal::StatusRecord;

  switch (sql_type) {
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_LONGVARCHAR: {
      return std::to_string(src_val);
    }
    case SQL_REAL:
    case SQL_FLOAT: {
      StatusRecord check_status =
          CheckLimitsArithmetic<SrcType, SQLREAL>(src_val);
      // In case of 'Numeric value out of range'(22003), we should throw error
      if (check_status.sql_state != SQLStates::k_22003()) {
        // We need to typecast it to dest type once
        SQLREAL dest_val = src_val;
        return std::to_string(dest_val);
      }
      return check_status;
    }
    case SQL_DOUBLE: {
      StatusRecord check_status =
          CheckLimitsArithmetic<SrcType, SQLDOUBLE>(src_val);
      if (check_status.sql_state != SQLStates::k_22003()) {
        // We need to typecast it to dest type once
        SQLDOUBLE dest_val = src_val;
        return std::to_string(dest_val);
      }
      return check_status;
    }
    case SQL_BIGINT: {
      StatusRecord check_status =
          CheckLimitsArithmetic<SrcType, SQLBIGINT>(src_val);
      if (check_status.sql_state != SQLStates::k_22003()) {
        // We need to typecast it to dest type once
        SQLBIGINT dest_val = src_val;
        return std::to_string(dest_val);
      }
      return check_status;
    }
    case SQL_SMALLINT: {
      StatusRecord check_status =
          CheckLimitsArithmetic<SrcType, SQLSMALLINT>(src_val);
      if (check_status.sql_state != SQLStates::k_22003()) {
        // We need to typecast it to dest type once
        SQLSMALLINT dest_val = src_val;
        return std::to_string(dest_val);
      }
      return check_status;
    }
    case SQL_TINYINT: {
      StatusRecord check_status =
          CheckLimitsArithmetic<SrcType, SQLCHAR>(src_val);
      if (check_status.sql_state != SQLStates::k_22003()) {
        // We need to typecast it to dest type once
        SQLCHAR dest_val = src_val;
        return std::to_string(dest_val);
      }
      return check_status;
    }
    case SQL_INTEGER: {
      StatusRecord check_status =
          CheckLimitsArithmetic<SrcType, SQLINTEGER>(src_val);
      if (check_status.sql_state != SQLStates::k_22003()) {
        // We need to typecast it to dest type once
        SQLINTEGER dest_val = src_val;
        return std::to_string(dest_val);
      }
      return check_status;
    }
    default: {
      return StatusRecord{SQLStates::k_HY000(), "Conversion is unsupported"};
    }
  }
  return StatusRecord::Ok();
}

// Translates src_data which is for a C Data Type -> SQL Data Type ->
// std::string This function is typically used to translate SQL_PARAM_INPUT type
// of parameter binding.
odbc_internal::StatusRecordOr<std::string> ConvertFromBuffer(
    DataBuffer& src_data, SQLSMALLINT sql_type);

std::string Base64Encode(const uint8_t* data, int length);
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_INV_H
