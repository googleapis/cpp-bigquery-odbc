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
#include "google/cloud/odbc/testing/odbc_utils/properties.h"

namespace google::cloud::odbc_tests {

void CheckDataTypes(std::shared_ptr<ODBCHandles> conn,
                    SQLSMALLINT in_data_type = SQL_ALL_TYPES,
                    bool is_supported = true, bool use_ansi = false,
                    SQLLEN bind_offset = 0) {
  SQLRETURN status;

  // Set row bind offset pointer
  status = SQLSetStmtAttr(conn->hstmt, SQL_ATTR_ROW_BIND_OFFSET_PTR,
                          &bind_offset, 0);
  CheckError(status, "SQLSetStmtAttr", conn, use_ansi);

  // Call SQLGetTypeInfo (ANSI or Unicode)
  status = use_ansi ? SQLGetTypeInfoA(conn->hstmt, in_data_type)
                    : SQLGetTypeInfo(conn->hstmt, in_data_type);
  CheckError(status, "SQLGetTypeInfo", conn, use_ansi);

  // Define a struct to hold all columns and lengths
  struct TypeInfoColumns {
    SQLCHAR type_name[kBufferLength] = {};
    SQLSMALLINT data_type = 0;
    SQLINTEGER col_size = 0;
    SQLCHAR literal_prefix[kBufferLength] = {};
    SQLCHAR literal_suffix[kBufferLength] = {};
    SQLCHAR create_params[kBufferLength] = {};
    SQLSMALLINT nullable = 0;
    SQLSMALLINT case_sensitive = 0;
    SQLSMALLINT searchable = 0;
    SQLSMALLINT unsigned_attribute = 0;
    SQLSMALLINT fixed_prec_scale = 0;
    SQLSMALLINT auto_unique_value = 0;
    SQLCHAR local_type_name[kBufferLength] = {};
    SQLSMALLINT minimum_scale = 0;
    SQLSMALLINT maximum_scale = 0;
    SQLSMALLINT sql_data_type = 0;
    SQLSMALLINT sql_datetime_sub = 0;
    SQLINTEGER num_prec_radix = 0;
    SQLSMALLINT interval_precision = 0;

    // Corresponding length indicators
    SQLLEN type_name_len = 0;
    SQLLEN data_type_len = 0;
    SQLLEN col_size_len = 0;
    SQLLEN literal_prefix_len = 0;
    SQLLEN literal_suffix_len = 0;
    SQLLEN create_params_len = 0;
    SQLLEN nullable_len = 0;
    SQLLEN case_sensitive_len = 0;
    SQLLEN searchable_len = 0;
    SQLLEN unsigned_attribute_len = 0;
    SQLLEN fixed_prec_scale_len = 0;
    SQLLEN auto_unique_value_len = 0;
    SQLLEN local_type_name_len = 0;
    SQLLEN minimum_scale_len = 0;
    SQLLEN maximum_scale_len = 0;
    SQLLEN sql_data_type_len = 0;
    SQLLEN sql_datetime_sub_len = 0;
    SQLLEN num_prec_radix_len = 0;
    SQLLEN interval_precision_len = 0;
  } cols;

  // Helper lambda to bind a single column and check error
  auto bind_col = [&](SQLUSMALLINT col_num, SQLSMALLINT c_type, void* buffer,
                      SQLLEN buffer_len, SQLLEN* len_ind, char const* desc) {
    SQLRETURN s = SQLBindCol(conn->hstmt, col_num, c_type,
                             reinterpret_cast<char*>(buffer) - bind_offset,
                             buffer_len, len_ind);
    CheckError(s, desc, conn);
  };

  // Bind all columns - note: bind_offset adjustment for pointer arithmetic
  bind_col(1, SQL_C_CHAR, &cols.type_name, sizeof(cols.type_name),
           &cols.type_name_len, "SQLBindCol(type_name)");
  bind_col(2, SQL_C_SSHORT, &cols.data_type, sizeof(cols.data_type),
           &cols.data_type_len, "SQLBindCol(data_type)");
  bind_col(3, SQL_C_SLONG, &cols.col_size, sizeof(cols.col_size),
           &cols.col_size_len, "SQLBindCol(col_size)");
  bind_col(4, SQL_C_CHAR, &cols.literal_prefix, sizeof(cols.literal_prefix),
           &cols.literal_prefix_len, "SQLBindCol(literal_prefix)");
  bind_col(5, SQL_C_CHAR, &cols.literal_suffix, sizeof(cols.literal_suffix),
           &cols.literal_suffix_len, "SQLBindCol(literal_suffix)");
  bind_col(6, SQL_C_CHAR, &cols.create_params, sizeof(cols.create_params),
           &cols.create_params_len, "SQLBindCol(create_params)");
  bind_col(7, SQL_C_SSHORT, &cols.nullable, sizeof(cols.nullable),
           &cols.nullable_len, "SQLBindCol(nullable)");
  bind_col(8, SQL_C_SSHORT, &cols.case_sensitive, sizeof(cols.case_sensitive),
           &cols.case_sensitive_len, "SQLBindCol(case_sensitive)");
  bind_col(9, SQL_C_SSHORT, &cols.searchable, sizeof(cols.searchable),
           &cols.searchable_len, "SQLBindCol(searchable)");
  bind_col(10, SQL_C_SSHORT, &cols.unsigned_attribute,
           sizeof(cols.unsigned_attribute), &cols.unsigned_attribute_len,
           "SQLBindCol(unsigned_attribute)");
  bind_col(11, SQL_C_SSHORT, &cols.fixed_prec_scale,
           sizeof(cols.fixed_prec_scale), &cols.fixed_prec_scale_len,
           "SQLBindCol(fixed_prec_scale)");
  bind_col(12, SQL_C_SSHORT, &cols.auto_unique_value,
           sizeof(cols.auto_unique_value), &cols.auto_unique_value_len,
           "SQLBindCol(auto_unique_value)");
  bind_col(13, SQL_C_CHAR, &cols.local_type_name, sizeof(cols.local_type_name),
           &cols.local_type_name_len, "SQLBindCol(local_type_name)");
  bind_col(14, SQL_C_SSHORT, &cols.minimum_scale, sizeof(cols.minimum_scale),
           &cols.minimum_scale_len, "SQLBindCol(minimum_scale)");
  bind_col(15, SQL_C_SSHORT, &cols.maximum_scale, sizeof(cols.maximum_scale),
           &cols.maximum_scale_len, "SQLBindCol(maximum_scale)");
  bind_col(16, SQL_C_SSHORT, &cols.sql_data_type, sizeof(cols.sql_data_type),
           &cols.sql_data_type_len, "SQLBindCol(sql_data_type)");
  bind_col(17, SQL_C_SSHORT, &cols.sql_datetime_sub,
           sizeof(cols.sql_datetime_sub), &cols.sql_datetime_sub_len,
           "SQLBindCol(sql_datetime_sub)");
  bind_col(18, SQL_C_SLONG, &cols.num_prec_radix,
           sizeof(cols.num_prec_radix_len), &cols.num_prec_radix_len,
           "SQLBindCol(num_prec_radix)");
  bind_col(19, SQL_C_SSHORT, &cols.interval_precision,
           sizeof(cols.interval_precision), &cols.interval_precision_len,
           "SQLBindCol(interval_precision)");

  bool fetched_some_data = false;

  while (true) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    CheckError(status, "SQLFetch", conn);

    fetched_some_data = true;
    std::string bq_data_type(reinterpret_cast<char*>(cols.type_name));

    if (in_data_type != SQL_ALL_TYPES) {
      ASSERT_EQ(cols.data_type, in_data_type);
    }
    if (bq_data_type != "RANGE") {
      ASSERT_TRUE(kSqlToBqDataTypes.count(cols.data_type));
      ASSERT_TRUE(kSqlToBqDataTypes.at(cols.data_type).count(bq_data_type));

      TypeInfoRow const& validationData =
          kSqlToBqDataTypes.at(cols.data_type).at(bq_data_type);

      EXPECT_STREQ(reinterpret_cast<char const*>(cols.type_name),
                   reinterpret_cast<char const*>(validationData.type_name));
      EXPECT_EQ(cols.data_type, validationData.data_type);
      EXPECT_EQ(cols.col_size, validationData.col_size);

      if (validationData.literal_prefix &&
          (kIsBqDriver || in_data_type == SQL_ALL_TYPES)) {
        EXPECT_STREQ(
            reinterpret_cast<char const*>(cols.literal_prefix),
            reinterpret_cast<char const*>(validationData.literal_prefix));
      }
      if (validationData.literal_suffix &&
          (kIsBqDriver || in_data_type == SQL_ALL_TYPES)) {
        EXPECT_STREQ(
            reinterpret_cast<char const*>(cols.literal_suffix),
            reinterpret_cast<char const*>(validationData.literal_suffix));
      }
      if (validationData.create_params) {
        EXPECT_STREQ(
            reinterpret_cast<char const*>(cols.create_params),
            reinterpret_cast<char const*>(validationData.create_params));
      }
      EXPECT_EQ(cols.nullable, validationData.nullable);
      EXPECT_EQ(cols.case_sensitive, validationData.case_sensitive);
      EXPECT_EQ(cols.searchable, validationData.searchable);

      if (kIsBqDriver || in_data_type == SQL_ALL_TYPES) {
        EXPECT_EQ(cols.unsigned_attribute, validationData.unsigned_attribute);
      }
      EXPECT_EQ(cols.fixed_prec_scale, validationData.fixed_prec_scale);
      if (validationData.auto_unique_value) {
        EXPECT_STREQ(
            reinterpret_cast<char const*>(cols.auto_unique_value),
            reinterpret_cast<char const*>(validationData.auto_unique_value));
      }
      EXPECT_STREQ(
          reinterpret_cast<char const*>(cols.local_type_name),
          reinterpret_cast<char const*>(validationData.local_type_name));

      if (kIsBqDriver || in_data_type == SQL_ALL_TYPES) {
        EXPECT_EQ(cols.minimum_scale, validationData.minimum_scale);
        EXPECT_EQ(cols.maximum_scale, validationData.maximum_scale);
        EXPECT_EQ(cols.num_prec_radix, validationData.num_prec_radix);
      }
      EXPECT_EQ(cols.sql_data_type, validationData.sql_data_type);
      if (validationData.sql_datetime_sub &&
          (kIsBqDriver || in_data_type == SQL_ALL_TYPES)) {
        EXPECT_EQ(cols.sql_datetime_sub, validationData.sql_datetime_sub);
      }
      if (validationData.interval_precision) {
        EXPECT_STREQ(
            reinterpret_cast<char const*>(cols.interval_precision),
            reinterpret_cast<char const*>(validationData.interval_precision));
      }
    }
  }

  EXPECT_EQ(fetched_some_data, is_supported);
}

TEST(SQLGetTypeInfoTest, all_datatypes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_ALL_TYPES);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#ifndef _WIN32
// On Windows, this test times-out for the existing driver and our driver
// CheckDataTypes function times out at the last statement and flow doesn't
// reach this TEST
TEST(SQLGetTypeInfoTest, all_datatypes_with_offset) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_ALL_TYPES, true, false, 7);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
#endif  // defined(BQ_DRIVER_INTEGRATION_TESTS) || !defined(_WIN32)

TEST(SQLGetTypeInfoTestAnsi, all_datatypes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_ALL_TYPES, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTestWide, all_datatypes) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  SQLRETURN status = SQLGetTypeInfoW(conn->hstmt, SQL_ALL_TYPES);
  CheckError(status, "SQLGetTypeInfo", conn);

  SQLCHAR type_name[kBufferLength];
  SQLLEN type_name_len;

  status = SQLBindCol(conn->hstmt, 1, SQL_C_CHAR, (SQLPOINTER)type_name,
                      (SQLLEN)sizeof(type_name), &type_name_len);
  CheckError(status, "SQLBindCol(SQL_C_CHAR)", conn);

  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_BIGINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_BIGINT, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Supported_SQL_BIGINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_BIGINT, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_BIT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_BIT, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTesAnsit, Supported_SQL_BIT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_BIT, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_DATE, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Supported_SQL_TYPE_DATE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_DATE, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_DOUBLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_DOUBLE, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Supported_SQL_DOUBLE) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_DOUBLE, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_TIME, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Supported_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_TIME, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_TIMESTAMP, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestANsi, Supported_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_TIMESTAMP, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_VARBINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_VARBINARY, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Supported_SQL_VARBINARY) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_VARBINARY, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_VARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_VARCHAR, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Supported_SQL_VARCHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_VARCHAR, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_NUMERIC, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Supported_SQL_NUMERIC) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_NUMERIC, true, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_CHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_CHAR, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Unsupported_SQL_CHAR) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_CHAR, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_DECIMAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_DECIMAL, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Unsupported_SQL_DECIMAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_DECIMAL, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_INTEGER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_INTEGER, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Unsupported_SQL_INTEGER) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_INTEGER, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_SMALLINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_SMALLINT, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Unsupported_SQL_SMALLINT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_SMALLINT, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_FLOAT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_FLOAT, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Unsupported_SQL_FLOAT) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_FLOAT, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_REAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_REAL, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
TEST(SQLGetTypeInfoTestAnsi, Unsupported_SQL_REAL) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn, true), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_REAL, false, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

// No ANSI version for SQLGetFunctions
TEST(DriverPropertiesTest, SQLGetFunctions) {
  auto conn = std::make_shared<ODBCHandles>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetAllFunctions(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}
}  // namespace google::cloud::odbc_tests
