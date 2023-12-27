
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_DRIVER_PROPERTIES_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_DRIVER_PROPERTIES_H

#include "testing/commons.h"

namespace google::cloud::bigquery_odbc {

using std::map;

const map<std::string, SQLSMALLINT> kBqToSqlDataTypes = {
    { "INT64", SQL_BIGINT },
    { "BOOL", SQL_BIT },
    { "DATE", SQL_TYPE_DATE },
    { "FLOAT64", SQL_DOUBLE },
    { "TIME", SQL_TYPE_TIME },
    { "TIMESTAMP", SQL_TYPE_TIMESTAMP },
    { "DATETIME", SQL_TYPE_TIMESTAMP },
    { "BYTES", SQL_VARBINARY },
    { "STRING", SQL_VARCHAR },
    { "ARRAY", SQL_VARCHAR },
    { "STRUCT", SQL_VARCHAR },
    { "INTERVAL", SQL_VARCHAR },
    { "JSON", SQL_VARCHAR },
    { "GEOGRAPHY", SQL_VARCHAR },
    { "NUMERIC", SQL_NUMERIC },
    { "BIGNUMERIC", SQL_NUMERIC }
};

SQLRETURN GetAllFunctions(std::shared_ptr<ConnectionHandle> conn);

} // namespace google::cloud::bigquery_odbc

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_DRIVER_PROPERTIES_H
