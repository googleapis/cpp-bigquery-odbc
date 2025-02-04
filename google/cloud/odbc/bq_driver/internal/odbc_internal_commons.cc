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

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::Options;
using ::google::cloud::bigquery_v2_minimal_internal::DatasetReference;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::JobCreationMode;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterType;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameterValue;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using ::google::cloud::bigquery_v2_minimal_internal::TableFieldSchema;
using ::google::cloud::bigquery_v2_minimal_internal::TableSchema;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using json = nlohmann::json;

// Constants for Unix timestamp calculations
int const kSecondsPerDay = 86400;
int const kSecondsPerYear = 31536000;
int const kSecondsPerLeapYear = 31622400;  // 366 days
int const kSecondsPerHour = 3600;
int const kSecondsPerMinute = 60;

bool IsLeapYear(int year) {
  return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

int DaysInMonth(int year, int month) {
  static int const kDaysInMonth[] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};
  if (month == 2 && IsLeapYear(year)) {
    return 29;
  }
  return kDaysInMonth[month - 1];
}

SQL_TIMESTAMP_STRUCT ConvertUnixTimestampToTimestampStruct(
    double unix_timestamp) {
  SQL_TIMESTAMP_STRUCT timestamp_struct;

  // Calculate whole seconds and fractional part
  auto total_seconds = static_cast<time_t>(unix_timestamp);
  int fractional_part =
      round((unix_timestamp - total_seconds) * 1000000);  // Microseconds

  // Calculate the date and time components
  int year = 1970;
  while (total_seconds >=
         (IsLeapYear(year) ? kSecondsPerLeapYear : kSecondsPerYear)) {
    total_seconds -= (IsLeapYear(year) ? kSecondsPerLeapYear : kSecondsPerYear);
    ++year;
  }

  int month = 1;
  while (total_seconds >= (DaysInMonth(year, month) * kSecondsPerDay)) {
    total_seconds -= (DaysInMonth(year, month) * kSecondsPerDay);
    ++month;
  }

  int day = total_seconds / kSecondsPerDay + 1;
  total_seconds %= kSecondsPerDay;

  int hour = total_seconds / kSecondsPerHour;
  total_seconds %= kSecondsPerHour;

  int minute = total_seconds / kSecondsPerMinute;
  total_seconds %= kSecondsPerMinute;

  int second = total_seconds;

  // Fill SQL_TIMESTAMP_STRUCT
  timestamp_struct.year = static_cast<int16_t>(year);
  timestamp_struct.month = static_cast<unsigned char>(month);
  timestamp_struct.day = static_cast<unsigned char>(day);
  timestamp_struct.hour = static_cast<unsigned char>(hour);
  timestamp_struct.minute = static_cast<unsigned char>(minute);
  timestamp_struct.second = static_cast<unsigned char>(second);
  timestamp_struct.fraction = fractional_part;

  return timestamp_struct;
}

SQL_DATE_STRUCT ConvertStringToDateStruct(std::string const& date_str) {
  if (date_str.empty() || date_str.size() < SQL_DATE_LEN) {
    throw std::invalid_argument(
        "Invalid date string format: the string is either empty or too short.");
  }
  int year = std::stoi(date_str.substr(0, 4));
  int month = std::stoi(date_str.substr(5, 2));
  int day = std::stoi(date_str.substr(8, 2));

  SQL_DATE_STRUCT date_struct;
  date_struct.year = static_cast<SQLSMALLINT>(year);
  date_struct.month = static_cast<SQLUSMALLINT>(month);
  date_struct.day = static_cast<SQLUSMALLINT>(day);
  return date_struct;
}

SQL_TIME_STRUCT ConvertToTimeStruct(std::string const& time_str) {
  int hr = std::stoi(time_str.substr(0, 2));
  int min = std::stoi(time_str.substr(3, 2));
  int sec = std::stoi(time_str.substr(6, 2));

  SQL_TIME_STRUCT time;
  time.hour = static_cast<SQLSMALLINT>(hr);
  time.minute = static_cast<SQLUSMALLINT>(min);
  time.second = static_cast<SQLUSMALLINT>(sec);
  return time;
}

void ConvertStringToIntervalStruct(std::string const& interval_str,
                                   SQL_INTERVAL_STRUCT& interval_struct) {
  if (interval_str.empty()) {
    throw std::invalid_argument("Interval string can't be empty.");
    return;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int fraction = 0;
  int matched_items =
      std::sscanf(interval_str.c_str(), "%d-%d %d %d:%d:%d.%d", &year, &month,
                  &day, &hour, &minute, &second, &fraction);
  if (matched_items == 6) {
    fraction = 0;
  } else if (matched_items != 7) {
    throw std::invalid_argument("Invalid interval string format");
    return;
  }
  interval_struct.interval_sign =
      (year < 0 || month < 0 || day < 0 || hour < 0 || minute < 0 || second < 0)
          ? -1
          : 1;

  if (year != 0 || month != 0) {
    if (day == 0 && hour == 0 && minute == 0 && second == 0) {
      if (year != 0 && month == 0) {
        interval_struct.interval_type = SQL_IS_YEAR;
        interval_struct.intval.year_month.year = static_cast<SQLUINTEGER>(year);
      } else if (year == 0 && month != 0) {
        interval_struct.interval_type = SQL_IS_MONTH;
        interval_struct.intval.year_month.month =
            static_cast<SQLUINTEGER>(month);
      } else if (year != 0 && month != 0) {
        interval_struct.interval_type = SQL_IS_YEAR_TO_MONTH;
        interval_struct.intval.year_month.year = static_cast<SQLUINTEGER>(year);
        interval_struct.intval.year_month.month =
            static_cast<SQLUINTEGER>(month);
      }
    }
  } else if (day != 0 || hour != 0 || minute != 0 || second != 0) {
    if (hour == 0 && minute == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_DAY;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
    } else if (day == 0 && minute == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_HOUR;
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
    } else if (day == 0 && hour == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_MINUTE;
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
    } else if (day == 0 && hour == 0 && minute == 0) {
      interval_struct.interval_type = SQL_IS_SECOND;
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    } else if (day != 0 && hour != 0 && minute == 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_DAY_TO_HOUR;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
    } else if (day != 0 && minute != 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_DAY_TO_MINUTE;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
    } else if (day == 0 && hour != 0 && minute != 0 && second == 0) {
      interval_struct.interval_type = SQL_IS_HOUR_TO_MINUTE;
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
    } else if (day == 0 && hour != 0 && second != 0) {
      interval_struct.interval_type = SQL_IS_HOUR_TO_SECOND;
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    } else if (day == 0 && hour == 0 && minute != 0 && second != 0) {
      interval_struct.interval_type = SQL_IS_MINUTE_TO_SECOND;
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    } else {
      interval_struct.interval_type = SQL_IS_DAY_TO_SECOND;
      interval_struct.intval.day_second.day = static_cast<SQLUINTEGER>(day);
      interval_struct.intval.day_second.hour = static_cast<SQLUINTEGER>(hour);
      interval_struct.intval.day_second.minute =
          static_cast<SQLUINTEGER>(minute);
      interval_struct.intval.day_second.second =
          static_cast<SQLUINTEGER>(second);
    }
  }
}

std::string FormatIntervalToString(const SQL_INTERVAL_STRUCT interval) {
  char buffer[80];
  switch (interval.interval_type) {
    case SQL_IS_YEAR:
      snprintf(buffer, sizeof(buffer), "%d-0 0 0:0:0",
               interval.intval.year_month.year);
      break;
    case SQL_IS_MONTH:
      snprintf(buffer, sizeof(buffer), "0-%d 0 0:0:0",
               interval.intval.year_month.month);
      break;
    case SQL_IS_YEAR_TO_MONTH:
      snprintf(buffer, sizeof(buffer), "%d-%d 0 0:0:0",
               interval.intval.year_month.year,
               interval.intval.year_month.month);
      break;
    case SQL_IS_DAY:
      snprintf(buffer, sizeof(buffer), "0-0 %d 0:0:0",
               interval.intval.day_second.day);
      break;
    case SQL_IS_HOUR:
      snprintf(buffer, sizeof(buffer), "0-0 0 %d:0:0",
               interval.intval.day_second.hour);
      break;
    case SQL_IS_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:0",
               interval.intval.day_second.minute);
      break;
    case SQL_IS_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:0:%d.%09d",
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:0:%d",
                 interval.intval.day_second.second);
      }
      break;
    case SQL_IS_DAY_TO_HOUR:
      snprintf(buffer, sizeof(buffer), "0-0 %d %d:0:0",
               interval.intval.day_second.day, interval.intval.day_second.hour);
      break;
    case SQL_IS_DAY_TO_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:0",
               interval.intval.day_second.day, interval.intval.day_second.hour,
               interval.intval.day_second.minute);
      break;
    case SQL_IS_DAY_TO_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:%d.%09d",
                 interval.intval.day_second.day,
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 %d %d:%d:%d",
                 interval.intval.day_second.day,
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second);
      }
      break;
    case SQL_IS_HOUR_TO_MINUTE:
      snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:0",
               interval.intval.day_second.hour,
               interval.intval.day_second.minute);
      break;
    case SQL_IS_HOUR_TO_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:%d.%09d",
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 0 %d:%d:%d",
                 interval.intval.day_second.hour,
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second);
      }
      break;
    case SQL_IS_MINUTE_TO_SECOND:
      if (interval.intval.day_second.fraction != 0) {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:%d.%09d",
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second,
                 interval.intval.day_second.fraction);
      } else {
        snprintf(buffer, sizeof(buffer), "0-0 0 0:%d:%d",
                 interval.intval.day_second.minute,
                 interval.intval.day_second.second);
      }
      break;
    default:
      snprintf(buffer, sizeof(buffer), "Unknown interval type");
      break;
  }
  return std::string(buffer);
}

SQL_TIMESTAMP_STRUCT ConvertStringToTimestampStruct(
    std::string const& date_str) {
  SQL_TIMESTAMP_STRUCT date_struct = {};

  int year = std::stoi(date_str.substr(0, 4));
  int month = std::stoi(date_str.substr(5, 2));
  int day = std::stoi(date_str.substr(8, 2));
  int hour = std::stoi(date_str.substr(11, 2));
  int minute = std::stoi(date_str.substr(14, 2));
  int second = std::stoi(date_str.substr(17, 2));

  int fraction = 0;
  std::size_t dot_pos = date_str.find('.', 19);
  if (dot_pos != std::string::npos) {
    std::string fraction_str = date_str.substr(dot_pos + 1);
    fraction_str = fraction_str.substr(0, 6);
    while (fraction_str.length() < 6) {
      fraction_str += "0";
    }
    fraction = std::stoi(fraction_str);
  }

  date_struct.year = static_cast<SQLSMALLINT>(year);
  date_struct.month = static_cast<SQLUSMALLINT>(month);
  date_struct.day = static_cast<SQLUSMALLINT>(day);
  date_struct.hour = static_cast<SQLUSMALLINT>(hour);
  date_struct.minute = static_cast<SQLUSMALLINT>(minute);
  date_struct.second = static_cast<SQLUSMALLINT>(second);
  date_struct.fraction = static_cast<SQLUINTEGER>(fraction);

  return date_struct;
}

StatusRecordOr<ResultSet> ProcessResultSetRows(
    TableSchema const& schema, std::vector<RowData> const& rows) {
  ResultSet result_set;
  // Populate the schema for each row. The row schema
  // indicates how they should converted back for the application buffers in
  // SQLFetch.
  for (int i = 0; i < schema.fields.size(); i++) {
    TableFieldSchema table_field_schema = schema.fields[i];
    StatusRecordOr<BQDataType> type_status_record =
        ConvertDSType(table_field_schema.type);
    if (!type_status_record.Ok()) {
      return type_status_record.GetStatusRecord();
    }
    ColumnSchema col_schema;
    col_schema.col_index = i;
    col_schema.col_type = *type_status_record;
    result_set.row_schema.emplace_back(col_schema);
  }
  // Populate the data for each row.
  for (auto const& row : rows) {
    DSRow rs_row;
    int i = 0;
    for (auto const& col : row.columns) {
      BQDataType col_type = result_set.row_schema[i++].col_type;
      std::string data = col.value;
      if (col.is_null) {
        rs_row.emplace_back(kNullValue);
      } else if (!data.empty()) {
        DSValue row_val;
        switch (col_type) {
          case BQDataType::kString: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kInt64: {
            SQLBIGINT l_data = std::stoll(data);
            ArithmeticToDSValue<SQLBIGINT>(l_data, row_val);
            break;
          }
          case BQDataType::kFloat64: {
            SQLDOUBLE d_data = std::stod(data);
            ArithmeticToDSValue<SQLDOUBLE>(d_data, row_val);
            break;
          }
          case BQDataType::kJson: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kDate: {
            SQL_DATE_STRUCT date_struct = ConvertStringToDateStruct(data);
            DateToDSValue(date_struct, row_val);
            break;
          }
          case BQDataType::kTime: {
            SQL_TIME_STRUCT t_data = ConvertToTimeStruct(data);
            TimeToDSValue(t_data, row_val);
            break;
          }
          case BQDataType::kTimeStamp: {
            double unix_timestamp = std::stod(data);
            SQL_TIMESTAMP_STRUCT time_struct =
                ConvertUnixTimestampToTimestampStruct(unix_timestamp);
            TimestampToDSValue(time_struct, row_val);
            break;
          }
          case BQDataType::kInterval: {
            StringToDSValue(data, row_val);
            break;
          }
          case BQDataType::kDatetime: {
            SQL_TIMESTAMP_STRUCT time_struct =
                ConvertStringToTimestampStruct(data);
            TimestampToDSValue(time_struct, row_val);
            break;
          }
          case BQDataType::kBool: {
            bool bool_val = false;
            std::transform(data.begin(), data.end(), data.begin(), ::tolower);
            if (data == "1" || data == "true" || data == "yes") {
              bool_val = true;
            } else if (data == "0" || data == "false" || data == "no") {
              bool_val = false;
            }
            BooleanToDSValue(bool_val, row_val);
            break;
          }
          default: {
            return StatusRecord{SQLStates::k_HY000(),
                                "Invalid or unsupported col BQ data type"};
          }
        }
        rs_row.emplace_back(row_val);
      }
    }
    result_set.rows.emplace_back(rs_row);
  }
  return result_set;
}

StatusRecordOr<ResultSet> ProcessPostQueryResults(
    PostQueryResults const& post_query_results) {
  if (!post_query_results.job_complete) {
    // If this method is being called then the assumption is PostQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(post_query_results.schema,
                              post_query_results.rows);
}

StatusRecordOr<ResultSet> ProcessGetQueryResults(
    GetQueryResults const& get_query_results) {
  if (!get_query_results.job_complete) {
    // If this method is being called then the assumption is GetQueryResults
    // contains all the results which in turn means job_complete would be set to
    // true.
    return StatusRecord{
        SQLStates::k_HY000(),
        "Internal Error: Unexpected value for job_complete: expecting true"};
  }
  return ProcessResultSetRows(get_query_results.schema, get_query_results.rows);
}

StatusRecordOr<ResultSet> ProcessQueryResults(DSResults const& query_results) {
  if (absl::holds_alternative<PostQueryResults>(
          query_results.data_source_results)) {
    return ProcessPostQueryResults(
        absl::get<PostQueryResults>(query_results.data_source_results));
  }
  if (absl::holds_alternative<GetQueryResults>(
          query_results.data_source_results)) {
    return ProcessGetQueryResults(
        absl::get<GetQueryResults>(query_results.data_source_results));
  }
  return StatusRecord{SQLStates::k_HY000(), "Invalid query results object"};
}

StatusRecordOr<std::vector<RowData>> GetRowsResults(
    DSResults const& query_results) {
  if (absl::holds_alternative<PostQueryResults>(
          query_results.data_source_results)) {
    auto results =
        absl::get<PostQueryResults>(query_results.data_source_results);
    if (!results.job_complete) {
      return StatusRecord{
          SQLStates::k_HY000(),
          "Internal Error: Unexpected value for job_complete: expecting true"};
    }
    return results.rows;
  }
  if (absl::holds_alternative<GetQueryResults>(
          query_results.data_source_results)) {
    auto results =
        absl::get<GetQueryResults>(query_results.data_source_results);
    if (!results.job_complete) {
      return StatusRecord{
          SQLStates::k_HY000(),
          "Internal Error: Unexpected value for job_complete: expecting true"};
    }
    return results.rows;
  }
  return StatusRecord{SQLStates::k_HY000(), "Invalid query results object"};
}

StatusRecordOr<Job> CancelBQJob(ConnectionHandle& conn_handle,
                                std::string const& job_id,
                                std::string const& location) {
  // validate we have a job.
  if (job_id.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid or empty job id"};
  }
  // Validate the  connection handle.
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  // Validate we have a bq client.
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }
  // validate we have a project_id.
  std::string project_id = conn_handle.GetDsn().catalog;
  if (project_id.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Invalid or empty catalog in connection handle"};
  }

  Options options;
  return bq_client->CancelJob(project_id, job_id, location, options);
}

// TODO(b/388947009): Add unit tests for this function
StatusRecordOr<DSResults> FetchBQData(
    ConnectionHandle& conn_handle, PostQueryRequest const& post_query_request) {
  std::cout << "FetchBQData called:: " << std::endl;
  // Validate the  connection handle.
  if (!conn_handle.IsConnected()) {
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  // nlohmann::json req;
  // to_json(req, post_query_request);
  // std::cout << "PostQueryRequest:: " << req.dump(4) << std::endl;

  // For now , we use default options.
  // We can set timeout here as needed later.
  Options options;
  auto pq_status = bq_client->PostQuery(post_query_request, options);
  if (!pq_status) {
    return pq_status.GetStatusRecord();
  }
  // nlohmann::json resp;
  // to_json(resp, *pq_status);
  // std::cout << "PostQueryResults:: " << resp.dump(4) << std::endl;

  DSResults results;
  results.dml_stats = pq_status->dml_stats;
  results.job_ref = pq_status->job_reference;
  std::cout << "FetchBQData:: CP3" << std::endl;
  if (pq_status->job_complete && pq_status->page_token.empty()) {
    std::cout << "FetchBQData:: CP4" << std::endl;
    // we have gotten all the results
    results.data_source_results = *pq_status;
  } else {
    std::cout << "FetchBQData:: CP5" << std::endl;
    // Call GetAllQueryResults to get all the query results.
    auto gq_status = bq_client->GetAllQueryResults(
        pq_status->job_reference.project_id, pq_status->job_reference.job_id,
        pq_status->job_reference.location,
        post_query_request.query_request().timeout(), options);
    if (!gq_status) {
      return gq_status.GetStatusRecord();
    }
    std::cout << "FetchBQData:: CP6" << std::endl;
    results.data_source_results = *gq_status;
  }
  if (!conn_handle.IsSessionStarted() &&
      !pq_status->session_info.session_id.empty()) {
    conn_handle.SetSessionId(pq_status->session_info.session_id);
  }
  return results;
}

StatusRecordOr<BQDataType> ConvertDSType(std::string const& type) {
  if (type == "STRING") {
    return BQDataType::kString;
  }
  if (type == "INTEGER" || type == "INT64") {
    return BQDataType::kInt64;
  }
  if (type == "BOOL" || type == "BOOLEAN") {
    return BQDataType::kBool;
  }
  if (type == "FLOAT64" || type == "FLOAT") {
    return BQDataType::kFloat64;
  }
  if (type == "DECIMAL" || type == "NUMERIC") {
    return BQDataType::kNumeric;
  }
  if (type == "BYTES") {
    return BQDataType::kBytes;
  }
  if (type == "DATE") {
    return BQDataType::kDate;
  }
  if (type == "DATETIME") {
    return BQDataType::kDatetime;
  }
  if (type == "TIME") {
    return BQDataType::kTime;
  }
  if (type == "TIMESTAMP") {
    return BQDataType::kTimeStamp;
  }
  if (type == "BIGNUMERIC") {
    return BQDataType::kBigNumeric;
  }
  if (type == "RANGE") {
    return BQDataType::kRange;
  }
  if (type == "STRUCT" || type == "RECORD") {
    return BQDataType::kStruct;
  }
  if (type == "JSON") {
    return BQDataType::kJson;
  }
  if (type == "NULL") {
    return BQDataType::kNull;
  }
  if (type == "INTERVAL") {
    return BQDataType::kInterval;
  }
  if (type == "GEOGRAPHY") {
    return BQDataType::kGeography;
  }
  if (type == "ARRAY") {
    return BQDataType::kArray;
  }
  std::string err_msg = "Invalid Data Type: ";
  err_msg.append(type);
  return StatusRecord{SQLStates::k_HY000(), err_msg};
}

StatusRecordOr<QueryParameter> ConstructStringQueryParameter(
    std::string const& parameter_name, std::string const& parameter_value) {
  if (parameter_name.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid parameter name"};
  }

  QueryParameter query_param;
  QueryParameterType query_param_type;
  QueryParameterValue query_param_value;

  query_param_type.type = "STRING";
  query_param_value.value = parameter_value;
  query_param.name = parameter_name;
  query_param.parameter_type = query_param_type;
  query_param.parameter_value = query_param_value;

  return query_param;
}

StatusRecordOr<QueryParameter> ConstructStringArrayQueryParameter(
    std::string const& parameter_name,
    std::vector<std::string> const& parameter_values) {
  if (parameter_name.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid parameter name"};
  }
  if (parameter_values.empty()) {
    return StatusRecord{SQLStates::k_HY000(), "Empty parameter values"};
  }

  QueryParameter query_param;
  QueryParameterType query_param_type;
  QueryParameterType query_param_array_type;
  QueryParameterValue query_param_value;

  query_param_array_type.type = "STRING";
  query_param_type.type = "ARRAY";
  query_param_type.array_type =
      std::make_shared<QueryParameterType>(query_param_array_type);
  for (auto const& param_val : parameter_values) {
    QueryParameterValue query_param_array_value;
    query_param_array_value.value = param_val;
    query_param_value.array_values.push_back(query_param_array_value);
  }
  query_param.name = parameter_name;
  query_param.parameter_type = query_param_type;
  query_param.parameter_value = query_param_value;

  return query_param;
}

StatusRecordOr<std::vector<QueryParameter>> ConstructStringQueryParameters(
    std::map<std::string, std::string> const& params) {
  std::vector<QueryParameter> query_params;
  for (auto const& [parameter_name, parameter_value] : params) {
    auto query_parameter_response =
        ConstructStringQueryParameter(parameter_name, parameter_value);
    if (!query_parameter_response) {
      return query_parameter_response.GetStatusRecord();
    }
    query_params.emplace_back(*query_parameter_response);
  }
  return query_params;
}

PostQueryRequest ConstructBasicPostQueryRequest(
    ConnectionHandle const& conn_handle, std::string const& query_str,
    int query_timeout) {
  std::string catalog = conn_handle.GetDsn().catalog;
  std::string default_dataset = conn_handle.GetDsn().default_dataset;
  bool is_bq_legacy_sql = conn_handle.GetDsn().is_bq_legacy_sql;
  bool is_job_creation_required = conn_handle.GetDsn().is_job_creation_required;
  PostQueryRequest post_request;
  QueryRequest query_request;
  // Construct query request.
  query_request.set_dry_run(false);
  query_request.set_query(query_str);
  query_request.set_timeout(std::chrono::milliseconds(query_timeout * 1000));
  query_request.set_use_legacy_sql(is_bq_legacy_sql);
  if (is_job_creation_required) {
    query_request.set_job_creation_mode(JobCreationMode::Required());
  }
  if (!default_dataset.empty()) {
    DatasetReference ds_ref;
    // Set dataset info.
    ds_ref.project_id = catalog;
    ds_ref.dataset_id = default_dataset;
    query_request.set_default_dataset(ds_ref);
  }
  if (conn_handle.IsSessionStarted()) {
    query_request.set_connection_properties(
        {{"session_id", conn_handle.GetSessionId()}});
  } else if (conn_handle.GetDsn().sessions_enabled) {
    query_request.set_create_session(true);
  }

  // Set billing info and query request.
  post_request.set_project_id(catalog);
  post_request.set_query_request(query_request);
  return post_request;
}

odbc_internal::StatusRecordOr<PostQueryRequest>
ConstructNamedParametersPostQueryRequest(
    std::string const& catalog, std::string const& dataset,
    std::string const& named_query,
    std::vector<QueryParameter> const& named_query_params) {
  if (catalog.empty()) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Cannot construct named parameter query "
                        "request: catalog name is required"};
  }
  if (dataset.empty()) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Cannot construct named parameter query "
                        "request: dataset name is required"};
  }
  if (named_query.empty()) {
    return StatusRecord{SQLStates::k_HY090(),
                        "Cannot construct named parameter query "
                        "request: parameterized query is required"};
  }
  PostQueryRequest post_request;
  QueryRequest query_request;
  DatasetReference ds_ref;
  // Set dataset info.
  ds_ref.project_id = catalog;
  ds_ref.dataset_id = dataset;
  // Construct query request.
  query_request.set_dry_run(false);
  query_request.set_default_dataset(ds_ref);
  query_request.set_query(named_query);
  // Following are specific to parameterized queries.
  query_request.set_parameter_mode("NAMED");
  query_request.set_query_parameters(named_query_params);
  query_request.set_use_legacy_sql(false);
  // Set billing info and query request.
  post_request.set_project_id(catalog);
  post_request.set_query_request(query_request);
  return post_request;
}

odbc_internal::StatusRecordOr<SQLSMALLINT> GetSQLDataType(
    std::string const& type, bool isArray) {
  if (isArray) {
    return SQL_VARCHAR;
  }
  if (type == "STRING") {
    return SQL_VARCHAR;
  }
  if (type == "INTEGER" || type == "INT64") {
    return SQL_BIGINT;
  }
  if (type == "BOOL" || type == "BOOLEAN") {
    return SQL_BIT;
  }
  if (type == "FLOAT64" || type == "FLOAT") {
    return SQL_DOUBLE;
  }
  if (type == "DECIMAL" || type == "NUMERIC" || type == "BIGNUMERIC") {
    return SQL_NUMERIC;
  }
  if (type == "BYTES") {
    return SQL_VARBINARY;
  }
  if (type == "DATE") {
    return SQL_TYPE_DATE;
  }
  if (type == "DATETIME") {
    return SQL_TYPE_TIMESTAMP;
  }
  if (type == "TIME") {
    return SQL_TYPE_TIME;
  }
  if (type == "TIMESTAMP") {
    return SQL_TYPE_TIMESTAMP;
  }
  if (type == "STRUCT" || type == "RECORD") {
    return SQL_VARCHAR;
  }
  if (type == "JSON") {
    return SQL_VARCHAR;
  }
  if (type == "INTERVAL") {
    return SQL_VARCHAR;
  }
  if (type == "GEOGRAPHY") {
    return SQL_VARCHAR;
  }
  if (type == "ARRAY") {
    return SQL_VARCHAR;
  }
  std::string err_msg = "Invalid Data Type: ";
  err_msg.append(type);
  return StatusRecord{SQLStates::k_HY000(), err_msg};
}

bool operator==(ColumnSchema const& lhs, ColumnSchema const& rhs) {
  return (lhs.col_index == rhs.col_index && lhs.col_type == rhs.col_type);
}
bool operator>(ColumnSchema const& lhs, ColumnSchema const& rhs) {
  return (lhs.col_index > rhs.col_index);
}
bool operator<(ColumnSchema const& lhs, ColumnSchema const& rhs) {
  return (lhs.col_index < rhs.col_index);
}

// verify the parameters for the connection and return any missing parameter as
// a string.
odbc_internal::StatusRecordOr<std::string> GetMissingAttributesStr(
    ConnectionHandle* conn_handle) {
  Dsn dsn = conn_handle->GetDsn();
  std::ostringstream missing;

  if (dsn.o_auth_mechanism.empty() && dsn.catalog.empty()) {
    missing << "Catalog:"
            << "Catalog=?;";
    missing << "OAuthMechanism:"
            << "OAuthMechanism=?;";
  } else {
    if (dsn.catalog.empty()) {
      missing << "Catalog:"
              << "Catalog=?;";
    }
    if (dsn.o_auth_mechanism.empty()) {
      missing << "OAuthMechanism:"
              << "OAuthMechanism=?;";
    }
    if (!dsn.o_auth_mechanism.empty() && dsn.key_file_path.empty()) {
      missing << "KeyFilePath:"
              << "KeyFilePath=?;";
    }
  }
  std::string missing_str = missing.str();
  if (!missing_str.empty()) {
    return missing_str;
  }
  return StatusRecord::Ok();
}

odbc_internal::StatusRecord ValidateAllowedAttributes(
    ConnectionHandle* conn_handle, Section const& attributes) {
  StatusRecord status_record = StatusRecord::Ok();
  Dsn dsn_fields = conn_handle->GetDsn();

  // TODO(b/384384699): Support ListProjectsParent as part of DSN from the UI
  std::unordered_map<std::string, std::string> dsn_map = {
      {"DRIVER", dsn_fields.driver},
      {"CATALOG", dsn_fields.catalog},
      {"DSN", dsn_fields.dsn_name},
      {"KEYFILEPATH", dsn_fields.key_file_path},
      {"OAUTHMECHANISM", dsn_fields.o_auth_mechanism}};

  for (auto const& [key, _] : attributes) {
    auto it = dsn_map.find(key);
    if (it != dsn_map.end()) {
      if (!it->second.empty()) {
        status_record = StatusRecord{
            SQLStates::k_HY000(), "Connection Error: Connection Attribute '" +
                                      key + "' already found!"};
      }
    } else {
      status_record = StatusRecord{
          SQLStates::k_HY000(),
          "Connection Error: Non Requested connection attribute '" + key +
              "' in ConnectionString"};
    }
  }
  return status_record;
}
}  // namespace google::cloud::odbc_bq_driver_internal
