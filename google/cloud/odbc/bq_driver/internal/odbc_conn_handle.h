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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_CONN_HANDLE_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_CONN_HANDLE_H

#include "google/cloud/odbc/bq_client_interface/odbc_bq_client.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_conn_attr.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/bq_driver/internal/utils.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bigquery_client_interface::Oauth;
using google::cloud::odbc_bigquery_client_interface::OauthMechanism;
using google::cloud::odbc_bigquery_client_interface::ODBCBQClient;

// Details of authentication provided in the odbc.ini/Windows Registry
struct Authentication {
  OauthMechanism auth_mechanism;
  std::string key_file_path;
  std::string email;
  // NOTE: This should be removed if we decide that we will not support refresh
  // tokens
  std::string refresh_token;
};

// This is populated by SQL*Connect APIs after parsing the DSN section from
// odbc.ini/Windows Registry
struct Dsn {
  std::string description;
  std::string driver;
  std::string catalog;
  std::string default_dataset;
  std::string dsn_name;
  std::string keyfilepath;
  std::string OAuthMechanism;
  bool is_bq_legacy_sql = false;
  bool is_job_creation_required = false;
  bool sessions_enabled = false;
};

class EnvironmentHandle;
class StatementHandle;
class DescriptorHandle;

class ConnectionHandle : public Handle {
 public:
  // This constructor is used only for tests
  explicit ConnectionHandle() = default;
  explicit ConnectionHandle(EnvironmentHandle* env_handle)
      : env_handle_(env_handle){};
  ~ConnectionHandle() = default;

  ConnectionHandle(ConnectionHandle const& connectionHandle);
  ConnectionHandle& operator=(ConnectionHandle const& connectionHandle);
  ConnectionHandle(ConnectionHandle&& connectionHandle) noexcept;
  ConnectionHandle& operator=(ConnectionHandle&& connectionHandle) noexcept;

  odbc_internal::StatusRecord Connect(Authentication& auth);

  inline void Disconnect() { is_connected_ = false; };

  void SetUp(Section& dsn_section, std::string const& dsn_name);

  Dsn GetDsn() const { return dsn_; }

  std::shared_ptr<ODBCBQClient> GetClient() { return client_; }

  odbc_internal::StatusRecord GetAttribute(SQLINTEGER attribute,
                                           SQLPOINTER value, SQLINTEGER buf_len,
                                           SQLINTEGER* str_len);

  odbc_internal::StatusRecord SetAttribute(SQLINTEGER attribute,
                                           SQLPOINTER value, SQLINTEGER length);

  [[nodiscard]] bool IsConnected() const { return is_connected_; }

  HandleType kType = HandleType::kConnHandle;

  std::set<StatementHandle*>& GetStatementHandles() { return stmt_handles_; }
  std::set<DescriptorHandle*>& GetDescriptorHandles() { return desc_handles_; }
  inline EnvironmentHandle* GetEnvironmentHandle() { return env_handle_; };

  std::mutex& GetMutex() const { return connection_handle_mutex_; }

  inline std::string GetSessionId() const { return session_id_; }
  inline void SetSessionId(std::string session_id) {
    session_id_ = std::move(session_id);
  }
  inline bool IsSessionStarted() const { return !session_id_.empty(); }

  inline bool IsTransactionActive() const { return is_transaction_active_; }
  inline void SetTransactionActive(bool is_transaction_active) {
    is_transaction_active_ = is_transaction_active;
  }

 protected:
  bool is_connected_ = false;

 private:
  Dsn dsn_;
  // We are storing this because we might need to handle connection retries.
  //  We don't want to read this information from the env again and again.
  Authentication auth_;
  // The ODBCBQClient we will use for APIs interacting with BigQuery
  std::shared_ptr<ODBCBQClient> client_;
  // stores non string attribute values.
  std::map<SQLINTEGER, SQLPOINTER> attribute_values_;
  // stores string attribute values.
  std::map<SQLINTEGER, std::string> attribute_str_values_;
  // storage of all statement handles associated with this connection handle
  std::set<StatementHandle*> stmt_handles_;
  // storage of all explicitly allocated descriptor handles associated with this
  // connection handle
  std::set<DescriptorHandle*> desc_handles_;
  EnvironmentHandle* env_handle_{nullptr};
  mutable std::mutex connection_handle_mutex_;
  // Session ID of the started session.
  // Empty string if a session wasn't started
  std::string session_id_;
  // True if transaction was begun within the session.
  // False otherwise.
  bool is_transaction_active_ = false;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_CONN_HANDLE_H
