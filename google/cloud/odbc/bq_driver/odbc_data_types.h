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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DATA_TYPES_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DATA_TYPES_H

#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver {

bool IsValidSQLDataType((SQLSMALLINT data_type) {
  switch (data_type) {
    case SQL_C_CHAR:
    case SQL_C_WCHAR:
    case SQL_C_SSHORT:
    case SQL_C_USHORT:
    case SQL_C_SLONG:
    case SQL_C_ULONG:
    case SQL_C_FLOAT:
    case SQL_C_DOUBLE:
    case SQL_C_BIT:
    case SQL_C_STINYINT:
    case SQL_C_CHAR:
    case SQL_C_CHAR:
    case SQL_C_CHAR:
  }
}

bool IsValidCDataType((SQLSMALLINT data_type) {
  switch (data_type) {
    case SQL_C_CHAR:
    case SQL_C_WCHAR:
    case SQL_C_SSHORT:
    case SQL_C_USHORT:
    case SQL_C_SLONG:
    case SQL_C_ULONG:
    case SQL_C_FLOAT:
    case SQL_C_DOUBLE:
    case SQL_C_BIT:
    case SQL_C_STINYINT:
    case SQL_C_CHAR:
    case SQL_C_CHAR:
    case SQL_C_CHAR:
  }
}

}  // namespace google::cloud::odbc_bq_driver

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_DATA_TYPES_H
