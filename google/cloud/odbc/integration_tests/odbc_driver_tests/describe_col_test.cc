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

#include "google/cloud/odbc/testing/odbc_utils/commons.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_tests {

TEST(SQLDescribeColumn, DescribeAllColumns) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto table_name = kDatasetWithTablePrefix + "ODBC_DESCRIBE_COLUMNS_TEST";
  Table table(table_name);
  table.CreateWithPrepare(conn, getSchemaStr(kFullSchema));

  auto select_stmt = "SELECT * FROM " + table_name;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)select_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_columns;
  status = SQLNumResultCols(conn->hstmt, &num_columns);
  CheckError(status, "SQLNumResultCols", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  EXPECT_TRUE(num_columns > 0);

  for (int i = 1; i <= num_columns; i++) {
    SQLSMALLINT data_type = 0;
    SQLULEN column_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    SQLCHAR column_name[kBufferLength];
    SQLSMALLINT column_name_len = 0;

    status = SQLDescribeCol(conn->hstmt, i, column_name, kBufferLength,
                            &column_name_len, &data_type, &column_size,
                            &decimal_digits, &nullable);
    CheckError(status, "SQLDescribeCol[" + std::to_string(i) + "]", conn);

    std::string table_col_name = kFullSchema[i - 1].name;
    std::string table_col_type = kFullSchema[i - 1].type;
    std::string col_type_sanitized = SanitizeBQColType(table_col_type);
    std::string ret_col_name = (char*)column_name;

    EXPECT_EQ(ret_col_name, table_col_name);
    EXPECT_EQ(column_name_len, table_col_name.size());
    EXPECT_TRUE(AreSqlAndBqTypesSame(data_type, col_type_sanitized));
    // Most of the values returned are supposed to be the same as
    // SQLGetTypeInfo. The exceptions are handled conditionally below
    TypeInfoRow type_info =
        kSqlToBqDataTypes.at(data_type).at(col_type_sanitized);
    if (table_col_type == "FLOAT64") {
      // Existing driver returns different value for FLOAT64 columns
      EXPECT_EQ(column_size, 15);
    } else {
      EXPECT_EQ(column_size, type_info.col_size);
    }
    // Existing driver returns different values for TIME and DATETIME columns
    if (table_col_type == "TIME" || table_col_type == "DATETIME") {
      EXPECT_EQ(decimal_digits, 6);
    } else {
      EXPECT_EQ(decimal_digits, type_info.maximum_scale);
    }
    EXPECT_EQ(nullable, type_info.nullable);
  }

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLDescribeColumnW, DescribeAllColumns) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  auto table_name = kDatasetWithTablePrefix + "ODBC_DESCRIBE_COLUMNS_WIDE_TEST";
  Table table(table_name);
  table.CreateWithPrepare(conn, getSchemaStr(kFullSchema));

  auto select_stmt = "SELECT * FROM " + table_name;
  auto status = SQLPrepare(conn->hstmt, (SQLCHAR*)select_stmt.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  SQLSMALLINT num_columns;
  status = SQLNumResultCols(conn->hstmt, &num_columns);
  CheckError(status, "SQLNumResultCols", conn);
  status =
      SQLGetStmtAttr(conn->hstmt, SQL_ATTR_IMP_ROW_DESC, &conn->ird, 0, NULL);
  CheckError(status, "SQLGetStmtAttr(SQL_ATTR_IMP_ROW_DESC)", conn);
  EXPECT_TRUE(num_columns > 0);

  for (int i = 1; i <= num_columns; i++) {
    SQLSMALLINT data_type = 0;
    SQLULEN column_size = 0;
    SQLSMALLINT decimal_digits = 0;
    SQLSMALLINT nullable = 0;
    SQLWCHAR column_name[kBufferLength];
    SQLSMALLINT column_name_len = 0;

    status = SQLDescribeColW(conn->hstmt, i, column_name, kBufferLength,
                             &column_name_len, &data_type, &column_size,
                             &decimal_digits, &nullable);
    CheckError(status, "SQLDescribeColW[" + std::to_string(i) + "]", conn);

    std::string table_col_name = kFullSchema[i - 1].name;
    std::string table_col_type = kFullSchema[i - 1].type;
    std::string col_type_sanitized = SanitizeBQColType(table_col_type);
    std::string ret_col_name =
        ConvertSQLWCHARToString(column_name, column_name_len);

    EXPECT_STREQ(ret_col_name.data(), table_col_name.data());
    EXPECT_EQ(column_name_len, table_col_name.size());
    EXPECT_TRUE(AreSqlAndBqTypesSame(data_type, col_type_sanitized));
    // Most of the values returned are supposed to be the same as
    // SQLGetTypeInfo. The exceptions are handled conditionally below
    TypeInfoRow type_info =
        kSqlToBqDataTypes.at(data_type).at(col_type_sanitized);
    if (table_col_type == "FLOAT64") {
      // Existing driver returns different value for FLOAT64 columns
      EXPECT_EQ(column_size, 15);
    } else {
      EXPECT_EQ(column_size, type_info.col_size);
    }
    // Existing driver returns different values for TIME and DATETIME columns
    if (table_col_type == "TIME" || table_col_type == "DATETIME") {
      EXPECT_EQ(decimal_digits, 6);
    } else {
      EXPECT_EQ(decimal_digits, type_info.maximum_scale);
    }
    EXPECT_EQ(nullable, type_info.nullable);
  }

  table.Drop(conn);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

namespace {

struct ExpectedColInfo {
  std::string name;
  std::string bq_type;
};

void VerifyColumnDescriptions(
    std::shared_ptr<ODBCHandles> const& conn,
    std::vector<ExpectedColInfo> const& expected_cols) {
  SQLSMALLINT num_columns = 0;
  SQLRETURN status = SQLNumResultCols(conn->hstmt, &num_columns);
  CheckError(status, "SQLNumResultCols", conn);
  EXPECT_EQ(num_columns, static_cast<SQLSMALLINT>(expected_cols.size()));

  for (int i = 1; i <= num_columns; ++i) {
    SQLCHAR col_name[kBufferLength] = {0};
    SQLSMALLINT col_name_len = 0;
    SQLSMALLINT data_type = 0;
    SQLULEN col_size = 0;
    SQLSMALLINT dec_digits = 0;
    SQLSMALLINT nullable = 0;

    status = SQLDescribeCol(conn->hstmt, i, col_name, kBufferLength,
                            &col_name_len, &data_type, &col_size, &dec_digits,
                            &nullable);
    CheckError(status, "SQLDescribeCol[" + std::to_string(i) + "]", conn);

    EXPECT_EQ(std::string(reinterpret_cast<char*>(col_name)),
              expected_cols[i - 1].name);
    EXPECT_EQ(col_name_len, expected_cols[i - 1].name.size());
    EXPECT_TRUE(AreSqlAndBqTypesSame(data_type, expected_cols[i - 1].bq_type));
  }
}

}  // namespace

TEST(SQLDescribeColumn, ScriptQueryAfterPrepare) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);

  // SQLPrepare dry-run returns empty schema for SCRIPT; SQLExecute routes
  // through ExecuteScript -> GetAllQueryResults -> PopulateIrdIfEmpty.
  std::string script =
      "DECLARE int_val INT64 DEFAULT 42; "
      "SELECT int_val AS id, 'PowerBI_Test' AS label, "
      "CAST(123.45 AS FLOAT64) AS amount;";

  SQLRETURN status =
      SQLPrepare(conn->hstmt, (SQLCHAR*)script.c_str(), SQL_NTS);
  CheckError(status, "SQLPrepare", conn);

  status = SQLExecute(conn->hstmt);
  CheckError(status, "SQLExecute", conn);

  VerifyColumnDescriptions(
      conn, {{"id", "INT64"}, {"label", "STRING"}, {"amount", "FLOAT64"}});

  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLDescribeColumn, AsyncQueryDescribeAndFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str =
      kDefaultConnectionString + ";AllowHtapiForLargeResults=0;";
  EXPECT_EQ(Connect(conn_str, conn, true), SQL_SUCCESS);

  // Exceed BigQuery's 10s PostQuery timeout so job_complete is false and
  // FetchBQData -> FetchNextPageOfQueryResults populates the IRD.
  std::string script =
      "DECLARE now_ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP(); "
      "DECLARE wait_until TIMESTAMP DEFAULT "
      "TIMESTAMP_ADD(now_ts, INTERVAL 12 SECOND); "
      "WHILE now_ts < wait_until DO "
      "SET now_ts = CURRENT_TIMESTAMP(); "
      "END WHILE; "
      "SELECT 1 AS id, 'async_ok' AS label;";

  SQLRETURN status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)script.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  VerifyColumnDescriptions(conn, {{"id", "INT64"}, {"label", "STRING"}});

  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLDescribeColumn, AsyncQueryHtapiDescribeAndFetch) {
  auto conn = std::make_shared<ODBCHandles>();
  std::string conn_str = kDefaultConnectionString +
                         ";AllowHtapiForLargeResults=1;"
                         "HTAPI_ActivationThreshold=0;";
  EXPECT_EQ(Connect(conn_str, conn, true), SQL_SUCCESS);

  // Plain SELECT exceeding BigQuery's 10s PostQuery timeout (CURRENT_TIMESTAMP
  // disables query cache) so job_complete is false and FetchBQData routes
  // through FetchBQDataReadFromJob -> FetchBQDataReadArrow -> PopulateIrdIfEmpty.
  std::string query =
      "SELECT SUM(MOD(a * b, 7)) AS s, COUNT(*) AS n, "
      "'htapi_async_ok' AS label, CURRENT_TIMESTAMP() AS ts "
      "FROM UNNEST(GENERATE_ARRAY(1, 20000)) AS a "
      "CROSS JOIN UNNEST(GENERATE_ARRAY(1, 20000)) AS b";

  SQLRETURN status =
      SQLExecDirect(conn->hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
  CheckError(status, "SQLExecDirect", conn);

  VerifyColumnDescriptions(conn, {{"s", "INT64"},
                                  {"n", "INT64"},
                                  {"label", "STRING"},
                                  {"ts", "TIMESTAMP"}});

  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_SUCCESS);
  EXPECT_EQ(SQLFetch(conn->hstmt), SQL_NO_DATA);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

}  // namespace google::cloud::odbc_tests
