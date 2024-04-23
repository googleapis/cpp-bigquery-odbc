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

#include "google/cloud/odbc/bq_driver/odbc_sql_results.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"
#include "google/cloud/odbc/bq_driver/odbc_descriptor.h"
#include "google/cloud/odbc/bq_driver/odbc_diagnostics.h"
#include "google/cloud/odbc/bq_driver/odbc_statement.h"
#include "google/cloud/odbc/testing/bq_driver_utils/handles.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorHandle;
using google::cloud::odbc_bq_driver_internal::DescriptorType;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateConnectionHandle;
using google::cloud::odbc_testing_bq_driver_utils::CreateStatementHandle;

TEST(SQLBindColInternal, Basic) {
  std::cout << "CP 1:: " << std::endl;
  StatementHandle handle = CreateStatementHandle();

  std::cout << "CP 2:: " << std::endl;
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLBindColInternal(&handle, 1, SQL_C_FLOAT, buf, 20, &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATA_PTR, &out_buf, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(buf, out_buf);

  SQLSMALLINT out_c_type;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_TYPE, &out_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_C_FLOAT, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_CONCISE_TYPE,
                                   &out_concise_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_C_FLOAT, out_concise_c_type);

  SQLLEN out_octet_length;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH,
                                   &out_octet_length, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(20, out_octet_length);

  SQLPOINTER out_desc_ind_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_desc_ind_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_desc_ind_ptr);

  SQLPOINTER out_octet_length_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_octet_length_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_octet_length_ptr);
}

TEST(SQLBindColInternal, Type_SQL_C_TYPE_DATE) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status =
      SQLBindColInternal(&handle, 1, SQL_C_TYPE_DATE, buf, 20, &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATA_PTR, &out_buf, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(buf, out_buf);

  SQLSMALLINT out_c_type;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_TYPE, &out_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DATETIME, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_CONCISE_TYPE,
                                   &out_concise_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_DATETIME, out_concise_c_type);

  SQLSMALLINT out_datetime_interval_code;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
                                   &out_datetime_interval_code, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CODE_DATE, out_datetime_interval_code);

  SQLLEN out_octet_length;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH,
                                   &out_octet_length, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(20, out_octet_length);

  SQLPOINTER out_desc_ind_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_desc_ind_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_desc_ind_ptr);

  SQLPOINTER out_octet_length_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_octet_length_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_octet_length_ptr);
}

TEST(SQLBindColInternal, Type_SQL_C_INTERVAL_MONTH) {
  StatementHandle handle = CreateStatementHandle();
  SQLCHAR buf[20];
  SQLLEN target_str_len;
  SQLRETURN status = SQLBindColInternal(&handle, 1, SQL_C_INTERVAL_MONTH, buf,
                                        20, &target_str_len);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER ard = nullptr;
  status =
      SQLGetStmtAttrInternal(&handle, SQL_ATTR_APP_ROW_DESC, &ard, 0, nullptr);
  ASSERT_EQ(SQL_SUCCESS, status);

  SQLPOINTER out_buf;
  SQLINTEGER str_len = 0;
  status =
      SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATA_PTR, &out_buf, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(buf, out_buf);

  // SQLSMALLINT out_c_type;
  // status =
  //     SQLGetDescFieldInternal(ard, 1, SQL_DESC_TYPE, &out_c_type, 0,
  //     &str_len);
  // EXPECT_EQ(SQL_SUCCESS, status);
  // EXPECT_EQ(SQL_INTERVAL, out_c_type);

  SQLSMALLINT out_concise_c_type;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_CONCISE_TYPE,
                                   &out_concise_c_type, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_INTERVAL, out_concise_c_type);

  SQLSMALLINT out_datetime_interval_code;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_DATETIME_INTERVAL_CODE,
                                   &out_datetime_interval_code, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(SQL_CODE_MONTH, out_datetime_interval_code);

  SQLLEN out_octet_length;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_OCTET_LENGTH,
                                   &out_octet_length, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(20, out_octet_length);

  SQLPOINTER out_desc_ind_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_desc_ind_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_desc_ind_ptr);

  SQLPOINTER out_octet_length_ptr;
  status = SQLGetDescFieldInternal(ard, 1, SQL_DESC_INDICATOR_PTR,
                                   &out_octet_length_ptr, 0, &str_len);
  EXPECT_EQ(SQL_SUCCESS, status);
  EXPECT_EQ(&target_str_len, out_octet_length_ptr);
}

}  // namespace google::cloud::odbc_bq_driver
