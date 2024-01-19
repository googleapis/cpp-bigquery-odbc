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

#include "google/cloud/odbc/bq_driver/odbc_driver_metadata.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_sql_fns.h"

namespace google::cloud::odbc_bq_driver {

using ::google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc2;
using ::google::cloud::odbc_bq_driver_internal::IsFunctionIdOdbc3;
using ::google::cloud::odbc_bq_driver_internal::kSqlApiAllFuncsSize;
using ::google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC2Functions;
using ::google::cloud::odbc_bq_driver_internal::PopulateSupportedODBC3Functions;
using ::google::cloud::odbc_bq_driver_internal::TraceOptions;
using ::google::cloud::odbc_bq_driver_internal::TracePrintInternal;

SQLRETURN SQLGetFunctionsInternal(SQLHDBC connectionHandle,
                                  SQLUSMALLINT functionId,
                                  SQLUSMALLINT* supportedFunction,
                                  TraceOptions& opts) {
  SQLRETURN rc = SQL_SUCCESS;
  // We are only checking the validity of the handle here.
  // No connection to data source is necessary for this ODBC API.
  if (!connectionHandle) {
    TracePrintInternal(opts, "Invalid Connection handle!");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField.
    return SQL_INVALID_HANDLE;
  }
  // Assumption here is memory for output is managed/owned by the caller.
  if (!supportedFunction) {
    TracePrintInternal(opts, "Argument supportedFunction cannot be null");
    // TODO(b/308656768,b/308656826): Record error or diagnostic info for
    // SQLDiagRec and/or SQLDiagField.
    return SQL_ERROR;
  }
  switch (functionId) {
    case SQL_API_ODBC3_ALL_FUNCTIONS: {
      Status status = PopulateSupportedODBC3Functions(opts, supportedFunction);
      if (!status.ok()) {
        TracePrintInternal(opts,
                           "Internal Error: PopulateSupportedODBCFunctions() "
                           "failed with status: " +
                               status.message());
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField.
        return SQL_ERROR;
      }
      return rc;
    }
    case SQL_API_ALL_FUNCTIONS: {
      Status status = PopulateSupportedODBC2Functions(opts, supportedFunction);
      if (!status.ok()) {
        TracePrintInternal(opts,
                           "Internal Error: PopulateSupportedODBCFunctions() "
                           "failed with status: " +
                               status.message());
        // TODO(b/308656768,b/308656826): Record error or diagnostic info for
        // SQLDiagRec and/or SQLDiagField.
        return SQL_ERROR;
      }
      return rc;
    }
    default:
      break;
  }
  if (IsFunctionIdOdbc3(functionId)) {
    SQLUSMALLINT odbc3_fns[SQL_API_ODBC3_ALL_FUNCTIONS_SIZE];
    Status status = PopulateSupportedODBC3Functions(opts, odbc3_fns);
    if (!status.ok()) {
      TracePrintInternal(opts,
                         "Internal Error: PopulateSupportedODBCFunctions() "
                         "failed with status: " +
                             status.message());
      // TODO(b/308656768,b/308656826): Record error or diagnostic info for
      // SQLDiagRec and/or SQLDiagField.
      return SQL_ERROR;
    }
    *supportedFunction = SQL_FUNC_EXISTS(odbc3_fns, functionId);
  } else if (IsFunctionIdOdbc2(functionId)) {
    SQLUSMALLINT odbc2_fns[kSqlApiAllFuncsSize];
    Status status = PopulateSupportedODBC2Functions(opts, odbc2_fns);
    if (!status.ok()) {
      TracePrintInternal(opts,
                         "Internal Error: PopulateSupportedODBCFunctions() "
                         "failed with status: " +
                             status.message());
      // TODO(b/308656768,b/308656826): Record error or diagnostic info for
      // SQLDiagRec and/or SQLDiagField.
      return SQL_ERROR;
    }
    *supportedFunction = odbc2_fns[functionId];
  }
  return rc;
}

}  // namespace google::cloud::odbc_bq_driver
