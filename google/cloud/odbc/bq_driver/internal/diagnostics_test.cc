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

#include "google/cloud/odbc/bq_driver/internal/diagnostics.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include <gtest/gtest.h>

namespace google::cloud::odbc_bq_driver_internal {

using odbc_internal::HeaderRecord;

TEST(ClearDiagnostics, ClearDiagnosticsSuccess) {
  auto diagnostics = std::make_unique<Diagnostics>();
  diagnostics->GetHeaderRecord().function = "new value";
  diagnostics->GetHeaderRecord().function_code = 111;
  diagnostics->GetHeaderRecord().cursor_row_count = 111;
  diagnostics->GetHeaderRecord().row_count = 111;
  diagnostics->AddStatusRecord({});
  EXPECT_FALSE(diagnostics->GetStatusRecords().empty());

  diagnostics->ClearDiagnostics();

  EXPECT_TRUE(diagnostics->GetStatusRecords().empty());
  HeaderRecord expected;
  HeaderRecord actual = diagnostics->GetHeaderRecord();
  EXPECT_EQ(expected.function, actual.function);
  EXPECT_EQ(expected.function_code, actual.function_code);
  EXPECT_EQ(expected.cursor_row_count, actual.cursor_row_count);
  EXPECT_EQ(expected.row_count, actual.row_count);
}

}  // namespace google::cloud::odbc_bq_driver_internal
