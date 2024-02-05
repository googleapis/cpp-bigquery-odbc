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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"

namespace google::cloud::odbc_bq_driver_internal {

class EnvironmentHandle : public Handle {
 public:
  explicit EnvironmentHandle() = default;
  ~EnvironmentHandle() = default;

  EnvironmentHandle(EnvironmentHandle const&) = default;
  EnvironmentHandle& operator=(EnvironmentHandle const&) = default;
  EnvironmentHandle(EnvironmentHandle&&) = default;
  EnvironmentHandle& operator=(EnvironmentHandle&&) = default;

  SQLRETURN GetAttribute(SQLINTEGER attribute, void* value, void* length);

  SQLRETURN SetAttribute(SQLINTEGER attribute, void* value, void* length);
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_ENV_HANDLE_H
