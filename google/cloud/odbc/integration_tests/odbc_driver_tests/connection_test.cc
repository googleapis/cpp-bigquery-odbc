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

#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_tests {

TEST(SQLGetInfo, CheckPositionalUpdate) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // Check SQL_DYNAMIC_CURSOR_ATTRIBUTES1
  SQLUINTEGER sql_bitmask_buf = 0;
  SQLRETURN status;
  status = SQLGetInfo(conn->hdbc, SQL_DYNAMIC_CURSOR_ATTRIBUTES1,
                      &sql_bitmask_buf, 0, nullptr);
  CheckError(status, "SQLGetInfo(SQL_DYNAMIC_CURSOR_ATTRIBUTES1)", conn);

  EXPECT_NE(SQL_CA1_POSITIONED_UPDATE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_UPDATE));
  EXPECT_NE(SQL_CA1_POSITIONED_DELETE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_DELETE));

  // Check SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1
  sql_bitmask_buf = 0;
  status = SQLGetInfo(conn->hdbc, SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1,
                      &sql_bitmask_buf, 0, nullptr);
  CheckError(status, "SQLGetInfo(SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1)", conn);

  EXPECT_NE(SQL_CA1_POSITIONED_UPDATE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_UPDATE));
  EXPECT_NE(SQL_CA1_POSITIONED_DELETE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_DELETE));

  // Check SQL_KEYSET_CURSOR_ATTRIBUTES1
  sql_bitmask_buf = 0;
  status = SQLGetInfo(conn->hdbc, SQL_KEYSET_CURSOR_ATTRIBUTES1,
                      &sql_bitmask_buf, 0, nullptr);
  CheckError(status, "SQLGetInfo(SQL_KEYSET_CURSOR_ATTRIBUTES1)", conn);

  EXPECT_NE(SQL_CA1_POSITIONED_UPDATE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_UPDATE));
  EXPECT_NE(SQL_CA1_POSITIONED_DELETE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_DELETE));

  // Check SQL_STATIC_CURSOR_ATTRIBUTES1
  sql_bitmask_buf = 0;
  status = SQLGetInfo(conn->hdbc, SQL_STATIC_CURSOR_ATTRIBUTES1,
                      &sql_bitmask_buf, 0, nullptr);
  CheckError(status, "SQLGetInfo(SQL_KEYSET_CURSOR_ATTRIBUTES1)", conn);

  EXPECT_NE(SQL_CA1_POSITIONED_UPDATE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_UPDATE));
  EXPECT_NE(SQL_CA1_POSITIONED_DELETE,
            (sql_bitmask_buf & SQL_CA1_POSITIONED_DELETE));

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#ifdef BQ_DRIVER_INTEGRATION_TESTS
namespace {
// Constants for GetBQDriverInfo
static std::map<SQLUSMALLINT, std::string> const kUnsupportedEmptyCharMap = {
    {SQL_KEYWORDS, ""},
    {SQL_PROCEDURE_TERM, ""},
    {SQL_SPECIAL_CHARACTERS, ""},
    {SQL_USER_NAME, ""}};

static std::map<SQLUSMALLINT, std::string> const kUnsupportedNCharMap = {
    {SQL_ACCESSIBLE_PROCEDURES, "N"},
    {SQL_DATA_SOURCE_READ_ONLY, "N"},
    {SQL_INTEGRITY, "N"},
    {SQL_LIKE_ESCAPE_CLAUSE, "N"},
    {SQL_MAX_ROW_SIZE_INCLUDES_LONG, "N"},
    {SQL_MULT_RESULT_SETS, "N"},
    {SQL_NEED_LONG_DATA_LEN, "N"},
    {SQL_ORDER_BY_COLUMNS_IN_SELECT, "N"},
    {SQL_ROW_UPDATES, "N"}};

static std::map<SQLUSMALLINT, std::string> const kSupportedCharMap = {
    {SQL_ACCESSIBLE_TABLES, "Y"},
    {SQL_CATALOG_NAME, "Y"},
    {SQL_CATALOG_NAME_SEPARATOR, "."},
    {SQL_CATALOG_TERM, "Project"},
    {SQL_COLLATION_SEQ, "UTF-16LE_BINARY"},
    {SQL_COLUMN_ALIAS, "Y"},
    {SQL_DBMS_NAME, "BigQuery"},
    {SQL_DBMS_VER, "2"},
    {SQL_DESCRIBE_PARAMETER, "Y"},
    {SQL_DRIVER_NAME, "Google ODBC Driver For BigQuery"},
    {SQL_DRIVER_ODBC_VER, "03.80"},
    {SQL_DRIVER_VER, "1.0.0.0000"},
    {SQL_EXPRESSIONS_IN_ORDERBY, "Y"},
    {SQL_IDENTIFIER_QUOTE_CHAR, "`"},
    {SQL_MULTIPLE_ACTIVE_TXN, "Y"},
    {SQL_PROCEDURES, "Y"},
    {SQL_SCHEMA_TERM, "Dataset"},
    {SQL_SEARCH_PATTERN_ESCAPE, "\\"},
    {SQL_SERVER_NAME, "Google"},
    {SQL_TABLE_TERM, "Table"}};

static std::map<SQLUSMALLINT, SQLUSMALLINT> const kUnsupportedUSmallIntMap = {
    {SQL_ACTIVE_ENVIRONMENTS, 0},
    {SQL_CONCAT_NULL_BEHAVIOR, 0},
    {SQL_FILE_USAGE, 0},
    {SQL_MAX_COLUMNS_IN_GROUP_BY, 0},
    {SQL_MAX_COLUMNS_IN_INDEX, 0},
    {SQL_MAX_COLUMNS_IN_ORDER_BY, 0},
    {SQL_MAX_COLUMNS_IN_SELECT, 0},
    {SQL_MAX_CONCURRENT_ACTIVITIES, 0},
    {SQL_MAX_CURSOR_NAME_LEN, 0},
    {SQL_MAX_DRIVER_CONNECTIONS, 0},
    {SQL_MAX_PROCEDURE_NAME_LEN, 0},
    {SQL_MAX_USER_NAME_LEN, 0},
    {SQL_NON_NULLABLE_COLUMNS, 0}};

static std::map<SQLUSMALLINT, SQLUSMALLINT> const kSupportedUSmallIntMap = {
    {SQL_CATALOG_LOCATION, 1},
    {SQL_CORRELATION_NAME, 2},
    {SQL_CURSOR_COMMIT_BEHAVIOR, 1},
    {SQL_CURSOR_ROLLBACK_BEHAVIOR, 1},
    {SQL_GROUP_BY, 2},
    {SQL_IDENTIFIER_CASE, 3},
    {SQL_MAX_CATALOG_NAME_LEN, 128},
    {SQL_MAX_COLUMNS_IN_TABLE, 10000},
    {SQL_MAX_COLUMN_NAME_LEN, 128},
    {SQL_MAX_IDENTIFIER_LEN, 255},
    {SQL_MAX_SCHEMA_NAME_LEN, 1024},
    {SQL_MAX_TABLES_IN_SELECT, 1000},
    {SQL_MAX_TABLE_NAME_LEN, 1024},
    {SQL_NULL_COLLATION, 1},
    {SQL_QUOTED_IDENTIFIER_CASE, 3},
    {SQL_TXN_CAPABLE, 1}};

static std::map<SQLUSMALLINT, SQLUINTEGER> const kSupportedUIntMap = {
    {SQL_ASYNC_MODE, 2},
    {SQL_DEFAULT_TXN_ISOLATION, 8},
    {SQL_ODBC_INTERFACE_CONFORMANCE, 1},
    {SQL_SQL_CONFORMANCE, 1}};

static std::map<SQLUSMALLINT, SQLUINTEGER> const kUnsupportedUIntMap = {
    {SQL_BATCH_ROW_COUNT, 0},
    {SQL_BATCH_SUPPORT, 0},
    {SQL_BOOKMARK_PERSISTENCE, 0},
    {SQL_CURSOR_SENSITIVITY, 0},
    {SQL_DDL_INDEX, 0},
    {SQL_MAX_ASYNC_CONCURRENT_STATEMENTS, 0},
    {SQL_MAX_BINARY_LITERAL_LEN, 0},
    {SQL_MAX_CHAR_LITERAL_LEN, 0},
    {SQL_MAX_INDEX_SIZE, 0},
    {SQL_MAX_ROW_SIZE, 0},
    {SQL_MAX_STATEMENT_LEN, 0},
    {SQL_PARAM_ARRAY_ROW_COUNTS, 0},
    {SQL_PARAM_ARRAY_SELECTS, 0}};

static std::map<SQLUSMALLINT, SQLUINTEGER> const kUnsupportedBitmaskMap = {
    {SQL_ALTER_DOMAIN, 0L},
    {SQL_ALTER_TABLE, 0L},
    {SQL_CONVERT_BINARY, 0L},
    {SQL_CONVERT_CHAR, 0L},
    {SQL_CONVERT_DECIMAL, 0L},
    {SQL_CONVERT_FLOAT, 0L},
    {SQL_CONVERT_INTEGER, 0L},
    {SQL_CONVERT_INTERVAL_DAY_TIME, 0L},
    {SQL_CONVERT_INTERVAL_YEAR_MONTH, 0L},
    {SQL_CONVERT_LONGVARBINARY, 0L},
    {SQL_CONVERT_LONGVARCHAR, 0L},
    {SQL_CONVERT_NUMERIC, 0L},
    {SQL_CONVERT_REAL, 0L},
    {SQL_CONVERT_SMALLINT, 0L},
    {SQL_CONVERT_TINYINT, 0L},
    {SQL_CREATE_ASSERTION, 0L},
    {SQL_CREATE_CHARACTER_SET, 0L},
    {SQL_CREATE_COLLATION, 0L},
    {SQL_CREATE_DOMAIN, 0L},
    {SQL_CREATE_SCHEMA, 0L},
    {SQL_CREATE_SCHEMA, 0L},
    {SQL_CREATE_TABLE, 0L},
    {SQL_CREATE_TRANSLATION, 0L},
    {SQL_CREATE_VIEW, 0L},
    {SQL_DROP_ASSERTION, 0L},
    {SQL_DROP_CHARACTER_SET, 0L},
    {SQL_DROP_COLLATION, 0L},
    {SQL_DROP_DOMAIN, 0L},
    {SQL_DROP_SCHEMA, 0L},
    {SQL_DROP_TABLE, 0L},
    {SQL_DROP_TRANSLATION, 0L},
    {SQL_DROP_VIEW, 0L},
    {SQL_DYNAMIC_CURSOR_ATTRIBUTES1, 0L},
    {SQL_DYNAMIC_CURSOR_ATTRIBUTES2, 0L},
    {SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1, 0L},
    {SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1, 0L},
    {SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2, 0L},
    {SQL_INDEX_KEYWORDS, 0L},
    {SQL_INFO_SCHEMA_VIEWS, 0L},
    {SQL_INSERT_STATEMENT, 0L},
    {SQL_KEYSET_CURSOR_ATTRIBUTES1, 0L},
    {SQL_KEYSET_CURSOR_ATTRIBUTES2, 0L},
    {SQL_POS_OPERATIONS, 0L},
    {SQL_SQL92_FOREIGN_KEY_DELETE_RULE, 0L},
    {SQL_SQL92_FOREIGN_KEY_UPDATE_RULE, 0L},
    {SQL_SQL92_GRANT, 0L},
    {SQL_SQL92_NUMERIC_VALUE_FUNCTIONS, 0L},
    {SQL_SQL92_REVOKE, 0L},
    {SQL_STATIC_CURSOR_ATTRIBUTES1, 0L},
    {SQL_STATIC_CURSOR_ATTRIBUTES2, 0L},
    {SQL_UNION, 0L}};

static std::map<SQLUSMALLINT, SQLUINTEGER> const kSupportedBitmaskMap = {
    {SQL_AGGREGATE_FUNCTIONS, 127},
    {SQL_CATALOG_USAGE, 1},
    {SQL_CONVERT_BIT, 20736},
    {SQL_CONVERT_DATE, 164096},
    {SQL_CONVERT_DOUBLE, 16768},
    {SQL_CONVERT_FUNCTIONS, 3},
    {SQL_CONVERT_TIME, 65792},
    {SQL_CONVERT_VARBINARY, 2304},
    {SQL_CONVERT_VARCHAR, 252288},
    {SQL_DATETIME_LITERALS, 4},
    {SQL_GETDATA_EXTENSIONS, 15},
    {SQL_NUMERIC_FUNCTIONS, 14221311},
    {SQL_CONVERT_TIMESTAMP, 196864},
    {SQL_OJ_CAPABILITIES, 127},
    {SQL_SCROLL_OPTIONS, 1},
    {SQL_SCHEMA_USAGE, 31},
    {SQL_SUBQUERIES, 31},
    {SQL_TXN_ISOLATION_OPTION, 8},
    {SQL_TIMEDATE_FUNCTIONS, 2097151},
    {SQL_SYSTEM_FUNCTIONS, 4},
    {SQL_TIMEDATE_ADD_INTERVALS, 510},
    {SQL_TIMEDATE_DIFF_INTERVALS, 478},
    {SQL_SQL92_PREDICATES, 16135},
    {SQL_SQL92_RELATIONAL_JOIN_OPERATORS, 346},
    {SQL_SQL92_ROW_VALUE_CONSTRUCTOR, 15},
    {SQL_SQL92_DATETIME_FUNCTIONS, 7},
    {SQL_SQL92_STRING_FUNCTIONS, 254},
    {SQL_SQL92_VALUE_EXPRESSIONS, 2},
    {SQL_STANDARD_CLI_CONFORMANCE, 2},
    {SQL_STRING_FUNCTIONS, 15756697},
    {SQL_CONVERT_BIGINT, 20864}};

void AssertBQDriverSQLGetInfo(std::shared_ptr<ODBCHandles> conn) {
  SQLCHAR sqlCharBuf[kBufferLength];
  SQLUSMALLINT sqlUSmallIntBuf;
  SQLUINTEGER sqlUIntegerBuf;
  SQLUINTEGER sqlBitmaskBuf;
  SQLSMALLINT out_len;
  SQLRETURN status;

  for (auto elem : kSupportedCharMap) {
    auto info_type = elem.first;
    auto expected_info_val = elem.second;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(sqlCharBuf), kBufferLength,
                        &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    std::string actual_val = reinterpret_cast<char*>(sqlCharBuf);
    EXPECT_EQ(expected_info_val, actual_val);
  }
  for (auto elem : kUnsupportedEmptyCharMap) {
    auto info_type = elem.first;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(sqlCharBuf), kBufferLength,
                        &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    std::string actual_val = (char*)sqlCharBuf;
    EXPECT_EQ("", actual_val);
  }
  for (auto elem : kUnsupportedNCharMap) {
    auto info_type = elem.first;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(sqlCharBuf), kBufferLength,
                        &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    std::string actual_val = (char*)sqlCharBuf;
    EXPECT_EQ("N", actual_val);
  }
  for (auto elem : kSupportedUSmallIntMap) {
    auto info_type = elem.first;
    auto expected_info_val = elem.second;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(&sqlUSmallIntBuf),
                        sizeof(sqlUSmallIntBuf), &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    SQLUSMALLINT actual_val = sqlUSmallIntBuf;
    EXPECT_EQ(expected_info_val, actual_val);
  }
  for (auto elem : kUnsupportedUSmallIntMap) {
    auto info_type = elem.first;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(&sqlUSmallIntBuf),
                        sizeof(sqlUSmallIntBuf), &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    SQLUSMALLINT actual_val = sqlUSmallIntBuf;
    EXPECT_EQ(0, actual_val);
  }
  for (auto elem : kSupportedUIntMap) {
    auto info_type = elem.first;
    auto expected_info_val = elem.second;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(&sqlUIntegerBuf),
                        sizeof(sqlUIntegerBuf), &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    SQLUINTEGER actual_val = sqlUIntegerBuf;
    EXPECT_EQ(expected_info_val, actual_val);
  }
  for (auto elem : kUnsupportedUIntMap) {
    auto info_type = elem.first;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(&sqlUIntegerBuf),
                        sizeof(sqlUIntegerBuf), &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    SQLUINTEGER actual_val = sqlUIntegerBuf;
    EXPECT_EQ(0, actual_val);
  }
  for (auto elem : kSupportedBitmaskMap) {
    auto info_type = elem.first;
    auto expected_info_val = elem.second;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(&sqlBitmaskBuf),
                        sizeof(sqlBitmaskBuf), &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    SQLUINTEGER actual_val = sqlBitmaskBuf;
    EXPECT_EQ(expected_info_val, actual_val);
  }
  for (auto elem : kUnsupportedBitmaskMap) {
    auto info_type = elem.first;
    status = SQLGetInfo(conn->hdbc, info_type,
                        reinterpret_cast<SQLPOINTER>(&sqlBitmaskBuf),
                        sizeof(sqlBitmaskBuf), &out_len);
    ASSERT_TRUE(SQL_SUCCEEDED(status));
    SQLUINTEGER actual_val = sqlBitmaskBuf;
    EXPECT_EQ(0L, actual_val);
  }
}

void AssertUnSupportedFnsODBC2(SQLUSMALLINT* odbc2_fns) {
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLERROR]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLPARAMOPTIONS]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETSCROLLOPTIONS]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETPARAM]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLALLOCCONNECT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLALLOCENV]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLALLOCSTMT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLFREECONNECT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLFREEENV]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLFREESTMT]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLBINDPARAMETER]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLGETCONNECTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLGETSTMTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETCONNECTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLSETSTMTOPTION]);
  EXPECT_EQ(SQL_FALSE, odbc2_fns[SQL_API_SQLTRANSACT]);
}

void AssertSupportedFnsODBC3(SQLUSMALLINT* odbc3_fns) {
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLALLOCHANDLE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDESCFIELD));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETCONNECTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDRIVERS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBINDCOL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDESCREC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCANCEL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDIAGFIELD));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCLOSECURSOR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDIAGREC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOLATTRIBUTE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETENVATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETFUNCTIONS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOPYDESC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETINFO));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDATASOURCES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETSTMTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDESCRIBECOL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETTYPEINFO));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDISCONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLNUMRESULTCOLS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPARAMDATA));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLENDTRAN));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPREPARE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLEXECDIRECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPUTDATA));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLEXECUTE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLROWCOUNT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFETCH));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFETCHSCROLL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETCURSORNAME));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFREEHANDLE));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETDESCFIELD));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETDESCREC));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETCONNECTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETENVATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETCURSORNAME));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETSTMTATTR));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLGETDATA));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOLUMNS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSTATISTICS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSPECIALCOLUMNS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLTABLES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBINDPARAM));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLNATIVESQL));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBROWSECONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLNUMPARAMS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPRIMARYKEYS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLCOLUMNPRIVILEGES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPROCEDURECOLUMNS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDESCRIBEPARAM));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLPROCEDURES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLDRIVERCONNECT));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLFOREIGNKEYS));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLTABLEPRIVILEGES));
  EXPECT_EQ(SQL_TRUE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLMORERESULTS));
}
}  // namespace

#endif  // BQ_DRIVER_INTEGRATION_TESTS

std::vector<int> GetMajorMinorVer(std::string version_str) {
  std::vector<int> versions;
  int start, end = -1;

  do {
    start = end + 1;
    end = version_str.find(".", start);
    versions.emplace_back(stoi(version_str.substr(start, end - start)));
  } while (end != -1);

  return versions;
}

void VerifyDriverInfo(std::shared_ptr<ODBCHandles> conn) {
  EXPECT_EQ(conn->metadata.dsn_name, GetDefaultDSN());
  std::vector<int> db_odbc_versions =
      GetMajorMinorVer(conn->metadata.db_odbc_ver);
  EXPECT_EQ(db_odbc_versions[0], 3);
  std::vector<int> driver_odbc_versions =
      GetMajorMinorVer(conn->metadata.driver_odbc_ver);
  EXPECT_EQ(driver_odbc_versions[0], 3);
  EXPECT_EQ(conn->metadata.driver_name,
            "Simba ODBC Driver for Google BigQuery");
}

void SetAttr(std::shared_ptr<ODBCHandles> conn, bool use_ansi = false) {
  SQLCHAR buf[256] = "test";
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, use_ansi), SQL_SUCCESS);

  SQLRETURN status;
  if (use_ansi) {
    status = SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                (SQLPOINTER)buf, SQL_NTS);
  } else {
    status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                               (SQLPOINTER)buf, SQL_NTS);
  }
  CheckError(status, "SQLSetConnectAttr", conn, use_ansi);

  SQLCHAR output[256];
  SQLINTEGER length;
  if (use_ansi) {
    status = SQLGetConnectAttrA(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                (SQLPOINTER)output, 256, &length);
  } else {
    status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                               (SQLPOINTER)output, 256, &length);
  }
  CheckError(status, "SQLGetConnectAttr", conn, use_ansi);

  std::string actual = reinterpret_cast<char*>(output);
  EXPECT_EQ("test", actual);
  EXPECT_EQ(4, length);
}

TEST(ConnectionTest, SQLDriverConnect) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

void CreateDriverConnection() {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(MultipleConnectionTest, SQLDriverConnect) {
  int const number_of_threads = 50;
  std::thread threads[number_of_threads];

  for (int i = 0; i < number_of_threads; i++) {
    threads[i] = std::thread(CreateDriverConnection);
  }

  for (int i = 0; i < number_of_threads; i++) {
    threads[i].join();
  }
}

TEST(ConnectionTest, SQLDriverConnectA) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttr_StringWithNullTermInMiddle) {
  SQLCHAR buf[256] = "te\0t";
  SQLINTEGER len = strlen(reinterpret_cast<char*>(buf));
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                  (SQLPOINTER)buf, len);
  CheckError(status, "SQLSetConnectAttr", conn);

  SQLCHAR output[256];
  SQLINTEGER length;
  status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                             (SQLPOINTER)output, 256, &length);
  CheckError(status, "SQLGetConnectAttr", conn);

  std::string actual = reinterpret_cast<char*>(output);
  EXPECT_EQ("te\0t", actual);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttrA_StringWithNullTermInMiddle) {
  SQLCHAR buf[256] = "te\0t";
  SQLINTEGER len = strlen(reinterpret_cast<char*>(buf));
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  auto status = SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                   (SQLPOINTER)buf, len);
  CheckError(status, "SQLSetConnectAttr", conn, true);

  SQLCHAR output[256];
  SQLINTEGER length;
  status = SQLGetConnectAttrA(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                              (SQLPOINTER)output, 256, &length);
  CheckError(status, "SQLGetConnectAttr", conn, true);

  std::string actual = reinterpret_cast<char*>(output);
  EXPECT_EQ("te\0t", actual);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttr_UpdateString) {
  SQLCHAR buf[256] = "test";
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  // As per the spec if valuePtr is a character string data, string length
  // should either the length of the string or SQL_NTS
  // Simba is not following the spec and accepts incorrect lengths,
  // Google driver will follow the spec and accept correct lengths.
  auto status = SQLSetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                  (SQLPOINTER)buf, 4);
  CheckError(status, "SQLSetConnectAttr", conn);

  std::string expected = "test";

  buf[0] = '0';
  std::string buffer = reinterpret_cast<char*>(buf);
  EXPECT_EQ("0est", buffer);

  SQLCHAR output[256];
  SQLINTEGER length;
  status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                             (SQLPOINTER)output, 256, &length);
  CheckError(status, "SQLGetConnectAttr", conn);

  std::string actual = reinterpret_cast<char*>(output);
  // Parity with Simba Driver - Original value is retained even though
  // input buf has been modified by the caller.
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(expected.size(), length);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttrA_UpdateString) {
  SQLCHAR buf[256] = "test";
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // As per the spec if valuePtr is a character string data, string length
  // should either the length of the string or SQL_NTS
  // Simba is not following the spec and accepts incorrect lengths,
  // Google driver will follow the spec and accept correct lengths.
  auto status = SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                   (SQLPOINTER)buf, 4);
  CheckError(status, "SQLSetConnectAttr", conn, true);

  std::string expected = "test";

  buf[0] = '0';
  std::string buffer = reinterpret_cast<char*>(buf);
  EXPECT_EQ("0est", buffer);

  SQLCHAR output[256];
  SQLINTEGER length;
  status = SQLGetConnectAttrA(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                              (SQLPOINTER)output, 256, &length);
  CheckError(status, "SQLGetConnectAttr", conn, true);

  std::string actual = reinterpret_cast<char*>(output);
  // Parity with Simba Driver - Original value is retained even though
  // input buf has been modified by the caller.
  EXPECT_EQ(expected, actual);
  EXPECT_EQ(expected.size(), length);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttr_DeleteString) {
  auto conn = std::make_shared<ODBCHandles>();

  SetAttr(conn);

  SQLCHAR output[256];
  SQLINTEGER length;
  auto status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                  (SQLPOINTER)output, 256, &length);
  CheckError(status, "SQLGetConnectAttr", conn);

  std::string actual = reinterpret_cast<char*>(output);
  EXPECT_EQ("test", actual);
  EXPECT_EQ(4, length);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttrA_DeleteString) {
  auto conn = std::make_shared<ODBCHandles>();

  SetAttr(conn, true);

  SQLCHAR output[256];
  SQLINTEGER length;
  auto status = SQLGetConnectAttrA(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                   (SQLPOINTER)output, 256, &length);
  CheckError(status, "SQLGetConnectAttr", conn, true);

  std::string actual = reinterpret_cast<char*>(output);
  EXPECT_EQ("test", actual);
  EXPECT_EQ(4, length);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttr_Integer) {
  SQLULEN buf = SQL_ASYNC_ENABLE_ON;
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  auto status =
      SQLSetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)buf, 4);
  CheckError(status, "SQLSetConnectAttr", conn);

  auto* buf_ptr = &buf;
  *buf_ptr = 222;
  EXPECT_EQ(222, buf);

  SQLULEN output = 0;
  SQLINTEGER len;
  // Spec doesn't say anything about len being null so its upto the driver to
  // implement. Google Driver does not accept nullptr for len.
  status =
      SQLGetConnectAttr(conn->hdbc, SQL_ATTR_ASYNC_ENABLE, &output, 256, &len);
  CheckError(status, "SQLGetConnectAttr", conn);

  // Parity with simba driver - Original value is retained even
  // though input buf has been modified by the caller.
  EXPECT_EQ(SQL_ASYNC_ENABLE_ON, output);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLSetConnectAttrA_Integer) {
  SQLULEN buf = SQL_ASYNC_ENABLE_ON;
  auto conn = std::make_shared<ODBCHandles>();

  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  auto status =
      SQLSetConnectAttrA(conn->hdbc, SQL_ATTR_ASYNC_ENABLE, (SQLPOINTER)buf, 4);
  CheckError(status, "SQLSetConnectAttr", conn, true);

  auto* buf_ptr = &buf;
  *buf_ptr = 222;
  EXPECT_EQ(222, buf);

  SQLULEN output = 0;
  SQLINTEGER len;
  // Spec doesn't say anything about len being null so its upto the driver to
  // implement. Google Driver does not accept nullptr for len.
  status =
      SQLGetConnectAttrA(conn->hdbc, SQL_ATTR_ASYNC_ENABLE, &output, 256, &len);
  CheckError(status, "SQLGetConnectAttr", conn, true);

  // Parity with simba driver - Original value is retained even
  // though input buf has been modified by the caller.
  EXPECT_EQ(SQL_ASYNC_ENABLE_ON, output);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLGetConnectAttr_DefaultCatalog) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLCHAR output[256];
  SQLINTEGER length;
  auto status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CURRENT_CATALOG,
                                  (SQLPOINTER)output, 256, &length);
  CheckError(status, "SQLGetConnectAttr", conn);

  std::string actual = reinterpret_cast<char*>(output);
  EXPECT_EQ(kCatalogName, actual);
  EXPECT_EQ(kCatalogName.size(), length);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, GetDefaultValueForAutocommit) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);

  SQLUINTEGER commit_mode = 0;
  auto status =
      SQLGetConnectAttr(conn->hdbc, SQL_ATTR_AUTOCOMMIT, &commit_mode, 0, NULL);
  CheckError(status, "SQLGetConnectAttr", conn);

  EXPECT_EQ(SQL_AUTOCOMMIT_ON, commit_mode);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST(ConnectionTest, SQLConnect) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(ConnectionTest, SQLConnectA) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, true), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DriverInfoTest, SQLGetInfo) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);
  VerifyDriverInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DriverInfoTest, SQLGetInfoA) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  EXPECT_EQ(GetDriverInfo(conn, true), SQL_SUCCESS);
  VerifyDriverInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This test is temporarily disabled till this issue is fixed for the driver
TEST(ConnectionTest, DISABLED_SQLGetConnectAttr) {
  srand(time(NULL));
  int timeout = (rand() % 30) + 1;
  SQLUINTEGER timeout_ret;
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, timeout), SQL_SUCCESS);

  auto status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                                  (SQLPOINTER)&timeout_ret,
                                  (SQLINTEGER)sizeof(timeout_ret), NULL);
  CheckError(status, "SQLGetConnectAttr", conn);
  EXPECT_EQ(timeout, timeout_ret);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#else

TEST(SQLDisconnect, CheckAllHandlesAreFreed) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto status = SQLAllocHandle(SQL_HANDLE_DESC, conn->hdbc, &conn->ard);
  CheckError(status, "SQLAllocHandle(SQL_HANDLE_DESC)", conn);

  status = SQLDisconnect(conn->hdbc);
  CheckError(status, "SQLDisconnect", conn);

  // Check that descriptor handle is freed
  SQLSMALLINT alloc_type;
  status =
      SQLGetDescField(conn->ard, 0, SQL_DESC_ALLOC_TYPE, &alloc_type, 0, NULL);
  if (kIsBqDriver) {
    EXPECT_EQ(SQL_INVALID_HANDLE, status);
  } else {
    EXPECT_EQ(SQL_SUCCESS, status);
  }
  // Check that statement handle is freed
  SQLULEN metadata_id_stmt;
  status = SQLGetStmtAttr(conn->hstmt, SQL_ATTR_METADATA_ID, &metadata_id_stmt,
                          0, NULL);
  EXPECT_EQ(SQL_INVALID_HANDLE, status);
  // Check connection handle is disconnected
  status = SQLAllocHandle(SQL_HANDLE_STMT, conn->hdbc, &conn->hstmt);
  EXPECT_EQ(SQL_ERROR, status);

  status = SQLFreeHandle(SQL_HANDLE_DBC, conn->hdbc);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_DBC)", conn);
  status = SQLFreeHandle(SQL_HANDLE_ENV, conn->henv);
  CheckError(status, "SQLFreeHandle(SQL_HANDLE_ENV)", conn);
}

// This test should not be run for Simba Driver since different values are
// returned between google and Simba for some information types. For more
// details please look at design doc: http://goto.google.com/sql-get-info-design
TEST(BQDriverTest, SQLGetInfo) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  AssertBQDriverSQLGetInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetInfoA) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  AssertBQDriverSQLGetInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetFunctions_ODBC3_AllSupported) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(
      SQL_SUCCESS,
      SQLGetFunctions(conn->hdbc, SQL_API_ODBC3_ALL_FUNCTIONS, odbc3_fns));
  AssertSupportedFnsODBC3(odbc3_fns);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetFunctions_ODBC3_AllUnSupported) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(
      SQL_SUCCESS,
      SQLGetFunctions(conn->hdbc, SQL_API_ODBC3_ALL_FUNCTIONS, odbc3_fns));
  EXPECT_EQ(SQL_FALSE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLBULKOPERATIONS));
  EXPECT_EQ(SQL_FALSE, SQL_FUNC_EXISTS(odbc3_fns, SQL_API_SQLSETPOS));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetFunctions_ODBC3_FunctionIdSupported) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT supported;

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetFunctions(conn->hdbc, SQL_API_SQLMORERESULTS, &supported));

  EXPECT_EQ(SQL_TRUE, supported);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetFunctions_ODBC3_FunctionIdNotSupported) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT supported;

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetFunctions(conn->hdbc, SQL_API_SQLSETPOS, &supported));

  EXPECT_EQ(SQL_FALSE, supported);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetFunctions_ODBC2_FunctionIdNotSupported) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT supported;

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetFunctions(conn->hdbc, SQL_API_SQLERROR, &supported));

  EXPECT_EQ(SQL_FALSE, supported);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(BQDriverTest, SQLGetFunctions_ODBC2_AllUnSupported) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT odbc2_fns[100];

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(SQL_SUCCESS,
            SQLGetFunctions(conn->hdbc, SQL_API_ALL_FUNCTIONS, odbc2_fns));
  AssertUnSupportedFnsODBC2(odbc2_fns);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// Negative test cases for SQLGetFunctions

TEST(SQLGetFunctionsInternal, SQLGetFunctions_ODBC2_NullConnectionHandle) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT odbc2_fns[100];

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(SQL_INVALID_HANDLE,
            SQLGetFunctions(nullptr, SQL_API_ALL_FUNCTIONS, odbc2_fns));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetFunctionsInternal, SQLGetFunctions_ODBC3_NullConnectionHandle) {
  auto conn = std::make_shared<ODBCHandles>();
  SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];

  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(SQL_INVALID_HANDLE,
            SQLGetFunctions(nullptr, SQL_API_ODBC3_ALL_FUNCTIONS, odbc3_fns));
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetFunctionsInternal,
     SQLGetFunctions_ODBC2_InvalidConnectionHandleType) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(SQL_INVALID_HANDLE,
            SQLGetFunctions(conn->henv, SQL_API_ALL_FUNCTIONS, nullptr));
}

TEST(SQLGetFunctionsInternal,
     SQLGetFunctions_ODBC3_InvalidConnectionHandleType) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(SQL_INVALID_HANDLE,
            SQLGetFunctions(conn->henv, SQL_API_ODBC3_ALL_FUNCTIONS, nullptr));
}

TEST(SQLGetFunctionsInternal,
     SQLGetFunctions_ODBC3_ConnectionHandleNotConnectedFailure) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(SQL_INVALID_HANDLE,
            SQLGetFunctions(conn->hdbc, SQL_API_ODBC3_ALL_FUNCTIONS, nullptr));
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
