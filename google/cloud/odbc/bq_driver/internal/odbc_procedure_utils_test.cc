// Copyright 2023 Google LLC
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
#include "google/cloud/odbc/testing/bq_driver_utils/status_utils.h"
#include "google/cloud/odbc/testing/bq_driver_utils/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_bq_driver_utils::CastToSQLCHAR;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

TEST(ValidateProcedureColumnParameters, Success_MetadataId_TRUE) {
  StatusRecord status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, CastToSQLCHAR("column"), 6, SQL_TRUE);

  EXPECT_TRUE(status.ok());
}

TEST(ValidateProcedureColumnParameters, Success_MetadataId_FALSE) {
  StatusRecord status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, CastToSQLCHAR("column"), 6, SQL_FALSE);

  EXPECT_TRUE(status.ok());
}

TEST(ValidateProcedureColumnParameters, Success_EmptyColumn) {
  StatusRecord status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, CastToSQLCHAR(""), 0, SQL_FALSE);

  EXPECT_TRUE(status.ok());
}

TEST(ValidateProcedureColumnParameters, Failure_ColumnNameLengthNegative) {
  StatusRecord status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, CastToSQLCHAR("column"), -6, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message, HasSubstr("column name length is invalid"));
}

TEST(ValidateProcedureColumnParameters,
     Failure_CatalogNameIsSearchPattern_MetadataId_TRUE) {
  StatusRecord status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project%"), 8, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, CastToSQLCHAR("column"), 6, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message,
              HasSubstr("Catalog name cannot be a search pattern"));
}

TEST(ValidateProcedureColumnParameters,
     Failure_CatalogNameIsSearchPattern_MetadataId_FALSE) {
  StatusRecord status = ValidateProcedureColumnParameters(
      CastToSQLCHAR("project%"), 8, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("Procedure"), 9, CastToSQLCHAR("column"), 6, SQL_FALSE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message,
              HasSubstr("Catalog name cannot be a search pattern"));
}

}  // namespace google::cloud::odbc_bq_driver_internal
