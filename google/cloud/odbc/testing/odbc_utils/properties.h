
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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_PROPERTIES_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_PROPERTIES_H

#include "google/cloud/odbc/testing/odbc_utils/commons.h"

namespace google::cloud::odbc_tests {

SQLRETURN GetAllFunctions(std::shared_ptr<ODBCHandles> conn);

}  // namespace google::cloud::odbc_tests

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_TESTING_ODBC_UTILS_PROPERTIES_H
