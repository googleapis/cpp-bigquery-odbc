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
    header_record_.count =
        descriptor_records_.empty() ? 0 : descriptor_records_.rbegin()->first;
    return erased;
  }
  return StatusRecord{SQLStates::k_HY000(),
                      "Trying to unbind non-existent descriptor record"};
}

StatusRecord DescriptorHandle::UnbindAllDescriptorRecordsFrom(int index) {
  if (index < 0) {
    return {SQLStates::k_07009(), "Invalid descriptor index"};
  }
  int old_val = header_record_.count;
  for (int i = index + 1; i <= old_val; i++) {
    if (descriptor_records_.count(i)) {
      descriptor_records_.erase(i);
    }
  }
  header_record_.count =
      descriptor_records_.empty() ? 0 : descriptor_records_.rbegin()->first;
  return StatusRecord::Ok();
}

void DescriptorRecord::SetName(std::string const& val,
                               SQLINTEGER const buffer_len) {
  if (val.empty() || buffer_len == 0) {
    name = "";
    unnamed = SQL_UNNAMED;
  } else {
    if (buffer_len == SQL_NTS ||
        buffer_len >= static_cast<SQLINTEGER>(val.size())) {
      name = val;
    } else {
      name = val.substr(0, buffer_len);
    }
    unnamed = SQL_NAMED;
  }
}

StatusRecord DescriptorRecord::SetNumPrecRadix(SQLINTEGER value) {
  if (value != kNumPrecRadixForNonNumeric &&
      value != kNumPrecRadixForApproximateNumeric &&
      value != kNumPrecRadixForExactNumeric) {
    return {SQLStates::k_HY092(), "Invalid attribute/option identifier"};
  }
  num_prec_radix = value;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetParameterType(SQLSMALLINT value) {
  if (value != SQL_PARAM_INPUT && value != SQL_PARAM_INPUT_OUTPUT &&
      value != SQL_PARAM_OUTPUT) {
    return {SQLStates::k_HY105(), "Invalid parameter type"};
  }
  parameter_type = value;
  return StatusRecord::Ok();
}

StatusRecord DescriptorRecord::SetUnnamed(SQLSMALLINT value) {
  if (value != SQL_UNNAMED) {
    return {SQLStates::k_HY091(), "Invalid descriptor field identifier"};
  }
  unnamed = value;
  return StatusRecord::Ok();
}

}  // namespace google::cloud::odbc_bq_driver_internal
