// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/odbc/testing/bq_driver_utils/status_utils.h"

namespace google::cloud::odbc_testing_bq_driver_utils {

using google::cloud::odbc_bq_driver_internal::ConnectionHandle;
using google::cloud::odbc_bq_driver_internal::EnvironmentHandle;
using google::cloud::odbc_bq_driver_internal::StatementHandle;
using google::cloud::odbc_internal::StatusRecord;

StatusRecord GetLastStatusRecord(EnvironmentHandle& handle) {
  auto status_records = handle.GetDiagnostics().GetStatusRecords();
  return status_records[status_records.size() - 1];
}

StatusRecord GetLastStatusRecord(ConnectionHandle& handle) {
  auto status_records = handle.GetDiagnostics().GetStatusRecords();
  return status_records[status_records.size() - 1];
}

StatusRecord GetLastStatusRecord(StatementHandle& handle) {
  auto status_records = handle.GetDiagnostics().GetStatusRecords();
  return status_records[status_records.size() - 1];
}

}  // namespace google::cloud::odbc_testing_bq_driver_utils
