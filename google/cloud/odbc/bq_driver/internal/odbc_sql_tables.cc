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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_tables.h"
#include "google/cloud/odbc/bq_client_interface/utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include <algorithm>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using google::cloud::odbc_bigquery_client_interface::DatasetFilter;
using google::cloud::odbc_bigquery_client_interface::MaxRetriesOption;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

namespace {

std::string const kTableNameParam = "table_name";
std::string const kTableTypeParam = "table_type";
std::string const kBasicQuery =
    "SELECT table_name, table_type FROM INFORMATION_SCHEMA.TABLES";

std::string const kBaseTable = "BASE TABLE";
std::string const kTable = "TABLE";
std::string const kClone = "CLONE";

// Map a BigQuery REST tables.list "type" value to the ODBC TABLE_TYPE spelling.
std::string NormalizeRestTableType(std::string type) {
  // Base tables and clones are surfaced as plain "TABLE" to third-party tools.
  if (type == kBaseTable || type == kClone || type == "BASE_TABLE" ||
      type == kTable) {
    return kTable;
  }
  // REST spells multi-word types with underscores (e.g. MATERIALIZED_VIEW);
  // ODBC clients expect spaces (e.g. MATERIALIZED VIEW).
  std::replace(type.begin(), type.end(), '_', ' ');
  return type;
}
}  // namespace

StatusRecord ValidateInputParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* table_name, SQLSMALLINT table_name_len,
    SQLSMALLINT table_type_len, SQLULEN metadata_id) {
  // Validate table and table related parameters.
  auto status_record = ValidateTableParameters(
      catalog_name, catalog_name_len, schema_name, schema_name_len, table_name,
      table_name_len, metadata_id);
  if (!status_record.ok()) {
    LOG(ERROR) << "ValidateInputParameters::ValidateTableParameters:: "
               << status_record.message;
    return status_record;
  }
  // SQLTables specific validation.
  if (table_type_len < 0 && table_type_len != SQL_NTS) {
    LOG(ERROR) << "ValidateInputParameters:: Invalid buffer length - table "
                  "type length is invalid.";
    return StatusRecord{SQLStates::k_HY090(),
                        "Invalid buffer length - table type length is invalid"};
  }

  return StatusRecord::Ok();
}

StatusRecordOr<std::vector<std::string>> GetFilteredProjectIds(
    ODBCBQClient& bq_client, std::string const& projects_filter,
    SQLULEN metadata_id) {
  LOG(INFO) << "GetFilteredProjectIds:: Start (filter='" << projects_filter
            << "', metadata_id=" << metadata_id << ")";
  std::vector<std::string> project_ids;
  auto filter_regex = BuildRegex(projects_filter, metadata_id);
  // For now, we use default options.
  // We can set timeout here as needed later.
  Options options;
  LOG(INFO) << "GetFilteredProjectIds:: calling ListAllProjects (filter='"
            << projects_filter << "', metadata_id=" << metadata_id << ")";
  StatusRecordOr<std::vector<Project>> projects =
      bq_client.ListAllProjects(options);
  if (!projects) {
    LOG(ERROR) << "GetFilteredProjectIds::ListAllProjects:: "
               << projects.GetStatusRecord().message;
    return projects.GetStatusRecord();
  }
  LOG(INFO) << "GetFilteredProjectIds:: ListAllProjects returned "
            << projects->size() << " projects; applying filter";
  for (auto const& project : *projects) {
    if ((!metadata_id && projects_filter == "%") ||
        re2::RE2::FullMatch(project.id, *filter_regex)) {
      project_ids.push_back(project.id);
    }
  }
  LOG(INFO) << "GetFilteredProjectIds:: kept " << project_ids.size()
            << " projects after filter";
  return project_ids;
}

StatusRecordOr<std::vector<std::string>> GetFilteredDatasetIds(
    ODBCBQClient& bq_client, std::string const& project_id,
    std::string const& datasets_filter, SQLULEN metadata_id) {
  std::vector<std::string> dataset_ids;
  auto filter_regex = BuildRegex(datasets_filter, metadata_id);
  // For now, we use default options.
  // We can set timeout here as needed later.
  Options options;
  DatasetFilter filter;
  filter.all = false;
  StatusRecordOr<std::vector<ListFormatDataset>> datasets =
      bq_client.FilterDatasets(project_id, filter, options);
  if (!datasets) {
    LOG(ERROR) << "GetFilteredDatasetIds::FilterDatasets:: "
               << datasets.GetStatusRecord().message;
    return datasets.GetStatusRecord();
  }
  for (auto const& dataset : *datasets) {
    if ((!metadata_id && datasets_filter == "%") ||
        re2::RE2::FullMatch(dataset.dataset_reference.dataset_id,
                            *filter_regex)) {
      dataset_ids.push_back(dataset.dataset_reference.dataset_id);
    }
  }
  return dataset_ids;
}

std::string ConstructTableNameWhereClause(std::string const& tables_filter,
                                          SQLULEN metadata_id) {
  if (metadata_id == SQL_TRUE) {
    return "LOWER(table_name) = LOWER(@" + kTableNameParam + ")";
  }
  if (tables_filter != "%") {
    return "table_name LIKE @" + kTableNameParam;
  }
  return "";
}

std::string ConstructTableTypeWhereClause(std::string table_types_filter) {
  Trim(table_types_filter);
  if (table_types_filter != "%") {
    return "table_type IN UNNEST (@" + kTableTypeParam + ")";
  }
  return "";
}

std::string ProcessTableTypes(std::string const& table_types_filter) {
  std::vector<std::string> types = SplitTableTypes(table_types_filter);
  for (std::string& type : types) {
    if (type == kTable) {
      type = kBaseTable;
      type.append(", ");
      type.append(kClone);
    }
  }
  return Join(types, ", ");
}

std::vector<ColumnSchema> ExtractColumnSchema(
    std::map<std::string, ColumnSchema> const& schema) {
  std::vector<ColumnSchema> col_schema;
  col_schema.reserve(schema.size());
  for (auto const& pair : schema) {
    col_schema.push_back(pair.second);
  }
  return col_schema;
}

StatusRecordOr<std::string> ConstructQuery(
    std::string tables_filter, std::string const& table_types_filter,
    SQLULEN metadata_id, std::vector<QueryParameter>& named_query_params) {
  if (metadata_id == SQL_TRUE) {
    RTrim(tables_filter);
  }
  std::string table_name_where_clause =
      ConstructTableNameWhereClause(tables_filter, metadata_id);
  std::string table_type_where_clause =
      ConstructTableTypeWhereClause(table_types_filter);
  if (!table_name_where_clause.empty()) {
    auto query_param =
        ConstructStringQueryParameter(kTableNameParam, tables_filter);
    if (!query_param) {
      return query_param.GetStatusRecord();
    }
    named_query_params.push_back(*query_param);
  }
  if (!table_type_where_clause.empty()) {
    std::vector<std::string> table_types = SplitTableTypes(table_types_filter);
    auto query_param =
        ConstructStringArrayQueryParameter(kTableTypeParam, table_types);
    if (!query_param) {
      return query_param.GetStatusRecord();
    }
    named_query_params.push_back(*query_param);
  }
  if (!table_name_where_clause.empty() && !table_type_where_clause.empty()) {
    return kBasicQuery + " WHERE " + table_name_where_clause + " AND " +
           table_type_where_clause;
  }
  if (!table_name_where_clause.empty() || !table_type_where_clause.empty()) {
    return kBasicQuery + " WHERE " + table_name_where_clause +
           table_type_where_clause;
  }
  return kBasicQuery;
}

std::vector<std::string> AppendAdditionalProjectsIfMissing(
    std::vector<std::string> base_projects,
    std::string const& additional_projects) {
  std::set<std::string> existing_ids(base_projects.begin(),
                                     base_projects.end());

  std::stringstream ss(additional_projects);
  std::string project_id;
  while (std::getline(ss, project_id, ',')) {
    // Trim leading/trailing whitespace
    project_id.erase(0, project_id.find_first_not_of(" \t"));
    project_id.erase(project_id.find_last_not_of(" \t") + 1);

    if (!project_id.empty() &&
        existing_ids.find(project_id) == existing_ids.end()) {
      base_projects.push_back(project_id);
    }
  }
  return base_projects;
}

StatusRecordOr<std::vector<FilteredTableResponse>> GetFilteredTables(
    ODBCBQClient& bq_client, std::string const& project_id,
    std::string const& dataset_id, std::string const& tables_filter,
    std::string const& table_types_filter, SQLULEN metadata_id,
    int max_retries) {
  // List all tables in the dataset via the lightweight tables.list REST API and
  // filter client-side. This avoids running an INFORMATION_SCHEMA.TABLES query
  // job per dataset, which carries several seconds of fixed latency each and
  // dominated SQLTables() runtime. (SQLColumns already uses this approach.)
  Options options;
  options.set<MaxRetriesOption>(max_retries);
  auto tables_status = bq_client.ListAllTables(project_id, dataset_id, options);
  if (!tables_status) {
    auto const& status = tables_status.GetStatusRecord();
    // A dataset may be deleted between listing datasets and reading its tables;
    // treat "not found" as an empty dataset rather than failing the whole call.
    if (status.native_error_code == 404) {
      LOG(WARNING) << "GetFilteredTables:: Skipping dataset not found: '"
                   << project_id << "." << dataset_id
                   << "': " << status.message;
      return std::vector<FilteredTableResponse>{};
    }
    LOG(ERROR) << "GetFilteredTables::ListAllTables:: " << status.message;
    return status;
  }

  // Match table names with the same semantics as project/dataset filtering: an
  // ODBC LIKE pattern, or a case-insensitive exact match when metadata_id is
  // set.
  auto name_regex = BuildRegex(tables_filter, metadata_id);

  // "%" (== SQL_ALL_TABLE_TYPES) means "all types"; otherwise build the set of
  // requested ODBC table types.
  bool const match_all_types = (table_types_filter == kMatchAll);
  std::set<std::string> requested_types;
  if (!match_all_types) {
    for (auto& type : SplitTableTypes(table_types_filter)) {
      requested_types.insert(std::move(type));
    }
  }

  std::vector<FilteredTableResponse> table_response;
  for (auto const& list_table : *tables_status) {
    std::string const& table_id = list_table.table_reference.table_id;
    bool const name_matches =
        (metadata_id == 0 && tables_filter == kMatchAll) ||
        re2::RE2::FullMatch(table_id, *name_regex);
    if (!name_matches) {
      continue;
    }
    std::string table_type = NormalizeRestTableType(list_table.type);
    if (!match_all_types &&
        requested_types.find(table_type) == requested_types.end()) {
      continue;
    }
    table_response.push_back({table_id, std::move(table_type)});
  }
  return table_response;
}

ResultSet CreateResultSetForProjects(
    std::vector<std::string> const& project_ids) {
  ResultSet result_set;
  result_set.row_schema = ExtractColumnSchema(kSchema);
  for (auto const& project_id : project_ids) {
    DSValue project_id_value;
    StringToDSValue(project_id, project_id_value);
    result_set.rows.push_back(
        {project_id_value, kNullValue, kNullValue, kNullValue, kNullValue});
  }
  return result_set;
}

ResultSet CreateResultSetForDatasets(
    std::vector<std::string> const& dataset_ids) {
  ResultSet result_set;
  result_set.row_schema = ExtractColumnSchema(kSchema);
  for (auto const& dataset_id : dataset_ids) {
    DSValue dataset_id_value;
    StringToDSValue(dataset_id, dataset_id_value);
    result_set.rows.push_back(
        {kNullValue, dataset_id_value, kNullValue, kNullValue, kNullValue});
  }
  return result_set;
}

ResultSet CreateResultSetForTableTypes() {
  ResultSet result_set;
  result_set.row_schema = ExtractColumnSchema(kSchema);
  for (auto const& table_type : kAllTableTypes) {
    DSValue table_type_value;
    StringToDSValue(table_type, table_type_value);
    result_set.rows.push_back(
        {kNullValue, kNullValue, kNullValue, table_type_value, kNullValue});
  }
  return result_set;
}

ResultSet ProcessStringResults(
    std::vector<std::vector<std::string>> const& rows) {
  ResultSet result_set;
  result_set.row_schema = ExtractColumnSchema(kSchema);
  for (auto const& row : rows) {
    DSRow rs_row;
    for (auto const& col : row) {
      DSValue value;
      StringToDSValue(col, value);
      rs_row.emplace_back(value);
    }
    result_set.rows.emplace_back(rs_row);
  }
  return result_set;
}

StatusRecordOr<ResultSet> GetResultSetForProjects(
    ODBCBQClient& bq_client, SQLULEN metadata_id,
    std::string const& additional_projects,
    std::string const& connection_catalog) {
  LOG(INFO) << "GetResultSetForProjects:: Start (metadata_id=" << metadata_id
            << ", additional_projects='" << additional_projects
            << "', connection_catalog='" << connection_catalog << "')";

  // Fast path: when the connection has a catalog bound via the DSN, skip the
  // BigQuery ListAllProjects round-trip. Returning [catalog] +
  // additional_projects is what HANA SDA's catalog-discovery probe actually
  // needs, and it bypasses the failure in ListAllProjects that surfaces as
  // "Cannot get remote source objects" when SDA calls CHECK_REMOTE_SOURCE.
  if (!connection_catalog.empty()) {
    LOG(INFO) << "GetResultSetForProjects:: fast-path entry; using catalog '"
              << connection_catalog << "'";
    std::vector<std::string> project_list = {connection_catalog};
    LOG(INFO) << "GetResultSetForProjects:: fast-path; project_list.size()="
              << project_list.size();
    if (!additional_projects.empty()) {
      LOG(INFO) << "GetResultSetForProjects:: fast-path; appending additional";
      project_list = AppendAdditionalProjectsIfMissing(std::move(project_list),
                                                       additional_projects);
      LOG(INFO)
          << "GetResultSetForProjects:: fast-path; after AppendAdditional: "
          << project_list.size();
    }
    LOG(INFO) << "GetResultSetForProjects:: fast-path; building result set";
    auto rs = CreateResultSetForProjects(project_list);
    LOG(INFO) << "GetResultSetForProjects:: fast-path; end ("
              << project_list.size() << " projects)";
    return rs;
  }

  LOG(INFO) << "GetResultSetForProjects:: slow-path; calling "
               "GetFilteredProjectIds";
  auto project_ids_status =
      GetFilteredProjectIds(bq_client, kMatchAll, metadata_id);
  LOG(INFO) << "GetResultSetForProjects:: slow-path; GetFilteredProjectIds "
               "returned, ok="
            << static_cast<int>(static_cast<bool>(project_ids_status));
  if (!project_ids_status) {
    LOG(ERROR) << "GetResultSetForProjects::GetFilteredProjectIds:: "
               << project_ids_status.GetStatusRecord().message;
    return project_ids_status.GetStatusRecord();
  }

  std::vector<std::string> project_list = *project_ids_status;
  LOG(INFO) << "GetResultSetForProjects:: slow-path; project_list.size()="
            << project_list.size();
  if (!additional_projects.empty()) {
    project_list = AppendAdditionalProjectsIfMissing(std::move(project_list),
                                                     additional_projects);
    LOG(INFO) << "GetResultSetForProjects:: slow-path; after AppendAdditional: "
              << project_list.size();
  }

  LOG(INFO) << "GetResultSetForProjects:: slow-path; building result set";
  auto rs = CreateResultSetForProjects(project_list);
  LOG(INFO) << "GetResultSetForProjects:: slow-path; end";
  return rs;
}

StatusRecordOr<ResultSet> GetResultSetForDatasets(
    ODBCBQClient& bq_client, SQLULEN metadata_id,
    std::string const& catalog_name, std::string const& additional_projects) {
  auto project_ids_status =
      GetFilteredProjectIds(bq_client, catalog_name, metadata_id);
  if (!project_ids_status) {
    LOG(ERROR) << "GetResultSetForDatasets::GetFilteredProjectIds:: "
               << project_ids_status.GetStatusRecord().message;
    return project_ids_status.GetStatusRecord();
  }

  std::vector<std::string> project_list = *project_ids_status;

  if (!additional_projects.empty()) {
    project_list = AppendAdditionalProjectsIfMissing(std::move(project_list),
                                                     additional_projects);
  }

  // Fetch datasets for each project in parallel. Previously this was a serial
  // loop issuing one FilterDatasets REST call per project, which dominated the
  // SQLTables(SQL_ALL_SCHEMAS) latency when many projects are accessible.
  using DatasetTaskResult = std::vector<std::string>;
  auto dataset_task =
      [&](std::string const& project_id) -> StatusRecordOr<DatasetTaskResult> {
    return GetFilteredDatasetIds(bq_client, project_id, kMatchAll, metadata_id);
  };

  std::shared_ptr<TraceOptions> trace_option = TraceOptions::GetTraceOption();
  int max_threads = trace_option->max_threads;
  auto parallel_results_or =
      ExecuteParallelTasks<std::string, DatasetTaskResult>(
          max_threads, project_list, dataset_task);
  if (!parallel_results_or) {
    LOG(ERROR) << "GetResultSetForDatasets::GetFilteredDatasetIds:: "
               << parallel_results_or.GetStatusRecord().message;
    return parallel_results_or.GetStatusRecord();
  }

  std::vector<std::string> dataset_ids;
  for (auto& ids : *parallel_results_or) {
    dataset_ids.insert(dataset_ids.end(), std::make_move_iterator(ids.begin()),
                       std::make_move_iterator(ids.end()));
  }

  return CreateResultSetForDatasets(dataset_ids);
}

StatusRecordOr<ResultSet> GetResultSetForTables(
    StatementHandle& stmt_handle, ODBCBQClient& bq_client,
    std::string const& project_filter, std::string const& dataset_filter,
    std::string const& table_filter, std::string const& table_type_filter,
    SQLULEN metadata_id) {
  // When SQL_ATTR_METADATA_ID is true, the catalog argument is an exact project
  // identifier (not a pattern). Skip enumerating every accessible project via
  // projects.list and use the name directly.
  std::vector<std::string> project_list;
  if (metadata_id == SQL_TRUE) {
    project_list = {project_filter};
  } else {
    auto projects_status_record_or =
        GetFilteredProjectIds(bq_client, project_filter, metadata_id);
    if (!projects_status_record_or) {
      LOG(ERROR) << "GetResultSetForTables::GetFilteredProjectIds:: "
                 << projects_status_record_or.GetStatusRecord().message;
      return projects_status_record_or.GetStatusRecord();
    }
    project_list = *projects_status_record_or;
  }
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  // Append additional projects if any
  project_list = AppendAdditionalProjectsIfMissing(
      std::move(project_list), conn_handle.GetDsn().additional_projects);

  // 1. Prepare the list of tasks (Project + Dataset combinations)
  struct TaskInput {
    std::string project_id;
    std::string dataset_id;
  };
  std::vector<TaskInput> tasks;

  for (auto const& project_id : project_list) {
    // When SQL_ATTR_METADATA_ID is true, the schema argument is an exact
    // dataset identifier (not a pattern). Skip listing every dataset in the
    // project via datasets.list
    if (metadata_id == SQL_TRUE) {
      tasks.push_back({project_id, dataset_filter});
      continue;
    }
    auto datasets_status_record_or = GetFilteredDatasetIds(
        bq_client, project_id, dataset_filter, metadata_id);
    if (!datasets_status_record_or) {
      LOG(ERROR) << "GetResultSetForTables::GetFilteredDatasetIds:: "
                 << datasets_status_record_or.GetStatusRecord().message;
      return datasets_status_record_or.GetStatusRecord();
    }
    for (auto const& dataset_id : *datasets_status_record_or) {
      tasks.push_back({project_id, dataset_id});
    }
  }

  // 2. Define the unit of work for the parallel utility
  using TaskResult = std::vector<std::vector<std::string>>;

  int const max_retries = conn_handle.GetDsn().max_retries;
  auto parallel_func =
      [&](TaskInput const& input) -> StatusRecordOr<TaskResult> {
    auto tables_status_record_or = GetFilteredTables(
        bq_client, input.project_id, input.dataset_id, table_filter,
        table_type_filter, metadata_id, max_retries);

    if (!tables_status_record_or) {
      LOG(ERROR) << "GetResultSetForTables::GetFilteredTables:: "
                 << tables_status_record_or.GetStatusRecord().message;
      return tables_status_record_or.GetStatusRecord();
    }

    TaskResult batch_rows;
    batch_rows.reserve(tables_status_record_or->size());
    for (auto const& table : *tables_status_record_or) {
      batch_rows.push_back({input.project_id, input.dataset_id,
                            table.table_name, table.table_type,
                            input.project_id});
    }
    return batch_rows;
  };

  // 3. Execute tasks using the generic utility
  std::shared_ptr<TraceOptions> trace_option = TraceOptions::GetTraceOption();
  int max_threads = trace_option->max_threads;
  auto parallel_results_or = ExecuteParallelTasks<TaskInput, TaskResult>(
      max_threads, tasks, parallel_func);

  if (!parallel_results_or) {
    return parallel_results_or.GetStatusRecord();
  }

  // 4. Flatten the results
  std::vector<std::vector<std::string>> tables_result_set;
  for (auto& batch : *parallel_results_or) {
    tables_result_set.insert(tables_result_set.end(),
                             std::make_move_iterator(batch.begin()),
                             std::make_move_iterator(batch.end()));
  }

  return ProcessStringResults(tables_result_set);
}

}  // namespace google::cloud::odbc_bq_driver_internal
