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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DRIVER_INFO_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DRIVER_INFO_H

///////////////////////////////////////////////////////////
// Defines the following internal APIs related to
// features or metadata supported by driver or datasource:
//
// SQLGetInfoInternal
// SQLGetFunctionsInternal
// SQLGetTypeInfoInternal
// SQLColumnsInternal
// SQLTablesInternal
// SQLPrimaryKeysInternal
// SQLForeignKeysInternal
// SQLProcedureColumnsInternal
// SQLProcedureInternal
// SQLSpecialColumnsInternal
// SQLStatisticsInternal
// SQLTablePrivilegesInternal
// SQLColumnPrivilegesInternal
///////////////////////////////////////////////////////////

#include "google/cloud/odbc/bq_driver/internal/odbc_includes.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"
#include "google/cloud/odbc/bq_driver/odbc_connection.h"

namespace google::cloud::odbc_bq_driver {

// Implements the semantics for SQLGetFunctions
// as per the ODBC 3.8 spec. For details on
// semantics please refer to:
//
// https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetfunctions-function
SQLRETURN SQLGetFunctionsInternal(SQLHDBC connectionHandle,
                                  SQLUSMALLINT functionId,
                                  SQLUSMALLINT* supportedFunction);

}  // namespace google::cloud::odbc_bq_driver

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DRIVER_INFO_H
