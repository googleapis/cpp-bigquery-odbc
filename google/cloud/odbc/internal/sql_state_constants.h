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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_SQL_STATE_CONSTANTS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_SQL_STATE_CONSTANTS_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include <string>

namespace google::cloud::odbc_internal {

struct SQLStates {
 public:
  explicit SQLStates() = delete;
  ~SQLStates() = delete;

  SQLStates(SQLStates const&) = delete;
  SQLStates& operator=(SQLStates const&) = delete;
  SQLStates(SQLStates&&) = delete;
  SQLStates& operator=(SQLStates&&) = delete;

  // SQLSTATE list
  // (https://learn.microsoft.com/en-us/sql/odbc/reference/appendixes/appendix-a-odbc-error-codes?view=sql-server-ver16)
  // As the list is very long and we don't need them all,
  // new values can be added once they are needed.
  static inline std::string k_01000() { return "01000"; };
  static inline std::string k_01004() { return "01004"; };
  static inline std::string k_01S00() { return "01S00"; };
  static inline std::string k_01S01() { return "01S01"; };
  static inline std::string k_01S02() { return "01S02"; };
  static inline std::string k_01S06() { return "01S06"; };
  static inline std::string k_01S07() { return "01S07"; };
  static inline std::string k_07S01() { return "07S01"; };
  static inline std::string k_08S01() { return "08S01"; };
  static inline std::string k_08003() { return "08003"; };
  static inline std::string k_21S01() { return "21S01"; };
  static inline std::string k_21S02() { return "21S02"; };
  static inline std::string k_25S01() { return "25S01"; };
  static inline std::string k_25S02() { return "25S02"; };
  static inline std::string k_25S03() { return "25S03"; };
  static inline std::string k_28000() { return "28000"; };
  static inline std::string k_42000() { return "42000"; };
  static inline std::string k_42S01() { return "42S01"; };
  static inline std::string k_42S02() { return "42S02"; };
  static inline std::string k_42S11() { return "42S11"; };
  static inline std::string k_42S12() { return "42S12"; };
  static inline std::string k_42S21() { return "42S21"; };
  static inline std::string k_42S22() { return "42S22"; };
  static inline std::string k_HY000() { return "HY000"; };
  static inline std::string k_HY001() { return "HY001"; };
  static inline std::string k_HY024() { return "HY024"; };
  static inline std::string k_HY090() { return "HY090"; };
  static inline std::string k_HY092() { return "HY092"; };
  static inline std::string k_HY095() { return "HY095"; };
  static inline std::string k_HY096() { return "HY096"; };
  static inline std::string k_HY097() { return "HY097"; };
  static inline std::string k_HY098() { return "HY098"; };
  static inline std::string k_HY099() { return "HY099"; };
  static inline std::string k_HY100() { return "HY100"; };
  static inline std::string k_HY101() { return "HY101"; };
  static inline std::string k_HY105() { return "HY105"; };
  static inline std::string k_HY107() { return "HY107"; };
  static inline std::string k_HY109() { return "HY109"; };
  static inline std::string k_HY110() { return "HY110"; };
  static inline std::string k_HY111() { return "HY111"; };
  static inline std::string k_HYT00() { return "HYT00"; };
  static inline std::string k_HYT01() { return "HYT01"; };
  static inline std::string k_IM001() { return "IM001"; };
  static inline std::string k_IM002() { return "IM002"; };
  static inline std::string k_IM003() { return "IM003"; };
  static inline std::string k_IM004() { return "IM004"; };
  static inline std::string k_IM005() { return "IM005"; };
  static inline std::string k_IM006() { return "IM006"; };
  static inline std::string k_IM007() { return "IM007"; };
  static inline std::string k_IM008() { return "IM008"; };
  static inline std::string k_IM010() { return "IM010"; };
  static inline std::string k_IM011() { return "IM011"; };
  static inline std::string k_IM012() { return "IM012"; };
};

}  // namespace google::cloud::odbc_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_INTERNAL_SQL_STATE_CONSTANTS_H
