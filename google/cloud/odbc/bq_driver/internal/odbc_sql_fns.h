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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FNS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FNS_H

#include "google/cloud/odbc/bq_driver/internal/odbc_includes.h"
#include "google/cloud/odbc/bq_driver/internal/trace_utils.h"

namespace google::cloud::odbc_bq_driver_internal {

// Size for ODBC2 supported functions array,
// not defined in driver manager.
constexpr int kSqlApiAllFuncsSize = 100;

Status PopulateSupportedODBC3Functions(TraceOptions& opts,
                                       SQLUSMALLINT* supportedFunction);

Status PopulateSupportedODBC2Functions(TraceOptions& opts,
                                       SQLUSMALLINT* supportedFunction);

int IsOdbcFunctionIdSupported(UWORD fid);
bool IsFunctionIdOdbc3(UWORD fid);
bool IsFunctionIdOdbc2(UWORD fid);
}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_SQL_FNS_H
