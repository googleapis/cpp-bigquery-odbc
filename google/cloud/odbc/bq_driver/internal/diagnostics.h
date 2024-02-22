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

#ifndef GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DIAGNOSTICS_H
#define GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DIAGNOSTICS_H

#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <string>
#include <vector>

namespace google::cloud::odbc_bq_driver_internal {

class Diagnostics {
 public:
  explicit Diagnostics() = default;
  ~Diagnostics() = default;

  Diagnostics(Diagnostics const&) = default;
  Diagnostics& operator=(Diagnostics const&) = default;
  Diagnostics(Diagnostics&&) = default;
  Diagnostics& operator=(Diagnostics&&) = default;

  void AddStatusRecord(odbc_internal::StatusRecord const& status_record);

  void ClearDiagnostics();

  std::vector<odbc_internal::StatusRecord> const& GetStatusRecords() {
    return status_records_;
  }

  inline odbc_internal::StatusRecord const& GetLastStatusRecord() {
    return status_records_[status_records_.size() - 1];
  }

  odbc_internal::HeaderRecord& GetHeaderRecord() { return header_record_; }

 private:
  odbc_internal::HeaderRecord header_record_;
  std::vector<odbc_internal::StatusRecord> status_records_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_DIAGNOSTICS_H
