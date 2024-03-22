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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_CONN_ATTR_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_CONN_ATTR_H

#include "google/cloud/odbc/internal/odbc_includes.h"
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {
enum ConnectionValidation { kBefore, kAfter, kEither, kInvalid };
enum SupportedAttribute { kGet, kSet, kBoth };
enum ConnectionValueType {
  kSqlUInt,
  kSqlULen,
  kSqlChr,
  kSqlInt,
  kSqlIntBitmask,
  kSqlInvalid
};

class ConnectionAttr {
 public:
  explicit ConnectionAttr();
  ~ConnectionAttr() = default;

  // Attribute accessor functions that can be used
  // for validation and logging.
  bool IsAttributeSupported(SQLINTEGER attribute);
  bool IsGetAttributeSupported(SQLINTEGER attribute);
  bool IsSetAttributeSupported(SQLINTEGER attribute);
  std::string GetAttributeStringValue(SQLINTEGER attribute);
  ConnectionValidation GetAttributeConnectionBehavior(SQLINTEGER attribute);
  ConnectionValueType GetAttributeValueType(SQLINTEGER attribute);
  std::vector<SQLPOINTER> GetAttributePossibleValues(SQLINTEGER attribute);
  SQLPOINTER GetAttributeDefaultValue(SQLINTEGER attribute);

 private:
  // stores the attribute metadata related to connection behavior,
  // supportability with regards to get and set and
  // possible values for the attribute.
  std::map<SQLINTEGER, std::tuple<std::string, ConnectionValidation,
                                  SupportedAttribute, SQLPOINTER>>
      supported_connection_attributes_;
  // stores the attribute metadata related to value type,
  // and default values.
  std::map<SQLINTEGER, std::tuple<std::string, ConnectionValueType,
                                  std::vector<SQLPOINTER>>>
      supported_connection_attribute_values_;
};
}  // namespace google::cloud::odbc_bq_driver_internal
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_CONN_ATTR_H
