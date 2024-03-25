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

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/internal/sql_state_constants.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

void DescriptorHandle::BindNewDescriptorRecord(
    SQLSMALLINT index, DescriptorRecord descriptor_record) {
  descriptor_records_[index] = std::move(descriptor_record);
  header_record_.count = descriptor_records_.rbegin()->first;
}

StatusRecordOr<DescriptorRecord> DescriptorHandle::UnbindDescriptorRecord(
    int index) {
  if (descriptor_records_.count(index)) {
    DescriptorRecord erased = descriptor_records_[index];
    descriptor_records_.erase(index);
    header_record_.count = descriptor_records_.rbegin()->first;
    return erased;
  }
  return StatusRecord{SQLStates::k_HY000(),
                      "Trying to unbind non-existent descriptor record"};
}

}  // namespace google::cloud::odbc_bq_driver_internal
