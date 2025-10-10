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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_INTERNAL_COMMONS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_INTERNAL_COMMONS_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "absl/types/variant.h"
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
#include <arrow/api.h>
#include <arrow/io/memory.h>
#include <arrow/ipc/api.h>
#include <arrow/ipc/reader.h>
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
#include <chrono>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

// The Base64 character set
// (ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/) is a
// well-defined, standardized string that doesn’t change across different
// implementations of Base64 encoding. The characters in this string represent
// the 64 possible values used in Base64 encoding.
static std::string const kBasE64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

template <typename T>
SQLRETURN LogAndReturnCode(
    Handle& handle, odbc_internal::StatusRecordOr<T> const& status_record_or) {
  if (!status_record_or) {
    auto status_record = status_record_or.GetStatusRecord();
    // Handling special case for SQL_NO_DATA where we are not supposed to have
    // an error diagnostic message
    if (status_record.sql_state ==
        google::cloud::odbc_internal::SQLStates::k_SQL_NO_DATA()) {
      return SQL_NO_DATA;
    }
    handle.GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record_or.GetCalculatedReturnCode();
}

inline SQLRETURN LogAndReturnCode(
    Handle& handle, odbc_internal::StatusRecord const& status_record) {
  if (!status_record.ok()) {
    // Handling special case for SQL_NO_DATA where we are not supposed to have
    // an error diagnostic message
    if (status_record.sql_state ==
        google::cloud::odbc_internal::SQLStates::k_SQL_NO_DATA()) {
      return SQL_NO_DATA;
    }
    handle.GetDiagnostics().AddStatusRecord(status_record);
  }
  return status_record.CalculateReturnCode();
}

odbc_internal::StatusRecordOr<std::string> GetMissingAttributesStr(
    ConnectionHandle* conn_handle);

odbc_internal::StatusRecord ValidateAllowedAttributes(
    ConnectionHandle* conn_handle, Section const& attributes);

// Data Types as supported by the BQ DataSource.
enum BQDataType {
  kArray,
  kBigNumeric,
  kNumeric,
  kBytes,
  kInt64,
  kDate,
  kFloat64,
  kInterval,
  kGeography,
  kDatetime,
  kTime,
  kBool,
  kString,
  kRange,
  kStruct,
  kJson,
  kTimeStamp,
  kNull
};

// Contains the column data types, as represented by the data source,
// for each column in a ResultSet row.
struct ColumnSchema {
  int col_index;
  BQDataType col_type;
  bool is_mode_repeated = false;
};
bool operator==(ColumnSchema const& lhs, ColumnSchema const& rhs);
bool operator>(ColumnSchema const& lhs, ColumnSchema const& rhs);
bool operator<(ColumnSchema const& lhs, ColumnSchema const& rhs);

// Data Source Value.
using DSValue = std::vector<char>;

// Data Source Row.
using DSRow = std::vector<DSValue>;

// ResultSet rows containing data source rows.
using ResultSetRows = std::vector<DSRow>;

// Row schema representing the column schema of the
// data source row columns.
using RowSchema = std::vector<ColumnSchema>;

struct TranslatedData {
  SQLLEN row_offset;  // Offset to manage last fetch row index in case
  // of partial data fetch in SQLGetData
  DSValue data;  // To store translated data when buffer length is less
  // and SQLGetData fetches partial data.
  SQLSMALLINT last_target_c_type;  // Holds the last fetched target data type
                                   // (SQL C data type) for the column.
  // This is used to track the type of the data fetched in the previous
  // SQLGetData call.
  int last_column_index{
      -1};  // contains the column number fetched in last SQLGetData call
};

// ResultSet structure representing the data from the BQ DataSource.
struct ResultSet {
  RowSchema row_schema;
  ResultSetRows rows;
  mutable int cursor{-1};  // points before the next row to fetch
  mutable TranslatedData translated_data;
};

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
using RowSchemaRead = std::vector<arrow::Type>;
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

DSValue const kNullValue{0};

inline bool IsDSValueNull(DSValue const& value) {
  return value.size() == 1 && value[0] == 0;
}
// converting the given string to Numeric number
// getting scale ,precision, sign and the value from sting parameter
odbc_internal::StatusRecord GetNumericDetailsFromStr(
    std::string const& src_dsval, SQL_NUMERIC_STRUCT& numst);
inline void StringToDSValue(std::string const& str, DSValue& value) {
  value.resize(str.size());
  std::copy(str.begin(), str.end(), value.begin());
}

// Func to decode a Base64-encoded string into a vector of bytes.
inline void Base64Decode(std::string const& encoded,
                         std::vector<uint8_t>& result) {
  int val = 0;
  int valb = -8;

  for (unsigned char c : encoded) {
    if (!absl::StrContains(kBasE64Chars, c)) {
      break;  // Stop at non-base64 characters (ignore padding)
    }

    val = (val << 6) + kBasE64Chars.find(c);
    valb += 6;

    if (valb >= 0) {
      result.push_back((val >> valb) & 0xFF);
      valb -= 8;
    }
  }
}

inline void NumericToDSValue(std::string const& str, DSValue& DSval) {
  DSval.resize(str.size());
  std::copy(str.begin(), str.end(), DSval.begin());
}
// Function to convert byte data to a hex string
inline void BytesToHex(std::vector<uint8_t> const& data,
                       std::string& restult_str) {
  std::stringstream ss;
  for (auto byte : data) {
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
       << static_cast<int>(byte);
  }
  restult_str = "0x" + ss.str();
}

// Function to convert hex string to byte data
inline std::vector<uint8_t> HexToBytes(std::string const& hex) {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_string = hex.substr(i, 2);
    bytes.push_back(static_cast<uint8_t>(std::stoul(byte_string, nullptr, 16)));
  }
  return bytes;
}

#ifdef _WIN32
// Encrypt password using Windows DPAPI
std::string EncryptPassword(std::string const& password);

// Decrypt password using Windows DPAPI
std::string DecryptPassword(std::string const& encrypted_hex);
#endif  //_WIN32

inline void ArrayJsonToDSValue(std::string const& str, DSValue& value,
                               BQDataType array_type) {
  nlohmann::json json_data = nlohmann::json::parse(str);

  nlohmann::json obj;
  obj["v"] = json_data;
  if (array_type == BQDataType::kBytes) {
    // Iterate through each element in the JSON array
    for (auto& element : obj["v"]) {
      // Extract base64-encoded string
      std::string base64_str = element["v"];

      // Decode base64 string to byte data
      std::vector<uint8_t> decoded_data;
      Base64Decode(base64_str, decoded_data);

      // Convert the decoded byte data to hexadecimal
      std::string hex_str;
      BytesToHex(decoded_data, hex_str);

      element["v"] = hex_str;
    }
  }

  std::string str_data = obj.dump();
  value.resize(str_data.size());
  std::copy(str_data.begin(), str_data.end(), value.begin());
}

inline void StringToDSValue(const SQLCHAR* c_str, DSValue& value) {
  std::string str = reinterpret_cast<char const*>(c_str);
  StringToDSValue(str, value);
}

inline void DSValueToString(DSValue const& value, std::string& str) {
  str.assign(value.begin(), value.end());
}

inline void IntToDSValue(int64_t int_val, DSValue& ds_value) {
  ds_value.resize(sizeof(int_val));
  std::memcpy(ds_value.data(), &int_val, sizeof(int64_t));
}

template <typename SrcType>
inline void ArithmeticToDSValue(SrcType arithmetic_val, DSValue& ds_value) {
  ds_value.resize(sizeof(SrcType));
  std::memcpy(ds_value.data(), &arithmetic_val, sizeof(SrcType));
}

template <typename SrcType>
inline SrcType DSValueToArithmetic(DSValue& ds_value) {
  SrcType val;
  std::memcpy(&val, ds_value.data(), sizeof(val));
  return val;
}

inline int64_t DSValueToInt(DSValue& ds_value) {
  int64_t int_val;
  std::memcpy(&int_val, ds_value.data(), sizeof(int_val));
  return int_val;
}

inline void DateToDSValue(const SQL_DATE_STRUCT& date, DSValue& value) {
  value.resize(sizeof(SQL_DATE_STRUCT));
  std::memcpy(value.data(), &date, sizeof(SQL_DATE_STRUCT));
}

inline SQL_DATE_STRUCT DSValueToDate(DSValue const& value,
                                     SQL_DATE_STRUCT& date_struct) {
  std::memcpy(&date_struct, value.data(), sizeof(SQL_DATE_STRUCT));
  return date_struct;
}

inline std::string FormatTimestampToString(
    const SQL_TIMESTAMP_STRUCT& timestamp) {
  char buffer[30];
  auto const* timestamp_format = (timestamp.fraction ==0) ? "%04d-%02d-%02d %02d:%02d:%02d" : "%04d-%02d-%02d %02d:%02d:%02d.%06d";
  snprintf(buffer, sizeof(buffer), timestamp_format, timestamp.year, timestamp.month,
           timestamp.day, timestamp.hour, timestamp.minute, timestamp.second,
           timestamp.fraction);
  return buffer;
}

inline std::string FormateDatetimeToString(const SQL_TIMESTAMP_STRUCT& datetime){
  char buffer[30];
  auto const* datetime_format = (datetime.fraction ==0) ? "%04d-%02d-%02dT%02d:%02d:%02d" : "%04d-%02d-%02dT%02d:%02d:%02d.%06d";
  snprintf(buffer, sizeof(buffer), datetime_format,
           datetime.year, datetime.month, datetime.day, datetime.hour,
           datetime.minute, datetime.second, datetime.fraction);
  return buffer;
}

inline void TimestampToDSValue(const SQL_TIMESTAMP_STRUCT& timestamp,
                               DSValue& value) {
  value.resize(sizeof(SQL_TIMESTAMP_STRUCT));
  std::memcpy(value.data(), &timestamp, sizeof(SQL_TIMESTAMP_STRUCT));
}

inline void DSValueToTimestamp(DSValue const& value,
                               SQL_TIMESTAMP_STRUCT& timestamp_struct) {
std::memcpy(&timestamp_struct, value.data(), sizeof(SQL_TIMESTAMP_STRUCT));
}

inline void DSValueToDatetime(DSValue const& value,
                               SQL_TIMESTAMP_STRUCT& timestamp_struct){
std::memcpy(&timestamp_struct, value.data(), sizeof(SQL_TIMESTAMP_STRUCT));
}

inline void TimeToDSValue(const SQL_TIME_STRUCT& time, DSValue& value) {
  value.resize(sizeof(SQL_TIME_STRUCT));
  std::memcpy(value.data(), &time, sizeof(SQL_TIME_STRUCT));
}

inline SQL_TIME_STRUCT DSValueToTime(DSValue const& value,
                                     SQL_TIME_STRUCT& time_struct) {
  std::memcpy(&time_struct, value.data(), sizeof(SQL_TIME_STRUCT));
  return time_struct;
}

inline std::string FormatTimetoString(const SQL_TIME_STRUCT& time) {
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", time.hour, time.minute,
           time.second);
  return buffer;
}

template <typename SrcType>
inline std::string FormatFloatToString(SrcType val) {
  std::ostringstream oss;
  if constexpr (std::is_same_v<SrcType, double>) {
    oss << std::setprecision(std::numeric_limits<double>::max_digits10);
  } else if constexpr (std::is_same_v<SrcType, float>) {
    oss << std::setprecision(std::numeric_limits<float>::max_digits10);
  } else {
    oss << std::setprecision(8);  // fallback for other cases
  }
  oss << std::defaultfloat << val;
  return oss.str();
}

inline void BooleanToDSValue(bool bool_val, DSValue& value) {
  std::string str_val = bool_val ? "true" : "false";
  value.resize(str_val.size());
  std::copy(str_val.begin(), str_val.end(), value.begin());
}

inline void DSValueToBoolean(DSValue const& value, bool& bool_val) {
  std::string str_value(value.begin(), value.end());
  bool_val = !(str_value == "false" || str_value == "0" || str_value.empty());
}

std::string FormatIntervalToString(SQL_INTERVAL_STRUCT interval);
odbc_internal::StatusRecordOr<std::string> FormatDateToString(
    SQL_DATE_STRUCT date);

std::string FormatNumericToString(SQL_NUMERIC_STRUCT numeric);

odbc_internal::StatusRecord ConvertUnixTimestampToTimestampStruct(
    double unix_timestamp, SQL_TIMESTAMP_STRUCT& timestamp_struct);

odbc_internal::StatusRecord ConvertStringToIntervalStruct(
    std::string const& interval_str, SQL_INTERVAL_STRUCT& interval_struct);

inline void GetSinglePrecisionInterval(
    const SQL_INTERVAL_STRUCT interval_struct, SQLUINTEGER& value) {
  using odbc_internal::StatusRecord;
  StatusRecord status_record = StatusRecord::Ok();

  switch (interval_struct.interval_type) {
    case SQL_IS_YEAR:
      value = interval_struct.intval.year_month.year;
      break;
    case SQL_IS_MONTH:
      value = interval_struct.intval.year_month.month;
      break;
    case SQL_IS_DAY:
      value = interval_struct.intval.day_second.day;
      break;
    case SQL_IS_HOUR:
      value = interval_struct.intval.day_second.hour;
      break;
    case SQL_IS_MINUTE:
      value = interval_struct.intval.day_second.minute;
      break;
    case SQL_IS_SECOND:
      value = interval_struct.intval.day_second.second;
      break;
    default:
      status_record = odbc_internal::StatusRecord{
          odbc_internal::SQLStates::k_07006(),
          "Interval precision was not a single field"};
      break;
  }
}

// This is the result populated by performing a bq query API.
// For each call, onely one of PostQueryResults or GetQueryResults will be
// populated with the following semantics:
//
// PostQueryResults:
//   - Query finished in specified or default timeout.
//   - All query results rows will be present in PostQueryResults.
// GetQueryResults
//   - Query did not finish in specified or default timeout and odbc bq client's
//     GetAllQueryResults was called.
//   - All query results rows will be present in GetQueryResults.
struct DSResults {
  absl::variant<absl::monostate,
                google::cloud::bigquery_v2_minimal_internal::PostQueryResults,
                google::cloud::bigquery_v2_minimal_internal::GetQueryResults,
                google::cloud::odbc_bq_driver_internal::ResultSet>
      data_source_results;
  std::int64_t num_dml_affected_rows = 0;
  // We need optional here because:
  //   1) In async execution, PostQuery might not have finished.
  //   2) No job has been created using JOB_CREATION_OPTIONAL
  std::optional<google::cloud::bigquery_v2_minimal_internal::JobReference>
      job_ref;
};

//////////////////////////////////////////////////////////////////////
// Common Helper functions related to BQ data source.
/////////////////////////////////////////////////////////////////////
odbc_internal::StatusRecordOr<DSResults> FetchBQData(
    ConnectionHandle& conn_handle,
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest const&
        post_query_request);

odbc_internal::StatusRecordOr<google::cloud::bigquery_v2_minimal_internal::Job>
CancelBQJob(ConnectionHandle& conn_handle, std::string const& job_id,
            std::string const& location = "");

////////////////////////////////////////////////////////////////////////
// Common Helper functions for processing data results from BQ data source and
// converting that to ODBC result sets.
////////////////////////////////////////////////////////////////////////

odbc_internal::StatusRecordOr<ResultSet> ProcessResultSetRows(
    google::cloud::bigquery_v2_minimal_internal::TableSchema const& schema,
    std::vector<google::cloud::bigquery_v2_minimal_internal::RowData> const&
        rows);

odbc_internal::StatusRecordOr<ResultSet> ProcessPostQueryResults(
    google::cloud::bigquery_v2_minimal_internal::PostQueryResults const&
        post_query_results);

odbc_internal::StatusRecordOr<ResultSet> ProcessGetQueryResults(
    google::cloud::bigquery_v2_minimal_internal::GetQueryResults const&
        get_query_results);

odbc_internal::StatusRecordOr<ResultSet> ProcessQueryResults(
    DSResults const& query_results);

odbc_internal::StatusRecordOr<
    std::vector<google::cloud::bigquery_v2_minimal_internal::RowData>>
GetRowsResults(DSResults const& query_results);

///////////////////////////////////////////////////////
// Common helper functions.
////////////////////////////////////////////////////////
odbc_internal::StatusRecordOr<BQDataType> ConvertDSType(
    std::string const& type);

odbc_internal::StatusRecordOr<SQLSMALLINT> GetSQLDataType(
    std::string const& type, bool isArray = false);

odbc_internal::StatusRecordOr<
    google::cloud::bigquery_v2_minimal_internal::QueryParameter>
ConstructStringArrayQueryParameter(
    std::string const& parameter_name,
    std::vector<std::string> const& parameter_values);

odbc_internal::StatusRecordOr<
    google::cloud::bigquery_v2_minimal_internal::QueryParameter>
ConstructStringQueryParameter(std::string const& parameter_name,
                              std::string const& parameter_value);

odbc_internal::StatusRecordOr<
    std::vector<google::cloud::bigquery_v2_minimal_internal::QueryParameter>>
ConstructStringQueryParameters(
    std::map<std::string, std::string> const& params);

google::cloud::bigquery_v2_minimal_internal::PostQueryRequest
ConstructBasicPostQueryRequest(ConnectionHandle const& conn_handle,
                               std::string const& query_str,
                               int query_timeout = 0);

odbc_internal::StatusRecordOr<
    google::cloud::bigquery_v2_minimal_internal::PostQueryRequest>
ConstructNamedParametersPostQueryRequest(
    std::string const& catalog, std::string const& dataset,
    std::string const& named_query,
    std::vector<
        google::cloud::bigquery_v2_minimal_internal::QueryParameter> const&
        named_query_params);

odbc_internal::StatusRecordOr<SQL_TIMESTAMP_STRUCT>
ConvertStringToTimestampStruct(std::string const& date_str);

enum class LanguageDialect {
  kStandardSQL = 1,
  kLegacySQL = 0,
};

odbc_internal::StatusRecordOr<std::string> GetDataTypeInStr(BQDataType type);

odbc_internal::StatusRecordOr<
    google::cloud::bigquery_v2_minimal_internal::TableSchema>
BuildTableSchemaFromRowSchema(
    RowSchema& row_schema,
    std::map<std::string, ColumnSchema> const& metadata_schema);
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_INTERNAL_COMMONS_H
