// Copyright 2026 Google LLC
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


#include "google/cloud/odbc/internal/commons.h"

namespace google::cloud::odbc_internal {
using ::google::cloud::odbc_bq_driver_internal::kTraceOptsFile;

bool ShouldLog(LogLevel level) {
  if (!kTraceOptsFile.Ok()) return false;
  auto const& trace_opts = *kTraceOptsFile.GetValue();
  if (!trace_opts.logging_enabled) {
    return false;
  }
  return static_cast<int>(level) <= static_cast<int>(trace_opts.log_level);
}
}  // namespace google::cloud::odbc_internal
