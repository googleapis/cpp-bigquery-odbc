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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google::cloud::odbc_bq_driver_internal {

enum EnvAttrConnectionPoolVal { CP_OFF, ONE_PER_DRIVER, ONE_PER_HENV };
enum EnvAttrCPMatchVal { STRICT_MATCH, RELAXED_MATCH };
enum EnvAttrOdbcVersVal { ODBC_2, ODBC_3 };
enum EnvAttrOutputNTSVal { NTS_TRUE };

class EnvAttrConnectionPool {
 public:
  explicit EnvAttrConnectionPool() : name_("SQL_CP_OFF"), val_(SQL_CP_OFF) {}
  explicit EnvAttrConnectionPool(EnvAttrConnectionPoolVal const& val);
  ~EnvAttrConnectionPool() = default;

  EnvAttrConnectionPool(EnvAttrConnectionPool const&) = default;
  EnvAttrConnectionPool& operator=(EnvAttrConnectionPool const&) = default;
  EnvAttrConnectionPool(EnvAttrConnectionPool&&) = default;
  EnvAttrConnectionPool& operator=(EnvAttrConnectionPool&&) = default;

  static StatusOr<EnvAttrConnectionPoolVal> ParseVal(void* value);
  inline std::string Name() { return name_; }
  inline SQLUINTEGER Value() { return val_; }

 private:
  std::string name_;
  SQLUINTEGER val_;
};

struct EnvAttrConnectionPoolMatch {
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

  static StatusOr<EnvAttrCPMatchVal> ParseVal(void* value);
  inline std::string Name() { return name_; }
  inline SQLUINTEGER Value() { return val_; }

 private:
  std::string name_;
  SQLUINTEGER val_;
};

struct EnvAttrOdbcVersion {
 public:
  explicit EnvAttrOdbcVersion() : name_("SQL_OV_ODBC3"), val_(SQL_OV_ODBC3) {}
  explicit EnvAttrOdbcVersion(EnvAttrOdbcVersVal const& val);
  ~EnvAttrOdbcVersion() = default;

  EnvAttrOdbcVersion(EnvAttrOdbcVersion const&) = default;
  EnvAttrOdbcVersion& operator=(EnvAttrOdbcVersion const&) = default;
  EnvAttrOdbcVersion(EnvAttrOdbcVersion&&) = default;
  EnvAttrOdbcVersion& operator=(EnvAttrOdbcVersion&&) = default;

  static StatusOr<EnvAttrOdbcVersVal> ParseVal(void* value);
  inline std::string Name() { return name_; }
  inline SQLUINTEGER Value() { return val_; }

 private:
  std::string name_;
  SQLINTEGER val_;
};

struct EnvAttrOutputNTS {
 public:
  explicit EnvAttrOutputNTS() : name_("SQL_TRUE"), val_(SQL_TRUE) {}
  ~EnvAttrOutputNTS() = default;

  EnvAttrOutputNTS(EnvAttrOutputNTS const&) = default;
  EnvAttrOutputNTS& operator=(EnvAttrOutputNTS const&) = default;
  EnvAttrOutputNTS(EnvAttrOutputNTS&&) = default;
  EnvAttrOutputNTS& operator=(EnvAttrOutputNTS&&) = default;

  static Status ParseVal(void* value);
  inline std::string Name() { return name_; }
  inline SQLUINTEGER Value() { return val_; }

 private:
  std::string name_;
  SQLINTEGER val_;
};

class EnvironmentHandle : public Handle {
 public:
  explicit EnvironmentHandle();
  ~EnvironmentHandle() = default;

  EnvironmentHandle(EnvironmentHandle const&) = default;
  EnvironmentHandle& operator=(EnvironmentHandle const&) = default;
  EnvironmentHandle(EnvironmentHandle&&) = default;
  EnvironmentHandle& operator=(EnvironmentHandle&&) = default;

  SQLRETURN GetAttribute(SQLINTEGER attribute, void* value, void* length);

  SQLRETURN SetAttribute(SQLINTEGER attribute, void* value, void* length);

 private:
  std::shared_ptr<EnvAttrConnectionPool> connection_pool_;
  std::shared_ptr<EnvAttrConnectionPoolMatch> connection_pool_match_;
  std::shared_ptr<EnvAttrOdbcVersion> odbc_ver_;
  std::shared_ptr<EnvAttrOutputNTS> output_nts_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H
