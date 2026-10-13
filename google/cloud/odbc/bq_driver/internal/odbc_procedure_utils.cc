// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/odbc/bq_driver/internal/odbc_procedure_utils.h"
#include "google/cloud/odbc/bq_client_interface/utils.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_columns_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/bigquery/v2/routine.pb.h"
#include "google/cloud/bigquery/v2/standard_sql.pb.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {
using ::google::cloud::bigquery::v2::Routine;
using ::google::cloud::bigquery::v2::StandardSqlDataType;
using google::cloud::odbc_bigquery_client_interface::MaxRetriesOption;
using ::google::cloud::odbc_bq_driver_internal::GetFixedColumnMetadata;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {

std::string ProcedureParameterToString(SQLCHAR const* value,
                                       SQLSMALLINT length) {
  if (value == nullptr) {
    return {};
  }

  auto const* text = reinterpret_cast<char const*>(value);
  if (length == SQL_NTS) {
    return std::string(text);
  }
  return std::string(text, static_cast<std::size_t>(length));
}

StatusRecordOr<std::string> GetRoutineArgumentTypeName(
    Routine::Argument const& argument) {
  auto const argument_kind =
      Routine::Argument::ArgumentKind_Name(argument.argument_kind());

  if (argument_kind == "ANY_TYPE") {
    return std::string("ANY TYPE");
  }

  if (argument_kind == "FIXED_TABLE" || argument_kind == "ANY_TABLE") {
    return std::string("TABLE");
  }

  if (argument_kind != "ARGUMENT_KIND_UNSPECIFIED" &&
      argument_kind != "FIXED_TYPE") {
    return StatusRecord{SQLStates::k_HY000(),
                        "Unsupported routine argument kind: " + argument_kind};
  }

  if (!argument.has_data_type()) {
    return StatusRecord{
        SQLStates::k_HY000(),
        "Missing data type for routine argument: " + argument.name()};
  }

  auto type_name =
      StandardSqlDataType::TypeKind_Name(argument.data_type().type_kind());

  auto sql_type = GetSQLDataType(type_name, false);
  if (!sql_type) {
    return sql_type.GetStatusRecord();
  }

  return type_name;
}

// Represent templated and table arguments using string metadata while
// preserving their declared kind in TYPE_NAME.
bool UsesStringRoutineArgumentMetadata(std::string const& type_name) {
  return type_name == "ANY TYPE" || type_name == "TABLE";
}

}  // namespace

/**
 * Validates the parameters for retrieving procedure column metadata.
 *
 * @param metadata_id - Indicates whether to use standard metadata retrieval.
 * @return StatusRecord indicating validation SUCCESS or FAILURE.
 */
StatusRecordOr<Procedure> ValidateProcedureColumnParameters(
    SQLCHAR const* catalog_name, SQLSMALLINT catalog_name_len,
    SQLCHAR const* schema_name, SQLSMALLINT schema_name_len,
    SQLCHAR const* procedure_name, SQLSMALLINT procedure_name_len,
    SQLULEN metadata_id) {
  if (catalog_name_len < 0 && catalog_name_len != SQL_NTS) {
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Invalid catalog length.";
    return StatusRecord{SQLStates::k_HY090(), "Invalid catalog length"};
  }
  if (schema_name_len < 0 && schema_name_len != SQL_NTS) {
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Invalid schema length.";
    return StatusRecord{SQLStates::k_HY090(), "Invalid schema length"};
  }
  if (procedure_name_len < 0 && procedure_name_len != SQL_NTS) {
    LOG(ERROR)
        << "ValidateProcedureColumnParameters:: Invalid procedure name length.";
    return StatusRecord{SQLStates::k_HY090(), "Invalid procedure name length"};
  }

  if (metadata_id == SQL_TRUE) {
    if (catalog_name == nullptr) {
      LOG(ERROR)
          << "ValidateProcedureColumnParameters:: Catalog name cannot be NULL.";
      return StatusRecord{SQLStates::k_HY009(), "Catalog name cannot be NULL"};
    }
    if (schema_name == nullptr) {
      LOG(ERROR)
          << "ValidateProcedureColumnParameters:: Schema name cannot be NULL.";
      return StatusRecord{SQLStates::k_HY009(), "Schema name cannot be NULL"};
    }
    if (procedure_name == nullptr) {
      LOG(ERROR) << "ValidateProcedureColumnParameters:: Procedure name cannot "
                    "be NULL.";
      return StatusRecord{SQLStates::k_HY009(),
                          "Procedure name cannot be NULL"};
    }
  }

  auto catalog = ProcedureParameterToString(catalog_name, catalog_name_len);
  auto dataset = ProcedureParameterToString(schema_name, schema_name_len);
  auto proc_name =
      ProcedureParameterToString(procedure_name, procedure_name_len);

  if (IsSearchPatternArgument(catalog)) {
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Catalog name cannot be "
                  "a search pattern.";
    return StatusRecord{SQLStates::k_HY090(),
                        "Catalog name cannot be a search pattern"};
  }

  if (catalog.empty()) {
    LOG(ERROR)
        << "ValidateProcedureColumnParameters:: Catalog cannot be empty.";
    return StatusRecord{SQLStates::k_HY000(), "Catalog cannot be empty"};
  }
  if (dataset.empty()) {
    LOG(ERROR)
        << "ValidateProcedureColumnParameters:: Dataset cannot be empty.";
    return StatusRecord{SQLStates::k_HY000(), "Dataset cannot be empty"};
  }
  if (proc_name.empty()) {
    LOG(ERROR) << "ValidateProcedureColumnParameters:: Procedure name cannot "
                  "be empty.";
    return StatusRecord{SQLStates::k_HY000(), "Procedure name cannot be empty"};
  }

  Procedure procedure;
  procedure.catalog = catalog;
  procedure.dataset = dataset;
  procedure.procedure_name = proc_name;

  return procedure;
}

std::string GetProcedureArgumentMode(
    ::google::cloud::bigquery::v2::Routine_Argument_Mode mode) {
  switch (mode) {
    case ::google::cloud::bigquery::v2::Routine_Argument_Mode_IN:
      return "IN";
    case ::google::cloud::bigquery::v2::Routine_Argument_Mode_OUT:
      return "OUT";
    case ::google::cloud::bigquery::v2::Routine_Argument_Mode_INOUT:
      return "INOUT";
    case ::google::cloud::bigquery::v2::Routine_Argument_Mode_MODE_UNSPECIFIED:
      return "IN";
    default:
      return "";
  }
}

StatusRecordOr<Procedure> FetchBQProcedureData(std::string const& catalog,
                                               std::string const& dataset,
                                               Routine const& routine) {
  Procedure procedure;
  procedure.catalog = catalog;
  procedure.dataset = dataset;
  procedure.procedure_name = routine.routine_reference().routine_id();

  int ordinal_position = 1;
  for (auto const& argument : routine.arguments()) {
    if (argument.name().empty()) {
      continue;
    }

    auto type_name = GetRoutineArgumentTypeName(argument);
    if (!type_name) {
      return type_name.GetStatusRecord();
    }

    procedure.schema.fields.emplace_back(
        ProcedureFieldSchema{catalog, dataset, procedure.procedure_name,
                             std::to_string(ordinal_position++),
                             GetProcedureArgumentMode(argument.mode()), "YES",
                             argument.name(), *type_name});
  }

  return procedure;
}

StatusRecordOr<Routine> FetchBQRoutineData(ConnectionHandle& conn_handle,
                                           std::string const& catalog,
                                           std::string const& dataset,
                                           std::string const& procedure_name) {
  if (!conn_handle.IsConnected()) {
    LOG(ERROR)
        << "FetchBQRoutineData:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR) << "FetchBQRoutineData:: Invalid or null BQ Client within the "
                  "connection handle.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  Options options;
  options.set<MaxRetriesOption>(conn_handle.GetDsn().max_retries);

  auto routine_status =
      bq_client->GetRoutine(catalog, dataset, procedure_name, options);

  if (!routine_status) {
    LOG(ERROR) << "FetchBQRoutineData:: GetRoutine failed: "
               << routine_status.GetStatusRecord().message;
    return routine_status.GetStatusRecord();
  }

  return *routine_status;
}

StatusRecordOr<Procedure> FetchBQProcedureData(ConnectionHandle& conn_handle,
                                               Procedure& in_proc) {
  auto routine_status = FetchBQRoutineData(
      conn_handle, in_proc.catalog, in_proc.dataset, in_proc.procedure_name);

  if (!routine_status) {
    return routine_status.GetStatusRecord();
  }

  return FetchBQProcedureData(in_proc.catalog, in_proc.dataset,
                              *routine_status);
}

StatusRecordOr<std::vector<FilteredProcedureResponse>> GetFilteredProcedures(
    ConnectionHandle& conn_handle, std::string const& project_id,
    std::string const& dataset_id, std::string const& procedure_pattern,
    SQLULEN metadata_id) {
  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR)
        << "GetFilteredProcedures:: Invalid or null BQ Client within the "
           "connection handle.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  Options options;
  options.set<MaxRetriesOption>(conn_handle.GetDsn().max_retries);

  auto routines = bq_client->ListRoutines(project_id, dataset_id, options);

  if (!routines) {
    LOG(ERROR) << "GetFilteredProcedures::ListRoutines:: "
               << routines.GetStatusRecord().message;
    return routines.GetStatusRecord();
  }

  auto pattern = BuildRegex(procedure_pattern, metadata_id);
  std::vector<FilteredProcedureResponse> procedure_response;

  for (auto const& routine : *routines) {
    std::string routine_type_name;

    if (routine.routine_type() == Routine::PROCEDURE) {
      routine_type_name = "PROCEDURE";
    } else if (routine.routine_type() == Routine::SCALAR_FUNCTION ||
               routine.routine_type() == Routine::TABLE_VALUED_FUNCTION) {
      routine_type_name = "FUNCTION";
    } else {
      continue;
    }

    auto const& routine_name = routine.routine_reference().routine_id();

    if (!re2::RE2::FullMatch(routine_name, *pattern)) {
      continue;
    }

    procedure_response.push_back({routine_name, routine_type_name, routine});
  }

  return procedure_response;
}

StatusRecordOr<DSRow> CreateSQLProceduresResultSetDSRow(
    SQLProcedures const& procedure) {
  DSRow ds_row;

  // PROCEDURE_CAT
  DSValue ds_procedure_cat = kNullValue;
  if (!procedure.procedure_catalog.empty()) {
    StringToDSValue(procedure.procedure_catalog, ds_procedure_cat);
  }
  ds_row.emplace_back(ds_procedure_cat);

  // PROCEDURE_SCHEMA
  DSValue ds_procedure_schema = kNullValue;
  if (!procedure.procedure_schema.empty()) {
    StringToDSValue(procedure.procedure_schema, ds_procedure_schema);
  }
  ds_row.emplace_back(ds_procedure_schema);

  // PROCEDURE_NAME
  DSValue ds_procedure_name = kNullValue;
  if (!procedure.procedure_name.empty()) {
    StringToDSValue(procedure.procedure_name, ds_procedure_name);
  }
  ds_row.emplace_back(ds_procedure_name);

  DSValue ds_num_input_params = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(procedure.num_input_params,
                                 ds_num_input_params);
  ds_row.emplace_back(ds_num_input_params);

  // NUM_OUTPUT_PARAMS
  DSValue ds_num_output_params = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(procedure.num_output_params,
                                 ds_num_output_params);
  ds_row.emplace_back(ds_num_output_params);

  // NUM_RESULT_SETS
  DSValue ds_num_result_set = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(-1, ds_num_result_set);
  ds_row.emplace_back(ds_num_result_set);

  // REMARKS
  DSValue ds_remarks = kNullValue;
  if (!procedure.remarks.empty()) {
    StringToDSValue(procedure.remarks, ds_remarks);
  }
  ds_row.emplace_back(ds_remarks);

  // PROCEDURE_TYPE (Assumed to be SQL_PT_PROCEDURE as default)
  DSValue ds_procedure_type = kNullValue;
  ArithmeticToDSValue<SQLBIGINT>(procedure.procedure_type, ds_procedure_type);
  ds_row.emplace_back(ds_procedure_type);

  return ds_row;
}

StatusRecordOr<SQLProcedures> FetchBQSQLProcedureData(
    std::string const& catalog, std::string const& dataset,
    Routine const& routine) {
  SQLProcedures procedure;

  procedure.procedure_catalog = catalog;
  procedure.procedure_schema = dataset;
  procedure.procedure_name = routine.routine_reference().routine_id();

  switch (routine.language()) {
    case ::google::cloud::bigquery::v2::Routine_Language_SQL:
      procedure.remarks = "SQL";
      break;

    case ::google::cloud::bigquery::v2::Routine_Language_JAVASCRIPT:
      procedure.remarks = "JAVASCRIPT";
      break;

    case ::google::cloud::bigquery::v2::Routine_Language_PYTHON:
      procedure.remarks = "PYTHON";
      break;

    case ::google::cloud::bigquery::v2::Routine_Language_JAVA:
      procedure.remarks = "JAVA";
      break;

    case ::google::cloud::bigquery::v2::Routine_Language_SCALA:
      procedure.remarks = "SCALA";
      break;

    default:
      procedure.remarks.clear();
      break;
  }

  switch (routine.routine_type()) {
    case ::google::cloud::bigquery::v2::Routine_RoutineType_PROCEDURE:
      procedure.procedure_type = SQL_PT_PROCEDURE;
      break;

    case ::google::cloud::bigquery::v2::Routine_RoutineType_SCALAR_FUNCTION:
      procedure.procedure_type = SQL_PT_FUNCTION;
      break;

    case ::google::cloud::bigquery::v2::
        Routine_RoutineType_TABLE_VALUED_FUNCTION:
    default:
      procedure.procedure_type = SQL_PT_UNKNOWN;
      break;
  }

  procedure.num_input_params = 0;
  procedure.num_output_params = 0;

  for (auto const& argument : routine.arguments()) {
    // BigQuery functions can have an unnamed return argument.
    if (argument.name().empty()) {
      continue;
    }

    if (argument.mode() == ::google::cloud::bigquery::v2::
                               Routine_Argument_Mode_MODE_UNSPECIFIED ||
        argument.mode() ==
            ::google::cloud::bigquery::v2::Routine_Argument_Mode_IN ||
        argument.mode() ==
            ::google::cloud::bigquery::v2::Routine_Argument_Mode_INOUT) {
      ++procedure.num_input_params;
    }

    if (argument.mode() ==
            ::google::cloud::bigquery::v2::Routine_Argument_Mode_OUT ||
        argument.mode() ==
            ::google::cloud::bigquery::v2::Routine_Argument_Mode_INOUT) {
      ++procedure.num_output_params;
    }
  }

  return procedure;
}

static std::map<std::string, ColumnSchema> const kCommonProcedureFields = {
    {"PROCEDURE_CAT", ColumnSchema{0, BQDataType::kString}},
    {"PROCEDURE_SCHEMA", ColumnSchema{1, BQDataType::kString}},
    {"PROCEDURE_NAME", ColumnSchema{2, BQDataType::kString}},
};

static std::map<std::string, ColumnSchema> const kODBCProceduresColumnsMap =
    [] {
      std::map<std::string, ColumnSchema> map = kCommonProcedureFields;
      map.insert({
          {"NUM_INPUT_PARAMS", ColumnSchema{3, BQDataType::kInt64}},
          {"NUM_OUTPUT_PARAMS", ColumnSchema{4, BQDataType::kInt64}},
          {"NUM_RESULT_COLS", ColumnSchema{5, BQDataType::kInt64}},
          {"REMARKS", ColumnSchema{6, BQDataType::kString}},
          {"PROCEDURE_TYPE", ColumnSchema{7, BQDataType::kInt64}},
      });
      return map;
    }();

StatusRecordOr<ColumnSchema> GetProcedureSchema(std::string const& proc_name) {
  auto map_item = kODBCProceduresColumnsMap.find(proc_name);
  if (map_item != kODBCProceduresColumnsMap.end()) {
    return map_item->second;
  }
  LOG(ERROR) << "GetProcedureSchema:: Invalid column name: " << proc_name;
  return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                     "Invalid column name: " + proc_name};
}

StatusRecord CreateSQLProcedureResultSetRowSchema(ResultSet& result_set) {
  for (auto const& entry : kODBCProceduresColumnsMap) {
    auto col_schema_status = GetProcedureSchema(entry.first);
    if (!col_schema_status) {
      return col_schema_status.GetStatusRecord();
    }
    result_set.row_schema.emplace_back(*col_schema_status);
  }
  return StatusRecord::Ok();
}

StatusRecordOr<ResultSet> ProcessProcedures(
    std::vector<SQLProcedures> const& bq_procedure) {
  ResultSet result_set;

  auto row_schema_status = CreateSQLProcedureResultSetRowSchema(result_set);
  if (!row_schema_status.ok()) {
    return row_schema_status;
  }

  result_set.rows.reserve(bq_procedure.size());

  for (auto const& procedure : bq_procedure) {
    auto ds_row_status = CreateSQLProceduresResultSetDSRow(procedure);
    if (!ds_row_status) {
      return ds_row_status.GetStatusRecord();
    }

    result_set.rows.emplace_back(*ds_row_status);
  }

  return result_set;
}

template <typename ProcedureType>
StatusRecordOr<std::vector<ProcedureType>> FetchProceduresData(
    StatementHandle& stmt_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id,
    std::function<
        StatusRecordOr<ProcedureType>(ConnectionHandle&, std::string const&,
                                      std::string const&, Routine const&)>
        fetch_procedure_fn) {
  std::vector<ProcedureType> result;
  ConnectionHandle& conn_handle = *(stmt_handle.GetConnectionHandle());
  if (!conn_handle.IsConnected()) {
    LOG(ERROR)
        << "FetchProceduresData:: Connection to the data source is broken.";
    return StatusRecord{SQLStates::k_08S01(),
                        "Connection to the data source is broken"};
  }

  auto bq_client = conn_handle.GetClient();
  if (!bq_client) {
    LOG(ERROR) << "FetchProceduresData:: Invalid or null BQ Client within the "
                  "connection handle.";
    return StatusRecord{
        SQLStates::k_HY000(),
        "Invalid or null BQ Client within the connection handle"};
  }

  StatusRecordOr<std::vector<std::string>> datasets_status =
      GetFilteredDatasetIds(*bq_client, catalog, dataset_pattern, metadata_id);
  if (!datasets_status) {
    auto const& status = datasets_status.GetStatusRecord();
    if (status.native_error_code == 403 || status.native_error_code == 404) {
      LOG(WARNING) << "FetchProceduresData:: Skipping inaccessible project: '"
                   << catalog << "': " << status.message;
      return result;
    }
    return status;
  }

  for (auto const& dataset : *datasets_status) {
    StatusRecordOr<std::vector<FilteredProcedureResponse>> procedure_status =
        GetFilteredProcedures(conn_handle, catalog, dataset, procedure_pattern,
                              metadata_id);
    if (!procedure_status) {
      auto const& status = procedure_status.GetStatusRecord();
      if (status.native_error_code == 403 || status.native_error_code == 404) {
        LOG(WARNING) << "FetchProceduresData:: Skipping inaccessible dataset: '"
                     << dataset << "': " << status.message;
        continue;
      }
      return status;
    }

    for (auto const& filtered_proc : *procedure_status) {
      StatusRecordOr<ProcedureType> procedure = fetch_procedure_fn(
          conn_handle, catalog, dataset, filtered_proc.routine);
      if (!procedure) {
        return procedure.GetStatusRecord();
      }
      result.emplace_back(*procedure);
    }
  }

  return result;
}

StatusRecordOr<std::vector<SQLProcedures>> FetchBQSQLProceduresData(
    StatementHandle& stmt_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id) {
  return FetchProceduresData<SQLProcedures>(
      stmt_handle, catalog, dataset_pattern, procedure_pattern, metadata_id,
      [](ConnectionHandle& conn_handle, std::string const& cat,
         std::string const& ds, Routine const& routine) {
        auto detailed_routine = FetchBQRoutineData(
            conn_handle, cat, ds, routine.routine_reference().routine_id());

        if (!detailed_routine) {
          return StatusRecordOr<SQLProcedures>(
              detailed_routine.GetStatusRecord());
        }

        return FetchBQSQLProcedureData(cat, ds, *detailed_routine);
      });
}

StatusRecordOr<std::vector<Procedure>> FetchBQProceduresData(
    StatementHandle& stmt_handle, std::string const& catalog,
    std::string const& dataset_pattern, std::string const& procedure_pattern,
    SQLULEN metadata_id) {
  return FetchProceduresData<Procedure>(
      stmt_handle, catalog, dataset_pattern, procedure_pattern, metadata_id,
      [](ConnectionHandle& conn_handle, std::string const& cat,
         std::string const& ds, Routine const& routine) {
        Procedure procedure;
        procedure.catalog = cat;
        procedure.dataset = ds;
        procedure.procedure_name = routine.routine_reference().routine_id();

        return FetchBQProcedureData(conn_handle, procedure);
      });
}

StatusRecordOr<DSRow> CreateProcedureColumnResultSetDSRow(
    ProcedureFieldSchema const& proc_column) {
  DSRow ds_row;

  bool const uses_string_metadata =
      UsesStringRoutineArgumentMetadata(proc_column.type_name);
  std::string const metadata_type =
      uses_string_metadata ? "STRING" : proc_column.type_name;

  // PROCEDURE_CAT
  DSValue ds_procedure_cat = kNullValue;
  if (!proc_column.catalog.empty()) {
    StringToDSValue(proc_column.catalog, ds_procedure_cat);
  }
  ds_row.emplace_back(ds_procedure_cat);

  // PROCEDURE_SCHEMA
  DSValue ds_procedure_schema = kNullValue;
  if (!proc_column.dataset.empty()) {
    StringToDSValue(proc_column.dataset, ds_procedure_schema);
  }
  ds_row.emplace_back(ds_procedure_schema);

  // PROCEDURE_NAME
  DSValue ds_procedure_name = kNullValue;
  if (!proc_column.procedure.empty()) {
    StringToDSValue(proc_column.procedure, ds_procedure_name);
  }
  ds_row.emplace_back(ds_procedure_name);

  // COLUMN_NAME
  DSValue ds_column_name = kNullValue;
  if (!proc_column.name.empty()) {
    StringToDSValue(proc_column.name, ds_column_name);
  }
  ds_row.emplace_back(ds_column_name);

  // COLUMN_TYPE
  DSValue ds_column_type;
  std::string value;
  if (proc_column.column_type == "OUT") {
    value = "4";
  } else if (proc_column.column_type == "INOUT") {
    value = "2";
  } else {
    value = "1";
  }
  SQLBIGINT bigint_value = std::stoll(value);
  ArithmeticToDSValue<SQLBIGINT>(bigint_value, ds_column_type);
  ds_row.emplace_back(ds_column_type);

  // DATA_TYPE
  DSValue ds_data_type = kNullValue;
  auto data_type_status = GetSQLDataType(metadata_type, false);
  if (!data_type_status) {
    return data_type_status.GetStatusRecord();
  }
  optional<SQLSMALLINT> data_type = data_type_status.GetValue();
  if (data_type.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*data_type),
                                   ds_data_type);
  }
  ds_row.emplace_back(ds_data_type);

  // TYPE_NAME
  DSValue ds_type_name = kNullValue;
  std::string type_name;
  if (uses_string_metadata) {
    type_name = proc_column.type_name;
  } else {
    auto type_status = GetTypeDescription(proc_column.type_name);
    if (!type_status) {
      return type_status.GetStatusRecord();
    }
    type_name = *type_status;
  }
  if (!type_name.empty()) {
    StringToDSValue(type_name, ds_type_name);
  }
  ds_row.emplace_back(ds_type_name);

  auto fixed_col_status = GetFixedColumnMetadata(metadata_type);
  if (!fixed_col_status.Ok()) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Failed to retrieve fixed column metadata"};
  }
  FixedColumnMetadata fixed_column_metadata = *fixed_col_status;

  // COLUMN_SIZE
  DSValue ds_col_size = kNullValue;
  if (fixed_column_metadata.precision.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.precision.value_or(0)),
        ds_col_size);
  }
  ds_row.emplace_back(ds_col_size);

  // BUFFER_LENGTH
  DSValue ds_buf_len = kNullValue;
  if (fixed_column_metadata.buf_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.buf_len.value_or(0)),
        ds_buf_len);
  }
  ds_row.emplace_back(ds_buf_len);

  // DECIMAL_DIGITS
  DSValue ds_dec_digits = kNullValue;
  if (fixed_column_metadata.scale.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.scale.value_or(0)),
        ds_dec_digits);
  }
  ds_row.emplace_back(ds_dec_digits);

  // NUM_PREC_RADIX
  DSValue ds_radix = kNullValue;
  if (fixed_column_metadata.radix.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(fixed_column_metadata.radix.value_or(0)),
        ds_radix);
  }
  ds_row.emplace_back(ds_radix);

  // NULLABLE
  DSValue ds_nullable;
  if (proc_column.nullable == "YES") {
    ArithmeticToDSValue<SQLBIGINT>(1, ds_nullable);
  } else {
    ArithmeticToDSValue<SQLBIGINT>(0, ds_nullable);
  }
  ds_row.emplace_back(ds_nullable);

  // REMARKS
  DSValue ds_remarks = kNullValue;
  ds_row.emplace_back(ds_remarks);

  // COLUMN_DEF
  DSValue column_def = kNullValue;
  ds_row.emplace_back(column_def);

  // SQL_DATA_TYPE
  DSValue ds_sql_data_type = kNullValue;
  optional<SQLSMALLINT> sql_data_type;
  if (data_type.has_value()) {
    auto sql_data_type_status = GetSQLDataType(*data_type);
    if (!sql_data_type_status) {
      return sql_data_type_status.GetStatusRecord();
    }
    sql_data_type = *sql_data_type_status;
    if (sql_data_type.has_value()) {
      ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_data_type),
                                     ds_sql_data_type);
    }
  }
  ds_row.emplace_back(ds_sql_data_type);

  // SQL_DATETIME_SUB
  DSValue ds_sql_datetime_sub = kNullValue;
  if (sql_data_type.has_value() && data_type.has_value()) {
    auto sql_data_time_sub_status =
        GetSQLDateTimeSub(*sql_data_type, *data_type);
    if (!sql_data_time_sub_status) {
      return sql_data_time_sub_status.GetStatusRecord();
    }
    optional<SQLSMALLINT> sql_date_time_sub = *sql_data_time_sub_status;
    if (sql_date_time_sub.has_value()) {
      ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(*sql_date_time_sub),
                                     ds_sql_datetime_sub);
    }
  }
  ds_row.emplace_back(ds_sql_datetime_sub);

  // CHAR_OCTET_LENGTH
  DSValue ds_char_octet_len = kNullValue;
  if (proc_column.column_type == "OUT") {
    ArithmeticToDSValue<SQLBIGINT>(static_cast<SQLBIGINT>(16384),
                                   ds_char_octet_len);
  } else if (fixed_column_metadata.char_octet_len.has_value()) {
    ArithmeticToDSValue<SQLBIGINT>(
        static_cast<SQLBIGINT>(
            fixed_column_metadata.char_octet_len.value_or(0)),
        ds_char_octet_len);
  }
  ds_row.emplace_back(ds_char_octet_len);

  // ORDINAL_POSITION
  DSValue ds_ord_pos = kNullValue;
  if (std::stoll(proc_column.ordinal_number) < 0) {
    return StatusRecord{SQLStates::k_HY000(), "Invalid ordinal position"};
  }
  ArithmeticToDSValue<SQLBIGINT>(std::stoll(proc_column.ordinal_number),
                                 ds_ord_pos);
  ds_row.emplace_back(ds_ord_pos);

  // IS_NULLABLE
  DSValue ds_is_nullable;
  std::string is_nullable = proc_column.nullable;
  StringToDSValue(is_nullable, ds_is_nullable);
  ds_row.emplace_back(ds_is_nullable);

  return ds_row;
}

static std::map<std::string, ColumnSchema> const kODBCProcedureColumnsMap = {
    {"PROCEDURE_CAT",
     ColumnSchema{0, BQDataType::kString}},  // Procedure catalog
    {"PROCEDURE_SCHEMA",
     ColumnSchema{1, BQDataType::kString}},  // Procedure schema
    {"PROCEDURE_NAME", ColumnSchema{2, BQDataType::kString}},  // Procedure name
    {"COLUMN_NAME", ColumnSchema{3, BQDataType::kString}},     // Column name
    {"COLUMN_TYPE",
     ColumnSchema{4, BQDataType::kInt64}},  // Column type (input, output, etc.)
    {"DATA_TYPE", ColumnSchema{5, BQDataType::kInt64}},   // SQL data type
    {"TYPE_NAME", ColumnSchema{6, BQDataType::kString}},  // Type name
    {"COLUMN_SIZE",
     ColumnSchema{7, BQDataType::kInt64}},  // Column size (precision)
    {"BUFFER_LENGTH", ColumnSchema{8, BQDataType::kInt64}},   // Buffer length
    {"DECIMAL_DIGITS", ColumnSchema{9, BQDataType::kInt64}},  // Decimal digits
    {"NUM_PREC_RADIX",
     ColumnSchema{10, BQDataType::kInt64}},  // Numeric precision radix
    {"NULLABLE", ColumnSchema{11, BQDataType::kInt64}},  // Nullable flag
    {"REMARKS", ColumnSchema{12, BQDataType::kString}},  // Remarks/comments
    {"COLUMN_DEF",
     ColumnSchema{13, BQDataType::kString}},  // Column default value
    {"SQL_DATA_TYPE",
     ColumnSchema{14,
                  BQDataType::kInt64}},  // SQL data type (same as DATA_TYPE)
    {"SQL_DATETIME_SUB",
     ColumnSchema{15, BQDataType::kInt64}},  // Date/time subtype
    {"CHAR_OCTET_LENGTH",
     ColumnSchema{16, BQDataType::kInt64}},  // Character octet length
    {"ORDINAL_POSITION",
     ColumnSchema{17, BQDataType::kInt64}},  // Ordinal position
    {"IS_NULLABLE",
     ColumnSchema{18, BQDataType::kString}}  // "YES", "NO", or "UNKNOWN"
};

StatusRecordOr<ColumnSchema> GetProcedureColumnSchema(
    std::string const& col_name) {
  auto map_item = kODBCProcedureColumnsMap.find(col_name);
  if (map_item != kODBCProcedureColumnsMap.end()) {
    return map_item->second;
  }
  LOG(ERROR) << "GetProcedureColumnSchema:: Invalid column name: " << col_name;
  return odbc_internal::StatusRecord{odbc_internal::SQLStates::k_HY000(),
                                     "Invalid column name: " + col_name};
}

StatusRecord CreateProcedureColumnResultSetRowSchema(ResultSet& result_set) {
  for (auto const& entry : kODBCProcedureColumnsMap) {
    auto col_schema_status = GetProcedureColumnSchema(entry.first);
    if (!col_schema_status) {
      return col_schema_status.GetStatusRecord();
    }
    result_set.row_schema.emplace_back(*col_schema_status);
  }
  return StatusRecord::Ok();
}

StatusRecordOr<ResultSet> ProcessProcedureColumnResults(
    Procedure const& bq_procedure, std::string const& bq_procedure_column,
    SQLULEN metadata_id) {
  ResultSet result_set;

  auto row_schema_status = CreateProcedureColumnResultSetRowSchema(result_set);
  if (!row_schema_status.ok()) {
    return row_schema_status;
  }

  if (!metadata_id &&
      (bq_procedure_column.empty() || bq_procedure_column == "%")) {
    for (auto const& procedure_field : bq_procedure.schema.fields) {
      auto ds_row_status = CreateProcedureColumnResultSetDSRow(procedure_field);
      if (!ds_row_status) {
        return ds_row_status.GetStatusRecord();
      }
      result_set.rows.emplace_back(*ds_row_status);
    }
  } else {
    auto column_pattern = BuildRegex(bq_procedure_column, metadata_id);

    for (auto const& procedure_field : bq_procedure.schema.fields) {
      if (re2::RE2::FullMatch(procedure_field.name, *column_pattern)) {
        auto ds_row_status =
            CreateProcedureColumnResultSetDSRow(procedure_field);
        if (!ds_row_status) {
          return ds_row_status.GetStatusRecord();
        }

        result_set.rows.emplace_back(*ds_row_status);
      }
    }
  }

  return result_set;
}

}  // namespace google::cloud::odbc_bq_driver_internal
