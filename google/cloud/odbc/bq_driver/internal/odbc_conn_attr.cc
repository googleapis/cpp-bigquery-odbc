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
       {"SQL_ATTR_ACCESS_MODE", ConnectionValidation::Either,
        SupportedAttribute::Both, (SQLPOINTER)SQL_MODE_READ_WRITE}},
      {SQL_ATTR_ASYNC_ENABLE,
       {"SQL_ATTR_ASYNC_ENABLE", ConnectionValidation::Either,
        SupportedAttribute::Both, (SQLPOINTER)SQL_ASYNC_ENABLE_OFF}},
      {SQL_ATTR_AUTOCOMMIT,
       {"SQL_ATTR_AUTOCOMMIT", ConnectionValidation::Either,
        SupportedAttribute::Both, (SQLPOINTER)SQL_AUTOCOMMIT_OFF}},
      {SQL_ATTR_CURRENT_CATALOG,
       {"SQL_ATTR_CURRENT_CATALOG", ConnectionValidation::Either,
        SupportedAttribute::Both, nullptr}},
      {SQL_ATTR_CONNECTION_TIMEOUT,
       {"SQL_ATTR_CONNECTION_TIMEOUT", ConnectionValidation::Either,
        SupportedAttribute::Both, (SQLPOINTER)0}},
      {SQL_ATTR_LOGIN_TIMEOUT,
       {"SQL_ATTR_LOGIN_TIMEOUT", ConnectionValidation::Before,
        SupportedAttribute::Both, (SQLPOINTER)0}},
      {SQL_ATTR_METADATA_ID,
       {"SQL_ATTR_METADATA_ID", ConnectionValidation::Either,
        SupportedAttribute::Both, (SQLPOINTER)SQL_FALSE}},
      {SQL_ATTR_TXN_ISOLATION,
       {"SQL_ATTR_TXN_ISOLATION", ConnectionValidation::Either,
        SupportedAttribute::Both, (SQLPOINTER)SQL_TXN_SERIALIZABLE}},
      {SQL_ATTR_PACKET_SIZE,
       {"SQL_ATTR_PACKET_SIZE", ConnectionValidation::Before,
        SupportedAttribute::Both, (SQLPOINTER)1024}},
      {SQL_ATTR_TRANSLATE_OPTION,
       {"SQL_ATTR_TRANSLATE_OPTION", ConnectionValidation::After,
        SupportedAttribute::Get, (SQLPOINTER)0}},
      {SQL_ATTR_TRANSLATE_LIB,
       {"SQL_ATTR_TRANSLATE_LIB", ConnectionValidation::After,
        SupportedAttribute::Get, nullptr}},
      {SQL_ATTR_CONNECTION_DEAD,
       {"SQL_ATTR_CONNECTION_DEAD", ConnectionValidation::After,
        SupportedAttribute::Get, (SQLPOINTER)SQL_CD_FALSE}},
      {SQL_ATTR_AUTO_IPD,
       {"SQL_ATTR_AUTO_IPD", ConnectionValidation::Either,
        SupportedAttribute::Get, (SQLPOINTER)SQL_TRUE}},
      {SQL_ATTR_TRACE,
       {"SQL_ATTR_TRACE", ConnectionValidation::Before,
        SupportedAttribute::Both, (SQLPOINTER)SQL_OPT_TRACE_OFF}},
      {SQL_ATTR_TRACEFILE,
       {"SQL_ATTR_TRACEFILE", ConnectionValidation::Before,
        SupportedAttribute::Both, nullptr}}};
  supported_connection_attribute_values = {
      {SQL_ATTR_ACCESS_MODE,
       {"SQL_ATTR_ACCESS_MODE",
        ConnectionValueType::SQL_U_INT,
        {(SQLPOINTER)SQL_MODE_READ_ONLY, (SQLPOINTER)SQL_MODE_READ_WRITE}}},
      {SQL_ATTR_ASYNC_ENABLE,
       {"SQL_ATTR_ASYNC_ENABLE",
        ConnectionValueType::SQL_U_LEN,
        {(SQLPOINTER)SQL_ASYNC_ENABLE_OFF, (SQLPOINTER)SQL_ASYNC_ENABLE_ON}}},
      {SQL_ATTR_AUTOCOMMIT,
       {"SQL_ATTR_AUTOCOMMIT",
        ConnectionValueType::SQL_U_INT,
        {(SQLPOINTER)SQL_AUTOCOMMIT_OFF, (SQLPOINTER)SQL_AUTOCOMMIT_ON}}},
      {SQL_ATTR_METADATA_ID,
       {"SQL_ATTR_METADATA_ID",
        ConnectionValueType::SQL_U_INT,
        {(SQLPOINTER)SQL_TRUE, (SQLPOINTER)SQL_FALSE}}},
      {SQL_ATTR_CURRENT_CATALOG,
       {"SQL_ATTR_CURRENT_CATALOG", ConnectionValueType::SQL_CHR, {}}},
      {SQL_ATTR_CONNECTION_TIMEOUT,
       {"SQL_ATTR_CONNECTION_TIMEOUT", ConnectionValueType::SQL_U_INT, {}}},
      {SQL_ATTR_LOGIN_TIMEOUT,
       {"SQL_ATTR_LOGIN_TIMEOUT", ConnectionValueType::SQL_U_INT, {}}},
      {SQL_ATTR_PACKET_SIZE,
       {"SQL_ATTR_PACKET_SIZE", ConnectionValueType::SQL_U_INT, {}}},
      {SQL_ATTR_TRACEFILE,
       {"SQL_ATTR_TRACEFILE", ConnectionValueType::SQL_CHR, {}}},
      {SQL_ATTR_TRACE,
       {"SQL_ATTR_TRACE",
        ConnectionValueType::SQL_U_INT,
        {(SQLPOINTER)SQL_OPT_TRACE_ON, (SQLPOINTER)SQL_OPT_TRACE_OFF}}},
      {SQL_ATTR_TRANSLATE_OPTION,
       {"SQL_ATTR_TRANSLATE_OPTION", ConnectionValueType::SQL_INT, {}}},
      {SQL_ATTR_TRANSLATE_LIB,
       {"SQL_ATTR_TRANSLATE_LIB", ConnectionValueType::SQL_CHR, {}}},
      {SQL_ATTR_CONNECTION_DEAD,
       {"SQL_ATTR_CONNECTION_DEAD", ConnectionValueType::SQL_U_INT, {}}},
      {SQL_ATTR_AUTO_IPD,
       {"SQL_ATTR_AUTO_IPD", ConnectionValueType::SQL_U_INT, {}}},
      {SQL_ATTR_TXN_ISOLATION,
       {"SQL_ATTR_TXN_ISOLATION",
        ConnectionValueType::SQL_INT_BITMASK,
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
  return (supported == SupportedAttribute::Both ||
          supported == SupportedAttribute::Get);
}

bool ConnectionAttr::IsSetAttributeSupported(SQLINTEGER attribute) {
  if (!IsAttributeSupported(attribute)) {
    return false;
  }
  auto attr_items = supported_connection_attributes.find(attribute);
  SupportedAttribute supported = std::get<2>(attr_items->second);
  return (supported == SupportedAttribute::Both ||
          supported == SupportedAttribute::Set);
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
    return ConnectionValidation::Invalid;
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
    return ConnectionValueType::SQL_Invalid;
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
