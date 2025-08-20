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
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include <regex>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::ListFormatDataset;
using ::google::cloud::bigquery_v2_minimal_internal::Project;
using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using ::google::cloud::bigquery_v2_minimal_internal::RowData;
using google::cloud::odbc_bigquery_client_interface::DatasetFilter;
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
  std::vector<std::string> project_ids;
  std::regex filter_regex = BuildRegex(projects_filter, metadata_id);
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
        std::regex_match(project.id, filter_regex)) {
      project_ids.push_back(project.id);
    }
  }
  return project_ids;
}

StatusRecordOr<std::vector<std::string>> GetFilteredDatasetIds(
    ODBCBQClient& bq_client, std::string const& project_id,
    std::string const& datasets_filter, SQLULEN metadata_id) {
  std::vector<std::string> dataset_ids;
  std::regex filter_regex = BuildRegex(datasets_filter, metadata_id);
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
        std::regex_match(dataset.dataset_reference.dataset_id, filter_regex)) {
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
    ConnectionHandle& conn_handle, std::string const& project_id,
    std::string const& dataset_id, std::string const& tables_filter,
    std::string const& table_types_filter, SQLULEN metadata_id) {
  std::vector<QueryParameter> named_query_params;
  // Normalize table type: client-library accepts type "BASE TABLE"
  std::string normalized_table_type_filter =
      ProcessTableTypes(table_types_filter);
  auto query_tables =
      ConstructQuery(tables_filter, normalized_table_type_filter, metadata_id,
                     named_query_params);
  if (!query_tables) {
    LOG(ERROR) << "GetFilteredTables::ConstructQuery:: "
               << query_tables.GetStatusRecord().message;
    return query_tables.GetStatusRecord();
  }

  auto post_query_request_status = ConstructNamedParametersPostQueryRequest(
      project_id, dataset_id, *query_tables, named_query_params);
  if (!post_query_request_status) {
    LOG(ERROR)
        << "GetFilteredTables::ConstructNamedParametersPostQueryRequest:: "
        << post_query_request_status.GetStatusRecord().message;
    return post_query_request_status.GetStatusRecord();
  }

  auto fetch_status_record_or =
      FetchBQData(conn_handle, *post_query_request_status);
  if (!fetch_status_record_or) {
    LOG(ERROR) << "GetFilteredTables::FetchBQData:: "
               << fetch_status_record_or.GetStatusRecord().message;
    return fetch_status_record_or.GetStatusRecord();
  }

  StatusRecordOr<std::vector<RowData>> rows =
      GetRowsResults(*fetch_status_record_or);
  if (!rows) {
    LOG(ERROR) << "GetFilteredTables::GetRowsResults:: "
               << rows.GetStatusRecord().message;
    return rows.GetStatusRecord();
  }

  std::vector<FilteredTableResponse> table_response;
  for (auto const& row : *rows) {
    // Normalize table type: third-party tool accepts type "TABLE"
    std::string table_type =
        (row.columns[1].value == kBaseTable || row.columns[1].value == kClone)
            ? kTable
            : row.columns[1].value;
    table_response.push_back({row.columns[0].value, table_type});
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
    std::string const& additional_projects) {
  auto project_ids_status =
      GetFilteredProjectIds(bq_client, kMatchAll, metadata_id);
  if (!project_ids_status) {
    LOG(ERROR) << "GetResultSetForProjects::GetFilteredProjectIds:: "
               << project_ids_status.GetStatusRecord().message;
    return project_ids_status.GetStatusRecord();
  }

  std::vector<std::string> project_list = *project_ids_status;
  if (!additional_projects.empty()) {
    project_list = AppendAdditionalProjectsIfMissing(std::move(project_list),
                                                     additional_projects);
  }

  return CreateResultSetForProjects(project_list);
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

  std::vector<std::string> dataset_ids;
  for (auto const& project_id : project_list) {
    auto dataset_ids_status =
        GetFilteredDatasetIds(bq_client, project_id, kMatchAll, metadata_id);
    if (!dataset_ids_status) {
      LOG(ERROR) << "GetResultSetForDatasets::GetFilteredDatasetIds:: "
                 << dataset_ids_status.GetStatusRecord().message;
      return dataset_ids_status.GetStatusRecord();
    }

    std::vector<std::string> const& ids = *dataset_ids_status;
    dataset_ids.insert(dataset_ids.end(), ids.begin(), ids.end());
  }

  return CreateResultSetForDatasets(dataset_ids);
}

StatusRecordOr<ResultSet> GetResultSetForTables(
    ConnectionHandle& conn_handle, ODBCBQClient& bq_client,
    std::string const& project_filter, std::string const& dataset_filter,
    std::string const& table_filter, std::string const& table_type_filter,
    SQLULEN metadata_id) {
  auto projects_status_record_or =
      GetFilteredProjectIds(bq_client, project_filter, metadata_id);
  if (!projects_status_record_or) {
    LOG(ERROR) << "GetResultSetForTables::GetFilteredProjectIds:: "
               << projects_status_record_or.GetStatusRecord().message;
    return projects_status_record_or.GetStatusRecord();
  }
  // Extract the list of project IDs (as strings)
  std::vector<std::string> project_list = *projects_status_record_or;

  // Append additional projects if any
  project_list = AppendAdditionalProjectsIfMissing(
      std::move(project_list), conn_handle.GetDsn().additional_projects);

  // Final list of project IDs
  std::vector<std::string> project_ids = std::move(project_list);

  std::map<std::string, std::vector<std::string>> projects_datasets;
  for (auto const& project_id : project_ids) {
    auto datasets_status_record_or = GetFilteredDatasetIds(
        bq_client, project_id, dataset_filter, metadata_id);
    if (!datasets_status_record_or) {
      LOG(ERROR) << "GetResultSetForTables::GetFilteredDatasetIds:: "
                 << datasets_status_record_or.GetStatusRecord().message;
      return datasets_status_record_or.GetStatusRecord();
    }
    projects_datasets.emplace(project_id, *datasets_status_record_or);
  }

  std::vector<std::vector<std::string>> tables_result_set;
  for (auto const& [project_id, datasets] : projects_datasets) {
    for (auto const& dataset_id : datasets) {
      auto tables_status_record_or =
          GetFilteredTables(conn_handle, project_id, dataset_id, table_filter,
                            table_type_filter, metadata_id);
      if (!tables_status_record_or) {
        LOG(ERROR) << "GetResultSetForTables::GetFilteredTables:: "
                   << tables_status_record_or.GetStatusRecord().message;
        return tables_status_record_or.GetStatusRecord();
      }
      for (auto const& table : *tables_status_record_or) {
        tables_result_set.push_back({project_id, dataset_id, table.table_name,
                                     table.table_type, project_id});
      }
    }
  }
  return ProcessStringResults(tables_result_set);
}

}  // namespace google::cloud::odbc_bq_driver_internal
