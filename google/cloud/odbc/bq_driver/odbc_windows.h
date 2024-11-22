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
#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_WINDOWS_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_WINDOWS_H

#ifdef _WIN32
#include "google/cloud/odbc/bq_driver/internal/utils.h"

namespace google::cloud::odbc_bq_driver {
using google::cloud::odbc_bq_driver_internal::Section;

bool ConfigDSNInternal(HWND hwnd_parent, WORD f_request, LPCSTR lpsz_driver,
                       LPCSTR lpsz_attributes);

}  // namespace google::cloud::odbc_bq_driver
#endif  /* WIN32*/
#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_ODBC_WINDOWS_H
