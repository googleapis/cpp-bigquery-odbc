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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include <thread>

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
using ::google::cloud::bigquery_v2_minimal_internal::Job;
using ::google::cloud::bigquery_v2_minimal_internal::QueryRequest;
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResults;
using ::google::cloud::bigquery_v2_minimal_internal::GetQueryResultsRequest;
using ::google::cloud::bigquery_v2_minimal_internal::PostQueryRequest;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using google::cloud::odbc_bq_driver_internal::DescriptorRecord;
using google::cloud::odbc_bq_driver_internal::DoubleStrToInt;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::SQLStates;
using chrono_ms = std::chrono::milliseconds;

StatusRecord ConstructPositionalQueryParams(
    DescriptorHandle& apd, DescriptorHandle& ipd,
    std::vector<QueryParameter>& basic_query_params, bool is_data_buff_req) {
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
    // SQL_NULL_DATA implies the application wants to use empty data.
    if (apd_rec.indicator_ptr != nullptr &&
        *apd_rec.indicator_ptr == SQL_NULL_DATA) {
      continue;
    }

    bool is_data_at_exec = false;
    if (apd_rec.indicator_ptr &&
        (*(apd_rec.indicator_ptr) == SQL_DATA_AT_EXEC ||
         *(apd_rec.indicator_ptr) <= SQL_LEN_DATA_AT_EXEC_OFFSET)) {
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
    DataBuffer data;
    if (is_data_buff_req && is_data_at_exec) {
      owned_octet_lengths.push_back(static_cast<SQLLEN>(
          apd_rec.data_buffer
              .size()));  // Handle stack-use-after-scope for octet_length
      SQLLEN* octet_length_ptr = &owned_octet_lengths.back();
      data = {apd_rec.concise_type, buff, *octet_length_ptr, octet_length_ptr};
    } else {
      data = {apd_rec.concise_type, buff, apd_rec.octet_length,
              apd_rec.octet_length_ptr};
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
  auto pq_status = bq_client->PostQuery(post_query_request, post_query_options);
  if (!pq_status) {
    LOG(ERROR) << "ExecuteScript::PostQuery:: "
               << pq_status.GetStatusRecord().message;
    return pq_status.GetStatusRecord();
  }

  DSResults results;
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

  // Retrieve job information
  Options list_job_options;
  auto all_jobs_status =
      bq_client->ListAllJobs(pq_status->job_reference.project_id,
                             pq_status->job_reference.job_id, list_job_options);
  if (!all_jobs_status) {
    LOG(ERROR) << "ExecuteScript::ListAllJobs:: "
               << all_jobs_status.GetStatusRecord().message;
    return all_jobs_status.GetStatusRecord();
  }

  for (auto const& job_status : all_jobs_status.GetValue()) {
    if (job_status.statistics.job_query_stats.statement_type !=
            "CREATE_PROCEDURE" &&
        job_status.statistics.script_statistics.evaluation_kind.value ==
            "STATEMENT") {
      stmt_handle.SetJobData(
          job_status.job_reference.job_id,
          job_status.statistics.job_query_stats.statement_type);
    }
  }

  // Fetch query results if job data is available
  if (!stmt_handle.HasJobData()) {
    return results;
  }
  auto job_status = stmt_handle.GetNextJobData();
  if (!job_status.Ok()) {
    LOG(ERROR) << "ExecuteScript::GetNextJobData:: "
               << job_status.GetStatusRecord().message;
    return job_status.GetStatusRecord();
  }
  auto job_data = job_status.GetValue();
  std::string job_id = job_data.first;
  std::string statement_type = job_data.second;

  Options query_results_options;
  auto gq_status = bq_client->GetAllQueryResults(
      pq_status->job_reference.project_id, job_id,
      pq_status->job_reference.location,
      post_query_request.query_request().timeout(), query_results_options);

  if (!gq_status) {
    LOG(ERROR) << "ExecuteScript::GetAllQueryResults:: "
               << gq_status.GetStatusRecord().message;
    return gq_status.GetStatusRecord();
  }

  // Assign DML row counts
  if (statement_type == "INSERT" || statement_type == "UPDATE" ||
      statement_type == "DELETE") {
    results.num_dml_affected_rows = gq_status->num_dml_affected_rows;
  }
  results.data_source_results = *gq_status;
  stmt_handle.SetDSResults(results);

  if (!conn_handle->IsSessionStarted() &&
      !pq_status->session_info.session_id.empty()) {
    conn_handle->SetSessionId(pq_status->session_info.session_id);
  }

  return results;
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
        col_schema.col_type = BQDataType::kInt64;
        break;
      case arrow::Type::BOOL:
        col_schema.col_type = BQDataType::kBool;
        break;
      case arrow::Type::TIMESTAMP:
        col_schema.col_type = BQDataType::kTimeStamp;
        break;
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

StatusRecord ProcessRecordBatch(
    std::shared_ptr<arrow::Schema> schema,
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

    // Helper lambda to handle nulls efficiently per column type
    auto is_null = [&](int64_t row) { return column->IsNull(row); };

    switch (type_id) {
      case arrow::Type::INT64: {
        auto int_arr = std::static_pointer_cast<arrow::Int64Array>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (int_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
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
          if (dbl_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
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
          if (str_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            StringToDSValue(str_arr->GetString(row),
                            result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::BOOL: {
        auto bool_arr = std::static_pointer_cast<arrow::BooleanArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (bool_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            BooleanToDSValue(bool_arr->Value(row), result_set.rows[row][col_i]);
          }
        }
        break;
      }
      case arrow::Type::BINARY: {
        auto bin_arr = std::static_pointer_cast<arrow::BinaryArray>(column);
        for (int64_t row = 0; row < num_rows; ++row) {
          if (bin_arr->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
          } else {
            StringToDSValue(bin_arr->GetString(row),
                            result_set.rows[row][col_i]);
          }
        }
        break;
      }
      // For complex types, we fall back to the existing logic but apply it
      // column-wise. We still avoid the GetScalar() overhead where possible,
      // but use ToString() to maintain compatibility with the existing parsing
      // helpers (ConvertStringTo...)
      default: {
        for (int64_t row = 0; row < num_rows; ++row) {
          if (column->IsNull(row)) {
            result_set.rows[row][col_i] = kNullValue;
            continue;
          }

          // We use GetScalar here only for complex types not optimized above.
          // Note: Creating a scalar per cell is slow, but doing it only for
          // timestamps/structs is better than doing it for Int64/Double too.
          auto scalar_res = column->GetScalar(row);
          if (!scalar_res.ok()) {
            return StatusRecord{SQLStates::k_HY000(),
                                "Internal Error: Unable to parse scalar"};
          }
          std::string data = scalar_res.ValueOrDie()->ToString();

          DSValue& row_val = result_set.rows[row][col_i];

          switch (type_id) {
            case arrow::Type::TIMESTAMP: {
              StatusRecordOr<SQL_TIMESTAMP_STRUCT> time_struct_status =
                  ConvertStringToTimestampStruct(data);
              if (!time_struct_status)
                return time_struct_status.GetStatusRecord();
              TimestampToDSValue(*time_struct_status, row_val);
              break;
            }
            case arrow::Type::TIME64: {
              SQL_TIME_STRUCT t_data = ConvertToTimeStruct(data);
              TimeToDSValue(t_data, row_val);
              break;
            }
            case arrow::Type::DATE32: {
              StatusRecordOr<SQL_DATE_STRUCT> date_struct =
                  ConvertStringToDateStruct(data);
              if (!date_struct.Ok()) return date_struct.GetStatusRecord();
              DateToDSValue(*date_struct, row_val);
              break;
            }
            case arrow::Type::LIST: {
              if (data.rfind("list<", 0) == 0) {
                auto pos = data.find('[');
                if (pos != std::string::npos) data = data.substr(pos);
              }
              StringToDSValue(data, row_val);
              break;
            }
            case arrow::Type::DECIMAL128:
            case arrow::Type::DECIMAL256: {
              NumericToDSValue(data, row_val);
              break;
            }
            default: {
              StringToDSValue(data, row_val);
              break;
            }
          }
        }
        break;
      }
    }
  }
  return StatusRecord::Ok();
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
    LOG(INFO) << "FetchBQDataReadArrow:: Read stream ended.";
    return StatusRecord({SQLStates::k_SQL_NO_DATA(), "Read stream ended."});
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

  Options options;
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
    stmt_handle.SetResultSet(result_set);
    std::shared_ptr<arrow::Schema> schema = *schema_status;
    stmt_handle.SetArrowSchema(schema);

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
  job.configuration.query.destination_table.table_id = GenerateTableId();

  job.configuration.query.parameter_mode = "POSITIONAL";
  job.configuration.query.allow_large_results = true;

  Options opt;
  auto bq_client = conn_handle.GetClient();
  auto insert_response =
      bq_client->InsertJob(conn_handle.GetDsn().catalog, job, opt);
  if (!insert_response.Ok()) {
    return insert_response.GetStatusRecord();
  }
  // Here we are replacing the dry run Job created during SQLPrepare.
  // This should be safe since the same query is executed during HTAPI flow too.
  stmt_handle.SetPreparedJob(*insert_response);

  // Wait for Job to complete
  std::string job_status = insert_response->status.state;
  ExponentialBackoffPolicy backoff(chrono_ms(100), chrono_ms(200), 2);
  StatusRecordOr<Job> get_job_response;
  while (job_status != "DONE") {
    std::this_thread::sleep_for(backoff.OnCompletion());
    get_job_response = bq_client->GetJob(
        conn_handle.GetDsn().catalog, insert_response->job_reference.job_id,
        insert_response->job_reference.location, opt);
    if (!get_job_response.Ok()) {
      return get_job_response.GetStatusRecord();
    }
    job_status = get_job_response->status.state;
  }
  std::string error_message = get_job_response->status.error_result.message;
  if (!error_message.empty()) {
    LOG(ERROR) << "FetchBQDataRead:: " << error_message;
    return StatusRecord{SQLStates::k_HY000(), error_message};
  }

  return FetchBQDataReadArrow(
      stmt_handle, insert_response->configuration.query.destination_table);
}

#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

// TODO(b/388947009): Add unit tests for this function
StatusRecordOr<DSResults> FetchBQData(
    StatementHandle& stmt_handle, PostQueryRequest const& post_query_request,
    [[maybe_unused]] bool with_htapi) {
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
#if (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)
  if (with_htapi && conn_handle.GetDsn().allow_htapi) {
    StatusRecord read_status = FetchBQDataRead(stmt_handle, post_query_request);
    if (!read_status.ok()) {
      return read_status;
    }
    DSResults results;
    results.data_source_results = stmt_handle.GetResultSet();
    return results;
  }
#endif  // (!defined(_WIN32) || defined(_WIN64)) && !defined(NO_ARROW)

  auto pq_status = PostQueryWithoutResults(conn_handle, post_query_request);
  if (!pq_status) {
    return pq_status.GetStatusRecord();
  }
  DSResults results;
  results.num_dml_affected_rows = pq_status->num_dml_affected_rows;
  results.job_ref = pq_status->job_reference;
  stmt_handle.GetPagingInfo().job_id = pq_status->job_reference.job_id;
  stmt_handle.GetPagingInfo().page_token = pq_status->page_token;
  if (pq_status->job_complete) {
    // we have gotten all the results
    results.data_source_results = *pq_status;
  } else {
    auto gq_status =
        FetchNextPageOfQueryResults(stmt_handle, post_query_request);
    if (!gq_status) {
      LOG(ERROR) << "FetchBQData::FetchNextPageOfQueryResults:: "
                 << gq_status.GetStatusRecord().message;
      return gq_status.GetStatusRecord();
    }
    results.num_dml_affected_rows = gq_status->num_dml_affected_rows;
    results.data_source_results = *gq_status;
  }
  if (!conn_handle.IsSessionStarted() &&
      !pq_status->session_info.session_id.empty()) {
    conn_handle.SetSessionId(pq_status->session_info.session_id);
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
  results.data_source_results = *ds_status_record_or;
  stmt_handle.GetPagingInfo().page_token = ds_status_record_or->page_token;
  stmt_handle.SetDSResults(results);
  auto rs_status_record_or = ProcessQueryResults(results);
  if (!rs_status_record_or) {
    stmt_handle.SetStmtState(StmtStates::kStatementPrepared);
    LOG(ERROR) << "FetchNextPageResultSet:: "
               << rs_status_record_or.GetStatusRecord().message;
    return rs_status_record_or.GetStatusRecord();
  }
  stmt_handle.SetResultSet(*rs_status_record_or);
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

  Options options;
  auto job_client = stmt_handle.GetConnectionHandle()->GetClient();

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
