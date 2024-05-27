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

DescriptorHandle::DescriptorHandle(DescriptorHandle const& descriptorHandle)
    : type_(descriptorHandle.type_),
      header_record_(descriptorHandle.header_record_) {
  std::mutex descriptor_handle_mutex_;
  descriptor_records_ = descriptorHandle.descriptor_records_;
  associated_stmt_handles_ = descriptorHandle.associated_stmt_handles_;
}

DescriptorHandle& DescriptorHandle::operator=(
    DescriptorHandle const& descriptorHandle) {
  std::mutex descriptor_handle_mutex_;
  type_ = descriptorHandle.type_;
  header_record_ = descriptorHandle.header_record_;
  descriptor_records_ = descriptorHandle.descriptor_records_;
  associated_stmt_handles_ = descriptorHandle.associated_stmt_handles_;
  return *this;
}
DescriptorHandle::DescriptorHandle(DescriptorHandle&& descriptorHandle) noexcept
    : type_(std::move(descriptorHandle.type_)),
      header_record_(std::move(descriptorHandle.header_record_)) {
  std::mutex descriptor_handle_mutex_;
  descriptor_records_ = std::move(descriptorHandle.descriptor_records_);
  associated_stmt_handles_ =
      std::move(descriptorHandle.associated_stmt_handles_);
}

DescriptorHandle& DescriptorHandle::operator=(
    DescriptorHandle&& descriptorHandle) noexcept {
  std::mutex descriptor_handle_mutex_;
  type_ = std::move(descriptorHandle.type_);
  header_record_ = std::move(descriptorHandle.header_record_);
  descriptor_records_ = std::move(descriptorHandle.descriptor_records_);
  associated_stmt_handles_ =
      std::move(descriptorHandle.associated_stmt_handles_);
  return *this;
}

void DescriptorHandle::BindNewDescriptorRecord(
    SQLSMALLINT index, DescriptorRecord descriptor_record) {
  descriptor_records_[index] = std::move(descriptor_record);
  header_record_.count = descriptor_records_.rbegin()->first;
}

StatusRecordOr<DescriptorRecord> DescriptorHandle::UnbindDescriptorRecord(
    SQLSMALLINT index) {
  if (descriptor_records_.count(index)) {
    DescriptorRecord erased = descriptor_records_[index];
    descriptor_records_.erase(index);
    header_record_.count =
        descriptor_records_.empty() ? 0 : descriptor_records_.rbegin()->first;
    return erased;
  }
  return StatusRecord{SQLStates::k_HY000(),
                      "Trying to unbind non-existent descriptor record"};
}

StatusRecord DescriptorHandle::UnbindAllDescriptorRecordsFrom(
    SQLSMALLINT index) {
  if (index < 0) {
    return {SQLStates::k_07009(), "Invalid descriptor index"};
  }
  SQLSMALLINT old_val = header_record_.count;
  for (SQLSMALLINT i = index + 1; i <= old_val; i++) {
    descriptor_records_.erase(i);
  }
  header_record_.count =
      descriptor_records_.empty() ? 0 : descriptor_records_.rbegin()->first;
  return StatusRecord::Ok();
}

StatusRecord DescriptorHandle::SetDescriptorRecords(
    std::map<SQLSMALLINT, DescriptorRecord> const& descriptor_records) {
  descriptor_records_.clear();
  header_record_.count = 0;
  for (auto const& [rec_number, record] : descriptor_records) {
    BindNewDescriptorRecord(rec_number, record);
    if (record.data_ptr) {
      StatusRecord status = record.ConsistencyCheck();
      if (!status.ok()) {
        descriptor_records_[rec_number].data_ptr = nullptr;
        return status;
      }
    }
  }
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
