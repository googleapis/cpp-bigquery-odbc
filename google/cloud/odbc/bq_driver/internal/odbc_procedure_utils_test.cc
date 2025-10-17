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

}  // namespace google::cloud::odbc_bq_driver_internal
