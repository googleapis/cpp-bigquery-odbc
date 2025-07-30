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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_TABLES_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_TABLES_H

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

std::vector<std::string> const kAllTableTypes = {
    "TABLE", "VIEW", "EXTERNAL", "MATERIALIZED VIEW", "SNAPSHOT"};

std::string const kMatchAll = "%";

static std::map<std::string, ColumnSchema> const kSchema = {
    {"TABLE_CAT", ColumnSchema{0, BQDataType::kString}},
    {"TABLE_SCHEM", ColumnSchema{1, BQDataType::kString}},
    {"TABLE_NAME", ColumnSchema{2, BQDataType::kString}},
    {"TABLE_TYPE", ColumnSchema{3, BQDataType::kString}},
    {"REMARKS", ColumnSchema{4, BQDataType::kString}},
};

struct FilteredTableResponse {
  std::string table_name;
  std::string table_type;
};

struct TablesResult {
  std::string project_id;
  std::string dataset_id;
  std::string table_name;
  std::string table_type;
  std::string description;
};

// Validate SQLTables input arguments before making any REST request.
odbc_internal::StatusRecord ValidateInputParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* table_name, SQLSMALLINT table_name_len,
    SQLSMALLINT table_type_len, SQLULEN metadata_id);

// Return a list of project ids depending on SQL_ATTR_METADATA_ID and
// 'projects_filter'. Returns all project ids if SQL_ATTR_METADATA_ID ==
// SQL_FALSE and projects_filter == "%".
odbc_internal::StatusRecordOr<std::vector<std::string>> GetFilteredProjectIds(
    ODBCBQClient& bq_client, std::string const& projects_filter,
    SQLULEN metadata_id);

// Return a list of dataset ids depending on SQL_ATTR_METADATA_ID and
// 'datasets_filter'. Returns all dataset ids if SQL_ATTR_METADATA_ID ==
// SQL_FALSE and datasets_filter == "%".
odbc_internal::StatusRecordOr<std::vector<std::string>> GetFilteredDatasetIds(
    ODBCBQClient& bq_client, std::string const& project_id,
    std::string const& datasets_filter, SQLULEN metadata_id);

// Construct a query to INFORMATION_SCHEMA.TABLES table depending on input
// parameters. Populate 'named_query_params' with named parameters if needed.
odbc_internal::StatusRecordOr<std::string> ConstructQuery(
    std::string tables_filter, std::string const& table_types_filter,
    SQLULEN metadata_id,
    std::vector<::google::cloud::bigquery_v2_minimal_internal::QueryParameter>&
        named_query_params);

// Return a list of table names and table types depending on input parameters.
// Returns all tables if SQL_ATTR_METADATA_ID == SQL_FALSE and tables_filter ==
// "%" and table_types_filter == "%".
odbc_internal::StatusRecordOr<std::vector<FilteredTableResponse>>
GetFilteredTables(ConnectionHandle& conn_handle, std::string const& project_id,
                  std::string const& dataset_id,
                  std::string const& tables_filter,
                  std::string const& table_types_filter, SQLULEN metadata_id);

// Creates ResultSet populating input arguments for project ids and NULL for
// other values.
ResultSet CreateResultSetForProjects(
    std::vector<std::string> const& project_ids);

// Creates ResultSet populating input arguments for dataset ids and NULL for
// other values.
ResultSet CreateResultSetForDatasets(
    std::vector<std::string> const& dataset_ids);

// Creates ResultSet for all supported Table Types
ResultSet CreateResultSetForTableTypes();

// Creates ResultSet from input arguments assuming that the size of the vector
// is according to ODBC spec.
ResultSet ProcessStringResults(
    std::vector<std::vector<std::string>> const& rows);

// Search for all projects and populate ResultSet for it.
odbc_internal::StatusRecordOr<ResultSet> GetResultSetForProjects(
    ODBCBQClient& bq_client, SQLULEN metadata_id,
    std::string const& additional_projects = "");

// Search for all datasets in all projects and populate ResultSet for it.
odbc_internal::StatusRecordOr<ResultSet> GetResultSetForDatasets(
    ODBCBQClient& bq_client, SQLULEN metadata_id,
    std::string const& catalog_name = kMatchAll,
    std::string const& additional_projects = "");

// Search for tables and populate ResultSet according to ODBC spec
odbc_internal::StatusRecordOr<ResultSet> GetResultSetForTables(
    ConnectionHandle& conn_handle, ODBCBQClient& bq_client,
    std::string const& project_filter, std::string const& dataset_filter,
    std::string const& table_filter, std::string const& table_type_filter,
    SQLULEN metadata_id);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_TABLES_H
