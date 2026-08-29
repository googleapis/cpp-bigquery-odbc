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
#include "google/cloud/odbc/testing/bq_driver_utils/utils.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::bigquery_v2_minimal_internal::QueryParameter;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_testing_bq_driver_utils::CastToSQLCHAR;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

TEST(ValidateInputParameters, Success) {
  StatusRecord status = ValidateInputParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, 5, SQL_TRUE);

  EXPECT_TRUE(status.ok());
}

TEST(ValidateInputParameters, FailureCatalognamelengthnegative) {
  StatusRecord status = ValidateInputParameters(
      CastToSQLCHAR("project"), -7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, 5, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message, HasSubstr("catalog length is invalid"));
}

TEST(ValidateInputParameters, FailureSchemanamelengthnegative) {
  StatusRecord status = ValidateInputParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), -7,
      CastToSQLCHAR("table"), 5, 5, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message, HasSubstr("schema length is invalid"));
}

TEST(ValidateInputParameters, FailureTablenamelengthnegative) {
  StatusRecord status = ValidateInputParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), -5, 5, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message, HasSubstr("table name length is invalid"));
}

TEST(ValidateInputParameters, FailureTabletypelengthnegative) {
  StatusRecord status = ValidateInputParameters(
      CastToSQLCHAR("project"), 7, CastToSQLCHAR("dataset"), 7,
      CastToSQLCHAR("table"), 5, -5, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY090(), status.sql_state);
  EXPECT_THAT(status.message, HasSubstr("table type length is invalid"));
}

TEST(ValidateInputParameters, FailureNullcatalog) {
  StatusRecord status =
      ValidateInputParameters(nullptr, 7, CastToSQLCHAR("dataset"), 7,
                              CastToSQLCHAR("table"), 5, 5, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY009(), status.sql_state);
  EXPECT_THAT(status.message,
              HasSubstr("Invalid use of NULL pointer for catalog name"));
}

TEST(ValidateInputParameters, FailureNullschema) {
  StatusRecord status =
      ValidateInputParameters(CastToSQLCHAR("project"), 7, nullptr, 7,
                              CastToSQLCHAR("table"), 5, 5, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY009(), status.sql_state);
  EXPECT_THAT(status.message,
              HasSubstr("Invalid use of NULL pointer for schema name"));
}

TEST(ValidateInputParameters, FailureNulltablename) {
  StatusRecord status = ValidateInputParameters(CastToSQLCHAR("project"), 7,
                                                CastToSQLCHAR("dataset"), 7,
                                                nullptr, 5, 5, SQL_TRUE);

  EXPECT_EQ(SQLStates::k_HY009(), status.sql_state);
  EXPECT_THAT(status.message,
              HasSubstr("Invalid use of NULL pointer for table name"));
}

TEST(ValidateInputParameters, SuccessAllnullsMetadatafalse) {
  StatusRecord status =
      ValidateInputParameters(nullptr, 0, nullptr, 0, nullptr, 0, 0, SQL_FALSE);

  EXPECT_TRUE(status.ok());
}

TEST(LiteralFromOdbcPattern, PlainNameIsLiteral) {
  auto literal = LiteralFromOdbcPattern("kirltest", SQL_FALSE);
  ASSERT_TRUE(literal.has_value());
  EXPECT_EQ(*literal, "kirltest");
}

TEST(LiteralFromOdbcPattern, HyphensAreLiteral) {
  auto literal = LiteralFromOdbcPattern("bigquery-devtools-drivers", SQL_FALSE);
  ASSERT_TRUE(literal.has_value());
  EXPECT_EQ(*literal, "bigquery-devtools-drivers");
}

TEST(LiteralFromOdbcPattern, PercentWildcardIsPattern) {
  EXPECT_FALSE(LiteralFromOdbcPattern("%", SQL_FALSE).has_value());
  EXPECT_FALSE(LiteralFromOdbcPattern("%timestamp%", SQL_FALSE).has_value());
}

TEST(LiteralFromOdbcPattern, UnderscoreWildcardIsPattern) {
  // '_' is an ODBC single-character wildcard, so a name containing it must be
  // expanded by listing rather than used as an exact identifier.
  EXPECT_FALSE(
      LiteralFromOdbcPattern("ODBC_TEST_DATASET", SQL_FALSE).has_value());
}

TEST(LiteralFromOdbcPattern, EscapedWildcardsAreLiteral) {
  auto literal = LiteralFromOdbcPattern("my\\_dataset\\%", SQL_FALSE);
  ASSERT_TRUE(literal.has_value());
  EXPECT_EQ(*literal, "my_dataset%");
}

TEST(LiteralFromOdbcPattern, EmptyPatternIsNotLiteral) {
  // Preserve the prior (match-nothing) listing path for empty filters.
  EXPECT_FALSE(LiteralFromOdbcPattern("", SQL_FALSE).has_value());
}

TEST(LiteralFromOdbcPattern, MetadataIdTrueIsAlwaysLiteral) {
  // With SQL_ATTR_METADATA_ID true, arguments are exact identifiers even when
  // they contain characters that would otherwise be wildcards.
  auto literal = LiteralFromOdbcPattern("ODBC_TEST_DATASET ", SQL_TRUE);
  ASSERT_TRUE(literal.has_value());
  EXPECT_EQ(*literal, "ODBC_TEST_DATASET");

  // Backslashes and percent signs are preserved literally and not unescaped
  auto literal_with_escapes =
      LiteralFromOdbcPattern("my\\_dataset\\%", SQL_TRUE);
  ASSERT_TRUE(literal_with_escapes.has_value());
  EXPECT_EQ(*literal_with_escapes, "my\\_dataset\\%");
}

TEST(ConstructQuery, ConstructWithTwoClausesMetadatafalse) {
  std::vector<QueryParameter> named_query_params;

  auto query =
      ConstructQuery("table-1", "BASE TABLE", SQL_FALSE, named_query_params);

  ASSERT_STATUS_RECORD_OK(query);
  EXPECT_EQ(
      *query,
      "SELECT table_name, table_type FROM INFORMATION_SCHEMA.TABLES WHERE "
      "table_name LIKE @table_name AND table_type IN UNNEST (@table_type)");
  EXPECT_EQ(2, named_query_params.size());
}

TEST(ConstructQuery, ConstructWithTwoClausesMetadatatrue) {
  std::vector<QueryParameter> named_query_params;

  auto query =
      ConstructQuery("table-1", "BASE TABLE", SQL_TRUE, named_query_params);

  ASSERT_STATUS_RECORD_OK(query);
  EXPECT_EQ(*query,
            "SELECT table_name, table_type FROM INFORMATION_SCHEMA.TABLES "
            "WHERE LOWER(table_name) = LOWER(@table_name) AND table_type IN "
            "UNNEST (@table_type)");
  EXPECT_EQ(2, named_query_params.size());
}

TEST(ConstructQuery, ConstructWithTableNameClauseMetadatafalse) {
  std::vector<QueryParameter> named_query_params;

  auto query = ConstructQuery("table-1", " % ", SQL_FALSE, named_query_params);

  ASSERT_STATUS_RECORD_OK(query);
  EXPECT_EQ(*query,
            "SELECT table_name, table_type FROM INFORMATION_SCHEMA.TABLES "
            "WHERE table_name LIKE @table_name");
  EXPECT_EQ(1, named_query_params.size());
}

TEST(ConstructQuery, ConstructWithTableNameClauseMetadatatrue) {
  std::vector<QueryParameter> named_query_params;

  auto query = ConstructQuery("table-1", " % ", SQL_TRUE, named_query_params);

  ASSERT_STATUS_RECORD_OK(query);
  EXPECT_EQ(*query,
            "SELECT table_name, table_type FROM INFORMATION_SCHEMA.TABLES "
            "WHERE LOWER(table_name) = LOWER(@table_name)");
  EXPECT_EQ(1, named_query_params.size());
}

TEST(ConstructQuery, ConstructWithTableTypeClause) {
  std::vector<QueryParameter> named_query_params;

  auto query = ConstructQuery("%", " ' BASE TABLE ' , ' VIEW ' ", SQL_FALSE,
                              named_query_params);

  ASSERT_STATUS_RECORD_OK(query);
  EXPECT_EQ(*query,
            "SELECT table_name, table_type FROM INFORMATION_SCHEMA.TABLES "
            "WHERE table_type IN UNNEST (@table_type)");
  EXPECT_EQ(1, named_query_params.size());
  EXPECT_EQ(2, named_query_params[0].parameter_value.array_values.size());
  EXPECT_EQ("BASE TABLE",
            named_query_params[0].parameter_value.array_values[0].value);
  EXPECT_EQ("VIEW",
            named_query_params[0].parameter_value.array_values[1].value);
}

TEST(ConstructQuery, ConstructWithTwoClausesEmptystrings) {
  std::vector<QueryParameter> named_query_params;

  auto query = ConstructQuery("", "", SQL_FALSE, named_query_params);

  ASSERT_STATUS_RECORD_OK(query);
  EXPECT_EQ(
      *query,
      "SELECT table_name, table_type FROM INFORMATION_SCHEMA.TABLES WHERE "
      "table_name LIKE @table_name AND table_type IN UNNEST (@table_type)");
  EXPECT_EQ(2, named_query_params.size());
}

TEST(FilterAllowedProjects, WildcardReturnsWholeAllowlistTrimmed) {
  EXPECT_THAT(FilterAllowedProjects(" project-1 ,project-2,\tproject-3 ", "%",
                                    SQL_FALSE),
              ElementsAre("project-1", "project-2", "project-3"));
}

TEST(FilterAllowedProjects, EmptyAllowlistReturnsNothing) {
  EXPECT_THAT(FilterAllowedProjects("", "%", SQL_FALSE), IsEmpty());
}

TEST(FilterAllowedProjects, BlankEntriesAreIgnored) {
  EXPECT_THAT(FilterAllowedProjects(" , project-1 ,,  ,", "%", SQL_FALSE),
              ElementsAre("project-1"));
}

TEST(FilterAllowedProjects, LikePatternFiltersAllowlist) {
  EXPECT_THAT(FilterAllowedProjects("prod-a,prod-b,dev-c", "prod%", SQL_FALSE),
              ElementsAre("prod-a", "prod-b"));
}

TEST(FilterAllowedProjects, UnderscoreMatchesSingleCharacter) {
  EXPECT_THAT(FilterAllowedProjects("pa,pb,pcc", "p_", SQL_FALSE),
              ElementsAre("pa", "pb"));
}

TEST(FilterAllowedProjects, NonMatchingPatternReturnsNothing) {
  EXPECT_THAT(FilterAllowedProjects("prod-a,prod-b", "staging%", SQL_FALSE),
              IsEmpty());
}

TEST(FilterAllowedProjects, MetadataIdMatchesExactIdCaseInsensitively) {
  EXPECT_THAT(
      FilterAllowedProjects("Project-1,project-2", "PROJECT-1", SQL_TRUE),
      ElementsAre("Project-1"));
}

TEST(FilterAllowedProjects, MetadataIdTreatsWildcardAsLiteral) {
  // With SQL_ATTR_METADATA_ID set the catalog argument is an identifier, not a
  // pattern, so "%" matches only a project literally named "%".
  EXPECT_THAT(FilterAllowedProjects("project-1,project-2", "%", SQL_TRUE),
              IsEmpty());
}

TEST(CreateResultSetForProjects, CreateResultSetForProjects) {
  std::vector<std::string> project_ids = {"id-1", "id-2"};

  ResultSet result_set = CreateResultSetForProjects(project_ids);

  EXPECT_EQ(2, result_set.rows.size());
  std::string data;
  EXPECT_EQ(5, result_set.rows[0].size());
  DSValueToString(result_set.rows[0][0], data);
  EXPECT_EQ("id-1", data);
  EXPECT_EQ(kNullValue, result_set.rows[0][1]);
  EXPECT_EQ(kNullValue, result_set.rows[0][2]);
  EXPECT_EQ(kNullValue, result_set.rows[0][3]);
  EXPECT_EQ(kNullValue, result_set.rows[0][4]);
  EXPECT_EQ(5, result_set.rows[1].size());
  DSValueToString(result_set.rows[1][0], data);
  EXPECT_EQ("id-2", data);
  EXPECT_EQ(kNullValue, result_set.rows[1][1]);
  EXPECT_EQ(kNullValue, result_set.rows[1][2]);
  EXPECT_EQ(kNullValue, result_set.rows[1][3]);
  EXPECT_EQ(kNullValue, result_set.rows[1][4]);
}

TEST(CreateResultSetForDatasets, CreateResultSetForDatasets) {
  std::vector<std::string> dataset_ids = {"id-1", "id-2"};

  ResultSet result_set = CreateResultSetForDatasets(dataset_ids);

  EXPECT_EQ(2, result_set.rows.size());
  std::string data;
  EXPECT_EQ(5, result_set.rows[0].size());
  EXPECT_EQ(kNullValue, result_set.rows[0][0]);
  DSValueToString(result_set.rows[0][1], data);
  EXPECT_EQ("id-1", data);
  EXPECT_EQ(kNullValue, result_set.rows[0][2]);
  EXPECT_EQ(kNullValue, result_set.rows[0][3]);
  EXPECT_EQ(kNullValue, result_set.rows[0][4]);
  EXPECT_EQ(5, result_set.rows[1].size());
  EXPECT_EQ(kNullValue, result_set.rows[1][0]);
  DSValueToString(result_set.rows[1][1], data);
  EXPECT_EQ("id-2", data);
  EXPECT_EQ(kNullValue, result_set.rows[1][2]);
  EXPECT_EQ(kNullValue, result_set.rows[1][3]);
  EXPECT_EQ(kNullValue, result_set.rows[1][4]);
}

TEST(CreateResultSetForTableTypes, CreateResultSetForTableTypes) {
  ResultSet result_set = CreateResultSetForTableTypes();

  EXPECT_EQ(kAllTableTypes.size(), result_set.rows.size());
  std::string data;
  for (int i = 0; i < result_set.rows.size(); i++) {
    EXPECT_EQ(5, result_set.rows[i].size());
    EXPECT_EQ(kNullValue, result_set.rows[i][0]);
    EXPECT_EQ(kNullValue, result_set.rows[i][1]);
    EXPECT_EQ(kNullValue, result_set.rows[i][2]);
    DSValueToString(result_set.rows[i][3], data);
    EXPECT_EQ(kAllTableTypes[i], data);
    EXPECT_EQ(kNullValue, result_set.rows[i][4]);
  }
}

TEST(ProcessStringResults, ProcessStringResults) {
  std::vector<std::vector<std::string>> rows = {
      {"project-1", "dataset-1", "table-1", "table-type-1", "desc-1"},
      {"project-2", "dataset-2", "table-2", "table-type-2", "desc-2"}};

  ResultSet result_set = ProcessStringResults(rows);

  EXPECT_EQ(2, result_set.rows.size());
  std::string data;
  EXPECT_EQ(5, result_set.rows[0].size());
  DSValueToString(result_set.rows[0][0], data);
  EXPECT_EQ("project-1", data);
  DSValueToString(result_set.rows[0][1], data);
  EXPECT_EQ("dataset-1", data);
  DSValueToString(result_set.rows[0][2], data);
  EXPECT_EQ("table-1", data);
  DSValueToString(result_set.rows[0][3], data);
  EXPECT_EQ("table-type-1", data);
  DSValueToString(result_set.rows[0][4], data);
  EXPECT_EQ("desc-1", data);
  EXPECT_EQ(5, result_set.rows[1].size());
  DSValueToString(result_set.rows[1][0], data);
  EXPECT_EQ("project-2", data);
  DSValueToString(result_set.rows[1][1], data);
  EXPECT_EQ("dataset-2", data);
  DSValueToString(result_set.rows[1][2], data);
  EXPECT_EQ("table-2", data);
  DSValueToString(result_set.rows[1][3], data);
  EXPECT_EQ("table-type-2", data);
  DSValueToString(result_set.rows[1][4], data);
  EXPECT_EQ("desc-2", data);
}

}  // namespace google::cloud::odbc_bq_driver_internal
