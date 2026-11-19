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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_execute_utils.h"
#include "google/cloud/odbc/bq_client_interface/utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
#include "google/cloud/bigquery/storage/v1/bigquery_read_options.h"
#include <arrow/array/array_decimal.h>
#if defined(__has_include)
#if __has_include(<charconv>)
#include <charconv>
#endif
#endif
#include <cmath>
#include <cstring>
#endif
#include <thread>

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW) && \
    defined(__GLIBCXX__)
template bool std::operator==(std::shared_ptr<arrow::Array> const&,
                              std::nullptr_t) noexcept;
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW) &&
        // defined(__GLIBCXX__)

//////////////////////////////////////////////////////////////////
// This file has query execution related utilities which can have
// statement or descriptor handles as arguments. We have some utils
// in `odbc_internal_commons` but those cannot include any handles
// except connection handle to avoid cyclic dependencies.
//////////////////////////////////////////////////////////////////

namespace google::cloud::odbc_bq_driver_internal {

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
using ::google::cloud::bigquery::storage::v1::CreateReadSessionRequest;
using ::google::cloud::bigquery::storage::v1::ReadRowsRequest;
using ::google::cloud::bigquery::storage::v1::ReadRowsResponse;
using ::google::cloud::bigquery::storage::v1::ReadSession;
using ::google::cloud::bigquery::storage::v1::DataFormat::ARROW;
using ::google::cloud::bigquery_v2_minimal_internal::JobReference;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::TableReference;
using google::cloud::odbc_bigquery_client_interface::MaxRetriesOption;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DoubleStrToInt;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using chrono_ms = std::chrono::milliseconds;

StatusRecord ConstructPositionalQueryParams(
    DescriptorHandle& apd, DescriptorHandle& ipd,
    std::vector<QueryParameter>& basic_query_params, bool is_data_buff_req,
    int paramset_index) {
  std::vector<SQLLEN> owned_octet_lengths;  // Owns any needed octet lengths
  for (int param_ind = 0; param_ind < basic_query_params.size(); param_ind++) {
    if (!apd.HasDescriptorRecord(param_ind + 1)) {
      LOG(ERROR) << "ConstructPositionalQueryParams:: APD record missing for "
                    "parameter "
                 << (param_ind + 1);
      return StatusRecord{
          SQLStates::k_07002(),
          "Expected descriptor record does not exist during query execution."};
    }

    DescriptorRecord& apd_rec = apd.GetDescriptorRecord(param_ind + 1);
    SQLLEN* bind_offset_ptr = apd.GetHeaderRecord().bind_offset_ptr;
    SQLLEN bind_offset = bind_offset_ptr ? *bind_offset_ptr : 0;
    SQLINTEGER bind_type = apd.GetHeaderRecord().bind_type;

    SQLLEN elem_size = 0, elem_size_ind = 0;
    if (bind_type == SQL_BIND_BY_COLUMN) {
      SQLSMALLINT target_c_type = apd_rec.concise_type;
      SQLLEN app_buffer_len = apd_rec.octet_length;
      elem_size = GetElemSize(target_c_type, app_buffer_len);
      elem_size_ind = sizeof(SQLLEN);
    } else {
      elem_size = bind_type;
      elem_size_ind = bind_type;
    }

    SQLLEN row_offset = paramset_index * elem_size;
    SQLLEN row_offset_ind = paramset_index * elem_size_ind;

    SQLLEN* indicator_ptr = apd_rec.indicator_ptr;
    if (indicator_ptr) {
      indicator_ptr =
          reinterpret_cast<SQLLEN*>(reinterpret_cast<char*>(indicator_ptr) +
                                    bind_offset + row_offset_ind);
    }
    SQLLEN* octet_length_ptr = apd_rec.octet_length_ptr;
    if (octet_length_ptr) {
      octet_length_ptr =
          reinterpret_cast<SQLLEN*>(reinterpret_cast<char*>(octet_length_ptr) +
                                    bind_offset + row_offset_ind);
    }

    // SQL_NULL_DATA implies the application wants to use empty data.
    if (indicator_ptr != nullptr && *indicator_ptr == SQL_NULL_DATA) {
      basic_query_params[param_ind].parameter_value.value = "";
      continue;
    }

    bool is_data_at_exec = false;
    if (indicator_ptr && (*indicator_ptr == SQL_DATA_AT_EXEC ||
                          *indicator_ptr <= SQL_LEN_DATA_AT_EXEC_OFFSET)) {
      is_data_at_exec = true;
    }

    if (!is_data_buff_req && is_data_at_exec) {
      LOG(INFO) << "ConstructPositionalQueryParams:: Parameter "
                << (param_ind + 1) << " requires data-at-execution.";
      return StatusRecord{
          SQLStates::k_SQL_NEED_DATA(),
          "The bound param is set for SQL_DATA_AT_EXEC/SQL_LEN_DATA_AT_EXEC"};
    }

    if (!is_data_buff_req && apd_rec.data_ptr == nullptr) {
      LOG(ERROR) << "ConstructPositionalQueryParams:: Bound parameter buffer "
                    "was null for parameter "
                 << (param_ind + 1);
      return StatusRecord{SQLStates::k_HY009(),
                          "The bound param buffer was null"};
    }

    // If a data buffer is needed (e.g., for SQLPutData or SQLParamData) and
    // it's not empty, use it; otherwise, use the original pointer from the
    // application.
    SQLPOINTER buff =
        ((is_data_buff_req && is_data_at_exec) && !apd_rec.data_buffer.empty())
            ? static_cast<SQLPOINTER>(apd_rec.data_buffer.data())
            : apd_rec.data_ptr;
    if (buff && !((is_data_buff_req && is_data_at_exec) &&
                  !apd_rec.data_buffer.empty())) {
      buff = static_cast<char*>(buff) + bind_offset + row_offset;
    }

    DataBuffer data;
    if (is_data_buff_req && is_data_at_exec) {
      owned_octet_lengths.push_back(static_cast<SQLLEN>(
          apd_rec.data_buffer
              .size()));  // Handle stack-use-after-scope for octet_length
      SQLLEN* temp_octet_length_ptr = &owned_octet_lengths.back();
      data = {apd_rec.concise_type, buff, *temp_octet_length_ptr,
              temp_octet_length_ptr};
    } else {
      data = {apd_rec.concise_type, buff, apd_rec.octet_length,
              octet_length_ptr};
    }

    DescriptorRecord& ipd_rec = ipd.GetDescriptorRecord(param_ind + 1);
    if (!ipd.HasDescriptorRecord(param_ind + 1)) {
      LOG(ERROR) << "ConstructPositionalQueryParams:: IPD record missing for "
                    "parameter "
                 << (param_ind + 1);
      return StatusRecord{
          SQLStates::k_07002(),
          "Expected descriptor record does not exist during query execution."};
    }
    SQLSMALLINT sql_type = ipd_rec.concise_type;
    StatusRecordOr<std::string> conv_status = ConvertFromBuffer(data, sql_type);
    if (!conv_status) {
      LOG(ERROR) << "ConstructPositionalQueryParams::ConvertFromBuffer:: "
                 << conv_status.GetStatusRecord().message;
      return conv_status.GetStatusRecord();
    }
    std::string& value_str = *conv_status;
    // "INT64" is a special case where a string like "23.000" will not be
    // accepted by the BQ Server. For ex, this may occur when translating from
    // SQL_C_CHAR->SQL_DOUBLE.
    if (basic_query_params[param_ind].parameter_type.type == "INT64") {
      // Both integral and floating point values can be expressed as a double.
      // DoubleStrToInt will succeed for those but fail for non-arithmetic
      // value.
      StatusRecord status = DoubleStrToInt(value_str);
      if (!status.ok()) {
        return status;
      }
    }
    basic_query_params[param_ind].parameter_value.value = value_str;
  }
  return StatusRecord::Ok();
}

StatusRecordOr<DSResults> ExecuteScript(
    StatementHandle& stmt_handle, PostQueryRequest const& post_query_request) {
  ConnectionHandle* conn_handle = stmt_handle.GetConnectionHandle();
  if (!conn_handle) {
    LOG(ERROR) << "ExecuteScript:: Invalid connection handle.";
    return StatusRecord{SQLStates::k_HY009(), "Invalid statement handle"};
  }

  // Validate connection handle
  if (!conn_handle->IsConnected()) {
    LOG(ERROR) << "ExecuteScript:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle->GetClient();
  if (!bq_client) {
    LOG(ERROR) << "ExecuteScript:: Invalid or null BQ Client within the "
                  "connection handle.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }
  // Execute the query
  Options post_query_options;
  post_query_options.set<MaxRetriesOption>(conn_handle->GetDsn().max_retries);
  auto pq_status = bq_client->PostQuery(post_query_request, post_query_options);
  if (!pq_status) {
    LOG(ERROR) << "ExecuteScript::PostQuery:: "
               << pq_status.GetStatusRecord().message;
    return pq_status.GetStatusRecord();
  }

  DSResults results;
  results.job_ref = pq_status->job_reference;
  if (pq_status->job_complete && pq_status->page_token.empty()) {
    // we have gotten all the results
    results.num_dml_affected_rows = pq_status->num_dml_affected_rows;
    results.data_source_results = *pq_status;
  } else {
    // Call GetAllQueryResults to get all the query results.
    auto gq_status = bq_client->GetAllQueryResults(
        pq_status->job_reference.project_id, pq_status->job_reference.job_id,
        pq_status->job_reference.location,
        post_query_request.query_request().timeout(), post_query_options);
    if (!gq_status) {
      LOG(ERROR) << "ExecuteScript::GetAllQueryResults:: "
                 << gq_status.GetStatusRecord().message;
      return gq_status.GetStatusRecord();
    }
    results.num_dml_affected_rows = gq_status->num_dml_affected_rows;
    results.data_source_results = *gq_status;
  }

  if (!conn_handle->IsSessionStarted() &&
      !pq_status->session_info.session_id.empty()) {
    conn_handle->SetSessionId(pq_status->session_info.session_id);
  }

  auto populate_status =
      PopulateScriptChildJobs(stmt_handle, results, post_query_request);
  if (!populate_status.ok()) {
    return populate_status;
  }

  return results;
}

StatusRecord PopulateScriptChildJobs(
    StatementHandle& stmt_handle, DSResults& results,
    PostQueryRequest const& post_query_request) {
  ConnectionHandle* conn_handle = stmt_handle.GetConnectionHandle();
  if (!conn_handle || !conn_handle->IsConnected()) {
    return StatusRecord::Ok();
  }
  auto bq_client = conn_handle->GetClient();
  if (!bq_client || !results.job_ref.has_value()) {
    return StatusRecord::Ok();
  }

  Options list_job_options;
  list_job_options.set<MaxRetriesOption>(conn_handle->GetDsn().max_retries);
  auto all_jobs_status = bq_client->ListAllJobs(
      results.job_ref->project_id, results.job_ref->job_id, list_job_options);
  if (!all_jobs_status) {
    LOG(ERROR) << "PopulateScriptChildJobs::ListAllJobs:: "
               << all_jobs_status.GetStatusRecord().message;
    return all_jobs_status.GetStatusRecord();
  }

  int statement_jobs_count = 0;
  for (auto const& job_status : all_jobs_status.GetValue()) {
    if (job_status.statistics.job_query_stats.statement_type !=
            "CREATE_PROCEDURE" &&
        job_status.statistics.script_statistics.evaluation_kind.value ==
            "STATEMENT") {
      stmt_handle.SetJobData(
          job_status.job_reference.job_id,
          job_status.statistics.job_query_stats.statement_type);
      statement_jobs_count++;
    }
  }

  if (statement_jobs_count <= 1) {
    return StatusRecord::Ok();
  }

  // If there are multiple statement jobs, results currently contains the
  // output of the LAST statement executed in the script.
  // We need to fetch and return the results of the FIRST statement.
  auto job_status = stmt_handle.GetNextJobData();
  if (!job_status.Ok()) {
    LOG(ERROR) << "PopulateScriptChildJobs::GetNextJobData:: "
               << job_status.GetStatusRecord().message;
    return job_status.GetStatusRecord();
  }
  auto job_data = job_status.GetValue();
  std::string first_job_id = job_data.first;
  std::string first_statement_type = job_data.second;

  Options query_results_options;
  query_results_options.set<MaxRetriesOption>(
      conn_handle->GetDsn().max_retries);
  auto gq_status = bq_client->GetAllQueryResults(
      results.job_ref->project_id, first_job_id, results.job_ref->location,
      post_query_request.query_request().timeout(), query_results_options);

  if (!gq_status) {
    LOG(ERROR) << "PopulateScriptChildJobs::GetAllQueryResults:: "
               << gq_status.GetStatusRecord().message;
    return gq_status.GetStatusRecord();
  }

  if (first_statement_type == "UPDATE" || first_statement_type == "INSERT" ||
      first_statement_type == "DELETE") {
    results.num_dml_affected_rows = gq_status->num_dml_affected_rows;
  }
  results.data_source_results = *gq_status;
  stmt_handle.SetDSResults(results);

  // Unbind IRD and populate with the first child statement's schema
  DescriptorHandle& ird = stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
  ird.UnbindAllDescriptorRecordsFrom(0);
  TableReference table_fields;
  StatementHandle::PopulateIrd(ird, gq_status->schema, table_fields);

  return StatusRecord::Ok();
}

#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

StatusRecordOr<std::shared_ptr<arrow::Schema>> GetArrowSchema(
    ::google::cloud::bigquery::storage::v1::ArrowSchema const& schema_in,
    RowSchema& row_schema) {
  std::shared_ptr<arrow::Buffer> buffer =
      std::make_shared<arrow::Buffer>(schema_in.serialized_schema());
  arrow::io::BufferReader buffer_reader(buffer);
  arrow::ipc::DictionaryMemo dictionary_memo;
  auto result = arrow::ipc::ReadSchema(&buffer_reader, &dictionary_memo);
  if (!result.ok()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Internal Error: Unable to parse arrow schema"};
  }
  std::shared_ptr<arrow::Schema> schema = result.ValueOrDie();

  row_schema.clear();
  int col_index = 0;
  for (auto const& field : schema->fields()) {
    ColumnSchema col_schema = {col_index++};
    arrow::Type::type data_type = field->type()->id();
    switch (data_type) {
      case arrow::Type::INT64:
        col_schema.col_type = BQDataType::kInt64;
        break;
      case arrow::Type::DOUBLE:
        col_schema.col_type = BQDataType::kFloat64;
        break;
      case arrow::Type::STRING:
        col_schema.col_type = BQDataType::kString;
        break;
      case arrow::Type::BINARY:
        col_schema.col_type = BQDataType::kBytes;
        break;
      case arrow::Type::BOOL:
        col_schema.col_type = BQDataType::kBool;
        break;
      case arrow::Type::TIMESTAMP: {
        auto ts_type =
            std::static_pointer_cast<arrow::TimestampType>(field->type());
        if (ts_type->timezone().empty()) {
          col_schema.col_type = BQDataType::kDatetime;
        } else {
          col_schema.col_type = BQDataType::kTimeStamp;
        }
        break;
      }
      case arrow::Type::TIME64:
        col_schema.col_type = BQDataType::kTime;
        break;
      case arrow::Type::DATE32:
        col_schema.col_type = BQDataType::kDate;
        break;
      case arrow::Type::DECIMAL128:
        col_schema.col_type = BQDataType::kNumeric;
        break;
      case arrow::Type::DECIMAL256:
        col_schema.col_type = BQDataType::kBigNumeric;
        break;
      case arrow::Type::INTERVAL_MONTHS:
      case arrow::Type::INTERVAL_DAY_TIME:
      case arrow::Type::INTERVAL_MONTH_DAY_NANO:
        col_schema.col_type = BQDataType::kInterval;
        break;
      case arrow::Type::LIST:
        // For other datatypes within an array, we don't have any special
        // handling. Setting 'is_mode_repeated' is enough
        if (static_cast<arrow::ListType const*>(field->type().get())
                ->value_type()
                ->ToString() == "binary") {
          col_schema.col_type = BQDataType::kBytes;
        }
        col_schema.is_mode_repeated = true;
        break;
      case arrow::Type::STRUCT: {
        col_schema.col_type = BQDataType::kString;
        break;
      }
      default:
        return StatusRecord{SQLStates::k_HY000(),
                            "Internal Error: Unsupported arrow data type"};
    }
    row_schema.emplace_back(col_schema);
  }
  return schema;
}

StatusRecordOr<std::shared_ptr<arrow::RecordBatch>> GetArrowRecordBatch(
    ::google::cloud::bigquery::storage::v1::ArrowRecordBatch const&
        record_batch_in,
    std::shared_ptr<arrow::Schema> schema) {
  std::shared_ptr<arrow::Buffer> buffer = std::make_shared<arrow::Buffer>(
      record_batch_in.serialized_record_batch());
  arrow::io::BufferReader buffer_reader(buffer);
  arrow::ipc::DictionaryMemo dictionary_memo;
  arrow::ipc::IpcReadOptions read_options;
  auto result = arrow::ipc::ReadRecordBatch(schema, &dictionary_memo,
                                            read_options, &buffer_reader);
  if (!result.ok()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Internal Error: Unable to parse record batch"};
  }
  std::shared_ptr<arrow::RecordBatch> record_batch = result.ValueOrDie();
  return record_batch;
}

namespace {

constexpr char kTwoDigits[201] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

inline void CivilFromDays(int64_t z, int64_t& year, uint32_t& month,
                          uint32_t& day) {
  z += 719468;
  int64_t const era = (z >= 0 ? z : z - 146096) / 146097;
  auto const doe = static_cast<uint32_t>(z - era * 146097);
  uint32_t const yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t const y = static_cast<int64_t>(yoe) + era * 400;
  uint32_t const doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t const mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp < 10 ? mp + 3 : mp - 9;
  year = y + (month <= 2 ? 1 : 0);
}

inline void AppendYearMonthDay(int64_t year, uint32_t month, uint32_t day,
                               DSValue& out) {
  if (year >= 0 && year <= 9999) {
    char buf[10];
    auto const y_hi = static_cast<uint32_t>(year / 100) * 2;
    auto const y_lo = static_cast<uint32_t>(year % 100) * 2;
    uint32_t const m2 = month * 2;
    uint32_t const d2 = day * 2;
    buf[0] = kTwoDigits[y_hi];
    buf[1] = kTwoDigits[y_hi + 1];
    buf[2] = kTwoDigits[y_lo];
    buf[3] = kTwoDigits[y_lo + 1];
    buf[4] = '-';
    buf[5] = kTwoDigits[m2];
    buf[6] = kTwoDigits[m2 + 1];
    buf[7] = '-';
    buf[8] = kTwoDigits[d2];
    buf[9] = kTwoDigits[d2 + 1];
    out.insert(out.end(), buf, buf + 10);
  } else {
    std::string s;
    if (year < 0 && year >= -9999) {
      auto const abs_y = static_cast<uint32_t>(-year);
      uint32_t const y_hi = (abs_y / 100) * 2;
      uint32_t const y_lo = (abs_y % 100) * 2;
      char ybuf[5] = {'-', kTwoDigits[y_hi], kTwoDigits[y_hi + 1],
                      kTwoDigits[y_lo], kTwoDigits[y_lo + 1]};
      s = absl::StrCat(absl::string_view(ybuf, 5), "-",
                       absl::string_view(&kTwoDigits[month * 2], 2), "-",
                       absl::string_view(&kTwoDigits[day * 2], 2));
    } else {
      s = absl::StrCat(year, "-", absl::string_view(&kTwoDigits[month * 2], 2),
                       "-", absl::string_view(&kTwoDigits[day * 2], 2));
    }
    out.insert(out.end(), s.data(), s.data() + s.size());
  }
}

inline void AppendFormattedDate32(int32_t days, DSValue& out) {
  int64_t year = 0;
  uint32_t month = 0;
  uint32_t day = 0;
  CivilFromDays(days, year, month, day);
  AppendYearMonthDay(year, month, day, out);
}

inline void AppendFormattedTimestamp(int64_t val, arrow::TimeUnit::type unit,
                                     bool has_tz, DSValue& out) {
  int64_t scale = 1;
  int frac_digits = 0;
  switch (unit) {
    case arrow::TimeUnit::SECOND:
      scale = 1;
      frac_digits = 0;
      break;
    case arrow::TimeUnit::MILLI:
      scale = 1000LL;
      frac_digits = 3;
      break;
    case arrow::TimeUnit::MICRO:
      scale = 1000000LL;
      frac_digits = 6;
      break;
    case arrow::TimeUnit::NANO:
      scale = 1000000000LL;
      frac_digits = 9;
      break;
  }

  int64_t total_sec = val / scale;
  int64_t subsec = val % scale;
  if (subsec < 0) {
    --total_sec;
    subsec += scale;
  }

  int64_t days = total_sec / 86400;
  int64_t sec_of_day = total_sec % 86400;
  if (sec_of_day < 0) {
    --days;
    sec_of_day += 86400;
  }

  int64_t year = 0;
  uint32_t month = 0;
  uint32_t day = 0;
  CivilFromDays(days, year, month, day);
  AppendYearMonthDay(year, month, day, out);

  auto const rem_s = static_cast<uint32_t>(sec_of_day);
  uint32_t const hour2 = (rem_s / 3600) * 2;
  uint32_t const rem_m = rem_s % 3600;
  uint32_t const min2 = (rem_m / 60) * 2;
  uint32_t const sec2 = (rem_m % 60) * 2;

  char tbuf[24];
  int pos = 0;
  tbuf[pos++] = ' ';
  tbuf[pos++] = kTwoDigits[hour2];
  tbuf[pos++] = kTwoDigits[hour2 + 1];
  tbuf[pos++] = ':';
  tbuf[pos++] = kTwoDigits[min2];
  tbuf[pos++] = kTwoDigits[min2 + 1];
  tbuf[pos++] = ':';
  tbuf[pos++] = kTwoDigits[sec2];
  tbuf[pos++] = kTwoDigits[sec2 + 1];

  if (frac_digits == 6) {
    auto const u = static_cast<uint32_t>(subsec);
    uint32_t const hi = (u / 10000) * 2;
    uint32_t const mid = ((u / 100) % 100) * 2;
    uint32_t const lo = (u % 100) * 2;
    tbuf[pos++] = '.';
    tbuf[pos++] = kTwoDigits[hi];
    tbuf[pos++] = kTwoDigits[hi + 1];
    tbuf[pos++] = kTwoDigits[mid];
    tbuf[pos++] = kTwoDigits[mid + 1];
    tbuf[pos++] = kTwoDigits[lo];
    tbuf[pos++] = kTwoDigits[lo + 1];
  } else if (frac_digits == 3) {
    auto const m = static_cast<uint32_t>(subsec);
    tbuf[pos++] = '.';
    tbuf[pos++] = static_cast<char>('0' + (m / 100));
    uint32_t const lo = (m % 100) * 2;
    tbuf[pos++] = kTwoDigits[lo];
    tbuf[pos++] = kTwoDigits[lo + 1];
  } else if (frac_digits == 9) {
    auto const n_hi = static_cast<uint32_t>(subsec / 1000);
    auto const n_lo = static_cast<uint32_t>(subsec % 1000);
    uint32_t const hi = (n_hi / 10000) * 2;
    uint32_t const mid = ((n_hi / 100) % 100) * 2;
    uint32_t const lo = (n_hi % 100) * 2;
    tbuf[pos++] = '.';
    tbuf[pos++] = kTwoDigits[hi];
    tbuf[pos++] = kTwoDigits[hi + 1];
    tbuf[pos++] = kTwoDigits[mid];
    tbuf[pos++] = kTwoDigits[mid + 1];
    tbuf[pos++] = kTwoDigits[lo];
    tbuf[pos++] = kTwoDigits[lo + 1];
    tbuf[pos++] = static_cast<char>('0' + (n_lo / 100));
    uint32_t const lo2 = (n_lo % 100) * 2;
    tbuf[pos++] = kTwoDigits[lo2];
    tbuf[pos++] = kTwoDigits[lo2 + 1];
  }
  if (has_tz) {
    tbuf[pos++] = 'Z';
  }
  out.insert(out.end(), tbuf, tbuf + pos);
}

inline void AppendFormattedDouble(double val, DSValue& out) {
  if (!std::isfinite(val)) {
    if (std::isnan(val)) {
      out.insert(out.end(), "nan", "nan" + 3);
    } else if (val > 0) {
      out.insert(out.end(), "inf", "inf" + 3);
    } else {
      out.insert(out.end(), "-inf", "-inf" + 4);
    }
    return;
  }
  char buf[64];
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
  auto res = std::to_chars(buf, buf + sizeof(buf), val);
  if (res.ec == std::errc()) {
    out.insert(out.end(), buf, res.ptr);
    return;
  }
#endif
  int const n = std::snprintf(buf, sizeof(buf), "%.16g", val);
  if (n > 0) out.insert(out.end(), buf, buf + n);
}

inline void AppendFormattedFloat(float val, DSValue& out) {
  if (!std::isfinite(val)) {
    if (std::isnan(val)) {
      out.insert(out.end(), "nan", "nan" + 3);
    } else if (val > 0) {
      out.insert(out.end(), "inf", "inf" + 3);
    } else {
      out.insert(out.end(), "-inf", "-inf" + 4);
    }
    return;
  }
  char buf[64];
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
  auto res = std::to_chars(buf, buf + sizeof(buf), val);
  if (res.ec == std::errc()) {
    out.insert(out.end(), buf, res.ptr);
    return;
  }
#endif
  int const n =
      std::snprintf(buf, sizeof(buf), "%.8g", static_cast<double>(val));
  if (n > 0) out.insert(out.end(), buf, buf + n);
}

struct CompiledArrowNode {
  std::shared_ptr<arrow::Array> array;
  arrow::Type::type type_id = arrow::Type::NA;
  bool may_have_nulls = true;
  bool is_flat_struct = false;
  arrow::TimeUnit::type time_unit = arrow::TimeUnit::MICRO;
  bool has_timezone = false;
  size_t initial_reserve = 32;

  arrow::StringArray const* str_arr = nullptr;
  arrow::Int64Array const* i64_arr = nullptr;
  arrow::Int32Array const* i32_arr = nullptr;
  arrow::Int16Array const* i16_arr = nullptr;
  arrow::Int8Array const* i8_arr = nullptr;
  arrow::UInt64Array const* u64_arr = nullptr;
  arrow::UInt32Array const* u32_arr = nullptr;
  arrow::UInt16Array const* u16_arr = nullptr;
  arrow::UInt8Array const* u8_arr = nullptr;
  arrow::DoubleArray const* dbl_arr = nullptr;
  arrow::FloatArray const* flt_arr = nullptr;
  arrow::BooleanArray const* bool_arr = nullptr;
  arrow::Date32Array const* date32_arr = nullptr;
  arrow::TimestampArray const* ts_arr = nullptr;
  arrow::Time64Array const* time64_arr = nullptr;
  arrow::Decimal128Array const* dec128_arr = nullptr;
  arrow::Decimal256Array const* dec256_arr = nullptr;
  arrow::ListArray const* list_arr = nullptr;
  arrow::StructArray const* struct_arr = nullptr;

  std::string list_prefix;
  std::vector<std::string> field_prefixes;
  std::vector<size_t> child_indices;
};

struct CompiledArrowPlan {
  std::vector<CompiledArrowNode> nodes;
};

struct FormatFrame {
  size_t node_idx = 0;
  int64_t row = 0;
  int32_t pos = 0;
  int32_t end = 0;
};

CompiledArrowNode InitArrowNode(std::shared_ptr<arrow::Array> const& col,
                                bool is_top_level_list) {
  CompiledArrowNode node;
  node.array = col;
  node.type_id = col->type_id();
  node.may_have_nulls = (col->null_count() != 0);

  switch (node.type_id) {
    case arrow::Type::STRING:
      node.str_arr = static_cast<arrow::StringArray const*>(col.get());
      node.initial_reserve = 24;
      break;
    case arrow::Type::INT64:
      node.i64_arr = static_cast<arrow::Int64Array const*>(col.get());
      node.initial_reserve = 20;
      break;
    case arrow::Type::INT32:
      node.i32_arr = static_cast<arrow::Int32Array const*>(col.get());
      node.initial_reserve = 12;
      break;
    case arrow::Type::INT16:
      node.i16_arr = static_cast<arrow::Int16Array const*>(col.get());
      node.initial_reserve = 8;
      break;
    case arrow::Type::INT8:
      node.i8_arr = static_cast<arrow::Int8Array const*>(col.get());
      node.initial_reserve = 6;
      break;
    case arrow::Type::UINT64:
      node.u64_arr = static_cast<arrow::UInt64Array const*>(col.get());
      node.initial_reserve = 20;
      break;
    case arrow::Type::UINT32:
      node.u32_arr = static_cast<arrow::UInt32Array const*>(col.get());
      node.initial_reserve = 12;
      break;
    case arrow::Type::UINT16:
      node.u16_arr = static_cast<arrow::UInt16Array const*>(col.get());
      node.initial_reserve = 8;
      break;
    case arrow::Type::UINT8:
      node.u8_arr = static_cast<arrow::UInt8Array const*>(col.get());
      node.initial_reserve = 6;
      break;
    case arrow::Type::DOUBLE:
      node.dbl_arr = static_cast<arrow::DoubleArray const*>(col.get());
      node.initial_reserve = 24;
      break;
    case arrow::Type::FLOAT:
      node.flt_arr = static_cast<arrow::FloatArray const*>(col.get());
      node.initial_reserve = 16;
      break;
    case arrow::Type::BOOL:
      node.bool_arr = static_cast<arrow::BooleanArray const*>(col.get());
      node.initial_reserve = 5;
      break;
    case arrow::Type::DATE32:
      node.date32_arr = static_cast<arrow::Date32Array const*>(col.get());
      node.initial_reserve = 10;
      break;
    case arrow::Type::TIMESTAMP: {
      node.ts_arr = static_cast<arrow::TimestampArray const*>(col.get());
      auto const* ts_type =
          static_cast<arrow::TimestampType const*>(col->type().get());
      node.time_unit = ts_type->unit();
      node.has_timezone = !ts_type->timezone().empty();
      node.initial_reserve = 28;
      break;
    }
    case arrow::Type::TIME64: {
      node.time64_arr = static_cast<arrow::Time64Array const*>(col.get());
      auto const* time_type =
          static_cast<arrow::Time64Type const*>(col->type().get());
      node.time_unit = time_type->unit();
      node.initial_reserve = 18;
      break;
    }
    case arrow::Type::DECIMAL128:
      node.dec128_arr = static_cast<arrow::Decimal128Array const*>(col.get());
      node.initial_reserve = 24;
      break;
    case arrow::Type::DECIMAL256:
      node.dec256_arr = static_cast<arrow::Decimal256Array const*>(col.get());
      node.initial_reserve = 40;
      break;
    case arrow::Type::LIST: {
      node.list_arr = static_cast<arrow::ListArray const*>(col.get());
      if (is_top_level_list) {
        std::string const type_str = col->type()->ToString();
        if (absl::StartsWith(type_str, "list<")) {
          auto const pos = type_str.find('[');
          if (pos != std::string::npos) {
            node.list_prefix = type_str.substr(pos) + "[";
          } else {
            node.list_prefix = "[";
          }
        } else {
          node.list_prefix = type_str + "[";
        }
      } else {
        node.list_prefix = col->type()->ToString() + "[";
      }
      break;
    }
    case arrow::Type::STRUCT: {
      node.struct_arr = static_cast<arrow::StructArray const*>(col.get());
      auto const* struct_type =
          static_cast<arrow::StructType const*>(col->type().get());
      int const n_fields = struct_type->num_fields();
      node.field_prefixes.reserve(n_fields);
      for (int i = 0; i < n_fields; ++i) {
        auto const& f = struct_type->field(i);
        std::string prefix = (i == 0) ? "{" : ", ";
        absl::StrAppend(&prefix, f->name(), ":", f->type()->ToString(), " = ");
        node.field_prefixes.push_back(std::move(prefix));
      }
      break;
    }
    default:
      node.initial_reserve = 32;
      break;
  }
  return node;
}

CompiledArrowPlan CompileArrowPlan(std::shared_ptr<arrow::Array> const& col,
                                   bool is_top_level_list) {
  CompiledArrowPlan plan;
  plan.nodes.push_back(InitArrowNode(col, is_top_level_list));

  for (size_t idx = 0; idx < plan.nodes.size(); ++idx) {
    arrow::Type::type const tid = plan.nodes[idx].type_id;
    if (tid == arrow::Type::LIST) {
      std::shared_ptr<arrow::Array> child_arr =
          plan.nodes[idx].list_arr->values();
      size_t const child_idx = plan.nodes.size();
      plan.nodes[idx].child_indices.push_back(child_idx);
      plan.nodes.push_back(InitArrowNode(child_arr, false));
    } else if (tid == arrow::Type::STRUCT) {
      arrow::StructArray const* s_arr = plan.nodes[idx].struct_arr;
      int const n_fields = s_arr->num_fields();
      plan.nodes[idx].child_indices.reserve(n_fields);
      bool all_leaf_children = true;
      for (int i = 0; i < n_fields; ++i) {
        std::shared_ptr<arrow::Array> field_arr = s_arr->field(i);
        arrow::Type::type const f_tid = field_arr->type_id();
        if (f_tid == arrow::Type::LIST || f_tid == arrow::Type::STRUCT) {
          all_leaf_children = false;
        }
        size_t const child_idx = plan.nodes.size();
        plan.nodes[idx].child_indices.push_back(child_idx);
        plan.nodes.push_back(InitArrowNode(field_arr, false));
      }
      plan.nodes[idx].is_flat_struct = all_leaf_children;
    }
  }

  for (size_t idx = plan.nodes.size(); idx-- > 0;) {
    CompiledArrowNode& node = plan.nodes[idx];
    if (node.type_id == arrow::Type::LIST) {
      size_t const child_res =
          plan.nodes[node.child_indices[0]].initial_reserve;
      node.initial_reserve = node.list_prefix.size() + 2 + child_res * 4;
    } else if (node.type_id == arrow::Type::STRUCT) {
      size_t est = 2;
      for (size_t i = 0; i < node.child_indices.size(); ++i) {
        est += node.field_prefixes[i].size();
        est += plan.nodes[node.child_indices[i]].initial_reserve;
      }
      node.initial_reserve = est;
    }
  }

  return plan;
}

StatusRecord AppendLeafArrowValue(CompiledArrowNode const& node, int64_t row,
                                  DSValue& out) {
  if (node.may_have_nulls && node.array->IsNull(row)) {
    out.insert(out.end(), "null", "null" + 4);
    return StatusRecord::Ok();
  }

  switch (node.type_id) {
    case arrow::Type::STRING: {
      auto view = node.str_arr->GetView(row);
      out.insert(out.end(), view.data(), view.data() + view.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::INT64: {
      absl::AlphaNum al(node.i64_arr->Value(row));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::INT32: {
      absl::AlphaNum al(node.i32_arr->Value(row));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::INT16: {
      absl::AlphaNum al(static_cast<int32_t>(node.i16_arr->Value(row)));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::INT8: {
      absl::AlphaNum al(static_cast<int32_t>(node.i8_arr->Value(row)));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::UINT64: {
      absl::AlphaNum al(node.u64_arr->Value(row));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::UINT32: {
      absl::AlphaNum al(node.u32_arr->Value(row));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::UINT16: {
      absl::AlphaNum al(static_cast<uint32_t>(node.u16_arr->Value(row)));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::UINT8: {
      absl::AlphaNum al(static_cast<uint32_t>(node.u8_arr->Value(row)));
      out.insert(out.end(), al.data(), al.data() + al.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::DOUBLE:
      AppendFormattedDouble(node.dbl_arr->Value(row), out);
      return StatusRecord::Ok();
    case arrow::Type::FLOAT:
      AppendFormattedFloat(node.flt_arr->Value(row), out);
      return StatusRecord::Ok();
    case arrow::Type::BOOL:
      if (node.bool_arr->Value(row)) {
        out.insert(out.end(), "true", "true" + 4);
      } else {
        out.insert(out.end(), "false", "false" + 5);
      }
      return StatusRecord::Ok();
    case arrow::Type::DATE32:
      AppendFormattedDate32(node.date32_arr->Value(row), out);
      return StatusRecord::Ok();
    case arrow::Type::TIMESTAMP:
      AppendFormattedTimestamp(node.ts_arr->Value(row), node.time_unit,
                               node.has_timezone, out);
      return StatusRecord::Ok();
    case arrow::Type::TIME64: {
      int64_t const val = node.time64_arr->Value(row);
      if (val >= 0 && (node.time_unit == arrow::TimeUnit::MICRO ||
                       node.time_unit == arrow::TimeUnit::NANO)) {
        int64_t const scale = (node.time_unit == arrow::TimeUnit::MICRO)
                                  ? 1000000LL
                                  : 1000000000LL;
        int64_t const total_sec = val / scale;
        int64_t const subsec = val % scale;
        if (total_sec < 86400) {
          auto const rem_s = static_cast<uint32_t>(total_sec);
          uint32_t const hour2 = (rem_s / 3600) * 2;
          uint32_t const rem_m = rem_s % 3600;
          uint32_t const min2 = (rem_m / 60) * 2;
          uint32_t const sec2 = (rem_m % 60) * 2;
          char tbuf[20];
          tbuf[0] = kTwoDigits[hour2];
          tbuf[1] = kTwoDigits[hour2 + 1];
          tbuf[2] = ':';
          tbuf[3] = kTwoDigits[min2];
          tbuf[4] = kTwoDigits[min2 + 1];
          tbuf[5] = ':';
          tbuf[6] = kTwoDigits[sec2];
          tbuf[7] = kTwoDigits[sec2 + 1];
          tbuf[8] = '.';
          if (node.time_unit == arrow::TimeUnit::MICRO) {
            auto const u = static_cast<uint32_t>(subsec);
            uint32_t const hi = (u / 10000) * 2;
            uint32_t const mid = ((u / 100) % 100) * 2;
            uint32_t const lo = (u % 100) * 2;
            tbuf[9] = kTwoDigits[hi];
            tbuf[10] = kTwoDigits[hi + 1];
            tbuf[11] = kTwoDigits[mid];
            tbuf[12] = kTwoDigits[mid + 1];
            tbuf[13] = kTwoDigits[lo];
            tbuf[14] = kTwoDigits[lo + 1];
            out.insert(out.end(), tbuf, tbuf + 15);
          } else {
            auto const n_hi = static_cast<uint32_t>(subsec / 1000);
            auto const n_lo = static_cast<uint32_t>(subsec % 1000);
            uint32_t const hi = (n_hi / 10000) * 2;
            uint32_t const mid = ((n_hi / 100) % 100) * 2;
            uint32_t const lo = (n_hi % 100) * 2;
            tbuf[9] = kTwoDigits[hi];
            tbuf[10] = kTwoDigits[hi + 1];
            tbuf[11] = kTwoDigits[mid];
            tbuf[12] = kTwoDigits[mid + 1];
            tbuf[13] = kTwoDigits[lo];
            tbuf[14] = kTwoDigits[lo + 1];
            tbuf[15] = static_cast<char>('0' + (n_lo / 100));
            uint32_t const lo2 = (n_lo % 100) * 2;
            tbuf[16] = kTwoDigits[lo2];
            tbuf[17] = kTwoDigits[lo2 + 1];
            out.insert(out.end(), tbuf, tbuf + 18);
          }
          return StatusRecord::Ok();
        }
      }
      auto scalar_res = node.array->GetScalar(row);
      if (!scalar_res.ok()) {
        return StatusRecord{SQLStates::k_HY000(),
                            "Internal Error: Unable to parse scalar"};
      }
      std::string const s = scalar_res.ValueOrDie()->ToString();
      out.insert(out.end(), s.data(), s.data() + s.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::DECIMAL128: {
      std::string const s = node.dec128_arr->FormatValue(row);
      out.insert(out.end(), s.data(), s.data() + s.size());
      return StatusRecord::Ok();
    }
    case arrow::Type::DECIMAL256: {
      std::string const s = node.dec256_arr->FormatValue(row);
      out.insert(out.end(), s.data(), s.data() + s.size());
      return StatusRecord::Ok();
    }
    default: {
      auto scalar_res = node.array->GetScalar(row);
      if (!scalar_res.ok()) {
        return StatusRecord{SQLStates::k_HY000(),
                            "Internal Error: Unable to parse scalar"};
      }
      std::string const s = scalar_res.ValueOrDie()->ToString();
      out.insert(out.end(), s.data(), s.data() + s.size());
      return StatusRecord::Ok();
    }
  }
}

StatusRecord AppendFormattedArrowValue(CompiledArrowPlan const& plan,
                                       int64_t root_row,
                                       std::vector<FormatFrame>& stack,
                                       DSValue& out) {
  CompiledArrowNode const& root = plan.nodes[0];
  if (root.is_flat_struct) {
    if (root.may_have_nulls && root.struct_arr->IsNull(root_row)) {
      out.insert(out.end(), "null", "null" + 4);
      return StatusRecord::Ok();
    }
    if (root.child_indices.empty()) {
      out.push_back('{');
      out.push_back('}');
      return StatusRecord::Ok();
    }
    for (size_t i = 0; i < root.child_indices.size(); ++i) {
      std::string const& prefix = root.field_prefixes[i];
      out.insert(out.end(), prefix.data(), prefix.data() + prefix.size());
      StatusRecord st = AppendLeafArrowValue(plan.nodes[root.child_indices[i]],
                                             root_row, out);
      if (!st.ok()) return st;
    }
    out.push_back('}');
    return StatusRecord::Ok();
  }

  stack.clear();
  auto enter_node = [&](size_t node_idx, int64_t row) -> StatusRecord {
    CompiledArrowNode const& node = plan.nodes[node_idx];
    if (node.may_have_nulls && node.array->IsNull(row)) {
      out.insert(out.end(), "null", "null" + 4);
      return StatusRecord::Ok();
    }
    if (node.type_id == arrow::Type::LIST) {
      out.insert(out.end(), node.list_prefix.data(),
                 node.list_prefix.data() + node.list_prefix.size());
      int32_t const start = node.list_arr->value_offset(row);
      int32_t const end = node.list_arr->value_offset(row + 1);
      if (start >= end) {
        out.push_back(']');
      } else {
        stack.push_back({node_idx, row, start, end});
      }
      return StatusRecord::Ok();
    }
    if (node.type_id == arrow::Type::STRUCT) {
      auto const n_fields = static_cast<int32_t>(node.child_indices.size());
      if (n_fields == 0) {
        out.push_back('{');
        out.push_back('}');
      } else {
        stack.push_back({node_idx, row, 0, n_fields});
      }
      return StatusRecord::Ok();
    }
    return AppendLeafArrowValue(node, row, out);
  };

  StatusRecord init_st = enter_node(0, root_row);
  if (!init_st.ok()) return init_st;

  while (!stack.empty()) {
    FormatFrame& top = stack.back();
    size_t const frame_node_idx = top.node_idx;
    int64_t const frame_row = top.row;
    CompiledArrowNode const& node = plan.nodes[frame_node_idx];
    if (node.type_id == arrow::Type::LIST) {
      if (top.pos >= top.end) {
        out.push_back(']');
        stack.pop_back();
        continue;
      }
      int32_t const elem_idx = top.pos++;
      if (elem_idx > node.list_arr->value_offset(frame_row)) {
        out.push_back(',');
        out.push_back(' ');
      }
      StatusRecord st = enter_node(node.child_indices[0], elem_idx);
      if (!st.ok()) return st;
    } else {
      if (top.pos >= top.end) {
        out.push_back('}');
        stack.pop_back();
        continue;
      }
      int32_t const field_i = top.pos++;
      std::string const& prefix = node.field_prefixes[field_i];
      out.insert(out.end(), prefix.data(), prefix.data() + prefix.size());
      StatusRecord st = enter_node(node.child_indices[field_i], frame_row);
      if (!st.ok()) return st;
    }
  }
  return StatusRecord::Ok();
}

}  // namespace

StatusRecord ProcessRecordBatch(
    std::shared_ptr<arrow::Schema> /*schema*/,
    std::shared_ptr<arrow::RecordBatch> record_batch, ResultSet& result_set) {
  int num_rows = record_batch->num_rows();
  int num_columns = record_batch->num_columns();

  int old_row_count = result_set.rows.size();
  result_set.rows.resize(num_rows);
  // Resize inner column vectors ONLY for new rows.
  // Existing rows (indices 0 to old_row_count-1) retain their capacity and
  // size.
  for (int i = old_row_count; i < num_rows; ++i) {
    result_set.rows[i].resize(num_columns);
  }

  // Column-Oriented Processing:
  // Arrow is columnar. Accessing data column-by-column allows us to cast the
  // array type ONCE per column, rather than performing type checks and
  // GetScalar() allocations for every single cell.
  for (int col_i = 0; col_i < num_columns; ++col_i) {
    auto column = record_batch->column(col_i);
    auto type_id = column->type_id();
    bool const has_nulls = (column->null_count() != 0);

    switch (type_id) {
      case arrow::Type::INT64: {
        auto int_arr = std::static_pointer_cast<arrow::Int64Array>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (has_nulls && int_arr->IsNull(row)) {
            result_set.rows[row][col_i].assign(1, '\0');
          } else {
            ArithmeticToDSValue<SQLBIGINT>(int_arr->Value(row),
                                           result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::DOUBLE: {
        auto dbl_arr = std::static_pointer_cast<arrow::DoubleArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (has_nulls && dbl_arr->IsNull(row)) {
            result_set.rows[row][col_i].assign(1, '\0');
          } else {
            ArithmeticToDSValue<SQLDOUBLE>(dbl_arr->Value(row),
                                           result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::STRING: {
        auto str_arr = std::static_pointer_cast<arrow::StringArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (has_nulls && str_arr->IsNull(row)) {
            result_set.rows[row][col_i].assign(1, '\0');
          } else {
            auto view = str_arr->GetView(row);
            StringToDSValue(std::string_view(view.data(), view.size()),
                            result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::BOOL: {
        auto bool_arr = std::static_pointer_cast<arrow::BooleanArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (has_nulls && bool_arr->IsNull(row)) {
            result_set.rows[row][col_i].assign(1, '\0');
          } else {
            BooleanToDSValue(bool_arr->Value(row), result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::BINARY: {
        auto bin_arr = std::static_pointer_cast<arrow::BinaryArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (has_nulls && bin_arr->IsNull(row)) {
            result_set.rows[row][col_i].assign(1, '\0');
          } else {
            auto view = bin_arr->GetView(row);
            StringToDSValue(std::string_view(view.data(), view.size()),
                            result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::TIMESTAMP: {
        auto ts_arr = std::static_pointer_cast<arrow::TimestampArray>(column);
        auto ts_type =
            std::static_pointer_cast<arrow::TimestampType>(column->type());
        arrow::TimeUnit::type unit = ts_type->unit();
        absl::TimeZone utc = absl::UTCTimeZone();

        for (int64_t row = 0; row < num_rows; ++row) {
          if (ts_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            int64_t val = ts_arr->Value(row);
            absl::Time time;
            switch (unit) {
              case arrow::TimeUnit::SECOND:
                time = absl::FromUnixSeconds(val);
                break;
              case arrow::TimeUnit::MILLI:
                time = absl::FromUnixMillis(val);
                break;
              case arrow::TimeUnit::MICRO:
                time = absl::FromUnixMicros(val);
                break;
              case arrow::TimeUnit::NANO:
                time = absl::FromUnixNanos(val);
                break;
            }
            absl::TimeZone::CivilInfo info = utc.At(time);

            SQL_TIMESTAMP_STRUCT date_struct = {};
            date_struct.year = static_cast<SQLSMALLINT>(info.cs.year());
            date_struct.month = static_cast<SQLUSMALLINT>(info.cs.month());
            date_struct.day = static_cast<SQLUSMALLINT>(info.cs.day());
            date_struct.hour = static_cast<SQLUSMALLINT>(info.cs.hour());
            date_struct.minute = static_cast<SQLUSMALLINT>(info.cs.minute());
            date_struct.second = static_cast<SQLUSMALLINT>(info.cs.second());

            int64_t micros = absl::ToInt64Microseconds(info.subsecond);
            date_struct.fraction = static_cast<SQLUINTEGER>(micros);

            TimestampToDSValue(date_struct, result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::DATE32: {
        auto date_arr = std::static_pointer_cast<arrow::Date32Array>(column);
        absl::CivilDay epoch(1970, 1, 1);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (date_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            int32_t days = date_arr->Value(row);
            absl::CivilDay cd = epoch + days;

            SQL_DATE_STRUCT date_struct = {};
            date_struct.year = static_cast<SQLSMALLINT>(cd.year());
            date_struct.month = static_cast<SQLUSMALLINT>(cd.month());
            date_struct.day = static_cast<SQLUSMALLINT>(cd.day());

            DateToDSValue(date_struct, result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::TIME64: {
        auto time_arr = std::static_pointer_cast<arrow::Time64Array>(column);
        auto time_type =
            std::static_pointer_cast<arrow::Time64Type>(column->type());
        arrow::TimeUnit::type unit = time_type->unit();

        for (int64_t row = 0; row < num_rows; ++row) {
          if (time_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            int64_t val = time_arr->Value(row);
            int64_t total_seconds = 0;
            switch (unit) {
              case arrow::TimeUnit::MICRO:
                total_seconds = val / 1000000LL;
                break;
              case arrow::TimeUnit::NANO:
                total_seconds = val / 1000000000LL;
                break;
              default:
                break;
            }
            int64_t hour = (total_seconds / 3600) % 24;
            int64_t minute = (total_seconds / 60) % 60;
            int64_t second = total_seconds % 60;

            SQL_TIME_STRUCT time_struct = {};
            time_struct.hour = static_cast<SQLUSMALLINT>(hour);
            time_struct.minute = static_cast<SQLUSMALLINT>(minute);
            time_struct.second = static_cast<SQLUSMALLINT>(second);

            TimeToDSValue(time_struct, result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::DECIMAL128: {
        auto dec_arr = std::static_pointer_cast<arrow::Decimal128Array>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (dec_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            NumericToDSValue(dec_arr->FormatValue(row),
                             result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::DECIMAL256: {
        auto dec_arr = std::static_pointer_cast<arrow::Decimal256Array>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (dec_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            NumericToDSValue(dec_arr->FormatValue(row),
                             result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::INTERVAL_MONTHS: {
        auto arr = std::static_pointer_cast<arrow::MonthIntervalArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            int32_t m = arr->Value(row);
            char buf[64];
            snprintf(buf, sizeof(buf), "%d-%d 0 0:0:0", m / 12, m % 12);
            StringToDSValue(std::string(buf), result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::INTERVAL_DAY_TIME: {
        auto arr =
            std::static_pointer_cast<arrow::DayTimeIntervalArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            auto val = arr->Value(row);
            int32_t days = val.days;
            int64_t total_sec = val.milliseconds / 1000;
            int32_t fraction = (val.milliseconds % 1000) * 1000000;
            int32_t hours = total_sec / 3600;
            int32_t minutes = (total_sec / 60) % 60;
            int32_t seconds = total_sec % 60;
            char buf[64];
            if (fraction > 0) {
              snprintf(buf, sizeof(buf), "0-0 %d %d:%d:%d.%09d", days, hours,
                       minutes, seconds, fraction);
            } else {
              snprintf(buf, sizeof(buf), "0-0 %d %d:%d:%d", days, hours,
                       minutes, seconds);
            }
            StringToDSValue(std::string(buf), result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::INTERVAL_MONTH_DAY_NANO: {
        auto arr =
            std::static_pointer_cast<arrow::MonthDayNanoIntervalArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            auto val = arr->Value(row);
            int32_t years = val.months / 12;
            int32_t months = val.months % 12;
            int32_t days = val.days;
            int64_t total_sec = val.nanoseconds / 1000000000LL;
            int32_t nanos = val.nanoseconds % 1000000000LL;
            int32_t hours = total_sec / 3600;
            int32_t minutes = (total_sec / 60) % 60;
            int32_t seconds = total_sec % 60;
            char buf[128];
            if (nanos > 0) {
              snprintf(buf, sizeof(buf), "%d-%d %d %d:%d:%d.%09d", years,
                       months, days, hours, minutes, seconds, nanos);
            } else {
              snprintf(buf, sizeof(buf), "%d-%d %d %d:%d:%d", years, months,
                       days, hours, minutes, seconds);
            }
            StringToDSValue(std::string(buf), result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::LIST: {
        auto list_arr = std::static_pointer_cast<arrow::ListArray>(column);
        if (list_arr->value_type()->id() == arrow::Type::STRING &&
            list_arr->values()->null_count() == 0) {
          auto const* val_str_arr =
              static_cast<arrow::StringArray const*>(list_arr->values().get());
          for (int64_t row = 0; row < num_rows; ++row) {
            if (has_nulls && list_arr->IsNull(row)) {
              result_set.rows[row][col_i].assign(1, '\0');
              continue;
            }
            int32_t const start = list_arr->value_offset(row);
            int32_t const end = list_arr->value_offset(row + 1);
            DSValue& out = result_set.rows[row][col_i];
            if (start >= end) {
              out.assign({'[', ']'});
              continue;
            }
            int32_t const count = end - start;
            size_t const chars_len =
                static_cast<size_t>(val_str_arr->value_offset(end) -
                                    val_str_arr->value_offset(start));
            out.resize(chars_len + 2 * static_cast<size_t>(count));
            char* dst = out.data();
            *dst++ = '[';
            for (int32_t idx = start; idx < end; ++idx) {
              if (idx > start) {
                dst[0] = ',';
                dst[1] = ' ';
                dst += 2;
              }
              auto view = val_str_arr->GetView(idx);
              if (!view.empty()) {
                std::memcpy(dst, view.data(), view.size());
                dst += view.size();
              }
            }
            *dst = ']';
          }
        } else {
          CompiledArrowPlan const plan =
              CompileArrowPlan(column, /*is_top_level_list=*/true);
          size_t const reserve_est = plan.nodes[0].initial_reserve;
          std::vector<FormatFrame> stack;
          stack.reserve(8);
          for (int64_t row = 0; row < num_rows; ++row) {
            if (has_nulls && list_arr->IsNull(row)) {
              result_set.rows[row][col_i].assign(1, '\0');
              continue;
            }
            DSValue& out = result_set.rows[row][col_i];
            out.clear();
            if (out.capacity() < reserve_est) {
              out.reserve(reserve_est);
            }
            StatusRecord st = AppendFormattedArrowValue(plan, row, stack, out);
            if (!st.ok()) return st;
          }
        }
        break;
      }
      case arrow::Type::STRUCT: {
        CompiledArrowPlan const plan =
            CompileArrowPlan(column, /*is_top_level_list=*/false);
        CompiledArrowNode const& root = plan.nodes[0];
        size_t const reserve_est = root.initial_reserve;
        std::vector<FormatFrame> stack;
        if (!root.is_flat_struct) {
          stack.reserve(8);
        }
        for (int64_t row = 0; row < num_rows; ++row) {
          if (has_nulls && root.struct_arr->IsNull(row)) {
            result_set.rows[row][col_i].assign(1, '\0');
            continue;
          }
          DSValue& out = result_set.rows[row][col_i];
          out.clear();
          if (out.capacity() < reserve_est) {
            out.reserve(reserve_est);
          }
          StatusRecord st = AppendFormattedArrowValue(plan, row, stack, out);
          if (!st.ok()) return st;
        }
        break;
      }
      default: {
        for (int64_t row = 0; row < num_rows; ++row) {
          if (has_nulls && column->IsNull(row)) {
            result_set.rows[row][col_i].assign(1, '\0');
            continue;
          }
          auto scalar_res = column->GetScalar(row);
          if (!scalar_res.ok()) {
            return StatusRecord{SQLStates::k_HY000(),
                                "Internal Error: Unable to parse scalar"};
          }
          std::string data = scalar_res.ValueOrDie()->ToString();
          StringToDSValue(data, result_set.rows[row][col_i]);
        }
        break;
      }
    }
  }
  return StatusRecord::Ok();
}

bool IsTransientStreamError(google::cloud::Status const& status) {
  using google::cloud::StatusCode;
  switch (status.code()) {
    case StatusCode::kUnavailable:
    case StatusCode::kDeadlineExceeded:
    case StatusCode::kAborted:
    case StatusCode::kResourceExhausted:
      return true;
    case StatusCode::kInternal:
    case StatusCode::kUnknown: {
      std::string message = status.message();
      std::transform(message.begin(), message.end(), message.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      if (message.find("rst_stream") != std::string::npos ||
          message.find("stream removed") != std::string::npos ||
          message.find("connection reset") != std::string::npos ||
          message.find("broken pipe") != std::string::npos ||
          message.find("connection closed") != std::string::npos ||
          message.find("socket closed") != std::string::npos ||
          message.find("transport closed") != std::string::npos ||
          message.find("endpoint read failed") != std::string::npos ||
          message.find("transport: lost connection") != std::string::npos) {
        return true;
      }
      return false;
    }
    default:
      return false;
  }
  return false;
}

StatusRecord ReadNextResultsFromStream(StatementHandle& stmt_handle) {
  std::optional<StreamRange<ReadRowsResponse>>& optional_stream =
      stmt_handle.GetReadRowsStream();
  if (!optional_stream.has_value()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Internal Error: No HTAPI read stream found!!"};
  }
  auto& read_rows_stream = *optional_stream;
  auto& optional_it = stmt_handle.GetReadRowsIterator();
  if (!optional_it.has_value()) {
    // Initialize iterator to begin() and store it.
    auto it = read_rows_stream.begin();
    optional_it = std::move(it);
  } else {
    // Advance the stored iterator.
    // The previous call processed the element at the iterator,
    // so we increment it to move to the next element.
    ++(*optional_it);
    // Irrespective of any errors, if the iterator hasn't reached the end, we
    // want to cache it
    stmt_handle.SetReadRowsIterator(*optional_it);
  }
  auto& it = *optional_it;
  if (it != read_rows_stream.end()) {
    auto const& read_row_status = *it;
    if (!read_row_status) {
      return StatusRecord::ConvertFrom(read_row_status.status());
    }
    ReadRowsResponse row = *read_row_status;
    if (row.has_arrow_record_batch()) {
      // The schema is coming from ResultSet cached in the statement handle.
      // We don't want to generate the schema again for every batch since it
      // will remain the same.
      std::shared_ptr<arrow::Schema> schema = stmt_handle.GetArrowSchema();
      StatusRecordOr<std::shared_ptr<arrow::RecordBatch>> record_batch_status =
          GetArrowRecordBatch(row.arrow_record_batch(), schema);
      if (!record_batch_status) {
        return record_batch_status.GetStatusRecord();
      }
      // We are reading ResultSet from the stmt_handle because we want to
      // preserve the previous state like `num_rows_fetched_yet`.
      ResultSet& result_set = stmt_handle.GetResultSet();
      // To have SQLFetch read the rows from the start, we are setting the
      // cursor to default.
      result_set.cursor = -1;
      return ProcessRecordBatch(schema, *record_batch_status, result_set);
    } else {
      return StatusRecord{
          SQLStates::k_HY000(),
          "Internal Error: cannot find arrow record batch to process!"};
    }
  } else {
    stmt_handle.ClearReadRowsStream();
    stmt_handle.ClearReadRowsIterator();
    // Empty result set.
    stmt_handle.GetResultSet().rows.clear();
    stmt_handle.GetResultSet().cursor = -1;
    LOG(INFO) << "FetchBQDataReadArrow:: Read stream ended.";
  }
  return StatusRecord::Ok();
}

StatusRecord FetchBQDataReadArrow(StatementHandle& stmt_handle,
                                  TableReference& table_ref) {
  std::string project_id = table_ref.project_id;
  std::string dataset_id = table_ref.dataset_id;
  std::string table_id = table_ref.table_id;
  std::string table_path = "projects/" + project_id + "/datasets/" +
                           dataset_id + "/tables/" + table_id;

  CreateReadSessionRequest create_read_session_request;
  create_read_session_request.set_parent("projects/" + project_id);
  create_read_session_request.set_max_stream_count(1);
  auto* read_session = create_read_session_request.mutable_read_session();
  read_session->set_table(table_path);
  read_session->set_data_format(ARROW);

  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  int const max_retries = static_cast<int>(conn_handle.GetDsn().max_retries);
  Options options;
  options.set<MaxRetriesOption>(conn_handle.GetDsn().max_retries);
  options
      .set<::google::cloud::bigquery_storage_v1::BigQueryReadRetryPolicyOption>(
          std::make_shared<HtapiStreamRetryPolicy>(max_retries));
  options.set<
      ::google::cloud::bigquery_storage_v1::BigQueryReadBackoffPolicyOption>(
      google::cloud::ExponentialBackoffPolicy(
          std::chrono::milliseconds(100), std::chrono::milliseconds(2000), 2.0)
          .clone());
  auto bq_client = stmt_handle.GetConnectionHandle()->GetClient();
  auto read_session_status =
      bq_client->CreateReadSession(create_read_session_request, options);
  if (!read_session_status) {
    return read_session_status.GetStatusRecord();
  }

  auto session = *read_session_status;

  if (!session.streams().empty()) {
    std::string read_stream_name = session.streams(0).name();

    ResultSet result_set;
    StatusRecordOr<std::shared_ptr<arrow::Schema>> schema_status =
        GetArrowSchema(session.arrow_schema(), result_set.row_schema);
    if (!schema_status) {
      return schema_status.GetStatusRecord();
    }
    // The ResultSet now contains valid `row_schema`
    std::shared_ptr<arrow::Schema> schema = *schema_status;
    result_set.arrow_schema = schema;
    stmt_handle.SetResultSet(std::move(result_set));

    // Create a ReadRowsRequest.
    ReadRowsRequest read_rows_request;
    read_rows_request.set_read_stream(read_stream_name);

    // Before we call ReadNextResultsFromStream, we are caching the stream
    StreamRange<google::cloud::bigquery::storage::v1::ReadRowsResponse>
        read_rows_stream =
            bq_client->GetReadRowsStream(read_rows_request, options);
    stmt_handle.SetReadRowsStream(std::move(read_rows_stream));
    return ReadNextResultsFromStream(stmt_handle);
  }

  return StatusRecord{SQLStates::k_HY000(),
                      "No valid stream found to read results"};
}

StatusRecord CreateLargeDatasetIfNeeded(std::shared_ptr<ODBCBQClient> bq_client,
                                        std::string project_id,
                                        std::string dataset_id,
                                        std::string location,
                                        std::string large_table_expiration_time,
                                        Options opt) {
  // 1. Construct the CREATE SCHEMA DDL.
  std::string full_dataset_name = "`" + project_id + "." + dataset_id + "`";
  std::string query = "CREATE SCHEMA IF NOT EXISTS " + full_dataset_name;

  // 2. Handle Expiration Time Conversion.
  // The input is in milliseconds (string), but SQL DDL OPTIONS expects
  // 'default_table_expiration_days'.
  if (!large_table_expiration_time.empty()) {
    try {
      long long expiration_ms = std::stoll(large_table_expiration_time);
      if (expiration_ms <= 0) {
        throw std::invalid_argument("Expiration time cannot be negative");
      }
      if (expiration_ms > 0) {
        // Convert milliseconds to days (MilliSeconds / 1000*24*60*60)
        double expiration_days = static_cast<double>(expiration_ms) / 86400000;

        query += " OPTIONS(default_table_expiration_days=" +
                 std::to_string(expiration_days) + ")";
      }
    } catch (std::exception const& e) {
      std::string err_msg =
          "CreateLargeDatasetIfNeeded:: Invalid large_table_expiration_time "
          "format: " +
          large_table_expiration_time;
      LOG(ERROR) << err_msg;
      return StatusRecord{SQLStates::k_HY000(), err_msg};
    }
  }
  LOG(INFO) << "CreateLargeDatasetIfNeeded:: Executing DDL: " << query;

  // 3. Prepare the QueryRequest.
  QueryRequest query_request;
  query_request.set_query(query);
  query_request.set_use_legacy_sql(false);
  query_request.set_location(std::move(location));

  // 4. Prepare the PostQueryRequest.
  PostQueryRequest post_query_request;
  post_query_request.set_project_id(project_id);
  post_query_request.set_query_request(std::move(query_request));

  // 5. Execute using the helper function.
  auto result = PostQueryWithoutResults(bq_client, post_query_request, opt);
  if (!result.Ok()) {
    LOG(ERROR) << "CreateLargeDatasetIfNeeded:: Failed to create dataset: "
               << result.GetStatusRecord().message;
    return result.GetStatusRecord();
  }
  return StatusRecord::Ok();
}

StatusRecordOr<Job> WaitForJobCompletion(
    std::shared_ptr<ODBCBQClient> const& bq_client, std::string const& catalog,
    std::string const& job_id, std::string const& location, Options const& opt,
    bool is_job_complete) {
  StatusRecordOr<Job> get_job_response;
  if (is_job_complete) {
    get_job_response = bq_client->GetJob(catalog, job_id, location, opt);
    if (!get_job_response.Ok()) {
      return get_job_response.GetStatusRecord();
    }
  } else {
    ExponentialBackoffPolicy backoff(chrono_ms(100), chrono_ms(200), 2);
    std::string job_status;
    while (job_status != "DONE") {
      get_job_response = bq_client->GetJob(catalog, job_id, location, opt);
      if (!get_job_response.Ok()) {
        return get_job_response.GetStatusRecord();
      }
      job_status = get_job_response->status.state;
      if (job_status != "DONE") {
        std::this_thread::sleep_for(backoff.OnCompletion());
      }
    }
  }

  std::string error_message = get_job_response->status.error_result.message;
  if (!error_message.empty()) {
    return StatusRecord{SQLStates::k_HY000(), error_message};
  }

  return get_job_response;
}

StatusRecord FetchBQDataRead(StatementHandle& stmt_handle,
                             PostQueryRequest const& post_query_request) {
  QueryRequest query_request = post_query_request.query_request();
  std::string query = query_request.query();
  Job job;
  job.configuration.query.query = query;
  job.configuration.query.use_query_cache = true;
  job.configuration.dry_run = false;
  job.configuration.query.allow_large_results = true;
  job.configuration.query.use_legacy_sql = false;
  job.configuration.query.create_disposition = "CREATE_IF_NEEDED";
  job.configuration.query.write_disposition = "WRITE_TRUNCATE";
  job.configuration.query.query_parameters = query_request.query_parameters();
  // FetchBQDataRead builds its own job configuration, so the ceiling already
  // set on the QueryRequest has to be carried across explicitly.
  if (query_request.maximum_bytes_billed() > 0) {
    job.configuration.query.maximum_bytes_billed =
        query_request.maximum_bytes_billed();
  }

  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  auto dsn = conn_handle.GetDsn();
  std::string catalog_name = dsn.catalog;
  std::string default_dataset = dsn.default_dataset;
  if (!default_dataset.empty()) {
    job.configuration.query.default_dataset.project_id = catalog_name;
    job.configuration.query.default_dataset.dataset_id = default_dataset;
  }
  job.configuration.query.destination_table.project_id = catalog_name;
  job.configuration.query.destination_table.dataset_id =
      dsn.use_default_large_results_dataset ? kDefaultDestDatasetId
                                            : dsn.large_results_dataset_id;
  std::string table_id = GenerateTableId();
  job.configuration.query.destination_table.table_id = table_id;

  job.job_reference.job_id = "job_" + table_id;
  job.job_reference.project_id = catalog_name;

  job.configuration.query.parameter_mode = "POSITIONAL";
  job.configuration.query.allow_large_results = true;

  Options opt;
  opt.set<MaxRetriesOption>(dsn.max_retries);
  auto bq_client = conn_handle.GetClient();
  std::string dataset_location;
  // Check if the destination dataset (LargeResultsDataSetId) exists.
  // 1. If it exists, we execute the query job in the same region as the
  // destination dataset.
  // 2. If it does not exist (404), we create it in the region where the query
  // would run
  //    (determined by the dry-run, which resolves to the DefaultDataset region,
  //    source tables region, or defaults to US). Subsequent runs will find the
  //    dataset and execute in its region.
  auto response = bq_client->GetDataset(
      catalog_name, job.configuration.query.destination_table.dataset_id, opt);
  if (!response.Ok()) {
    StatusRecord err_status = response.GetStatusRecord();
    if (err_status.native_error_code == 404) {
      dataset_location = query_request.location();
      // We need to first create large results dataset if it was not there
      StatusRecord create_dataset_status = CreateLargeDatasetIfNeeded(
          bq_client, dsn.catalog,
          job.configuration.query.destination_table.dataset_id,
          dataset_location, dsn.large_table_expiration_time, opt);
      if (!create_dataset_status.ok()) {
        return create_dataset_status;
      }
    } else {
      return err_status;
    }
  } else {
    dataset_location = response->location;
  }

  job.job_reference.location = dataset_location;

  // Insert job
  auto insert_response = bq_client->InsertJob(dsn.catalog, job, opt);
  if (!insert_response.Ok()) {
    return insert_response.GetStatusRecord();
  }
  // Here we are replacing the dry run Job created during SQLPrepare.
  // This should be safe since the same query is executed during HTAPI flow too.
  stmt_handle.SetPreparedJob(*insert_response);

  // Wait for Job to complete
  auto get_job_response =
      WaitForJobCompletion(bq_client, conn_handle.GetDsn().catalog,
                           insert_response->job_reference.job_id,
                           insert_response->job_reference.location, opt,
                           insert_response->status.state == "DONE");
  if (!get_job_response.Ok()) {
    LOG(ERROR) << "FetchBQDataRead:: "
               << get_job_response.GetStatusRecord().message;
    return get_job_response.GetStatusRecord();
  }

  return FetchBQDataReadArrow(
      stmt_handle, insert_response->configuration.query.destination_table);
}

StatusRecord FetchBQDataReadFromJob(StatementHandle& stmt_handle,
                                    JobReference const& job_ref,
                                    bool job_complete) {
  if (job_ref.job_id.empty()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "FetchBQDataReadFromJob:: Job ID is empty"};
  }

  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  Options opt;
  opt.set<MaxRetriesOption>(conn_handle.GetDsn().max_retries);
  auto bq_client = conn_handle.GetClient();

  auto get_job_response =
      WaitForJobCompletion(bq_client, conn_handle.GetDsn().catalog,
                           job_ref.job_id, job_ref.location, opt, job_complete);
  if (!get_job_response.Ok()) {
    LOG(ERROR) << "FetchBQDataReadFromJob:: "
               << get_job_response.GetStatusRecord().message;
    return get_job_response.GetStatusRecord();
  }

  stmt_handle.SetPreparedJob(*get_job_response);

  auto dest_table = get_job_response->configuration.query.destination_table;
  if (dest_table.table_id.empty()) {
    LOG(ERROR) << "FetchBQDataReadFromJob:: Destination table is empty";
    return StatusRecord{SQLStates::k_HY000(),
                        "Destination table is empty in query job"};
  }
  if (dest_table.project_id.empty()) {
    dest_table.project_id = conn_handle.GetDsn().catalog;
  }

  LOG(INFO) << "FetchBQDataReadFromJob:: Reusing job " << job_ref.job_id
            << " in location " << job_ref.location << " with destination table "
            << dest_table.project_id << "." << dest_table.dataset_id << "."
            << dest_table.table_id;

  return FetchBQDataReadArrow(stmt_handle, dest_table);
}

#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

// TODO(b/388947009): Add unit tests for this function
StatusRecordOr<DSResults> FetchBQData(
    StatementHandle& stmt_handle, PostQueryRequest const& post_query_request,
    [[maybe_unused]] bool with_htapi) {
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());

  auto pq_status = PostQueryWithoutResults(conn_handle, post_query_request);
  if (!pq_status) {
    return pq_status.GetStatusRecord();
  }

  // If session started, propagate session ID to the connection handle
  if (!conn_handle.IsSessionStarted() &&
      !pq_status->session_info.session_id.empty()) {
    conn_handle.SetSessionId(pq_status->session_info.session_id);
  }

  // If we skipped the dry-run job (e.g. SQLExecDirect without positional
  // params), populate the statement handle's prepared job metadata, IRD, and
  // location so that subsequent processing remains identical to the dry-run
  // flow.
  PostQueryRequest actual_post_query_request = post_query_request;
  if (!stmt_handle.GetPreparedJob().has_value()) {
    Job executed_job;
    executed_job.job_reference = pq_status->job_reference;
    executed_job.statistics.job_query_stats.statement_type =
        pq_status->statement_type;
    executed_job.statistics.job_query_stats.schema = pq_status->schema;
    stmt_handle.SetPreparedJob(executed_job);

    if (!pq_status->schema.fields.empty()) {
      DescriptorHandle& ird =
          stmt_handle.GetDescriptorHandle(DescriptorType::kIRD);
      ird.SetConnectionHandle(&conn_handle);
      ird.ClearDescriptorRecordsMap();
      TableReference table_fields;
      StatementHandle::PopulateIrd(ird, pq_status->schema, table_fields);
    }
    if (!pq_status->job_reference.location.empty()) {
      auto query_req = actual_post_query_request.query_request();
      query_req.set_location(pq_status->job_reference.location);
      actual_post_query_request.set_query_request(query_req);
      stmt_handle.SetPostQueryRequest(actual_post_query_request);
    }
  }

  DSResults results;
  results.num_dml_affected_rows = pq_status->num_dml_affected_rows;
  results.job_ref = pq_status->job_reference;
  stmt_handle.GetPagingInfo().job_id = pq_status->job_reference.job_id;
  stmt_handle.GetPagingInfo().page_token = pq_status->page_token;

  if (pq_status->job_complete && pq_status->page_token.empty()) {
    // Only one page of results, return it directly.
    results.data_source_results = std::move(*pq_status);
    return results;
  }

  // If there are more pages, check if we should use HTAPI fallback
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
  if (with_htapi && conn_handle.GetDsn().allow_htapi) {
    // Attempt to reuse destination table created by the query job
    StatusRecord read_status = FetchBQDataReadFromJob(
        stmt_handle, pq_status->job_reference, pq_status->job_complete);
    if (!read_status.ok()) {
      LOG(WARNING) << "FetchBQDataReadFromJob failed: " << read_status.message
                   << ", falling back to FetchBQDataRead.";
      read_status = FetchBQDataRead(stmt_handle, actual_post_query_request);
      if (!read_status.ok()) {
        return read_status;
      }
    }
    stmt_handle.GetPagingInfo().page_token.clear();
    results.data_source_results = std::move(stmt_handle.GetResultSet());
    return results;
  }
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

  // Otherwise, continue with standard REST API pagination
  if (pq_status->job_complete) {
    results.data_source_results = std::move(*pq_status);
  } else {
    auto gq_status =
        FetchNextPageOfQueryResults(stmt_handle, actual_post_query_request);
    if (!gq_status) {
      LOG(ERROR) << "FetchBQData::FetchNextPageOfQueryResults:: "
                 << gq_status.GetStatusRecord().message;
      return gq_status.GetStatusRecord();
    }
    results.num_dml_affected_rows = gq_status->num_dml_affected_rows;
    results.data_source_results = std::move(*gq_status);
  }
  return results;
}

StatusRecord FetchNextPageResultSet(StatementHandle& stmt_handle) {
  // In case of non-HTAPI execution there is no pagination, so we have to return
  // `SQL_NO_DATA`
  if (stmt_handle.GetPagingInfo().page_token.empty()) {
    return StatusRecord(
        {SQLStates::k_SQL_NO_DATA(), "No more data to return."});
  }

  stmt_handle.GetResultSet().rows.clear();
  auto ds_status_record_or = FetchNextPageOfQueryResults(
      stmt_handle, stmt_handle.GetPostQueryRequest());
  if (!ds_status_record_or) {
    stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
    return ds_status_record_or.GetStatusRecord();
  }
  DSResults results;
  results.num_dml_affected_rows = ds_status_record_or->num_dml_affected_rows;
  results.job_ref = ds_status_record_or->job_reference;
  stmt_handle.GetPagingInfo().page_token = ds_status_record_or->page_token;
  results.data_source_results = std::move(*ds_status_record_or);
  auto rs_status_record_or = ProcessQueryResults(results);
  stmt_handle.SetDSResults(std::move(results));
  if (!rs_status_record_or) {
    stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
    LOG(ERROR) << "FetchNextPageResultSet:: "
               << rs_status_record_or.GetStatusRecord().message;
    return rs_status_record_or.GetStatusRecord();
  }
  stmt_handle.SetResultSet(std::move(*rs_status_record_or));
  return StatusRecord::Ok();
}

StatusRecordOr<GetQueryResults> FetchNextPageOfQueryResults(
    StatementHandle& stmt_handle, PostQueryRequest const& post_query_request) {
  GetQueryResultsRequest get_query_results_request;
  get_query_results_request.set_project_id(post_query_request.project_id());
  get_query_results_request.set_job_id(stmt_handle.GetPagingInfo().job_id);
  get_query_results_request.set_location(
      post_query_request.query_request().location());
  get_query_results_request.set_timeout(
      post_query_request.query_request().timeout());
  get_query_results_request.set_page_token(
      stmt_handle.GetPagingInfo().page_token);

  ExponentialBackoffPolicy backoff(chrono_ms(10), chrono_ms(200), 2);
  auto start_time = std::chrono::system_clock::now();
  auto timeout_ms =
      std::chrono::milliseconds(post_query_request.query_request().timeout());

  auto* connection_handle = stmt_handle.GetConnectionHandle();
  Options options;
  auto job_client = connection_handle->GetClient();

  LOG(INFO) << "FetchNextPageOfQueryResults:: Request body: "
            << get_query_results_request.DebugString("");

  while (true) {
    if (timeout_ms.count() > 0 &&
        std::chrono::system_clock::now() > start_time + timeout_ms) {
      std::string message = "The query timeout period of " +
                            std::to_string(timeout_ms.count()) +
                            "ms has expired";
      LOG(ERROR) << "FetchNextPageOfQueryResults:: " << message;
      return StatusRecord{SQLStates::k_HYT00(), message};
    }

    auto get_query_results_partial =
        job_client->GetQueryResults(get_query_results_request, options);

    if (!get_query_results_partial) {
      LOG(ERROR) << "FetchNextPageOfQueryResults::QueryResults failed: "
                 << get_query_results_partial.status().message();
      return StatusRecord::ConvertFrom(get_query_results_partial.status());
    }

    // Wait if job is not yet complete and no rows have arrived
    if (!get_query_results_partial->job_complete &&
        get_query_results_partial->rows.empty()) {
      std::this_thread::sleep_for(backoff.OnCompletion());
      continue;
    }

    LOG(INFO) << "FetchNextPageOfQueryResults:: Response body: "
              << get_query_results_partial->DebugString("");

    // Replace get_query_results with this latest result
    GetQueryResults get_query_results = *get_query_results_partial;
    stmt_handle.GetPagingInfo().page_token =
        get_query_results_partial->page_token;

    return get_query_results;
  }
}
}  // namespace google::cloud::odbc_bq_driver_internal
