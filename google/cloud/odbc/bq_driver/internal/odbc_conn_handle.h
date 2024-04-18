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
  std::string dsn_name;
  bool is_bq_legacy_sql;
};

class ConnectionHandle : public Handle {
 public:
  explicit ConnectionHandle() = default;
  ~ConnectionHandle() = default;

  ConnectionHandle(ConnectionHandle const&) = default;
  ConnectionHandle& operator=(ConnectionHandle const&) = default;
  ConnectionHandle(ConnectionHandle&&) = default;
  ConnectionHandle& operator=(ConnectionHandle&&) = default;

  odbc_internal::StatusRecord Connect(Authentication& auth);

  void SetUp(Section& dsn_section, std::string const& dsn_name);

  Dsn GetDsn() { return dsn_; }

  std::shared_ptr<ODBCBQClient> GetClient() { return client_; }

  odbc_internal::StatusRecord GetAttribute(SQLINTEGER attribute,
                                           SQLPOINTER value, SQLINTEGER buf_len,
                                           SQLINTEGER* str_len);

  odbc_internal::StatusRecord SetAttribute(SQLINTEGER attribute,
                                           SQLPOINTER value, SQLINTEGER length);

  [[nodiscard]] bool IsConnected() const { return is_connected_; }

  HandleType kType = HandleType::kConnHandle;

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
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_CONN_HANDLE_H
