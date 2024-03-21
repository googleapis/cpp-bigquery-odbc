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

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_client_interface/odbc_authentication.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

namespace {
bool IsAttributeValueValid(std::vector<SQLPOINTER> possible_values,
                           SQLPOINTER value) {
  if (possible_values.empty()) {
    return true;
  }
  for (auto val : possible_values) {
    auto expected_val = reinterpret_cast<std::size_t>(val);
    auto actual_val = reinterpret_cast<std::size_t>(value);
    if (expected_val == actual_val) {
      return true;
    }
  }
  return false;
}

}  // namespace

void ConnectionHandle::SetUp(Section& dsn_section,
                             std::string const& dsn_name) {
  dsn_.description = dsn_section["Description"];
  dsn_.driver = dsn_section["Driver"];
  dsn_.catalog = dsn_section["Catalog"];
  dsn_.dsn_name = dsn_name;
}

StatusRecord ConnectionHandle::Connect(Authentication& auth) {
  Oauth oauth;
  oauth.auth_mechanism = auth.auth_mechanism;
  oauth.credentials_file_path = auth.key_file_path;

  StatusRecordOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(oauth);
  if (!response) {
    return response.GetStatusRecord();
  }
  client_ = *response;

  // Verify the credentials by calling ODBCBQClient::GetOAuth2Token
  StatusRecordOr<AccessToken> access_token_resp = client_->GetOAuth2Token();
  if (!access_token_resp) {
    return access_token_resp.GetStatusRecord();
  }

  auth_ = auth;
  is_connected_ = true;
  return StatusRecord::Ok();
}

StatusRecord ConnectionHandle::GetAttribute(SQLINTEGER attribute,
                                            SQLPOINTER value,
                                            SQLINTEGER length) {
  return StatusRecord::Ok();
}

StatusRecord ConnectionHandle::SetAttribute(SQLINTEGER attribute,
                                            SQLPOINTER value,
                                            SQLINTEGER length) {
  ConnectionAttr conn_attr;
  std::string err_msg = "Failed to set attribute [";
  err_msg.append(conn_attr.GetAttributeStringValue(attribute)).append("]: ");
  // Perform attribute validations.
  // 1) Check if attribute is supported.
  if (!conn_attr.IsSetAttributeSupported(attribute)) {
    err_msg.append("Attribute not supported by the driver");
    return StatusRecord{SQLStates::k_HY092(), err_msg};
  }
  // 2) Check if attribute is valid with repsect to connection.
  switch (conn_attr.GetAttributeConnectionBehavior(attribute)) {
    case ConnectionValidation::Before: {
      if (IsConnected()) {
        err_msg.append("Attribute cannot be set after connection is made");
        return StatusRecord{SQLStates::k_HY000(), err_msg};
      }
    }
    case ConnectionValidation::After: {
      if (!IsConnected()) {
        err_msg.append("Connection not open");
        return StatusRecord{SQLStates::k_08003(), err_msg};
      }
    }
    case ConnectionValidation::Invalid: {
      err_msg.append("Attribute not supported by the driver");
      return StatusRecord{SQLStates::k_HY092(), err_msg};
    }
  }
  // 3) Check validity with respect to attribute value.
  switch (conn_attr.GetAttributeValueType(attribute)) {
    case ConnectionValueType::SQL_U_LEN:
    case ConnectionValueType::SQL_U_INT:
    case ConnectionValueType::SQL_INT:
    case ConnectionValueType::SQL_INT_BITMASK: {
      auto possible_values = conn_attr.GetAttributePossibleValues(attribute);
      if (!IsAttributeValueValid(possible_values, value)) {
        err_msg.append("Invalid attribute value.");
        return StatusRecord{SQLStates::k_HY024(), err_msg};
      }
      // Store attribute.
      attribute_values.insert({attribute, value});
      break;
    }
    case ConnectionValueType::SQL_CHR: {
      auto* pVal = reinterpret_cast<SQLCHAR*>(value);
      if (!pVal) {
        err_msg.append("Invalid attribute value.");
        return StatusRecord{SQLStates::k_HY024(), err_msg};
      }
      if (length <= 0 && length != SQL_NTS) {
        err_msg.append("Invalid attribute length.");
        return StatusRecord{SQLStates::k_HY090(), err_msg};
      }
      auto* src = reinterpret_cast<char*>(pVal);
      int len = (length > 0) ? length : strlen(src);
      // Memory needs to be allocated here otherwise this will result in i
      // pointer being out of scope.
      SQLCHAR* store_val = new SQLCHAR[len + 1];
      strcpy(reinterpret_cast<char*>(store_val), src);
      // Store attribute.
      attribute_values.insert({attribute, (SQLPOINTER)store_val});
      break;
    }
    default: {
      err_msg.append("Invalid attribute value type.");
      return StatusRecord{SQLStates::k_HY024(), err_msg};
    }
  }

  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
