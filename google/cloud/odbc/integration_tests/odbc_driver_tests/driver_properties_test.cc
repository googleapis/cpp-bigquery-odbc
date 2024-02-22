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

#include "google/cloud/odbc/bq_driver/internal/odbc_sql_type_info.h"
#include "google/cloud/odbc/testing/odbc_utils/connection.h"
#include "google/cloud/odbc/testing/odbc_utils/properties.h"

namespace google::cloud::odbc_tests {

#ifdef BQ_DRIVER_INTEGRATION_TESTS
bool const kIsBqDriver = true;
#else
bool const kIsBqDriver = false;
#endif

using ::google::cloud::odbc_bq_driver_internal::kSqlToBqDataTypes;
using ::google::cloud::odbc_bq_driver_internal::TypeInfoRow;

void CheckDataTypes(std::shared_ptr<ConnectionHandle> conn,
                    SQLSMALLINT in_data_type = SQL_ALL_TYPES,
                    bool is_supported = true) {
  SQLRETURN status = SQLGetTypeInfo(conn->hstmt, in_data_type);
  CheckError(status, "SQLGetTypeInfo", conn);

  SQLCHAR type_name[kBufferLength];
  SQLSMALLINT data_type;
  SQLINTEGER col_size;
  SQLCHAR literal_prefix[kBufferLength];
  SQLCHAR literal_suffix[kBufferLength];
  SQLCHAR create_params[kBufferLength];
  SQLSMALLINT nullable;
  SQLSMALLINT case_sensitive;
  SQLSMALLINT searchable;
  SQLSMALLINT unsigned_attribute;
  SQLSMALLINT fixed_prec_scale;
  SQLSMALLINT auto_unique_value;
  SQLCHAR local_type_name[kBufferLength];
  SQLSMALLINT minimum_scale;
  SQLSMALLINT maximum_scale;

  SQLSMALLINT sql_data_type;
  SQLSMALLINT sql_datetime_sub;
  SQLINTEGER num_prec_radix;
  SQLSMALLINT interval_precision;

  SQLLEN type_name_len;
  SQLLEN data_type_len;
  SQLLEN col_size_len;
  SQLLEN literal_prefix_len;
  SQLLEN literal_suffix_len;
  SQLLEN create_params_len;
  SQLLEN nullable_len;
  SQLLEN case_sensitive_len;
  SQLLEN searchable_len;
  SQLLEN unsigned_attribute_len;
  SQLLEN fixed_prec_scale_len;
  SQLLEN auto_unique_value_len;
  SQLLEN local_type_name_len;
  SQLLEN minimum_scale_len;
  SQLLEN maximum_scale_len;
  SQLLEN sql_data_type_len;
  SQLLEN sql_datetime_sub_len;
  SQLLEN num_prec_radix_len;
  SQLLEN interval_precision_len;

  status = SQLBindCol(conn->hstmt, 1, SQL_C_CHAR, (SQLPOINTER)type_name,
                      (SQLLEN)sizeof(type_name), &type_name_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 2, SQL_C_SHORT, (SQLPOINTER)&data_type,
                      (SQLLEN)sizeof(data_type), &data_type_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 3, SQL_C_LONG, (SQLPOINTER)&col_size,
                      (SQLLEN)sizeof(col_size), &col_size_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 4, SQL_C_CHAR, (SQLPOINTER)&literal_prefix,
                      (SQLLEN)sizeof(literal_prefix), &literal_prefix_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 5, SQL_C_CHAR, (SQLPOINTER)&literal_suffix,
                      (SQLLEN)sizeof(literal_suffix), &literal_suffix_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 6, SQL_C_CHAR, (SQLPOINTER)&create_params,
                      (SQLLEN)sizeof(create_params), &create_params_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 7, SQL_C_SHORT, (SQLPOINTER)&nullable,
                      (SQLLEN)sizeof(nullable), &nullable_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 8, SQL_C_SHORT, (SQLPOINTER)&case_sensitive,
                      (SQLLEN)sizeof(case_sensitive), &case_sensitive_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 9, SQL_C_SHORT, (SQLPOINTER)&searchable,
                      (SQLLEN)sizeof(searchable), &searchable_len);
  CheckError(status, "SQLBindCol", conn);

  status =
      SQLBindCol(conn->hstmt, 10, SQL_C_SHORT, (SQLPOINTER)&unsigned_attribute,
                 (SQLLEN)sizeof(unsigned_attribute), &unsigned_attribute_len);
  CheckError(status, "SQLBindCol", conn);

  status =
      SQLBindCol(conn->hstmt, 11, SQL_C_SHORT, (SQLPOINTER)&fixed_prec_scale,
                 (SQLLEN)sizeof(fixed_prec_scale), &fixed_prec_scale_len);
  CheckError(status, "SQLBindCol", conn);

  status =
      SQLBindCol(conn->hstmt, 12, SQL_C_SHORT, (SQLPOINTER)&auto_unique_value,
                 (SQLLEN)sizeof(auto_unique_value), &auto_unique_value_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 13, SQL_C_CHAR, (SQLPOINTER)&local_type_name,
                      (SQLLEN)sizeof(local_type_name), &local_type_name_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 14, SQL_C_SHORT, (SQLPOINTER)&minimum_scale,
                      (SQLLEN)sizeof(minimum_scale), &minimum_scale_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 15, SQL_C_SHORT, (SQLPOINTER)&maximum_scale,
                      (SQLLEN)sizeof(maximum_scale), &maximum_scale_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 16, SQL_C_SHORT, (SQLPOINTER)&sql_data_type,
                      (SQLLEN)sizeof(sql_data_type), &sql_data_type_len);
  CheckError(status, "SQLBindCol", conn);

  status =
      SQLBindCol(conn->hstmt, 17, SQL_C_SHORT, (SQLPOINTER)&sql_datetime_sub,
                 (SQLLEN)sizeof(sql_datetime_sub), &sql_datetime_sub_len);
  CheckError(status, "SQLBindCol", conn);

  status = SQLBindCol(conn->hstmt, 18, SQL_C_LONG, (SQLPOINTER)&num_prec_radix,
                      (SQLLEN)sizeof(num_prec_radix), &num_prec_radix_len);
  CheckError(status, "SQLBindCol", conn);

  status =
      SQLBindCol(conn->hstmt, 19, SQL_C_SHORT, (SQLPOINTER)&interval_precision,
                 (SQLLEN)sizeof(interval_precision), &interval_precision_len);
  CheckError(status, "SQLBindCol", conn);

  bool fetched_some_data = false;
  while (1) {
    status = SQLFetch(conn->hstmt);
    if (status == SQL_NO_DATA) {
      break;
    }
    CheckError(status, "SQLFetch", conn);
    fetched_some_data = true;

    std::string bq_data_type = (char*)type_name;

    if (in_data_type != SQL_ALL_TYPES) {
      // Only the rows corresponding to the input SQL data type must be returned
      // if the input to SQLGetTypeInfo is not SQL_ALL_TYPES
      ASSERT_EQ(data_type, in_data_type);
    }

    // Check if the SQL data_type exists in validation data
    ASSERT_TRUE(kSqlToBqDataTypes.count(data_type));
    // Check if the BQ data_type exists in validation data
    ASSERT_TRUE(kSqlToBqDataTypes.at(data_type).count(bq_data_type));
    TypeInfoRow validationData =
        kSqlToBqDataTypes.at(data_type).at(bq_data_type);

    EXPECT_STREQ((char const*)type_name, (char const*)validationData.type_name);
    EXPECT_EQ(data_type, validationData.data_type);
    EXPECT_EQ(col_size, validationData.col_size);

    if (validationData.literal_prefix &&
        // The Simba driver doesn't return some fields when the application
        // fetches info for a specific SQL data type. In that case, we want to
        // test only for the Google Driver
        (kIsBqDriver || in_data_type == SQL_ALL_TYPES)) {
      EXPECT_STREQ((char const*)literal_prefix,
                   (char const*)validationData.literal_prefix);
    }
    if (validationData.literal_suffix &&
        (kIsBqDriver || in_data_type == SQL_ALL_TYPES)) {
      EXPECT_STREQ((char const*)literal_suffix,
                   (char const*)validationData.literal_suffix);
    }
    if (validationData.create_params) {
      EXPECT_STREQ((char const*)create_params,
                   (char const*)validationData.create_params);
    }
    EXPECT_EQ(nullable, validationData.nullable);
    EXPECT_EQ(case_sensitive, validationData.case_sensitive);
    EXPECT_EQ(searchable, validationData.searchable);
    if (kIsBqDriver || in_data_type == SQL_ALL_TYPES) {
      EXPECT_EQ(unsigned_attribute, validationData.unsigned_attribute);
    }
    EXPECT_EQ(fixed_prec_scale, validationData.fixed_prec_scale);
    if (validationData.auto_unique_value) {
      EXPECT_STREQ((char const*)auto_unique_value,
                   (char const*)validationData.auto_unique_value);
    }
    EXPECT_STREQ((char const*)local_type_name,
                 (char const*)validationData.local_type_name);
    if (kIsBqDriver || in_data_type == SQL_ALL_TYPES) {
      EXPECT_EQ(minimum_scale, validationData.minimum_scale);
    }
    if (kIsBqDriver || in_data_type == SQL_ALL_TYPES) {
      EXPECT_EQ(maximum_scale, validationData.maximum_scale);
    }
    EXPECT_EQ(sql_data_type, validationData.sql_data_type);
    if (validationData.sql_datetime_sub &&
        (kIsBqDriver || in_data_type == SQL_ALL_TYPES)) {
      EXPECT_EQ(sql_datetime_sub, validationData.sql_datetime_sub);
    }
    if (kIsBqDriver || in_data_type == SQL_ALL_TYPES) {
      EXPECT_EQ(num_prec_radix, validationData.num_prec_radix);
    }
    if (validationData.interval_precision) {
      EXPECT_STREQ((char const*)interval_precision,
                   (char const*)validationData.interval_precision);
    }
  }
  // SQLFetch should return some rows for the SQL data types that can be mapped
  // to a BQ data type
  EXPECT_EQ(fetched_some_data, is_supported);
}

// This preprocessor flag is used to disable tests for unimplemented bq_driver
// ODBC APIs
#ifndef BQ_DRIVER_INTEGRATION_TESTS

TEST(DriverPropertiesTest, SQLGetFunctions) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  EXPECT_EQ(GetAllFunctions(conn), SQL_SUCCESS);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, all_datatypes) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_ALL_TYPES);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_BIGINT) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_BIGINT, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_BIT) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_BIT, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_TYPE_DATE) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_DATE, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_DOUBLE) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_DOUBLE, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_TYPE_TIME) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_TIME, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_TYPE_TIMESTAMP) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_TYPE_TIMESTAMP, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_VARBINARY) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_VARBINARY, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_VARCHAR) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_VARCHAR, true);
  // CheckDataTypes(conn, SQL_C_SBIGINT);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Supported_SQL_NUMERIC) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_NUMERIC, true);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_CHAR) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_CHAR, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_DECIMAL) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_DECIMAL, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_INTEGER) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_INTEGER, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_SMALLINT) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_SMALLINT, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_FLOAT) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_FLOAT, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

TEST(SQLGetTypeInfoTest, Unsupported_SQL_REAL) {
  auto conn = std::make_shared<ConnectionHandle>();
  EXPECT_EQ(Connect(kDefaultConnectionString, conn), SQL_SUCCESS);
  CheckDataTypes(conn, SQL_REAL, false);
  EXPECT_EQ(Disconnect(conn), SQL_SUCCESS);
}

#endif  // BQ_DRIVER_INTEGRATION_TESTS

}  // namespace google::cloud::odbc_tests
