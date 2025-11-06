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
#include "google/cloud/odbc/bq_driver/internal/odbc_internal_commons.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_type_utils.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bigquery_client_interface::kDefaultTokenUrl;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeAws4;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeDefault;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeIdToken;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeJWT;
using google::cloud::odbc_bigquery_client_interface::kSubTokenTypeSaml2;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

#ifdef _WIN32
using google::cloud::odbc_bq_driver_internal::DecryptPassword;
#endif

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
        LOG(ERROR) << "ValidateConnection:: " << err_msg;
        return StatusRecord{SQLStates::k_HY000(), err_msg};
      }
      break;
    }
    case ConnectionValidation::kAfter: {
      if (!isConnected) {
        err_msg.append("Connection not open");
        LOG(ERROR) << "ValidateConnection:: " << err_msg;
        return StatusRecord{SQLStates::k_08003(), err_msg};
      }
      break;
    }
    case ConnectionValidation::kInvalid: {
      err_msg.append("Attribute not supported by the driver");
      LOG(ERROR) << "ValidateConnection:: " << err_msg;
      return StatusRecord{SQLStates::k_HY092(), err_msg};
    }
  }
  return StatusRecord::Ok();
}

}  // namespace

void ConnectionHandle::SetUp(Section& dsn_section,
                             std::string const& dsn_name) {
  LOG(INFO) << "ConnectionHandle::SetUp:: Setting up connection handle from "
               "DSN configuration for DSN: "
            << dsn_name;
  dsn_.description = dsn_section["DESCRIPTION"];
  dsn_.driver = dsn_section["DRIVER"];
  dsn_.catalog = dsn_section["CATALOG"];
  dsn_.default_dataset = dsn_section["DEFAULTDATASET"];
  dsn_.list_projects_parent = dsn_section["LISTPROJECTSPARENT"];
  dsn_.dsn_name = dsn_name;
  dsn_.key_file_path = dsn_section["KEYFILEPATH"];
  dsn_.o_auth_mechanism = dsn_section["OAUTHMECHANISM"];
  dsn_.email = dsn_section["EMAIL"];
  dsn_.refresh_token = dsn_section["REFRESHTOKEN"];
  std::string sql_dialect = dsn_section["SQLDIALECT"];
  dsn_.is_bq_legacy_sql = (sql_dialect == "0");
  std::string sessions_enabled = dsn_section["ENABLESESSION"];
  dsn_.sessions_enabled =
      (!sessions_enabled.empty() && sessions_enabled != "0");
  std::string max_row_fetched = dsn_section["ROWSFETCHEDPERBLOCK"];
  if (!max_row_fetched.empty()) {
    auto status = ParseStringToInteger(max_row_fetched);
    if (status) {
      dsn_.row_fetched_per_block = status.GetValue();
    }
  }
  std::string string_column_length = dsn_section["DEFAULTSTRINGCOLUMNLENGTH"];
  if (!string_column_length.empty()) {
    auto status = ParseStringToInteger(string_column_length);
    if (status) {
      dsn_.default_string_column_length = status.GetValue();
    }
  }
  // Disable query cache if CACHEQUERY is set to "false" or "0" in the DSN
  // section.
  std::string query_cache = dsn_section["USEQUERYCACHE"];
  if (query_cache == "false" || query_cache == "0") {
    dsn_.is_query_cache = false;
  }

  std::string connection_properties = dsn_section["QUERYPROPERTIES"];
  auto parse_result = ParseQueryProperties(connection_properties);
  if (parse_result) {
    dsn_.connection_properties = *parse_result;
  }

  dsn_.pem_file = dsn_section["TRUSTEDCERTS"];
  dsn_.kms_key_name = dsn_section["KMSKEYNAME"];
  dsn_.session_location = dsn_section["SESSIONLOCATION"];
  dsn_.additional_projects = dsn_section["ADDITIONALPROJECTS"];
  dsn_.psc = dsn_section["PRIVATESERVICECONNECTURIS"];
  dsn_.enable_tpc =
      dsn_section["ENABLETPC"] == "1" || dsn_section["ENABLETPC"] == "true";
  dsn_.universe_domain = dsn_section["UNIVERSEDOMAIN"];

  // As with the existing driver, the default value of JobCreationMode is
  // '2'(JOB_CREATION_OPTIONAL)
  std::string job_creation_mode = dsn_section["JOBCREATIONMODE"];
  dsn_.is_job_creation_required = (job_creation_mode == "1");

  if (attribute_str_values_.count(SQL_ATTR_CURRENT_CATALOG) == 0) {
    attribute_str_values_.insert({SQL_ATTR_CURRENT_CATALOG, dsn_.catalog});
  }

  // Populate HTAPI related configurations
  std::string use_default_large_results_dataset =
      dsn_section["USEDEFAULTLARGERESULTSDATASET"];
  dsn_.use_default_large_results_dataset =
      (use_default_large_results_dataset != "0");
  dsn_.large_results_dataset_id = dsn_section["LARGERESULTSDATASETID"];
  if (dsn_.large_results_dataset_id.empty()) {
    dsn_.large_results_dataset_id = kDefaultDestDatasetId;
  }
  std::string allow_htapi = dsn_section["ALLOWHTAPIFORLARGERESULTS"];
  dsn_.allow_htapi = (allow_htapi == "1");
  dsn_.htapi_activation_threshold = dsn_section["HTAPI_ACTIVATIONTHRESHOLD"];
  if (dsn_.htapi_activation_threshold.empty()) {
    // This is the default value set on the windows configuration screen too.
    dsn_.htapi_activation_threshold = "10000";
  }
  dsn_.large_table_expiration_time =
      dsn_section["LARGERESULTSTEMPTABLEEXPIRATIONTIME"];
  dsn_.proxy_options.hostname = dsn_section["PROXYHOST"];
  dsn_.proxy_options.port = dsn_section["PROXYPORT"];
  dsn_.proxy_options.username = dsn_section["PROXYUID"];
  dsn_.proxy_options.password = dsn_section["PROXYPWD"];
#ifdef _WIN32
  if (dsn_.proxy_options.password.empty()) {
    dsn_.proxy_options.password = DecryptPassword(dsn_section["PROXYPWD_ENC"]);
  }
#endif

  // Populate BYOID properties from DSN section.
  dsn_.byoid_aud_url = dsn_section["BYOID_AUDIENCEURL"];
  dsn_.byoid_creds_src = dsn_section["BYOID_CREDENTIALSOURCE"];
  dsn_.byoid_pool_user_project = dsn_section["BYOID_POOLUSERPROJECT"];
  dsn_.byoid_subj_token_type = dsn_section["BYOID_SUBJECTTOKENTYPE"];
  dsn_.byoid_token_url = dsn_section["BYOID_TOKENURL"];
  // Set default values for empty properties.
  if (dsn_.byoid_subj_token_type.empty()) {
    dsn_.byoid_subj_token_type = kSubTokenTypeDefault;
  }
  if (dsn_.byoid_token_url.empty()) {
    dsn_.byoid_token_url = kDefaultTokenUrl;
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

StatusRecord ConnectionHandle::ValidateExternalUser(
    Authentication const& auth) {
  if (auth.oauth.auth_mechanism == OauthMechanism::kExternalUser) {
    if (IsBYOIDPropertiesSet(auth.oauth.byoid_aud_url,
                             auth.oauth.byoid_creds_src,
                             auth.oauth.byoid_subj_token_type)) {
      return ValidateBYOIDProperties(auth.oauth.byoid_aud_url,
                                     auth.oauth.byoid_creds_src,
                                     auth.oauth.byoid_subj_token_type);
    }
    // Credentials file must be set.
    if (auth.oauth.credentials_file_path.empty()) {
      LOG(ERROR) << "ConnectionHandle::ValidateExternalUser:: JSON Credentials "
                    "File path is empty for external user.";
      return StatusRecord{
          SQLStates::k_HY000(),
          "JSON Credentials File path is empty for external user"};
    }
  }
  return StatusRecord::Ok();
}

StatusRecord ConnectionHandle::Connect(Authentication& auth) {
  // For external authentication, make sure either BYOID or JSON file is set.
  ValidateExternalUser(auth);
  StatusRecordOr<std::shared_ptr<ODBCBQClient>> response =
      ODBCBQClient::CreateBQClient(auth.oauth);
  if (!response) {
    LOG(ERROR) << "ConnectionHandle::Connect::CreateBQClient:: "
               << response.GetStatusRecord().message;
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
    LOG(ERROR) << "ConnectionHandle::Connect::GetOAuth2Token:: "
               << access_token_resp.GetStatusRecord().message;
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
    LOG(ERROR) << "ConnectionHandle::GetAttribute:: " << err_msg;
    return StatusRecord{SQLStates::k_HY092(), err_msg};
  }
  // 2) Check if attribute is valid with respect to connection.
  auto status_record =
      ValidateConnection(IsConnected(), err_msg,
                         conn_attr.GetAttributeConnectionBehavior(attribute));
  if (!status_record.ok()) {
    LOG(ERROR) << "ConnectionHandle::GetAttribute::ValidateConnection:: "
               << status_record.message;
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
    LOG(ERROR) << "ConnectionHandle::SetAttribute:: " << err_msg;
    return StatusRecord{SQLStates::k_HY092(), err_msg};
  }
  // 2) Check if attribute is valid with repsect to connection.
  auto status_record =
      ValidateConnection(IsConnected(), err_msg,
                         conn_attr.GetAttributeConnectionBehavior(attribute));
  if (!status_record.ok()) {
    LOG(ERROR) << "ConnectionHandle::SetAttribute::ValidateConnection:: "
               << status_record.message;
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
        LOG(ERROR) << "ConnectionHandle::SetAttribute:: " << err_msg;
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

odbc_internal::StatusRecord ConnectionHandle::ValidateBYOIDProperties(
    std::string const& byoid_aud_url, std::string const& byoid_creds_src,
    std::string const& byoid_subj_token_type) {
  // If BYOID properties are not set then we just return true.
  if (!IsBYOIDPropertiesSet(byoid_aud_url, byoid_creds_src,
                            byoid_subj_token_type))
    return StatusRecord::Ok();

  // Required properties must be set.
  if ((byoid_aud_url.empty() || byoid_subj_token_type.empty() ||
       byoid_creds_src.empty())) {
    LOG(ERROR) << "ConnectionHandle::ValidateBYOIDProperties:: Required BYOID "
                  "properties not set.";
    return StatusRecord{SQLStates::k_HY000(),
                        "Required BYOID properties not set"};
  }

  // Validate subject token type.
  if (byoid_subj_token_type != kSubTokenTypeJWT &&
      byoid_subj_token_type != kSubTokenTypeIdToken &&
      byoid_subj_token_type != kSubTokenTypeSaml2 &&
      byoid_subj_token_type != kSubTokenTypeAws4) {
    LOG(ERROR) << "ConnectionHandle::ValidateBYOIDProperties:: Invalid subject "
                  "token type: "
               << byoid_subj_token_type;
    return StatusRecord{SQLStates::k_HY000(), "Invalid subject token type"};
  }

  return StatusRecord::Ok();
}

StatusRecord ConnectionHandle::ValidateDsnBYOIDProperties() const {
  return ValidateBYOIDProperties(dsn_.byoid_aud_url, dsn_.byoid_creds_src,
                                 dsn_.byoid_subj_token_type);
}

}  // namespace google::cloud::odbc_bq_driver_internal
