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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/status_or.h"
#include <memory>
#include <mutex>
#include <set>

namespace google::cloud::odbc_bq_driver_internal {

enum EnvAttrConnectionPoolVal { kCpOff, kOnePerDriver, kOnePerHenv };
enum EnvAttrCPMatchVal { kStrictMatch, kRelaxedMatch };
enum EnvAttrOdbcVersVal { kOdbc2, kOdbc3 };
enum EnvAttrOutputNTSVal { kNtsTrue };

class EnvAttrConnectionPool {
 public:
  explicit EnvAttrConnectionPool() : name_("SQL_CP_OFF"), val_(SQL_CP_OFF) {}
  explicit EnvAttrConnectionPool(EnvAttrConnectionPoolVal const& val);
  ~EnvAttrConnectionPool() = default;

  EnvAttrConnectionPool(EnvAttrConnectionPool const&) = default;
  EnvAttrConnectionPool& operator=(EnvAttrConnectionPool const&) = default;
  EnvAttrConnectionPool(EnvAttrConnectionPool&&) = default;
  EnvAttrConnectionPool& operator=(EnvAttrConnectionPool&&) = default;

  static google::cloud::odbc_internal::StatusRecordOr<EnvAttrConnectionPoolVal>
  ParseVal(void* value);

  [[nodiscard]] std::string Name() const { return name_; }
  [[nodiscard]] SQLUINTEGER Value() const { return val_; }

 private:
  std::string name_;
  SQLUINTEGER val_;
};

class EnvAttrConnectionPoolMatch {
 public:
  explicit EnvAttrConnectionPoolMatch()
      : name_("SQL_CP_STRICT_MATCH"), val_(SQL_CP_STRICT_MATCH) {}
  explicit EnvAttrConnectionPoolMatch(EnvAttrCPMatchVal const& val);
  ~EnvAttrConnectionPoolMatch() = default;

  EnvAttrConnectionPoolMatch(EnvAttrConnectionPoolMatch const&) = default;
  EnvAttrConnectionPoolMatch& operator=(EnvAttrConnectionPoolMatch const&) =
      default;
  EnvAttrConnectionPoolMatch(EnvAttrConnectionPoolMatch&&) = default;
  EnvAttrConnectionPoolMatch& operator=(EnvAttrConnectionPoolMatch&&) = default;

  static google::cloud::odbc_internal::StatusRecordOr<EnvAttrCPMatchVal>
  ParseVal(void* value);
  [[nodiscard]] std::string Name() const { return name_; }
  [[nodiscard]] SQLUINTEGER Value() const { return val_; }

 private:
  std::string name_;
  SQLUINTEGER val_;
};

class EnvAttrOdbcVersion {
 public:
  explicit EnvAttrOdbcVersion() : name_("SQL_OV_ODBC3"), val_(SQL_OV_ODBC3) {}
  explicit EnvAttrOdbcVersion(EnvAttrOdbcVersVal const& val);
  ~EnvAttrOdbcVersion() = default;

  EnvAttrOdbcVersion(EnvAttrOdbcVersion const&) = default;
  EnvAttrOdbcVersion& operator=(EnvAttrOdbcVersion const&) = default;
  EnvAttrOdbcVersion(EnvAttrOdbcVersion&&) = default;
  EnvAttrOdbcVersion& operator=(EnvAttrOdbcVersion&&) = default;

  static google::cloud::odbc_internal::StatusRecordOr<EnvAttrOdbcVersVal>
  ParseVal(void* value);
  [[nodiscard]] std::string Name() const { return name_; }
  [[nodiscard]] SQLUINTEGER Value() const { return val_; }

 private:
  std::string name_;
  SQLINTEGER val_;
};

class EnvAttrOutputNTS {
 public:
  explicit EnvAttrOutputNTS() = default;
  ~EnvAttrOutputNTS() = default;

  EnvAttrOutputNTS(EnvAttrOutputNTS const&) = default;
  EnvAttrOutputNTS& operator=(EnvAttrOutputNTS const&) = default;
  EnvAttrOutputNTS(EnvAttrOutputNTS&&) = default;
  EnvAttrOutputNTS& operator=(EnvAttrOutputNTS&&) = default;

  static google::cloud::odbc_internal::StatusRecordOr<int> ParseVal(
      void* value);
  [[nodiscard]] std::string Name() const { return name_; }
  [[nodiscard]] SQLUINTEGER Value() const { return val_; }

 private:
  std::string name_{"SQL_TRUE"};
  SQLINTEGER val_{SQL_TRUE};
};

class ConnectionHandle;

class EnvironmentHandle : public Handle {
 public:
  explicit EnvironmentHandle();
  ~EnvironmentHandle() = default;

  EnvironmentHandle(EnvironmentHandle const& environmentHandle);
  EnvironmentHandle& operator=(EnvironmentHandle const& environmentHandle);
  EnvironmentHandle(EnvironmentHandle&& environmentHandle) noexcept;
  EnvironmentHandle& operator=(EnvironmentHandle&& environmentHandle) noexcept;

  SQLRETURN GetAttribute(SQLINTEGER attribute, void* value, void* length);

  SQLRETURN SetAttribute(SQLINTEGER attribute, void* value, void* length);

  HandleType kType = HandleType::kEnvHandle;

  std::mutex& GetMutex() const { return environment_handle_mutex_; }

  std::set<ConnectionHandle*>& GetConnectionHandles() { return conn_handles_; }

 private:
  std::shared_ptr<EnvAttrConnectionPool> connection_pool_;
  std::shared_ptr<EnvAttrConnectionPoolMatch> connection_pool_match_;
  std::shared_ptr<EnvAttrOdbcVersion> odbc_ver_;
  std::shared_ptr<EnvAttrOutputNTS> output_nts_;
  mutable std::mutex environment_handle_mutex_;
  // storage of all statement handles associated with this connection handle
  std::set<ConnectionHandle*> conn_handles_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H
