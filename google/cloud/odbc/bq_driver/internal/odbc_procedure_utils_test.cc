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

#include "google/cloud/odbc/bq_driver/internal/odbc_procedure_utils.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/testing/bq_driver_utils/status_utils.h"
#include "google/cloud/odbc/testing/bq_driver_utils/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include "google/cloud/bigquery/v2/routine.pb.h"
#include "google/cloud/bigquery/v2/standard_sql.pb.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {
using ::google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_testing_bq_driver_utils::CastToSQLCHAR;
using ::google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using ::testing::HasSubstr;

TEST(ValidateProcedureColumnParameters, SuccessMetadataidTrue) {
  auto status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, SQL_TRUE);
  EXPECT_TRUE(status.Ok());
  EXPECT_EQ(status.GetValue().catalog, "project");
  EXPECT_EQ(status.GetValue().dataset, "dataset");
  EXPECT_EQ(status.GetValue().procedure_name, "Procedure");
}

TEST(ValidateProcedureColumnParameters, SuccessMetadataidFalse) {
  auto status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, SQL_FALSE);
  EXPECT_TRUE(status.Ok());
  EXPECT_EQ(status.GetValue().catalog, "project");
  EXPECT_EQ(status.GetValue().dataset, "dataset");
  EXPECT_EQ(status.GetValue().procedure_name, "Procedure");
}

TEST(ValidateProcedureColumnParameters, FailureEmptycatalog) {
  auto status = ValidateProcedureColumnParameters(
      CastToSQLCHAR(""), 0, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, SQL_FALSE);
  EXPECT_EQ(SQLStates::k_HY000(), status.GetStatusRecord().sql_state);
  EXPECT_THAT(status.GetStatusRecord().message,
              HasSubstr("Catalog cannot be empty"));
}

TEST(ValidateProcedureColumnParameters,
     FailureCatalognameissearchpatternMetadataidTrue) {
  auto status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project%"), 8, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, SQL_TRUE);
  EXPECT_EQ(SQLStates::k_HY090(), status.GetStatusRecord().sql_state);
  EXPECT_THAT(status.GetStatusRecord().message,
              HasSubstr("Catalog name cannot be a search pattern"));
}

TEST(ValidateProcedureColumnParameters,
     FailureCatalognameissearchpatternMetadataidFalse) {
  auto status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project%"), 8, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, SQL_FALSE);
  EXPECT_EQ(SQLStates::k_HY090(), status.GetStatusRecord().sql_state);
  EXPECT_THAT(status.GetStatusRecord().message,
              HasSubstr("Catalog name cannot be a search pattern"));
}

TEST(FetchBQProceduresData, ConnectionNotEstablishedReturnsError) {
  auto conn_handle = CreateConnectionHandle(false);
  StatementHandle handle(&conn_handle);
  auto result =
      FetchBQProceduresData(handle, "catalog", "dataset", "procedure", 0);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_08S01());
  EXPECT_EQ(result.GetStatusRecord().message,
            "Connection to the data source is broken");
}

TEST(FetchBQProceduresData, NullClientReturnsError) {
  auto conn_handle = CreateConnectionHandle(true);
  conn_handle.GetClient();
  StatementHandle handle(&conn_handle);
  auto result =
      FetchBQProceduresData(handle, "catalog", "dataset", "procedure", 0);
  ASSERT_FALSE(result);
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(result.GetStatusRecord().message,
            "Invalid or null BQ Client within the connection handle");
}

TEST(GetProcedureArgumentModeTest, ReturnsExpectedMode) {
  EXPECT_EQ(GetProcedureArgumentMode(
                ::google::cloud::bigquery::v2::Routine_Argument_Mode_IN),
            "IN");

  EXPECT_EQ(GetProcedureArgumentMode(
                ::google::cloud::bigquery::v2::Routine_Argument_Mode_OUT),
            "OUT");

  EXPECT_EQ(GetProcedureArgumentMode(
                ::google::cloud::bigquery::v2::Routine_Argument_Mode_INOUT),
            "INOUT");

  EXPECT_EQ(
      GetProcedureArgumentMode(::google::cloud::bigquery::v2::
                                   Routine_Argument_Mode_MODE_UNSPECIFIED),
      "IN");
}

TEST(FetchBQProcedureDataTest, ConvertsRoutineArguments) {
  using ::google::cloud::bigquery::v2::Routine;
  using ::google::cloud::bigquery::v2::StandardSqlDataType;

  Routine routine;
  routine.mutable_routine_reference()->set_routine_id("TestProcedure");

  auto* input = routine.add_arguments();
  input->set_name("input_value");
  input->set_mode(Routine::Argument::IN);
  input->mutable_data_type()->set_type_kind(StandardSqlDataType::INT64);

  auto* output = routine.add_arguments();
  output->set_name("output_value");
  output->set_mode(Routine::Argument::OUT);
  output->mutable_data_type()->set_type_kind(StandardSqlDataType::STRING);

  auto* inout = routine.add_arguments();
  inout->set_name("inout_value");
  inout->set_mode(Routine::Argument::INOUT);
  inout->mutable_data_type()->set_type_kind(StandardSqlDataType::BOOL);

  // BigQuery functions can have an unnamed return argument.
  auto* return_argument = routine.add_arguments();
  return_argument->set_mode(Routine::Argument::OUT);
  return_argument->mutable_data_type()->set_type_kind(
      StandardSqlDataType::INT64);

  auto result = FetchBQProcedureData("test-project", "test-dataset", routine);

  ASSERT_TRUE(result.Ok());

  EXPECT_EQ(result->catalog, "test-project");
  EXPECT_EQ(result->dataset, "test-dataset");
  EXPECT_EQ(result->procedure_name, "TestProcedure");

  ASSERT_EQ(result->schema.fields.size(), 3);

  EXPECT_EQ(result->schema.fields[0].name, "input_value");
  EXPECT_EQ(result->schema.fields[0].ordinal_number, "1");
  EXPECT_EQ(result->schema.fields[0].column_type, "IN");
  EXPECT_EQ(result->schema.fields[0].type_name, "INT64");

  EXPECT_EQ(result->schema.fields[1].name, "output_value");
  EXPECT_EQ(result->schema.fields[1].ordinal_number, "2");
  EXPECT_EQ(result->schema.fields[1].column_type, "OUT");
  EXPECT_EQ(result->schema.fields[1].type_name, "STRING");

  EXPECT_EQ(result->schema.fields[2].name, "inout_value");
  EXPECT_EQ(result->schema.fields[2].ordinal_number, "3");
  EXPECT_EQ(result->schema.fields[2].column_type, "INOUT");
  EXPECT_EQ(result->schema.fields[2].type_name, "BOOL");
}

TEST(FetchBQSQLProcedureDataTest, ConvertsProcedure) {
  using ::google::cloud::bigquery::v2::Routine;

  Routine routine;
  routine.mutable_routine_reference()->set_routine_id("TestProcedure");
  routine.set_routine_type(Routine::PROCEDURE);
  routine.set_language(Routine::SQL);

  auto* input = routine.add_arguments();
  input->set_name("input_value");
  input->set_mode(Routine::Argument::IN);

  auto* output = routine.add_arguments();
  output->set_name("output_value");
  output->set_mode(Routine::Argument::OUT);

  auto result =
      FetchBQSQLProcedureData("test-project", "test-dataset", routine);

  ASSERT_TRUE(result.Ok());

  EXPECT_EQ(result->procedure_catalog, "test-project");
  EXPECT_EQ(result->procedure_schema, "test-dataset");
  EXPECT_EQ(result->procedure_name, "TestProcedure");

  EXPECT_EQ(result->remarks, "SQL");
  EXPECT_EQ(result->procedure_type, SQL_PT_PROCEDURE);

  EXPECT_EQ(result->num_input_params, 1);
  EXPECT_EQ(result->num_output_params, 1);
}

TEST(FetchBQSQLProcedureDataTest, ConvertsScalarFunction) {
  using ::google::cloud::bigquery::v2::Routine;

  Routine routine;
  routine.mutable_routine_reference()->set_routine_id("TestFunction");
  routine.set_routine_type(Routine::SCALAR_FUNCTION);
  routine.set_language(Routine::SQL);

  auto* input = routine.add_arguments();
  input->set_name("value");
  input->set_mode(Routine::Argument::IN);

  // Unnamed return argument.
  auto* return_argument = routine.add_arguments();
  return_argument->set_mode(Routine::Argument::OUT);

  auto result =
      FetchBQSQLProcedureData("test-project", "test-dataset", routine);

  ASSERT_TRUE(result.Ok());

  EXPECT_EQ(result->procedure_name, "TestFunction");
  EXPECT_EQ(result->procedure_type, SQL_PT_FUNCTION);

  EXPECT_EQ(result->num_input_params, 1);
  EXPECT_EQ(result->num_output_params, 0);
}

TEST(FetchBQSQLProcedureDataTest, ConvertsTableValuedFunction) {
  using ::google::cloud::bigquery::v2::Routine;

  Routine routine;
  routine.mutable_routine_reference()->set_routine_id("TestTableFunction");
  routine.set_routine_type(Routine::TABLE_VALUED_FUNCTION);
  routine.set_language(Routine::SQL);

  auto* input = routine.add_arguments();
  input->set_name("value");
  input->set_mode(Routine::Argument::IN);

  auto result =
      FetchBQSQLProcedureData("test-project", "test-dataset", routine);

  ASSERT_TRUE(result.Ok());

  EXPECT_EQ(result->procedure_name, "TestTableFunction");
  EXPECT_EQ(result->procedure_type, 0);
  EXPECT_EQ(result->num_input_params, 1);
  EXPECT_EQ(result->num_output_params, 0);
}

TEST(FetchBQRoutineData, ConnectionNotEstablishedReturnsError2) {
  auto conn_handle = CreateConnectionHandle(false);

  auto result =
      FetchBQRoutineData(conn_handle, "project", "dataset", "routine");

  ASSERT_FALSE(result);

  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_08S01());
  EXPECT_EQ(result.GetStatusRecord().message,
            "Connection to the data source is broken");
}

TEST(FetchBQRoutineData, NullClientReturnsError2) {
  auto conn_handle = CreateConnectionHandle(true);
  conn_handle.GetClient();

  auto result =
      FetchBQRoutineData(conn_handle, "project", "dataset", "routine");

  ASSERT_FALSE(result);

  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_EQ(result.GetStatusRecord().message,
            "Invalid or null BQ Client within the connection handle");
}

TEST(ProcessProcedureColumnResults, FiltersColumnByPattern) {
  Procedure procedure;

  procedure.catalog = "project";
  procedure.dataset = "dataset";
  procedure.procedure_name = "TestProcedure";

  procedure.schema.fields.emplace_back(
      ProcedureFieldSchema{"project", "dataset", "TestProcedure", "1", "IN",
                           "YES", "first_argument", "INT64"});

  procedure.schema.fields.emplace_back(
      ProcedureFieldSchema{"project", "dataset", "TestProcedure", "2", "IN",
                           "YES", "second_argument", "STRING"});

  auto result =
      ProcessProcedureColumnResults(procedure, "first_argument", SQL_FALSE);

  ASSERT_TRUE(result.Ok());

  ASSERT_EQ(result->rows.size(), 1);
}

TEST(FetchBQProcedureDataTest, AnyTypeArgumentBuildsResultRows) {
  using ::google::cloud::bigquery::v2::Routine;

  Routine routine;
  routine.mutable_routine_reference()->set_routine_id("TestFunction");
  routine.set_routine_type(Routine::SCALAR_FUNCTION);

  auto* argument = routine.add_arguments();
  argument->set_name("value");
  argument->set_argument_kind(Routine::Argument::ANY_TYPE);

  auto procedure = FetchBQProcedureData("project", "dataset", routine);
  ASSERT_TRUE(procedure.Ok());

  ASSERT_EQ(procedure->schema.fields.size(), 1);
  EXPECT_EQ(procedure->schema.fields[0].type_name, "ANY TYPE");

  auto result = ProcessProcedureColumnResults(*procedure, "%", SQL_FALSE);
  ASSERT_TRUE(result.Ok());
  ASSERT_EQ(result->rows.size(), 1);
  EXPECT_EQ(result->rows[0].size(), 19);
}

TEST(FetchBQProcedureDataTest, MissingFixedArgumentTypeReturnsError) {
  using ::google::cloud::bigquery::v2::Routine;

  Routine routine;
  routine.mutable_routine_reference()->set_routine_id("TestProcedure");

  auto* argument = routine.add_arguments();
  argument->set_name("value");
  argument->set_argument_kind(Routine::Argument::FIXED_TYPE);

  auto result = FetchBQProcedureData("project", "dataset", routine);

  ASSERT_FALSE(result.Ok());
  EXPECT_EQ(result.GetStatusRecord().sql_state, SQLStates::k_HY000());
  EXPECT_THAT(result.GetStatusRecord().message, HasSubstr("Missing data type"));
}

TEST(ProcessProcedureColumnResults, TableArgumentBuildsResultRows) {
  Procedure procedure;
  procedure.catalog = "project";
  procedure.dataset = "dataset";
  procedure.procedure_name = "TestFunction";

  procedure.schema.fields.emplace_back(
      ProcedureFieldSchema{"project", "dataset", "TestFunction", "1", "IN",
                           "YES", "table_argument", "TABLE"});

  auto result = ProcessProcedureColumnResults(procedure, "%", SQL_FALSE);

  ASSERT_TRUE(result.Ok());
  ASSERT_EQ(result->rows.size(), 1);
  EXPECT_EQ(result->rows[0].size(), 19);
}

TEST(ProcessProcedures, ReturnsAllProcedures) {
  SQLProcedures first{};
  first.procedure_catalog = "project";
  first.procedure_schema = "dataset";
  first.procedure_name = "FirstProcedure";
  first.procedure_type = SQL_PT_PROCEDURE;

  SQLProcedures second = first;
  second.procedure_name = "SecondProcedure";

  auto result = ProcessProcedures({first, second});

  ASSERT_TRUE(result.Ok());
  ASSERT_EQ(result->rows.size(), 2);
  EXPECT_EQ(result->rows[0].size(), 8);
  EXPECT_EQ(result->rows[1].size(), 8);
}

TEST(ProcessProcedures, EmptyInputReturnsEmptyRowsWithSchema) {
  auto result = ProcessProcedures({});

  ASSERT_TRUE(result.Ok());
  EXPECT_TRUE(result->rows.empty());
  EXPECT_EQ(result->row_schema.size(), 8);
}

TEST(ProcessProcedureColumnResults, ReturnsAllMatchingColumns) {
  Procedure procedure;
  procedure.catalog = "project";
  procedure.dataset = "dataset";
  procedure.procedure_name = "TestProcedure";

  procedure.schema.fields.emplace_back(
      ProcedureFieldSchema{"project", "dataset", "TestProcedure", "1", "IN",
                           "YES", "arg_first", "INT64"});

  procedure.schema.fields.emplace_back(
      ProcedureFieldSchema{"project", "dataset", "TestProcedure", "2", "IN",
                           "YES", "arg_second", "STRING"});

  procedure.schema.fields.emplace_back(
      ProcedureFieldSchema{"project", "dataset", "TestProcedure", "3", "IN",
                           "YES", "other", "BOOL"});

  auto result = ProcessProcedureColumnResults(procedure, "arg%", SQL_FALSE);

  ASSERT_TRUE(result.Ok());
  EXPECT_EQ(result->rows.size(), 2);
}

TEST(ProcessProcedureColumnResults, MetadataIdTreatsWildcardAsLiteral) {
  Procedure procedure;
  procedure.catalog = "project";
  procedure.dataset = "dataset";
  procedure.procedure_name = "TestProcedure";

  procedure.schema.fields.emplace_back(
      ProcedureFieldSchema{"project", "dataset", "TestProcedure", "1", "IN",
                           "YES", "arg_first", "INT64"});

  auto result = ProcessProcedureColumnResults(procedure, "arg%", SQL_TRUE);

  ASSERT_TRUE(result.Ok());
  EXPECT_TRUE(result->rows.empty());
}

}  // namespace google::cloud::odbc_bq_driver_internal
