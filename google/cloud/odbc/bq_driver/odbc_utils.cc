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

#include "google/cloud/odbc/bq_driver/odbc_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_commons.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using ::google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using ::google::cloud::odbc_bq_driver_internal::StatementHandle;

EnvironmentHandle* ValidateEnvironmentHandle(SQLHENV environment_handle) {
  return CastToInternalHandle<EnvironmentHandle>(environment_handle,
                                                 HandleType::kEnvHandle);
}

ConnectionHandle* ValidateConnectionHandle(SQLHDBC connection_handle) {
  return CastToInternalHandle<ConnectionHandle>(connection_handle,
                                                HandleType::kConnHandle);
}

StatementHandle* ValidateStatementHandle(SQLHSTMT statement_handle) {
  return CastToInternalHandle<StatementHandle>(statement_handle,
                                               HandleType::kStatementHandle);
}

}  // namespace google::cloud::odbc_bq_driver
