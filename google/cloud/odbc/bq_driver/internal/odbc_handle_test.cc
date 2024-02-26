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

#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

TEST(GetDiagnostics, GetDiagnosticsSuccess) {
  Handle handle;
  handle.GetDiagnostics().AddStatusRecord(
      odbc_internal::StatusRecord{"00000", "message"});

  auto diagnostics = handle.GetDiagnostics();

  EXPECT_FALSE(diagnostics.GetStatusRecord().empty());
}

}  // namespace google::cloud::odbc_bq_driver_internal
