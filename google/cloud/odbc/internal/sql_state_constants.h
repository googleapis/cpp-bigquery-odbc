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

#ifndef GOOGLE_CLOUD_ODBC_INTERNAL_SQL_STATE_CONSTANTS_H
#define GOOGLE_CLOUD_ODBC_INTERNAL_SQL_STATE_CONSTANTS_H

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
  static inline std::string const k_01000() { return "01000"; };
  static inline std::string const k_28000() { return "28000"; };
  static inline std::string const k_42000() { return "42000"; };
  static inline std::string const k_HY000() { return "HY000"; };
  static inline std::string const k_HY001() { return "HY001"; };
  static inline std::string const k_HY090() { return "HY090"; };
};

}  // namespace google::cloud::odbc_internal

#endif  // GOOGLE_CLOUD_ODBC_INTERNAL_SQL_STATE_CONSTANTS_H
