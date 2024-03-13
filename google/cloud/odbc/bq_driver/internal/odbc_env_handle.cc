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
#include "google/cloud/odbc/internal/diagnostic_records.h"

namespace google::cloud::odbc_bq_driver_internal {

using ::google::cloud::odbc_bq_driver_internal::kTraceOptsConsole;
using ::google::cloud::odbc_internal::SQLStates;
using ::google::cloud::odbc_internal::StatusRecord;
using ::google::cloud::odbc_internal::StatusRecordOr;

namespace {
StatusRecord UnSupportedValue(std::string const& msg_prefix,
                              std::string const& attribute_val) {
  std::string msg = msg_prefix;
  msg.append(attribute_val);
  return StatusRecord{SQLStates::k_HY024(), msg};
}

}  // namespace

EnvAttrConnectionPool::EnvAttrConnectionPool(
    EnvAttrConnectionPoolVal const& val) {
  switch (val) {
    case EnvAttrConnectionPoolVal::kCpOff: {
      name_ = "SQL_CP_OFF";
      val_ = SQL_CP_OFF;
      break;
    }
    case EnvAttrConnectionPoolVal::kOnePerDriver: {
      name_ = "SQL_CP_ONE_PER_DRIVER";
      val_ = SQL_CP_ONE_PER_DRIVER;
      break;
    }
    case EnvAttrConnectionPoolVal::kOnePerHenv: {
      name_ = "SQL_CP_ONE_PER_HENV";
      val_ = SQL_CP_ONE_PER_HENV;
      break;
    }
  }
}

EnvAttrConnectionPoolMatch::EnvAttrConnectionPoolMatch(
    EnvAttrCPMatchVal const& val) {
  switch (val) {
    case EnvAttrCPMatchVal::kRelaxedMatch: {
      name_ = "SQL_CP_RELAXED_MATCH";
      val_ = SQL_CP_RELAXED_MATCH;
      break;
    }
    case EnvAttrCPMatchVal::kStrictMatch: {
      name_ = "SQL_CP_STRICT_MATCH";
      val_ = SQL_CP_STRICT_MATCH;
      break;
    }
  }
}

EnvAttrOdbcVersion::EnvAttrOdbcVersion(EnvAttrOdbcVersVal const& val) {
  switch (val) {
    case EnvAttrOdbcVersVal::kOdbc2: {
      name_ = "SQL_OV_ODBC2";
      val_ = SQL_OV_ODBC2;
      break;
    }
    case EnvAttrOdbcVersVal::kOdbc3: {
      name_ = "SQL_OV_ODBC3";
      val_ = SQL_OV_ODBC3;
      break;
    }
  }
}

StatusRecordOr<EnvAttrConnectionPoolVal> EnvAttrConnectionPool::ParseVal(
    void* value) {
  auto actual_value = reinterpret_cast<std::size_t>(value);
  switch (actual_value) {
    case SQL_CP_OFF: {
      return EnvAttrConnectionPoolVal::kCpOff;
    }
    case SQL_CP_ONE_PER_DRIVER: {
      return EnvAttrConnectionPoolVal::kOnePerDriver;
    }
    case SQL_CP_ONE_PER_HENV: {
      return EnvAttrConnectionPoolVal::kOnePerHenv;
    }
    default: {
      return UnSupportedValue(
          "Unsupported attribute value for EnvAttrConnectionPool:",
          std::to_string(actual_value));
    }
  }
}

StatusRecordOr<EnvAttrCPMatchVal> EnvAttrConnectionPoolMatch::ParseVal(
    void* value) {
  auto actual_value = reinterpret_cast<std::size_t>(value);
  switch (actual_value) {
    case SQL_CP_RELAXED_MATCH: {
      return EnvAttrCPMatchVal::kRelaxedMatch;
    }
    case SQL_CP_STRICT_MATCH: {
      return EnvAttrCPMatchVal::kStrictMatch;
    }
    default: {
      return UnSupportedValue(
          "Unsupported attribute value for EnvAttrConnectionPoolMatch:",
          std::to_string(actual_value));
    }
  }
}

StatusRecordOr<EnvAttrOdbcVersVal> EnvAttrOdbcVersion::ParseVal(void* value) {
  auto actual_value = reinterpret_cast<std::size_t>(value);
  switch (actual_value) {
    case SQL_OV_ODBC2: {
      return EnvAttrOdbcVersVal::kOdbc2;
    }
    case SQL_OV_ODBC3: {
      return EnvAttrOdbcVersVal::kOdbc3;
    }
    default: {
      return UnSupportedValue(
          "Unsupported attribute value for EnvAttrOdbcVersion:",
          std::to_string(actual_value));
    }
  }
}

StatusRecordOr<int> EnvAttrOutputNTS::ParseVal(void* value) {
  auto actual_value = reinterpret_cast<std::size_t>(value);
  switch (actual_value) {
    case SQL_TRUE: {
      return static_cast<int>(actual_value);
    }
    default: {
      return UnSupportedValue(
          "Unsupported attribute value for EnvAttrOutputNTS:",
          std::to_string(actual_value));
    }
  }
}

SQLRETURN EnvironmentHandle::GetAttribute(SQLINTEGER attribute, void* value,
                                          void* /*length*/) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  switch (attribute) {
    case SQL_ATTR_CONNECTION_POOLING: {
      if (connection_pool_ == nullptr) {
        auto status_record = StatusRecord{
            SQLStates::k_HY001(), "Internal error: null connection pool"};
        GetDiagnostics().AddStatusRecord(status_record);
        return status_record.CalculateReturnCode();
      }
      auto* attrib_val = reinterpret_cast<SQLUINTEGER*>(value);
      *attrib_val = connection_pool_->Value();
      break;
    }
    case SQL_ATTR_CP_MATCH: {
      if (connection_pool_match_ == nullptr) {
        auto status_record = StatusRecord{
            SQLStates::k_HY001(),
            "Internal error: attribute value for cp match not initialized"};
        GetDiagnostics().AddStatusRecord(status_record);
        return status_record.CalculateReturnCode();
      }
      auto* attrib_val = reinterpret_cast<SQLUINTEGER*>(value);
      *attrib_val = connection_pool_match_->Value();
      break;
    }
    case SQL_ATTR_ODBC_VERSION: {
      if (odbc_ver_ == nullptr) {
        auto status_record =
            StatusRecord{SQLStates::k_HY001(),
                         "Internal error: attribute value for odbc version not "
                         "initialized"};
        GetDiagnostics().AddStatusRecord(status_record);
        return status_record.CalculateReturnCode();
      }
      auto* attrib_val = reinterpret_cast<SQLINTEGER*>(value);
      *attrib_val = odbc_ver_->Value();
      break;
    }
    case SQL_ATTR_OUTPUT_NTS: {
      if (output_nts_ == nullptr) {
        auto status_record =
            StatusRecord{SQLStates::k_HY001(),
                         "Internal error: attribute value for output nts not "
                         "initialized"};
        GetDiagnostics().AddStatusRecord(status_record);
        return status_record.CalculateReturnCode();
      }
      auto* attrib_val = reinterpret_cast<SQLINTEGER*>(value);
      *attrib_val = output_nts_->Value();
      break;
    }
    default: {
      auto status_record = UnSupportedValue(
          "Unsupported environment attribute passed to GetAttribute: ",
          std::to_string(attribute));
      GetDiagnostics().AddStatusRecord(status_record);
      return status_record.CalculateReturnCode();
    }
  }
  return SQL_SUCCESS;
}

SQLRETURN EnvironmentHandle::SetAttribute(SQLINTEGER attribute, void* value,
                                          void* /*length*/) {
  TraceOptions& opts = *(*kTraceOptsConsole);
  switch (attribute) {
    case SQL_ATTR_CONNECTION_POOLING: {
      auto conn_pool_val = EnvAttrConnectionPool::ParseVal(value);
      if (!conn_pool_val) {
        GetDiagnostics().AddStatusRecord(conn_pool_val.GetStatusRecord());
        return conn_pool_val.GetCalculatedReturnCode();
      }
      connection_pool_ =
          std::make_shared<EnvAttrConnectionPool>(*conn_pool_val);
      break;
    }
    case SQL_ATTR_CP_MATCH: {
      auto cp_match_val = EnvAttrConnectionPoolMatch::ParseVal(value);
      if (!cp_match_val) {
        GetDiagnostics().AddStatusRecord(cp_match_val.GetStatusRecord());
        return cp_match_val.GetCalculatedReturnCode();
      }
      connection_pool_match_ =
          std::make_shared<EnvAttrConnectionPoolMatch>(*cp_match_val);
      break;
    }
    case SQL_ATTR_ODBC_VERSION: {
      auto odbc_vers_val = EnvAttrOdbcVersion::ParseVal(value);
      if (!odbc_vers_val) {
        GetDiagnostics().AddStatusRecord(odbc_vers_val.GetStatusRecord());
        return odbc_vers_val.GetCalculatedReturnCode();
      }
      odbc_ver_ = std::make_shared<EnvAttrOdbcVersion>(*odbc_vers_val);
      break;
    }
    case SQL_ATTR_OUTPUT_NTS: {
      auto output_nts_val = EnvAttrOutputNTS::ParseVal(value);
      if (!output_nts_val) {
        GetDiagnostics().AddStatusRecord(output_nts_val.GetStatusRecord());
        return output_nts_val.GetCalculatedReturnCode();
      }
      output_nts_ = std::make_shared<EnvAttrOutputNTS>();
      break;
    }
    default: {
      auto status_record = UnSupportedValue(
          "Unsupported environment attribute passed to SetAttribute: ",
          std::to_string(attribute));
      GetDiagnostics().AddStatusRecord(status_record);
      return status_record.CalculateReturnCode();
    }
  }
  return SQL_SUCCESS;
}

EnvironmentHandle::EnvironmentHandle() {
  connection_pool_ =
      std::make_shared<EnvAttrConnectionPool>(EnvAttrConnectionPoolVal::kCpOff);
  connection_pool_match_ = std::make_shared<EnvAttrConnectionPoolMatch>(
      EnvAttrCPMatchVal::kStrictMatch);
  odbc_ver_ = std::make_shared<EnvAttrOdbcVersion>(EnvAttrOdbcVersVal::kOdbc3);
  output_nts_ = std::make_shared<EnvAttrOutputNTS>();
}
}  // namespace google::cloud::odbc_bq_driver_internal
