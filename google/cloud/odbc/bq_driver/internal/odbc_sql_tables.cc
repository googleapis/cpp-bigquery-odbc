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

std::string const kProcedureNameParam = "procedure_name";
std::string const kSchemaParam = "schema_name";
std::string const kProcedureQuery =
    "SELECT procedure_name, schema_name FROM INFORMATION_SCHEMA.ROUTINES";

std::vector<ColumnSchema> const kSchema = {{0, BQDataType::kString},
                                           {1, BQDataType::kString},
                                           {2, BQDataType::kString},
                                           {3, BQDataType::kString},
                                           {4, BQDataType::kString}};
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
    return status_record;
  }
  // SQLTables specific validation.
  if (table_type_len < 0 && table_type_len != SQL_NTS) {
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
std::string ConstructProcedureNameWhereClause(
    std::string const& procedures_filter, SQLULEN metadata_id) {
  if (metadata_id == SQL_TRUE) {
    return "LOWER(routine_name) = LOWER(@" + kProcedureNameParam + ")";
  }
  if (procedures_filter != "%") {
    return "routine_name LIKE @" + kProcedureNameParam;
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

std::string ConstructProcedureTypeWhereClause(
    std::string procedure_types_filter) {
  Trim(procedure_types_filter);
  if (procedure_types_filter != "%") {
    return "procedure_type IN UNNEST(@{" + kSchemaParam + ")";
  }
  return "";
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

// StatusRecordOr<std::string> ConstructProcedureQuery(
//   std::string procedures_filter, std::string const& schema_filter,
//   SQLULEN metadata_id, std::vector<QueryParameter>& named_query_params) {

// if (metadata_id == SQL_TRUE) {
//   RTrim(procedures_filter);
// }

// std::string procedure_name_where_clause =
//     ConstructProcedureNameWhereClause(procedures_filter, metadata_id);
// std::string schema_where_clause =
// ConstructProcedureTypeWhereClause(schema_filter);

// if (!procedure_name_where_clause.empty()) {
//   auto query_param =
//       ConstructStringQueryParameter(kProcedureNameParam, procedures_filter);
//   if (!query_param) {
//     return query_param.GetStatusRecord();
//   }
//   named_query_params.push_back(*query_param);
// }

// if (!schema_where_clause.empty()) {
//   auto query_param =
//       ConstructStringQueryParameter(kSchemaParam, schema_filter);
//   if (!query_param) {
//     return query_param.GetStatusRecord();
//   }
//   named_query_params.push_back(*query_param);
// }

// if (!procedure_name_where_clause.empty() && !schema_where_clause.empty()) {
//   return kProcedureQuery + " WHERE " + procedure_name_where_clause + " AND "
//   +
//          schema_where_clause;
// }

// if (!procedure_name_where_clause.empty() || !schema_where_clause.empty()) {
//   return kProcedureQuery + " WHERE " + procedure_name_where_clause +
//          schema_where_clause;
// }

// return kProcedureQuery;
// }

StatusRecordOr<std::string> ConstructProcedureQuery(
    std::string const& procedures_filter,
    std::string const& procedure_types_filter, SQLULEN metadata_id,
    std::vector<QueryParameter>& named_query_params) {
  std::string query = R"(
  SELECT procedure_name, schema_name
  FROM INFORMATION_SCHEMA.ROUTINES
  WHERE routine_name LIKE @procedure_name
  AND procedure_type IN UNNEST(@procedure_types)
)";

  // Set parameters correctly
  named_query_params.push_back({"procedure_name", procedures_filter});
  named_query_params.push_back({"procedure_types", procedure_types_filter});

  return query;
}

StatusRecordOr<std::vector<FilteredTableResponse>> GetFilteredTables(
    ConnectionHandle& conn_handle, std::string const& project_id,
    std::string const& dataset_id, std::string const& tables_filter,
    std::string const& table_types_filter, SQLULEN metadata_id) {
  // Print input parameters
  std::cout << "GetFilteredTables called with:\n"
            << "  Project ID: " << project_id << "\n"
            << "  Dataset ID: " << dataset_id << "\n"
            << "  Tables Filter: " << tables_filter << "\n"
            << "  Table Types Filter: " << table_types_filter << "\n"
            << "  Metadata ID: " << metadata_id << "\n";

  std::vector<QueryParameter> named_query_params;
  auto query_tables = ConstructQuery(tables_filter, table_types_filter,
                                     metadata_id, named_query_params);

  if (!query_tables) {
    std::cout << "ConstructQuery failed: "
              << query_tables.GetStatusRecord().message << std::endl;
    return query_tables.GetStatusRecord();
  }

  std::cout << "ConstructQuery result: " << *query_tables << std::endl;

  auto post_query_request_status = ConstructNamedParametersPostQueryRequest(
      project_id, dataset_id, *query_tables, named_query_params);

  if (!post_query_request_status) {
    std::cout << "ConstructNamedParametersPostQueryRequest failed: "
              << post_query_request_status.GetStatusRecord().message
              << std::endl;
    return post_query_request_status.GetStatusRecord();
  }

  std::cout << "ConstructNamedParametersPostQueryRequest successful.\n";

  auto fetch_status_record_or =
      FetchBQData(conn_handle, *post_query_request_status);

  if (!fetch_status_record_or) {
    std::cout << "FetchBQData failed: "
              << fetch_status_record_or.GetStatusRecord().message << std::endl;
    return fetch_status_record_or.GetStatusRecord();
  }

  std::cout << "FetchBQData successful.\n";

  StatusRecordOr<std::vector<RowData>> rows =
      GetRowsResults(*fetch_status_record_or);

  if (!rows) {
    std::cout << "GetRowsResults failed: " << rows.GetStatusRecord().message
              << std::endl;
    return rows.GetStatusRecord();
  }

  std::cout << "GetRowsResults returned " << rows->size() << " rows.\n";

  std::vector<FilteredTableResponse> table_response;
  for (auto const& row : *rows) {
    std::cout << "Processing row: col0=" << row.columns[0].value
              << ", col1=" << row.columns[1].value << std::endl;
    table_response.push_back({row.columns[0].value, row.columns[1].value});
  }

  std::cout << "Final table response size: " << table_response.size()
            << std::endl;

  return table_response;
}

StatusRecordOr<std::vector<FilteredProcedureResponse>> GetFilteredProcedures(
    ConnectionHandle& conn_handle, std::string const& project_id,
    std::string const& dataset_id, std::string const& procedures_filter,
    std::string const& procedure_types_filter, SQLULEN metadata_id) {
  std::cout << "Entering GetFilteredProcedures function" << std::endl;
  std::cout << "Project ID: " << project_id << ", Dataset ID: " << dataset_id
            << std::endl;
  std::cout << "Procedures Filter: " << procedures_filter
            << ", Procedure Types Filter: " << procedure_types_filter
            << std::endl;
  std::cout << "Metadata ID: " << metadata_id << std::endl;

  std::vector<QueryParameter> named_query_params;
  QueryParameter param;
  param.name = "procedure_name";
  param.parameter_type.type = "STRING";  // Ensure correct type if required
  param.parameter_value.value = procedures_filter;  // Assign value

  named_query_params.push_back(param);

  std::string query = R"(
  SELECT routine_name, routine_schema
  FROM `)" + project_id +
                      "." + dataset_id + R"(.INFORMATION_SCHEMA.ROUTINES`
  WHERE routine_name LIKE @procedure_name
  AND routine_type = 'PROCEDURE'
)";

  std::cout << "Query constructed successfully: " << query << std::endl;

  // Debug: Print parameters
  for (auto const& param : named_query_params) {
    std::cout << "Query Parameter - Name: " << param.name
              << ", Type: " << param.parameter_type.type
              << ", Value: " << param.parameter_value.value << std::endl;
  }

  // Construct Post Query Request
  auto post_query_request_status = ConstructNamedParametersPostQueryRequest(
      project_id, dataset_id, query, named_query_params);

  if (!post_query_request_status) {
    std::cout << "ConstructNamedParametersPostQueryRequest failed" << std::endl;
    return post_query_request_status.GetStatusRecord();
  }

  std::cout << "Post Query Request constructed successfully" << std::endl;

  // Debug: Print Post Query Request details
  std::cout << "Post Query Request Details:" << std::endl;
  std::cout << "Project ID: " << post_query_request_status->project_id()
            << std::endl;
  std::cout << "Dataset ID: " << dataset_id << std::endl;
  std::cout << "Query: " << query << std::endl;

  for (auto const& param : named_query_params) {
    std::cout << "Query Param - Name: " << param.name
              << ", Value: " << param.parameter_value.value << std::endl;
  }

  // Ensure Connection Handle is Valid
  if (!conn_handle.IsConnected()) {
    std::cout << "Error: Connection to BigQuery is not established!"
              << std::endl;
    return StatusRecord{SQLStates::k_08S01(), "Connection lost"};
  }

  // Fetch Data
  std::cout << "Starting FetchBQData..." << std::endl;
  auto fetch_status_record_or =
      FetchBQData(conn_handle, *post_query_request_status);
  if (!fetch_status_record_or) {
    std::cout << "FetchBQData failed" << std::endl;
    return fetch_status_record_or.GetStatusRecord();
  }

  std::cout << "Fetched BQ Data successfully" << std::endl;

  StatusRecordOr<std::vector<RowData>> rows =
      GetRowsResults(*fetch_status_record_or);
  std::cout << "ROWS:" << rows.GetValue().data()->DebugString("") << std::endl;
  if (!rows) {
    return rows.GetStatusRecord();
  }
  std::vector<FilteredProcedureResponse> procedure_response;
  for (auto const& row : *rows) {
    procedure_response.push_back({row.columns[0].value, row.columns[1].value});
  }
  std::cout << "Returning filtered procedures. Total procedures: "
            << procedure_response.size() << std::endl;

  return procedure_response;
}

ResultSet CreateResultSetForProjects(
    std::vector<std::string> const& project_ids) {
  ResultSet result_set;
  result_set.row_schema = kSchema;
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
  result_set.row_schema = kSchema;
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
  result_set.row_schema = kSchema;
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
  result_set.row_schema = kSchema;
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

StatusRecordOr<ResultSet> GetResultSetForProjects(ODBCBQClient& bq_client,
                                                  SQLULEN metadata_id) {
  auto project_ids_status =
      GetFilteredProjectIds(bq_client, kMatchAll, metadata_id);
  if (!project_ids_status) {
    return project_ids_status.GetStatusRecord();
  }
  return CreateResultSetForProjects(*project_ids_status);
}

StatusRecordOr<ResultSet> GetResultSetForDatasets(
    ODBCBQClient& bq_client, SQLULEN metadata_id,
    std::string const& catalog_name) {
  auto project_ids_status =
      GetFilteredProjectIds(bq_client, catalog_name, metadata_id);
  if (!project_ids_status) {
    return project_ids_status.GetStatusRecord();
  }
  std::vector<std::string> dataset_ids;
  for (auto const& project_id : *project_ids_status) {
    auto dataset_ids_status =
        GetFilteredDatasetIds(bq_client, project_id, kMatchAll, metadata_id);
    if (!dataset_ids_status) {
      return dataset_ids_status.GetStatusRecord();
    }
    std::vector<std::string> ids = *dataset_ids_status;
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
    return projects_status_record_or.GetStatusRecord();
  }
  std::vector<std::string> project_ids = *projects_status_record_or;

  std::map<std::string, std::vector<std::string>> projects_datasets;
  for (auto const& project_id : project_ids) {
    auto datasets_status_record_or = GetFilteredDatasetIds(
        bq_client, project_id, dataset_filter, metadata_id);
    if (!datasets_status_record_or) {
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
