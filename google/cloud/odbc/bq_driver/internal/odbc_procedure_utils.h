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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_PROCEDURE_UTILS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_PROCEDURE_UTILS_H

#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

struct SQLProcedureColumn {
  std::string procedure_catalog;
  std::string procedure_schema;
  std::string procedure_name;
  std::string column_name;
  SQLSMALLINT column_type;  // IN, OUT, INOUT, or NULL
  SQLSMALLINT data_type;
  std::string type_name;
  SQLINTEGER column_size;
  SQLINTEGER buffer_length;
  SQLSMALLINT decimal_digits;
  SQLSMALLINT num_prec_radix;
  SQLSMALLINT nullable;
  std::string remarks;
  std::string column_def;
  SQLSMALLINT sql_data_type;
  SQLSMALLINT sql_datetime_sub;
  SQLINTEGER char_octet_length;
  SQLINTEGER ordinal_position;
  std::string is_nullable;
};

struct ProcedureFieldSchema {
  std::string catalog;
  std::string dataset;
  std::string procedure;
  std::string ordinal_number;
  std::string column_type;
  std::string nullable;
  std::string name;
  std::string type_name;
};

struct ProcedureSchema {
  std::vector<ProcedureFieldSchema> fields;
};

struct FilteredProcedureResponse {
  std::string proc_name;
  std::string proc_type;
};

struct Procedure {
  std::string catalog;
  std::string dataset;
  std::string procedure_name;
  ProcedureSchema schema;
};

struct SQLProcedures {
  std::string procedure_catalog;
  std::string procedure_schema;
  std::string procedure_name;
  SQLSMALLINT num_input_params;
  SQLSMALLINT num_output_params;
  SQLSMALLINT num_result_sets;
  std::string remarks;
  SQLSMALLINT procedure_type;
};

odbc_internal::StatusRecordOr<Procedure> ValidateProcedureColumnParameters(
    const SQLCHAR* catalog_name, SQLSMALLINT catalog_name_len,
    const SQLCHAR* schema_name, SQLSMALLINT schema_name_len,
    const SQLCHAR* procedure_name, SQLSMALLINT procedure_name_len,
    SQLULEN metadata_id);

odbc_internal::StatusRecordOr<std::vector<Procedure>> FetchBQProceduresData(
    ConnectionHandle& conn_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id);

odbc_internal::StatusRecordOr<ResultSet> ProcessProcedures(
    std::vector<SQLProcedures> const& bq_procedure);

odbc_internal::StatusRecordOr<std::vector<SQLProcedures>>
FetchBQSQLProceduresData(ConnectionHandle& conn_handle,
                         std::string const& catalog,
                         std::string const& dataset_pattern,
                         std::string const& procedure_pattern,
                         SQLULEN metadata_id);

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_PROCEDURE_UTILS_H
