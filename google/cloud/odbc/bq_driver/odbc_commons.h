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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_COMMONS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_COMMONS_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include <cstring>
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver {

using DSValue = std::vector<char>;

using DSRow = std::vector<DSValue>;

using ResultSet = std::vector<DSRow>;

inline void StringToDSValue(std::string& str, DSValue& value) {
  value.resize(str.size());
  std::copy(str.begin(), str.end(), value.begin());
}

inline void DSValueToString(DSValue& value, std::string& str) {
  str.assign(value.begin(), value.end());
}

////////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// common ODBC APIs which can be called from other internal APIs:
//
// SQLFreeHandleInternal
/////////////////////////////////////////////////////////////

SQLRETURN SQLFreeHandleInternal(SQLSMALLINT handle_type, SQLHANDLE in_handle);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_COMMONS_H
