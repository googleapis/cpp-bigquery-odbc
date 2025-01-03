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
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

namespace {
bool IsAttributeValueValid(std::vector<SQLPOINTER>& possible_values,
                           SQLPOINTER value) {
  if (possible_values.empty()) {
    return true;
  }
  return std::any_of(possible_values.begin(), possible_values.end(),
                     [value](SQLPOINTER val) {
                       auto expected_val = reinterpret_cast<std::size_t>(val);
                       auto actual_val = reinterpret_cast<std::size_t>(value);
                       return (expected_val == actual_val);
                     });
}

StatusRecord ValidateConnection(bool isConnected, std::string& err_msg,
                                ConnectionValidation const& connect_attrib) {
  switch (connect_attrib) {
    case ConnectionValidation::kBefore: {
      if (isConnected) {
        err_msg.append("Attribute cannot be set after connection is made");
        return StatusRecord{SQLStates::k_HY000(), err_msg};
      }
      break;
    }
    case ConnectionValidation::kAfter: {
      if (!isConnected) {
        err_msg.append("Connection not open");
        return StatusRecord{SQLStates::k_08003(), err_msg};
      }
      break;
    }
    case ConnectionValidation::kInvalid: {
      err_msg.append("Attribute not supported by the driver");
      return StatusRecord{SQLStates::k_HY092(), err_msg};
    }
  }
  return StatusRecord::Ok();
}

}  // namespace

// TODO(b/385136383): Change DSN Section Keys to Uppercase for Consistency
void ConnectionHandle::SetUp(Section& dsn_section,
                             std::string const& dsn_name) {
  dsn_.description = dsn_section["DESCRIPTION"];
  dsn_.driver = dsn_section["DRIVER"];
  dsn_.catalog = dsn_section["CATALOG"];
  dsn_.default_dataset = dsn_section["DEFAULTDATASET"];
  dsn_.list_projects_parent = dsn_section["LISTPROJECTSPARENT"];
  dsn_.dsn_name = dsn_name;
  dsn_.key_file_path = dsn_section["KEYFILEPATH"];
  dsn_.o_auth_mechanism = dsn_section["OAUTHMECHANISM"];

  std::string sql_dialect = dsn_section["SQLDIALECT"];
  dsn_.is_bq_legacy_sql = (sql_dialect == "0");
  std::string sessions_enabled = dsn_section["ENABLESESSION"];
  dsn_.sessions_enabled =
      (!sessions_enabled.empty() && sessions_enabled != "0");

  // As with the existing driver, the default value of JobCreationMode is
  // '2'(JOB_CREATION_OPTIONAL)
  std::string job_creation_mode = dsn_section["JOBCREATIONMODE"];
  dsn_.is_job_creation_required = (job_creation_mode == "1");

  if (attribute_str_values_.count(SQL_ATTR_CURRENT_CATALOG) == 0) {
    attribute_str_values_.insert({SQL_ATTR_CURRENT_CATALOG, dsn_.catalog});
  }
}

ConnectionHandle::ConnectionHandle(ConnectionHandle const& connectionHandle)
    : Handle(connectionHandle) {
  client_ = connectionHandle.client_;
  dsn_ = connectionHandle.dsn_;
  auth_ = connectionHandle.auth_;
  kType = connectionHandle.kType;
  attribute_str_values_ = connectionHandle.attribute_str_values_;
  is_connected_ = connectionHandle.is_connected_;
  // TODO(b/349757194): Convert shallow copy to deep copy
  attribute_values_ = connectionHandle.attribute_values_;
  stmt_handles_ = connectionHandle.stmt_handles_;
  desc_handles_ = connectionHandle.desc_handles_;
}

ConnectionHandle& ConnectionHandle::operator=(
    ConnectionHandle const& connectionHandle) {
  if (this != &connectionHandle) {
    client_ = connectionHandle.client_;
    dsn_ = connectionHandle.dsn_;
    auth_ = connectionHandle.auth_;
    kType = connectionHandle.kType;
    attribute_str_values_ = connectionHandle.attribute_str_values_;
    is_connected_ = connectionHandle.is_connected_;
    // TODO(b/349757194): Convert shallow copy to deep copy
    attribute_values_ = connectionHandle.attribute_values_;
    stmt_handles_ = connectionHandle.stmt_handles_;
    desc_handles_ = connectionHandle.desc_handles_;
    return *this;
  }
}

ConnectionHandle::ConnectionHandle(
    ConnectionHandle&& connectionHandle) noexcept {
  client_ = std::move(connectionHandle.client_);
  dsn_ = std::move(connectionHandle.dsn_);
  auth_ = std::move(connectionHandle.auth_);
  kType = std::move(connectionHandle.kType);
  attribute_str_values_ = std::move(connectionHandle.attribute_str_values_);
  attribute_values_ = std::move(connectionHandle.attribute_values_);
  stmt_handles_ = std::move(connectionHandle.stmt_handles_);
  desc_handles_ = std::move(connectionHandle.desc_handles_);
  is_connected_ = std::move(connectionHandle.is_connected_);
}

ConnectionHandle& ConnectionHandle::operator=(
    ConnectionHandle&& connectionHandle) noexcept {
  client_ = std::move(connectionHandle.client_);
  dsn_ = std::move(connectionHandle.dsn_);
  auth_ = std::move(connectionHandle.auth_);
  kType = std::move(connectionHandle.kType);
  attribute_str_values_ = std::move(connectionHandle.attribute_str_values_);
  attribute_values_ = std::move(connectionHandle.attribute_values_);
  stmt_handles_ = std::move(connectionHandle.stmt_handles_);
  desc_handles_ = std::move(connectionHandle.desc_handles_);
  is_connected_ = std::move(connectionHandle.is_connected_);
  return *this;
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
  // Set optional parent value for RM ListProjects if provided.
  if (!GetDsn().list_projects_parent.empty()) {
    client_->SetListProjectsParent(GetDsn().list_projects_parent);
  }

  // Verify the credentials by calling ODBCBQClient::GetOAuth2Token
  StatusRecordOr<AccessToken> access_token_resp = client_->GetOAuth2Token();
  if (!access_token_resp) {
    return access_token_resp.GetStatusRecord();
  }

  auth_ = auth;
  is_connected_ = true;
  return StatusRecord::Ok();
}

odbc_internal::StatusRecord ConnectionHandle::GetAttribute(
    SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER buf_len,
    SQLINTEGER* str_len) {
  ConnectionAttr conn_attr;
  std::string err_msg = "Failed to get attribute [";
  err_msg.append(conn_attr.GetAttributeStringValue(attribute)).append("]: ");
  // 1) Check if GET attribute is supported.
  if (!conn_attr.IsGetAttributeSupported(attribute)) {
    err_msg.append("Attribute not supported by the driver");
    return StatusRecord{SQLStates::k_HY092(), err_msg};
  }
  // 2) Check if attribute is valid with respect to connection.
  auto status_record =
      ValidateConnection(IsConnected(), err_msg,
                         conn_attr.GetAttributeConnectionBehavior(attribute));
  if (!status_record.ok()) {
    return status_record;
  }
  // 3) Get attribute value
  bool attrib_val_found =
      attribute_values_.find(attribute) != attribute_values_.end();
  SQLPOINTER default_value = conn_attr.GetAttributeDefaultValue(attribute);
  SQLPOINTER attr_val =
      (attrib_val_found ? attribute_values_[attribute] : default_value);
  SQLRETURN rc;
  switch (conn_attr.GetAttributeValueType(attribute)) {
    case ConnectionValueType::kSqlInt:
    case ConnectionValueType::kSqlIntBitmask: {
      rc = IntValueToOutputBufferResponse<SQLINTEGER>(
          reinterpret_cast<size_t>(attr_val), value, str_len);
      break;
    }
    case ConnectionValueType::kSqlUInt: {
      rc = IntValueToOutputBufferResponse<SQLUINTEGER>(
          reinterpret_cast<size_t>(attr_val), value, str_len);
      break;
    }
    case ConnectionValueType::kSqlULen: {
      rc = IntValueToOutputBufferResponse<SQLULEN>(
          reinterpret_cast<size_t>(attr_val), value, str_len);
      break;
    }
    case ConnectionValueType::kSqlChr: {
      char* src;
      bool attrib_str_val_found =
          attribute_str_values_.find(attribute) != attribute_str_values_.end();

      if (attrib_str_val_found) {
        src = const_cast<char*>(attribute_str_values_[attribute].c_str());
      } else if (default_value != nullptr) {
        src = reinterpret_cast<char*>(default_value);
      }
      return StringValueToOutputBufferResponse(src, value, buf_len, str_len);
    }
    default: {
      err_msg.append("Invalid attribute value type.");
      return StatusRecord{SQLStates::k_HY024(), err_msg};
    }
  }

  if (rc != SQL_SUCCESS) {
    return StatusRecord{SQLStates::k_HY000(),
                        "Unable to retrieve int attribute value"};
  }

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
  auto status_record =
      ValidateConnection(IsConnected(), err_msg,
                         conn_attr.GetAttributeConnectionBehavior(attribute));
  if (!status_record.ok()) {
    return status_record;
  }
  // 3) Check validity with respect to attribute value.
  switch (conn_attr.GetAttributeValueType(attribute)) {
    case ConnectionValueType::kSqlInt:
    case ConnectionValueType::kSqlIntBitmask:
    case ConnectionValueType::kSqlUInt:
    case ConnectionValueType::kSqlULen: {
      auto possible_values = conn_attr.GetAttributePossibleValues(attribute);
      if (!IsAttributeValueValid(possible_values, value)) {
        err_msg.append("Invalid attribute value.");
        return StatusRecord{SQLStates::k_HY024(), err_msg};
      }
      if (attribute == SQL_ATTR_TXN_ISOLATION) {
        // BigQuery supports only one level of isolation
        value = reinterpret_cast<SQLPOINTER>(SQL_TXN_SERIALIZABLE);
      }
      // Store non char attributes.
      attribute_values_.insert_or_assign(attribute, value);
      break;
    }
    case ConnectionValueType::kSqlChr: {
      auto* p_val = reinterpret_cast<SQLCHAR*>(value);
      if (!p_val) {
        err_msg.append("Invalid attribute value pointer.");
        return StatusRecord{SQLStates::k_HY009(), err_msg};
      }
      if (length <= 0 && length != SQL_NTS) {
        err_msg.append("Invalid attribute length.");
        return StatusRecord{SQLStates::k_HY090(), err_msg};
      }
      std::string val(reinterpret_cast<char*>(p_val));
      SQLINTEGER p_val_len = val.length();
      if (length != p_val_len && length != SQL_NTS) {
        err_msg.append("Invalid attribute length.");
        return StatusRecord{SQLStates::k_HY090(), err_msg};
      }
      // Store char attributes.
      attribute_str_values_.insert_or_assign(attribute, val);
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
