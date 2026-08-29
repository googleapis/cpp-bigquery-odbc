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
#include <optional>

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

std::optional<std::string> LiteralFromOdbcPattern(std::string const& filter,
                                                  SQLULEN metadata_id) {
  if (metadata_id == SQL_TRUE) {
    std::string literal = filter;
    RTrim(literal);
    return literal;
  }
  if (filter.empty()) {
    return std::nullopt;
  }
  std::string literal;
  literal.reserve(filter.size());
  for (size_t i = 0; i < filter.size(); ++i) {
    char c = filter[i];
    if (c == '\\') {
      // Escape: the next char is a literal (consume both). A lone trailing
      // backslash is dropped, matching CastOdbcRegexToCppRegex().
      if (i + 1 < filter.size()) {
        literal.push_back(filter[i + 1]);
        ++i;
      }
    } else if (c == '%' || c == '_') {
      // A genuine wildcard: the filter must be expanded by enumeration.
      return std::nullopt;
    } else {
      literal.push_back(c);
    }
  }
  return literal;
}

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

std::vector<std::string> FilterAllowedProjects(
    std::string const& allowed_projects, std::string const& projects_filter,
    SQLULEN metadata_id) {
  std::vector<std::string> project_ids;
  auto filter_regex = BuildRegex(projects_filter, metadata_id);
  std::vector<std::string> allowed_ids = Split(allowed_projects, ",");
  for (auto& project_id : allowed_ids) {
    Trim(project_id);
    if (project_id.empty()) {
      continue;
    }
    if ((!metadata_id && projects_filter == kMatchAll) ||
        re2::RE2::FullMatch(project_id, *filter_regex)) {
      project_ids.push_back(std::move(project_id));
    }
  }
  return project_ids;
}

StatusRecordOr<std::vector<std::string>> GetFilteredProjectIds(
    ODBCBQClient& bq_client, std::string const& projects_filter,
    SQLULEN metadata_id, std::string const& allowed_projects) {
  // An explicit allowlist replaces REST-based project discovery entirely.
  // projects.list enumerates every project the authenticated principal can
  // reach, which is slow and quota-heavy for accounts with access to many
  // projects; when the user has named the projects they care about there is
  // nothing to discover.
  if (!allowed_projects.empty()) {
    return FilterAllowedProjects(allowed_projects, projects_filter,
                                 metadata_id);
  }

  std::vector<std::string> project_ids;
  std::unique_ptr<re2::RE2> filter_regex =
      BuildRegex(projects_filter, metadata_id);
  // For now, we use default options.
  // We can set timeout here as needed later.
  Options options;
  StatusRecordOr<std::vector<Project>> projects =
      bq_client.ListAllProjects(options);
  if (!projects) {
    LOG(ERROR) << "GetFilteredProjectIds::ListAllProjects:: "
               << projects.GetStatusRecord().message;
    return projects.GetStatusRecord();
  }
  for (auto const& project : *projects) {
    if ((!metadata_id && projects_filter == "%") ||
        re2::RE2::FullMatch(project.id, *filter_regex)) {
      project_ids.push_back(project.id);
    }
  }
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
    auto const& status = datasets.GetStatusRecord();
    // A catalog passed as a literal (no wildcard) is used directly without
    // first confirming it via projects.list, so an unknown or inaccessible
    // project surfaces here. Treat "not found" as an empty catalog rather than
    // failing the whole call: the pattern-based path simply would not have
    // matched that project. Mirrors GetFilteredTables' 404 handling.
    if (status.native_error_code == 404) {
      LOG(WARNING) << "GetFilteredDatasetIds:: Skipping project not found: '"
                   << project_id << "': " << status.message;
      return std::vector<std::string>{};
    }
    LOG(ERROR) << "GetFilteredDatasetIds::FilterDatasets:: " << status.message;
    return status;
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
    ODBCBQClient& bq_client, SQLULEN metadata_id,
    std::vector<std::string> base_projects,
    std::string const& additional_projects) {
  std::set<std::string> existing_ids(base_projects.begin(),
                                     base_projects.end());

  std::stringstream ss(additional_projects);
  std::string project_id;
  while (std::getline(ss, project_id, ',')) {
    // Trim leading whitespace (spaces, tabs, newlines)
    project_id.erase(0, project_id.find_first_not_of(" \t\n\r"));
    // Trim trailing whitespace (spaces, tabs, newlines)
    project_id.erase(project_id.find_last_not_of(" \t\n\r") + 1);

    if (project_id.empty() || existing_ids.count(project_id)) {
      continue;
    }
    auto validation_status =
        GetFilteredDatasetIds(bq_client, project_id, "%", metadata_id);

    if (validation_status.Ok()) {
      base_projects.push_back(project_id);
      existing_ids.insert(project_id);
    } else {
      auto const& status_record = validation_status.GetStatusRecord();

      if (status_record.native_error_code == 404) {
        LOG(INFO) << "Additional project '" << project_id
                  << "' from DSN is not found or inaccessible. Skipping.";
      } else {
        LOG(ERROR) << "Validation of additional project '" << project_id
                   << "' failed with code " << status_record.native_error_code
                   << ": " << status_record.message;
      }
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
    if (status.native_error_code == 404 || status.native_error_code == 403) {
      LOG(WARNING)
          << "GetFilteredTables:: Skipping inaccessible or missing dataset: '"
          << project_id << "." << dataset_id << "': " << status.message;
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
    std::string const& allowed_projects) {
  auto project_ids_status = GetFilteredProjectIds(
      bq_client, kMatchAll, metadata_id, allowed_projects);
  if (!project_ids_status) {
    LOG(ERROR) << "GetResultSetForProjects::GetFilteredProjectIds:: "
               << project_ids_status.GetStatusRecord().message;
    return project_ids_status.GetStatusRecord();
  }

  std::vector<std::string> project_list = *project_ids_status;
  if (!additional_projects.empty()) {
    project_list = AppendAdditionalProjectsIfMissing(
        bq_client, metadata_id, std::move(project_list), additional_projects);
  }

  return CreateResultSetForProjects(project_list);
}

StatusRecordOr<ResultSet> GetResultSetForDatasets(
    ODBCBQClient& bq_client, SQLULEN metadata_id,
    std::string const& catalog_name, std::string const& additional_projects,
    std::string const& allowed_projects) {
  auto project_ids_status = GetFilteredProjectIds(
      bq_client, catalog_name, metadata_id, allowed_projects);
  if (!project_ids_status) {
    LOG(ERROR) << "GetResultSetForDatasets::GetFilteredProjectIds:: "
               << project_ids_status.GetStatusRecord().message;
    return project_ids_status.GetStatusRecord();
  }

  std::vector<std::string> project_list = *project_ids_status;

  if (!additional_projects.empty()) {
    project_list = AppendAdditionalProjectsIfMissing(
        bq_client, metadata_id, std::move(project_list), additional_projects);
  }

  // Fetch datasets for each project in parallel. Previously this was a serial
  // loop issuing one FilterDatasets REST call per project, which dominated the
  // SQLTables(SQL_ALL_SCHEMAS) latency when many projects are accessible.
  using DatasetTaskResult = std::vector<std::string>;
  auto dataset_task =
      [&](std::string const& project_id) -> StatusRecordOr<DatasetTaskResult> {
    auto dataset_ids_or =
        GetFilteredDatasetIds(bq_client, project_id, kMatchAll, metadata_id);
    if (!dataset_ids_or) {
      auto const& status = dataset_ids_or.GetStatusRecord();
      if (status.native_error_code == 403 || status.native_error_code == 404) {
        LOG(WARNING)
            << "GetResultSetForDatasets:: Skipping inaccessible project: '"
            << project_id << "': " << status.message;
        return DatasetTaskResult{};
      }
      return status;
    }
    return dataset_ids_or;
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
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  // When the catalog argument is an exact project identifier -- either because
  // SQL_ATTR_METADATA_ID is true, or because the LIKE pattern contains no
  // wildcard -- skip enumerating every accessible project via projects.list and
  // use the name directly. projects.list has no server-side filter, so it
  // returns every project the caller can see just to select one by name.
  std::vector<std::string> project_list;
  if (auto project_literal =
          LiteralFromOdbcPattern(project_filter, metadata_id)) {
    project_list = {std::move(*project_literal)};
  } else {
    auto projects_status_record_or =
        GetFilteredProjectIds(bq_client, project_filter, metadata_id,
                              conn_handle.GetDsn().allowed_projects);
    if (!projects_status_record_or) {
      LOG(ERROR) << "GetResultSetForTables::GetFilteredProjectIds:: "
                 << projects_status_record_or.GetStatusRecord().message;
      return projects_status_record_or.GetStatusRecord();
    }
    project_list = *projects_status_record_or;
  }
  // Append additional projects if any
  project_list = AppendAdditionalProjectsIfMissing(
      bq_client, metadata_id, std::move(project_list),
      conn_handle.GetDsn().additional_projects);

  // 1. Prepare the list of tasks (Project + Dataset combinations)
  struct TaskInput {
    std::string project_id;
    std::string dataset_id;
  };
  std::vector<TaskInput> tasks;

  std::shared_ptr<TraceOptions> trace_option = TraceOptions::GetTraceOption();
  int max_threads = trace_option->max_threads;

  // The schema argument is an exact dataset identifier when
  // SQL_ATTR_METADATA_ID is true, or when the LIKE pattern contains no
  // wildcard. Computed once: it does not depend on the project.
  auto const dataset_literal =
      LiteralFromOdbcPattern(dataset_filter, metadata_id);

  if (dataset_literal) {
    // For an exact dataset identifier, skip listing every dataset in the
    // project via datasets.list and address the dataset directly.
    for (auto const& project_id : project_list) {
      tasks.push_back({project_id, *dataset_literal});
    }
  } else {
    // Enumerate datasets for each project in parallel. A serial loop here
    // issues one datasets.list REST call per project, which dominates
    // SQLTables latency when the catalog pattern matches many projects
    // (e.g. catalog = "%").
    auto dataset_task = [&](std::string const& project_id)
        -> StatusRecordOr<std::vector<TaskInput>> {
      auto datasets_status_record_or = GetFilteredDatasetIds(
          bq_client, project_id, dataset_filter, metadata_id);
      if (!datasets_status_record_or) {
        auto const& status = datasets_status_record_or.GetStatusRecord();
        if (status.native_error_code == 403 ||
            status.native_error_code == 404) {
          LOG(WARNING)
              << "GetResultSetForTables:: Skipping inaccessible project: '"
              << project_id << "': " << status.message;
          return std::vector<TaskInput>{};
        }
        LOG(ERROR) << "GetResultSetForTables::GetFilteredDatasetIds:: "
                   << status.message;
        return status;
      }
      std::vector<TaskInput> project_tasks;
      project_tasks.reserve(datasets_status_record_or->size());
      for (auto& dataset_id : *datasets_status_record_or) {
        project_tasks.push_back({project_id, std::move(dataset_id)});
      }
      return project_tasks;
    };
    auto dataset_results_or =
        ExecuteParallelTasks<std::string, std::vector<TaskInput>>(
            max_threads, project_list, dataset_task);
    if (!dataset_results_or) {
      return dataset_results_or.GetStatusRecord();
    }
    for (auto& project_tasks : *dataset_results_or) {
      tasks.insert(tasks.end(), std::make_move_iterator(project_tasks.begin()),
                   std::make_move_iterator(project_tasks.end()));
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
