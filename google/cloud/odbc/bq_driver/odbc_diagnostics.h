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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DIAGNOSTICS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DIAGNOSTICS_H

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// ODBC diagnostics:
//
// SQLGetDiagFieldInternal
// SQLGetDiagRecInternal
///////////////////////////////////////////////////////////

#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver {

SQLRETURN SQLGetDiagFieldInternal(SQLSMALLINT handle_type, SQLHANDLE handle,
                                  SQLSMALLINT rec_number,
                                  SQLSMALLINT diag_identifier,
                                  SQLPOINTER diag_info,
                                  SQLSMALLINT diag_info_buffer_len,
                                  SQLSMALLINT* diag_info_string_len);

SQLRETURN SQLGetDiagRecInternal(SQLSMALLINT handle_type, SQLHANDLE handle,
                                SQLSMALLINT rec_number, SQLCHAR* sql_state,
                                SQLINTEGER* native_error, SQLCHAR* message_text,
                                SQLSMALLINT message_text_buffer_len,
                                SQLSMALLINT* message_text_len);

}  // namespace google::cloud::odbc_bq_driver

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DIAGNOSTICS_H
