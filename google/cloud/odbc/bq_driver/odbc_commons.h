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

#include "google/cloud/odbc/bq_driver/internal/odbc_conn_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_statement_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver {

////////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// common ODBC APIs which can be called from other internal APIs:
//
// SQLFreeHandleInternal
/////////////////////////////////////////////////////////////

enum class HandleType {
  kConnHandle,
  kEnvHandle,
  kStatementHandle,
  kDescriptorHandle
};

struct HandleWrapped {
  explicit HandleWrapped(HandleType handle_type, SQLHANDLE handle_ref)
      : handle_type(handle_type), handle_ref(handle_ref){};
  ~HandleWrapped() = default;

  HandleType handle_type;
  SQLHANDLE handle_ref;  // reference to the internal handle we created
};

SQLRETURN SQLFreeHandleInternal(SQLSMALLINT handle_type, SQLHANDLE in_handle);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_COMMONS_H
