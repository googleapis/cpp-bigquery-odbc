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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_info.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecordOr;
using google::cloud::odbc_testing_utils::StatusRecordIs;
using ::testing::HasSubstr;

constexpr SQLUSMALLINT kSupportedInfoType =
    static_cast<SQLUSMALLINT>(SQL_DATABASE_NAME);
constexpr SQLUSMALLINT kUnsupportedInfoType =
    static_cast<SQLUSMALLINT>(SQL_KEYWORDS);

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
    {SQL_STRING_FUNCTIONS, 15822265},
    {SQL_CONVERT_BIGINT, 20864}};

TEST(SQLGetInfoUnsupported, SqlCharEmpty) {
  for (auto const& elem : kUnsupportedEmptyCharMap) {
    SQLUSMALLINT info_type = elem.first;
    std::string info_val = elem.second;
    auto* expected_info_val =
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(info_val.c_str()));
    StatusRecordOr<SQLGetInfoSqlChar> actual_info =
        UnSupportedInfoType<SQLGetInfoSqlChar>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(*expected_info_val, *(actual_info->info_val));
  }
}

TEST(SQLGetInfoUnsupported, SqlCharN) {
  for (auto const& elem : kUnsupportedNCharMap) {
    SQLUSMALLINT info_type = elem.first;
    std::string info_val = elem.second;
    auto* expected_info_val =
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(info_val.c_str()));
    StatusRecordOr<SQLGetInfoSqlChar> actual_info =
        UnSupportedInfoType<SQLGetInfoSqlChar>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);

    EXPECT_EQ(*expected_info_val, *(actual_info->info_val));
  }
}

TEST(SQLGetInfoUnsupported, SqlCharInvalid) {
  StatusRecordOr<SQLGetInfoSqlChar> actual_info =
      UnSupportedInfoType<SQLGetInfoSqlChar>(kSupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(SQLGetInfoUnsupported, SQLUSmallIntValue) {
  for (auto const& elem : kUnsupportedUSmallIntMap) {
    SQLUSMALLINT info_type = elem.first;
    SQLUSMALLINT expected_info_val = elem.second;
    StatusRecordOr<SQLGetInfoSqlUSmallInt> actual_info =
        UnSupportedInfoType<SQLGetInfoSqlUSmallInt>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(expected_info_val, actual_info->info_val);
  }
}

TEST(SQLGetInfoUnsupported, SQLUSmallIntInvalid) {
  StatusRecordOr<SQLGetInfoSqlUSmallInt> actual_info =
      UnSupportedInfoType<SQLGetInfoSqlUSmallInt>(kSupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(SQLGetInfoUnsupported, SQLUIntValue) {
  for (auto const& elem : kUnsupportedUIntMap) {
    SQLUSMALLINT info_type = elem.first;
    SQLUINTEGER expected_info_val = elem.second;
    StatusRecordOr<SQLGetInfoSqlUInt> actual_info =
        UnSupportedInfoType<SQLGetInfoSqlUInt>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(expected_info_val, actual_info->info_val);
  }
}

TEST(SQLGetInfoUnsupported, SQLUIntInvalid) {
  StatusRecordOr<SQLGetInfoSqlUInt> actual_info =
      UnSupportedInfoType<SQLGetInfoSqlUInt>(kSupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(SQLGetInfoUnsupported, BitmaskValue) {
  for (auto const& elem : kUnsupportedBitmaskMap) {
    SQLUSMALLINT info_type = elem.first;
    SQLUINTEGER expected_info_val = elem.second;
    StatusRecordOr<SQLGetInfoBitmask> actual_info =
        UnSupportedInfoType<SQLGetInfoBitmask>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(expected_info_val, actual_info->info_val);
  }
}

TEST(SQLGetInfoUnsupported, BitmaskInvalid) {
  StatusRecordOr<SQLGetInfoBitmask> actual_info =
      UnSupportedInfoType<SQLGetInfoBitmask>(kSupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(SQLGetInfoSupported, SqlChar) {
  for (auto const& elem : kSupportedCharMap) {
    SQLUSMALLINT info_type = elem.first;
    std::string info_val = elem.second;
    auto* expected_info_val =
        reinterpret_cast<SQLCHAR*>(const_cast<char*>(info_val.c_str()));
    StatusRecordOr<SQLGetInfoSqlChar> actual_info =
        SupportedInfoType<SQLGetInfoSqlChar>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(*expected_info_val, *(actual_info->info_val));
  }
}

TEST(SQLGetInfoSupported, SqlCharInvalid) {
  StatusRecordOr<SQLGetInfoSqlChar> actual_info =
      SupportedInfoType<SQLGetInfoSqlChar>(kUnsupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(SQLGetInfoSupported, SqlUSmallInt) {
  for (auto const& elem : kSupportedUSmallIntMap) {
    SQLUSMALLINT info_type = elem.first;
    SQLUSMALLINT expected_info_val = elem.second;
    StatusRecordOr<SQLGetInfoSqlUSmallInt> actual_info =
        SupportedInfoType<SQLGetInfoSqlUSmallInt>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(expected_info_val, actual_info->info_val);
  }
}

TEST(SQLGetInfoSupported, SqlUSmallIntInvalid) {
  StatusRecordOr<SQLGetInfoSqlUSmallInt> actual_info =
      SupportedInfoType<SQLGetInfoSqlUSmallInt>(kUnsupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(SQLGetInfoSupported, SqlUInteger) {
  for (auto const& elem : kSupportedUIntMap) {
    SQLUSMALLINT info_type = elem.first;
    SQLUINTEGER expected_info_val = elem.second;
    StatusRecordOr<SQLGetInfoSqlUInt> actual_info =
        SupportedInfoType<SQLGetInfoSqlUInt>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(expected_info_val, actual_info->info_val);
  }
}

TEST(SQLGetInfoSupported, SqlUIntegerInvalid) {
  StatusRecordOr<SQLGetInfoSqlUInt> actual_info =
      SupportedInfoType<SQLGetInfoSqlUInt>(kUnsupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(SQLGetInfoSupported, SqlBitmask) {
  for (auto const& elem : kSupportedBitmaskMap) {
    SQLUSMALLINT info_type = elem.first;
    SQLUINTEGER expected_info_val = elem.second;
    StatusRecordOr<SQLGetInfoBitmask> actual_info =
        SupportedInfoType<SQLGetInfoBitmask>(info_type);
    ASSERT_STATUS_RECORD_OK(actual_info);
    EXPECT_EQ(expected_info_val, actual_info->info_val);
  }
}

TEST(SQLGetInfoSupported, SqlBitmaskInvalid) {
  StatusRecordOr<SQLGetInfoBitmask> actual_info =
      SupportedInfoType<SQLGetInfoBitmask>(kUnsupportedInfoType);

  EXPECT_THAT(actual_info, StatusRecordIs(SQLStates::k_HY096(),
                                          HasSubstr("Invalid infoType")));
}

TEST(InfoValToResponse, SQLGetInfoCharDestbufferlenGtSrclen) {
  SQLGetInfoSqlChar info_val_char;
  info_val_char.info_val =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>("sample-test"));
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 15;
  SQLCHAR dest[15];
  ConnectionHandle handle;

  info_val_char.InfoValToResponse(&handle, reinterpret_cast<SQLPOINTER>(&dest),
                                  buffer_len, &str_len);
  EXPECT_TRUE(handle.GetDiagnostics().GetStatusRecords().empty());

  std::string actual = reinterpret_cast<char*>(dest);

  EXPECT_EQ("sample-test", actual);
  EXPECT_EQ(str_len, 11);
}

TEST(InfoValToResponse, SQLGetInfoCharDestbufferlenLtSrclen) {
  SQLGetInfoSqlChar info_val_char;
  info_val_char.info_val =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>("sample-test"));
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 5;
  SQLCHAR dest[5];
  ConnectionHandle handle;

  info_val_char.InfoValToResponse(&handle, reinterpret_cast<SQLPOINTER>(&dest),
                                  buffer_len, &str_len);
  EXPECT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());

  std::string actual = reinterpret_cast<char*>(dest);

  EXPECT_EQ("samp", actual);
  EXPECT_EQ(str_len, 4);
}

TEST(InfoValToResponse, SQLGetInfoCharDestbufferlenEqSrclen) {
  SQLGetInfoSqlChar info_val_char;
  info_val_char.info_val =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>("sampl"));
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 5;
  SQLCHAR dest[5];

  ConnectionHandle handle;
  info_val_char.InfoValToResponse(&handle, reinterpret_cast<SQLPOINTER>(&dest),
                                  buffer_len, &str_len);
  EXPECT_FALSE(handle.GetDiagnostics().GetStatusRecords().empty());

  std::string actual = reinterpret_cast<char*>(dest);

  EXPECT_EQ("samp", actual);
  EXPECT_EQ(str_len, 4);
}

TEST(InfoValToResponse, SQLGetInfoCharDestbufferlenzero) {
  SQLGetInfoSqlChar info_val_char;
  info_val_char.info_val =
      reinterpret_cast<SQLCHAR*>(const_cast<char*>("sample-test"));
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 0;
  SQLCHAR dest[15];
  ConnectionHandle handle;

  info_val_char.InfoValToResponse(&handle, reinterpret_cast<SQLPOINTER>(&dest),
                                  buffer_len, &str_len);
  EXPECT_TRUE(handle.GetDiagnostics().GetStatusRecords().empty());

  std::string actual = reinterpret_cast<char*>(dest);

  EXPECT_EQ("", actual);
  EXPECT_EQ(str_len, 0);
}

TEST(InfoValToResponse, SQLGetInfoCharSrclenzero) {
  SQLGetInfoSqlChar info_val_char;
  info_val_char.info_val = reinterpret_cast<SQLCHAR*>(const_cast<char*>(""));
  SQLSMALLINT str_len;
  SQLSMALLINT buffer_len = 15;
  SQLCHAR dest[15];
  ConnectionHandle handle;

  info_val_char.InfoValToResponse(&handle, reinterpret_cast<SQLPOINTER>(&dest),
                                  buffer_len, &str_len);
  EXPECT_TRUE(handle.GetDiagnostics().GetStatusRecords().empty());

  std::string actual = reinterpret_cast<char*>(dest);

  EXPECT_EQ("", actual);
  EXPECT_EQ(str_len, 0);
}

TEST(InfoValToResponse, SQLGetInfoBitmask) {
  SQLGetInfoBitmask info_val_bit;
  info_val_bit.info_val = static_cast<SQLUINTEGER>(127);
  SQLSMALLINT str_len;
  SQLUINTEGER dest;

  info_val_bit.InfoValToResponse(reinterpret_cast<SQLPOINTER>(&dest), &str_len);

  EXPECT_EQ(info_val_bit.info_val, dest);
  EXPECT_EQ(4, str_len);
}

TEST(InfoValToResponse, SQLGetInfoInt) {
  SQLGetInfoSqlUInt info_val_i;
  info_val_i.info_val = static_cast<SQLUINTEGER>(10);
  SQLSMALLINT str_len;
  SQLUINTEGER dest;
  ConnectionHandle handle;

  info_val_i.InfoValToResponse(reinterpret_cast<SQLPOINTER>(&dest), &str_len);

  EXPECT_EQ(info_val_i.info_val, dest);
  EXPECT_EQ(4, str_len);
}

TEST(InfoValToResponse, SQLGetInfoSmallInt) {
  SQLGetInfoSqlUSmallInt info_val_si;
  info_val_si.info_val = static_cast<SQLUSMALLINT>(10);
  SQLSMALLINT str_len;
  SQLUSMALLINT dest;
  info_val_si.InfoValToResponse(reinterpret_cast<SQLPOINTER>(&dest), &str_len);

  EXPECT_EQ(info_val_si.info_val, dest);
  EXPECT_EQ(2, str_len);
}

}  // namespace google::cloud::odbc_bq_driver_internal
