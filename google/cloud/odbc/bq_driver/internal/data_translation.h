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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H

#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver_internal {

struct DataBuffer {
  // C data type of the data the application expects
  SQLSMALLINT type;

  // Pointer to the buffer provided by the application
  SQLPOINTER buf;

  // Length of the buffer provided by the application
  SQLLEN buflen;

  // Length of the result populated by the driver
  const SQLLEN* result_len;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DATA_TRANSLATION_H
