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

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_attr.h"
#include "google/cloud/odbc/testing/utils/status_matchers.h"
#include <gtest/gtest.h>

/*
  std::string GetAttributeStringValue(SQLINTEGER attribute);
  ConnectionSetAttribute GetAttributeConnectionBehavior(SQLINTEGER attribute);
  ConnectionValueType GetAttributeValueType(SQLINTEGER attribute);
  std::vector<SQLPOINTER> GetAttributePossibleValues(SQLINTEGER attribute);
*/

namespace google::cloud::odbc_bq_driver_internal {

using ::testing::HasSubstr;

TEST(ConnectionAttributeTest, IsAttributeSupported) {
  ConnectionAttr conn_attr;

  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_ACCESS_MODE));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_ASYNC_ENABLE));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_AUTOCOMMIT));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_CURRENT_CATALOG));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_CONNECTION_TIMEOUT));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_LOGIN_TIMEOUT));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_METADATA_ID));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_TXN_ISOLATION));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_PACKET_SIZE));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_TRANSLATE_OPTION));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_TRANSLATE_LIB));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_CONNECTION_DEAD));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_AUTO_IPD));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_TRACE));
  EXPECT_TRUE(conn_attr.IsAttributeSupported(SQL_ATTR_TRACEFILE));
}

TEST(ConnectionAttributeTest, AttributeNotSupported) {
  ConnectionAttr conn_attr;

  EXPECT_FALSE(conn_attr.IsAttributeSupported(SQL_ATTR_ODBC_CURSORS));
  EXPECT_FALSE(conn_attr.IsAttributeSupported(SQL_ATTR_ENLIST_IN_DTC));
}

TEST(ConnectionAttributeTest, IsGetAttributeSupported) {
  ConnectionAttr conn_attr;

  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_ACCESS_MODE));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_ASYNC_ENABLE));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_AUTOCOMMIT));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_CURRENT_CATALOG));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_CONNECTION_TIMEOUT));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_LOGIN_TIMEOUT));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_METADATA_ID));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_TXN_ISOLATION));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_PACKET_SIZE));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_TRANSLATE_OPTION));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_TRANSLATE_LIB));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_CONNECTION_DEAD));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_AUTO_IPD));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_TRACE));
  EXPECT_TRUE(conn_attr.IsGetAttributeSupported(SQL_ATTR_TRACEFILE));
}

TEST(ConnectionAttributeTest, IsSetAttributeSupported) {
  ConnectionAttr conn_attr;

  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_ACCESS_MODE));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_ASYNC_ENABLE));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_AUTOCOMMIT));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_CURRENT_CATALOG));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_CONNECTION_TIMEOUT));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_LOGIN_TIMEOUT));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_METADATA_ID));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_TXN_ISOLATION));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_PACKET_SIZE));
  EXPECT_FALSE(conn_attr.IsSetAttributeSupported(SQL_ATTR_TRANSLATE_OPTION));
  EXPECT_FALSE(conn_attr.IsSetAttributeSupported(SQL_ATTR_TRANSLATE_LIB));
  EXPECT_FALSE(conn_attr.IsSetAttributeSupported(SQL_ATTR_CONNECTION_DEAD));
  EXPECT_FALSE(conn_attr.IsSetAttributeSupported(SQL_ATTR_AUTO_IPD));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_TRACE));
  EXPECT_TRUE(conn_attr.IsSetAttributeSupported(SQL_ATTR_TRACEFILE));
}

TEST(ConnectionAttributeTest, GetAttributeStringValue) {
  ConnectionAttr conn_attr;

  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_ACCESS_MODE),
            "SQL_ATTR_ACCESS_MODE");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_ASYNC_ENABLE),
            "SQL_ATTR_ASYNC_ENABLE");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_AUTOCOMMIT),
            "SQL_ATTR_AUTOCOMMIT");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_CURRENT_CATALOG),
            "SQL_ATTR_CURRENT_CATALOG");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_CONNECTION_TIMEOUT),
            "SQL_ATTR_CONNECTION_TIMEOUT");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_LOGIN_TIMEOUT),
            "SQL_ATTR_LOGIN_TIMEOUT");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_METADATA_ID),
            "SQL_ATTR_METADATA_ID");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_TXN_ISOLATION),
            "SQL_ATTR_TXN_ISOLATION");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_PACKET_SIZE),
            "SQL_ATTR_PACKET_SIZE");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_TRANSLATE_OPTION),
            "SQL_ATTR_TRANSLATE_OPTION");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_TRANSLATE_LIB),
            "SQL_ATTR_TRANSLATE_LIB");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_CONNECTION_DEAD),
            "SQL_ATTR_CONNECTION_DEAD");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_AUTO_IPD),
            "SQL_ATTR_AUTO_IPD");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_TRACE),
            "SQL_ATTR_TRACE");
  EXPECT_EQ(conn_attr.GetAttributeStringValue(SQL_ATTR_TRACEFILE),
            "SQL_ATTR_TRACEFILE");
}

TEST(ConnectionAttributeTest, GetAttributeConnectionBehavior) {
  ConnectionAttr conn_attr;

  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_ACCESS_MODE),
            ConnectionValidation::kEither);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_ASYNC_ENABLE),
            ConnectionValidation::kEither);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_AUTOCOMMIT),
            ConnectionValidation::kEither);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_CURRENT_CATALOG),
            ConnectionValidation::kEither);
  EXPECT_EQ(
      conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_CONNECTION_TIMEOUT),
      ConnectionValidation::kEither);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_LOGIN_TIMEOUT),
            ConnectionValidation::kBefore);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_METADATA_ID),
            ConnectionValidation::kEither);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_TXN_ISOLATION),
            ConnectionValidation::kEither);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_PACKET_SIZE),
            ConnectionValidation::kBefore);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_TRANSLATE_OPTION),
            ConnectionValidation::kAfter);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_TRANSLATE_LIB),
            ConnectionValidation::kAfter);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_CONNECTION_DEAD),
            ConnectionValidation::kAfter);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_AUTO_IPD),
            ConnectionValidation::kEither);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_TRACE),
            ConnectionValidation::kBefore);
  EXPECT_EQ(conn_attr.GetAttributeConnectionBehavior(SQL_ATTR_TRACEFILE),
            ConnectionValidation::kBefore);
}

TEST(ConnectionAttributeTest, GetAttributeValueType) {
  ConnectionAttr conn_attr;

  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_ACCESS_MODE),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_ASYNC_ENABLE),
            ConnectionValueType::kSqlULen);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_AUTOCOMMIT),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_CURRENT_CATALOG),
            ConnectionValueType::kSqlChr);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_CONNECTION_TIMEOUT),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_LOGIN_TIMEOUT),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_METADATA_ID),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_TXN_ISOLATION),
            ConnectionValueType::kSqlIntBitmask);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_PACKET_SIZE),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_TRANSLATE_OPTION),
            ConnectionValueType::kSqlInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_TRANSLATE_LIB),
            ConnectionValueType::kSqlChr);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_CONNECTION_DEAD),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_AUTO_IPD),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_TRACE),
            ConnectionValueType::kSqlUInt);
  EXPECT_EQ(conn_attr.GetAttributeValueType(SQL_ATTR_TRACEFILE),
            ConnectionValueType::kSqlChr);
}

TEST(ConnectionAttributeTest, GetAttributeDefaultValue) {
  ConnectionAttr conn_attr;

  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_ACCESS_MODE),
            (SQLPOINTER)SQL_MODE_READ_WRITE);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_ASYNC_ENABLE),
            (SQLPOINTER)SQL_ASYNC_ENABLE_OFF);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_AUTOCOMMIT),
            (SQLPOINTER)SQL_AUTOCOMMIT_OFF);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_CURRENT_CATALOG),
            nullptr);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_CONNECTION_TIMEOUT),
            (SQLPOINTER)0);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_LOGIN_TIMEOUT),
            (SQLPOINTER)0);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_METADATA_ID),
            (SQLPOINTER)SQL_FALSE);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_TXN_ISOLATION),
            (SQLPOINTER)SQL_TXN_SERIALIZABLE);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_PACKET_SIZE),
            (SQLPOINTER)1024);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_TRANSLATE_OPTION),
            (SQLPOINTER)0);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_TRANSLATE_LIB),
            nullptr);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_CONNECTION_DEAD),
            (SQLPOINTER)SQL_CD_FALSE);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_AUTO_IPD),
            (SQLPOINTER)SQL_TRUE);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_TRACE),
            (SQLPOINTER)SQL_OPT_TRACE_OFF);
  EXPECT_EQ(conn_attr.GetAttributeDefaultValue(SQL_ATTR_TRACEFILE), nullptr);
}

}  // namespace google::cloud::odbc_bq_driver_internal
