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

#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;

EnvAttrConnectionPool::EnvAttrConnectionPool(
    EnvAttrConnectionPoolVal const& val) {
  switch (val) {
    case EnvAttrConnectionPoolVal::CP_OFF: {
      name_ = "SQL_CP_OFF";
      val_ = SQL_CP_OFF;
      break;
    }
    case EnvAttrConnectionPoolVal::ONE_PER_DRIVER: {
      name_ = "SQL_CP_ONE_PER_DRIVER";
      val_ = SQL_CP_ONE_PER_DRIVER;
      break;
    }
    case EnvAttrConnectionPoolVal::ONE_PER_HENV: {
      name_ = "SQL_CP_ONE_PER_HENV";
      val_ = SQL_CP_ONE_PER_HENV;
      break;
    }
  }
}

EnvAttrConnectionPoolMatch::EnvAttrConnectionPoolMatch(
    EnvAttrCPMatchVal const& val) {
  switch (val) {
    case EnvAttrCPMatchVal::RELAXED_MATCH: {
      name_ = "SQL_CP_RELAXED_MATCH";
      val_ = SQL_CP_RELAXED_MATCH;
      break;
    }
    case EnvAttrCPMatchVal::STRICT_MATCH: {
      name_ = "SQL_CP_STRICT_MATCH";
      val_ = SQL_CP_STRICT_MATCH;
      break;
    }
  }
}

EnvAttrOdbcVersion::EnvAttrOdbcVersion(EnvAttrOdbcVersVal const& val) {
  switch (val) {
    case EnvAttrOdbcVersVal::ODBC_2: {
      name_ = "SQL_OV_ODBC2";
      val_ = SQL_OV_ODBC2;
      break;
    }
    case EnvAttrOdbcVersVal::ODBC_3: {
      name_ = "SQL_OV_ODBC3";
      val_ = SQL_OV_ODBC3;
      break;
    }
  }
}

StatusOr<EnvAttrConnectionPoolVal> EnvAttrConnectionPool::ParseVal(
    void* value) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  if (value == nullptr) {
    std::string msg = "Invalid null attribute value for EnvAttrConnectionPool";
    TracePrintInternal(opts, msg);
    return Status(StatusCode::kInvalidArgument, msg);
  }
  SQLUINTEGER* actual_value = reinterpret_cast<SQLUINTEGER*>(value);
  switch (*actual_value) {
    case SQL_CP_OFF: {
      return EnvAttrConnectionPoolVal::CP_OFF;
    }
    case SQL_CP_ONE_PER_DRIVER: {
      return EnvAttrConnectionPoolVal::ONE_PER_DRIVER;
    }
    case SQL_CP_ONE_PER_HENV: {
      return EnvAttrConnectionPoolVal::ONE_PER_HENV;
    }
    default: {
      std::string msg =
          "Unsupported attribute value for EnvAttrConnectionPool: ";
      msg.append(std::to_string(*actual_value));
      TracePrintInternal(opts, msg);
      return Status(StatusCode::kInvalidArgument, msg);
    }
  }
}

StatusOr<EnvAttrCPMatchVal> EnvAttrConnectionPoolMatch::ParseVal(void* value) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  if (value == nullptr) {
    std::string msg =
        "Invalid null attribute value for EnvAttrConnectionPoolMatch";
    TracePrintInternal(opts, msg);
    return Status(StatusCode::kInvalidArgument, msg);
  }
  SQLUINTEGER* actual_value = reinterpret_cast<SQLUINTEGER*>(value);
  switch (*actual_value) {
    case SQL_CP_RELAXED_MATCH: {
      return EnvAttrCPMatchVal::RELAXED_MATCH;
    }
    case SQL_CP_STRICT_MATCH: {
      return EnvAttrCPMatchVal::STRICT_MATCH;
    }
    default: {
      std::string msg =
          "Unsupported attribute value for EnvAttrConnectionPoolMatch: ";
      msg.append(std::to_string(*actual_value));
      TracePrintInternal(opts, msg);
      return Status(StatusCode::kInvalidArgument, msg);
    }
  }
}

StatusOr<EnvAttrOdbcVersVal> EnvAttrOdbcVersion::ParseVal(void* value) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  if (value == nullptr) {
    std::string msg = "Invalid null attribute value for EnvAttrOdbcVersion";
    TracePrintInternal(opts, msg);
    return Status(StatusCode::kInvalidArgument, msg);
  }
  SQLINTEGER* actual_value = reinterpret_cast<SQLINTEGER*>(value);
  switch (*actual_value) {
    case SQL_OV_ODBC2: {
      return EnvAttrOdbcVersVal::ODBC_2;
    }
    case SQL_OV_ODBC3: {
      return EnvAttrOdbcVersVal::ODBC_3;
    }
    default: {
      std::string msg = "Unsupported attribute value for EnvAttrOdbcVersion: ";
      msg.append(std::to_string(*actual_value));
      TracePrintInternal(opts, msg);
      return Status(StatusCode::kInvalidArgument, msg);
    }
  }
}

Status EnvAttrOutputNTS::ParseVal(void* value) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  if (value == nullptr) {
    std::string msg = "Invalid null attribute value for EnvAttrOutputNTS";
    TracePrintInternal(opts, msg);
    return Status(StatusCode::kInvalidArgument, msg);
  }
  SQLINTEGER* actual_value = reinterpret_cast<SQLINTEGER*>(value);
  switch (*actual_value) {
    case SQL_TRUE: {
      return Status(StatusCode::kOk, "");
    }
    default: {
      std::string msg = "Unsupported attribute value for EnvAttrOutputNTS: ";
      msg.append(std::to_string(*actual_value));
      TracePrintInternal(opts, msg);
      return Status(StatusCode::kInvalidArgument, msg);
    }
  }
}

SQLRETURN EnvironmentHandle::GetAttribute(SQLINTEGER attribute, void* value,
                                          void* length) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  if (value == nullptr) {
    TracePrintInternal(opts, "Null attribute output value ptr");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
    return SQL_ERROR;
  }

  switch (attribute) {
    case SQL_ATTR_CONNECTION_POOLING: {
      if (connection_pool_ == nullptr) {
        TracePrintInternal(opts,
                           "Internal error: handle connection pool attribute "
                           "value not initialized");
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      SQLUINTEGER* attrib_val = reinterpret_cast<SQLUINTEGER*>(value);
      *attrib_val = connection_pool_->Value();
      break;
    }
    case SQL_ATTR_CP_MATCH: {
      if (connection_pool_match_ == nullptr) {
        TracePrintInternal(opts,
                           "Internal error: handle connection pool match "
                           "attribute value not initialized");
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      SQLUINTEGER* attrib_val = reinterpret_cast<SQLUINTEGER*>(value);
      *attrib_val = connection_pool_match_->Value();
      break;
    }
    case SQL_ATTR_ODBC_VERSION: {
      if (odbc_ver_ == nullptr) {
        TracePrintInternal(opts,
                           "Internal error: handle odbc version"
                           "attribute value not initialized");
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      SQLINTEGER* attrib_val = reinterpret_cast<SQLINTEGER*>(value);
      *attrib_val = odbc_ver_->Value();
      break;
    }
    case SQL_ATTR_OUTPUT_NTS: {
      if (output_nts_ == nullptr) {
        TracePrintInternal(opts,
                           "Internal error: handle output NTS"
                           "attribute value not initialized");
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      SQLINTEGER* attrib_val = reinterpret_cast<SQLINTEGER*>(value);
      *attrib_val = output_nts_->Value();
      break;
    }
    default: {
      std::string msg =
          "Unsupported environment attribute passed to GetAttribute: ";
      msg.append(std::to_string(attribute));
      TracePrintInternal(opts, msg);
      // TODO(b/308656768,b/308656826): Record error or diagnostic info for
      // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
      return SQL_ERROR;
    }
  }
  return SQL_SUCCESS;
}

SQLRETURN EnvironmentHandle::SetAttribute(SQLINTEGER attribute, void* value,
                                          void* length) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  switch (attribute) {
    case SQL_ATTR_CONNECTION_POOLING: {
      auto conn_pool_val = EnvAttrConnectionPool::ParseVal(value);
      if (!conn_pool_val.ok()) {
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      connection_pool_ =
          std::make_shared<EnvAttrConnectionPool>(*conn_pool_val);
      break;
    }
    case SQL_ATTR_CP_MATCH: {
      auto cp_match_val = EnvAttrConnectionPoolMatch::ParseVal(value);
      if (!cp_match_val.ok()) {
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      connection_pool_match_ =
          std::make_shared<EnvAttrConnectionPoolMatch>(*cp_match_val);
      break;
    }
    case SQL_ATTR_ODBC_VERSION: {
      auto odbc_vers_val = EnvAttrOdbcVersion::ParseVal(value);
      if (!odbc_vers_val.ok()) {
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      odbc_ver_ = std::make_shared<EnvAttrOdbcVersion>(*odbc_vers_val);
      break;
    }
    case SQL_ATTR_OUTPUT_NTS: {
      auto output_nts_val = EnvAttrOutputNTS::ParseVal(value);
      if (!output_nts_val.ok()) {
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
        return SQL_ERROR;
      }
      output_nts_ = std::make_shared<EnvAttrOutputNTS>();
      break;
    }
    default: {
      std::string msg =
          "Unsupported environment attribute passed to SetAttribute: ";
      msg.append(std::to_string(attribute));
      TracePrintInternal(opts, msg);
      // TODO(b/308656768,b/308656826): Record error or diagnostic info for
      // SQLDiagRec and/or SQLDiagField and return correct SQLSTATE.
      return SQL_ERROR;
    }
  }
  return SQL_SUCCESS;
}

EnvironmentHandle::EnvironmentHandle() {
  connection_pool_ =
      std::make_shared<EnvAttrConnectionPool>(EnvAttrConnectionPoolVal::CP_OFF);
  connection_pool_match_ = std::make_shared<EnvAttrConnectionPoolMatch>(
      EnvAttrCPMatchVal::STRICT_MATCH);
  odbc_ver_ = std::make_shared<EnvAttrOdbcVersion>(EnvAttrOdbcVersVal::ODBC_3);
  output_nts_ = std::make_shared<EnvAttrOutputNTS>();
}
}  // namespace google::cloud::odbc_bq_driver_internal
