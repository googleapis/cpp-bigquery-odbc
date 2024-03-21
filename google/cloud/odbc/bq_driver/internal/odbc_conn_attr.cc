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

namespace google::cloud::odbc_bq_driver_internal {

ConnectionAttr::ConnectionAttr() {
  supported_connection_attributes = {
      {SQL_ATTR_ACCESS_MODE,
       {"SQL_ATTR_ACCESS_MODE", ConnectionValidation::kEither,
        SupportedAttribute::kBoth, (SQLPOINTER)SQL_MODE_READ_WRITE}},
      {SQL_ATTR_ASYNC_ENABLE,
       {"SQL_ATTR_ASYNC_ENABLE", ConnectionValidation::kEither,
        SupportedAttribute::kBoth, (SQLPOINTER)SQL_ASYNC_ENABLE_OFF}},
      {SQL_ATTR_AUTOCOMMIT,
       {"SQL_ATTR_AUTOCOMMIT", ConnectionValidation::kEither,
        SupportedAttribute::kBoth, (SQLPOINTER)SQL_AUTOCOMMIT_OFF}},
      {SQL_ATTR_CURRENT_CATALOG,
       {"SQL_ATTR_CURRENT_CATALOG", ConnectionValidation::kEither,
        SupportedAttribute::kBoth, nullptr}},
      {SQL_ATTR_CONNECTION_TIMEOUT,
       {"SQL_ATTR_CONNECTION_TIMEOUT", ConnectionValidation::kEither,
        SupportedAttribute::kBoth, (SQLPOINTER)0}},
      {SQL_ATTR_LOGIN_TIMEOUT,
       {"SQL_ATTR_LOGIN_TIMEOUT", ConnectionValidation::kBefore,
        SupportedAttribute::kBoth, (SQLPOINTER)0}},
      {SQL_ATTR_METADATA_ID,
       {"SQL_ATTR_METADATA_ID", ConnectionValidation::kEither,
        SupportedAttribute::kBoth, (SQLPOINTER)SQL_FALSE}},
      {SQL_ATTR_TXN_ISOLATION,
       {"SQL_ATTR_TXN_ISOLATION", ConnectionValidation::kEither,
        SupportedAttribute::kBoth, (SQLPOINTER)SQL_TXN_SERIALIZABLE}},
      {SQL_ATTR_PACKET_SIZE,
       {"SQL_ATTR_PACKET_SIZE", ConnectionValidation::kBefore,
        SupportedAttribute::kBoth, (SQLPOINTER)1024}},
      {SQL_ATTR_TRANSLATE_OPTION,
       {"SQL_ATTR_TRANSLATE_OPTION", ConnectionValidation::kAfter,
        SupportedAttribute::kGet, (SQLPOINTER)0}},
      {SQL_ATTR_TRANSLATE_LIB,
       {"SQL_ATTR_TRANSLATE_LIB", ConnectionValidation::kAfter,
        SupportedAttribute::kGet, nullptr}},
      {SQL_ATTR_CONNECTION_DEAD,
       {"SQL_ATTR_CONNECTION_DEAD", ConnectionValidation::kAfter,
        SupportedAttribute::kGet, (SQLPOINTER)SQL_CD_FALSE}},
      {SQL_ATTR_AUTO_IPD,
       {"SQL_ATTR_AUTO_IPD", ConnectionValidation::kEither,
        SupportedAttribute::kGet, (SQLPOINTER)SQL_TRUE}},
      {SQL_ATTR_TRACE,
       {"SQL_ATTR_TRACE", ConnectionValidation::kBefore,
        SupportedAttribute::kBoth, (SQLPOINTER)SQL_OPT_TRACE_OFF}},
      {SQL_ATTR_TRACEFILE,
       {"SQL_ATTR_TRACEFILE", ConnectionValidation::kBefore,
        SupportedAttribute::kBoth, nullptr}}};
  supported_connection_attribute_values = {
      {SQL_ATTR_ACCESS_MODE,
       {"SQL_ATTR_ACCESS_MODE",
        ConnectionValueType::kSqlUInt,
        {(SQLPOINTER)SQL_MODE_READ_ONLY, (SQLPOINTER)SQL_MODE_READ_WRITE}}},
      {SQL_ATTR_ASYNC_ENABLE,
       {"SQL_ATTR_ASYNC_ENABLE",
        ConnectionValueType::kSqlULen,
        {(SQLPOINTER)SQL_ASYNC_ENABLE_OFF, (SQLPOINTER)SQL_ASYNC_ENABLE_ON}}},
      {SQL_ATTR_AUTOCOMMIT,
       {"SQL_ATTR_AUTOCOMMIT",
        ConnectionValueType::kSqlUInt,
        {(SQLPOINTER)SQL_AUTOCOMMIT_OFF, (SQLPOINTER)SQL_AUTOCOMMIT_ON}}},
      {SQL_ATTR_METADATA_ID,
       {"SQL_ATTR_METADATA_ID",
        ConnectionValueType::kSqlUInt,
        {(SQLPOINTER)SQL_TRUE, (SQLPOINTER)SQL_FALSE}}},
      {SQL_ATTR_CURRENT_CATALOG,
       {"SQL_ATTR_CURRENT_CATALOG", ConnectionValueType::kSqlChr, {}}},
      {SQL_ATTR_CONNECTION_TIMEOUT,
       {"SQL_ATTR_CONNECTION_TIMEOUT", ConnectionValueType::kSqlUInt, {}}},
      {SQL_ATTR_LOGIN_TIMEOUT,
       {"SQL_ATTR_LOGIN_TIMEOUT", ConnectionValueType::kSqlUInt, {}}},
      {SQL_ATTR_PACKET_SIZE,
       {"SQL_ATTR_PACKET_SIZE", ConnectionValueType::kSqlUInt, {}}},
      {SQL_ATTR_TRACEFILE,
       {"SQL_ATTR_TRACEFILE", ConnectionValueType::kSqlChr, {}}},
      {SQL_ATTR_TRACE,
       {"SQL_ATTR_TRACE",
        ConnectionValueType::kSqlUInt,
        {(SQLPOINTER)SQL_OPT_TRACE_ON, (SQLPOINTER)SQL_OPT_TRACE_OFF}}},
      {SQL_ATTR_TRANSLATE_OPTION,
       {"SQL_ATTR_TRANSLATE_OPTION", ConnectionValueType::kSqlInt, {}}},
      {SQL_ATTR_TRANSLATE_LIB,
       {"SQL_ATTR_TRANSLATE_LIB", ConnectionValueType::kSqlChr, {}}},
      {SQL_ATTR_CONNECTION_DEAD,
       {"SQL_ATTR_CONNECTION_DEAD", ConnectionValueType::kSqlUInt, {}}},
      {SQL_ATTR_AUTO_IPD,
       {"SQL_ATTR_AUTO_IPD", ConnectionValueType::kSqlUInt, {}}},
      {SQL_ATTR_TXN_ISOLATION,
       {"SQL_ATTR_TXN_ISOLATION",
        ConnectionValueType::kSqlIntBitmask,
        {(SQLPOINTER)SQL_TXN_READ_UNCOMMITTED,
         (SQLPOINTER)SQL_TXN_READ_COMMITTED,
         (SQLPOINTER)SQL_TXN_REPEATABLE_READ,
         (SQLPOINTER)SQL_TXN_SERIALIZABLE}}}};
}

bool ConnectionAttr::IsAttributeSupported(SQLINTEGER attribute) {
  return (supported_connection_attributes.find(attribute) !=
          supported_connection_attributes.end());
}

bool ConnectionAttr::IsGetAttributeSupported(SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return false;
  }
  auto attr_items = supported_connection_attributes.find(attribute);
  SupportedAttribute supported = std::get<2>(attr_items->second);
  return (supported == SupportedAttribute::kBoth ||
          supported == SupportedAttribute::kGet);
}

bool ConnectionAttr::IsSetAttributeSupported(SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return false;
  }
  auto attr_items = supported_connection_attributes.find(attribute);
  SupportedAttribute supported = std::get<2>(attr_items->second);
  return (supported == SupportedAttribute::kBoth ||
          supported == SupportedAttribute::kSet);
}

std::string ConnectionAttr::GetAttributeStringValue(SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return "";
  }
  auto attr_items = supported_connection_attributes.find(attribute);
  return std::get<0>(attr_items->second);
}

ConnectionValidation ConnectionAttr::GetAttributeConnectionBehavior(
    SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return ConnectionValidation::kInvalid;
  }
  auto attr_items = supported_connection_attributes.find(attribute);
  return std::get<1>(attr_items->second);
}

SQLPOINTER ConnectionAttr::GetAttributeDefaultValue(SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return nullptr;
  }
  auto attr_items = supported_connection_attributes.find(attribute);
  return std::get<3>(attr_items->second);
}

ConnectionValueType ConnectionAttr::GetAttributeValueType(
    SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return ConnectionValueType::kSqlInvalid;
  }
  auto attr_items = supported_connection_attribute_values.find(attribute);
  return std::get<1>(attr_items->second);
}

std::vector<SQLPOINTER> ConnectionAttr::GetAttributePossibleValues(
    SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return {};
  }
  auto attr_items = supported_connection_attribute_values.find(attribute);
  return std::get<2>(attr_items->second);
}

}  // namespace google::cloud::odbc_bq_driver_internal
