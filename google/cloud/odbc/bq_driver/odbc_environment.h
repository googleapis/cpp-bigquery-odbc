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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_ENVIRONMENT_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_ENVIRONMENT_H

#include "google/cloud/odbc/bq_driver/internal/odbc_env_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// ODBC environment:
//
// SQLAllocEnvInternal
// SQLSetEnvAttrInternal
// SQLGetEnvAttrInternal
// SQLFreeHandleInternal
// SQLCancelHandleInternal
///////////////////////////////////////////////////////////

namespace google::cloud::odbc_bq_driver {

SQLRETURN SQLAllocEnvHandle(SQLHANDLE* out_env_handle);

// Methods related to setting and getting Environment attributes.
SQLRETURN SQL_API SQLSetEnvAttrInternal(SQLHENV environment_handle,
                                        SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER val_str_len);
SQLRETURN SQL_API SQLGetEnvAttrInternal(SQLHENV environment_handle,
                                        SQLINTEGER attribute, SQLPOINTER value,
                                        SQLINTEGER value_buffer_len,
                                        SQLINTEGER* val_str_len);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_ENVIRONMENT_H
