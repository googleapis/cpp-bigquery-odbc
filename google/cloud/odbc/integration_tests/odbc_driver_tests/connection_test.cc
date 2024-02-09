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

namespace google::cloud::odbc_tests {

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

void AssertBQDriverSQLGetInfo(std::shared_ptr<ConnectionHandle> conn) {
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

void VerifyDriverInfo(std::shared_ptr<ConnectionHandle> conn) {
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

TEST(ConnectionTest, SQLDriverConnect) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST(ConnectionTest, SQLConnect) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(DriverInfoTest, SQLGetInfo) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetDriverInfo(conn), SQL_SUCCESS);
  VerifyDriverInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// This test is temporarily disabled till this issue is fixed for the driver
TEST(ConnectionTest, DISABLED_SQLGetConnectAttr) {
  srand(time(NULL));
  int timeout = (rand() % 30) + 1;
  SQLUINTEGER timeout_ret;
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(ConnectDsn(kDefaultDataSource, conn, timeout), SQL_SUCCESS);

  auto status = SQLGetConnectAttr(conn->hdbc, SQL_ATTR_CONNECTION_TIMEOUT,
                                  (SQLPOINTER)&timeout_ret,
                                  (SQLINTEGER)sizeof(timeout_ret), NULL);
  CheckError(status, "SQLGetConnectAttr", conn);
  EXPECT_EQ(timeout, timeout_ret);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#else

// This test should not be run for Simba Driver since different values are
// returned between google and Simba for some information types. For more
// details please look at design doc: http://goto.google.com/sql-get-info-design
TEST(BQDriverInfoTest, SQLGetInfo) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  AssertBQDriverSQLGetInfo(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
