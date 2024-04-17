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

#include "google/cloud/odbc/bq_driver/internal/odbc_stmt_handle.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/status_record_or.h"

namespace google::cloud::odbc_bq_driver_internal {

using google::cloud::odbc_bq_driver_internal::HandleWrapped;
using google::cloud::odbc_internal::SQLStates;
using google::cloud::odbc_internal::StatusRecord;
using google::cloud::odbc_internal::StatusRecordOr;

SQLRETURN StatementHandle::BindColumn(SQLUSMALLINT col_idx,
                                      SQLSMALLINT data_type, SQLPOINTER buf,
                                      SQLLEN buf_len, const SQLLEN* res_len) {
  if (!buf) {
    StatusRecord status_record = {SQLStates::k_HY001(),
                                  "TargetValuePtr should not be null"};
    GetDiagnostics().AddStatusRecord(status_record);
    return SQL_ERROR;
  }

  if (buf_len < 0) {
    StatusRecord status_record = {SQLStates::k_HY090(),
                                  "BufferLength should not be less than zero"};
    GetDiagnostics().AddStatusRecord(status_record);
    return SQL_ERROR;
  }

  if (!res_len) {
    StatusRecord status_record = {SQLStates::k_HY000(),
                                  "TargetValueStrLen should not be null"};
    GetDiagnostics().AddStatusRecord(status_record);
    return SQL_ERROR;
  }

  DataBuffer data_buffer = {data_type, buf, buf_len, res_len};
  column_bindings_[col_idx] = data_buffer;
  return SQL_SUCCESS;
}

DescriptorHandle& StatementHandle::GetDescriptorHandle(
    DescriptorType type) const {
  HandleWrapped* wrapped = nullptr;
  switch (type) {
    // DescriptorType::kApplication should not be used as input argument
    case DescriptorType::kApplication:
    case DescriptorType::kARD:
      wrapped = descriptors_.ard_expl_ != nullptr ? descriptors_.ard_expl_
                                                  : descriptors_.ard_;
      break;
    case DescriptorType::kAPD:
      wrapped = descriptors_.apd_expl_ != nullptr ? descriptors_.apd_expl_
                                                  : descriptors_.apd_;
      break;
    case DescriptorType::kIRD:
      wrapped = descriptors_.ird_;
      break;
    case DescriptorType::kIPD:
      wrapped = descriptors_.ipd_;
      break;
  }
  return *reinterpret_cast<DescriptorHandle*>(wrapped->handle_ref);
}

StatusRecord StatementHandle::SetDescriptorHandle(
    DescriptorType type, HandleWrapped* desc_wrapper_ptr) {
  switch (type) {
    case DescriptorType::kARD:
      descriptors_.ard_expl_ = desc_wrapper_ptr;
      break;
    case DescriptorType::kAPD:
      descriptors_.apd_expl_ = desc_wrapper_ptr;
      break;
    default:
      return StatusRecord{SQLStates::k_HY017(),
                          "Invalid try to set implementation descriptor"};
  }
  return StatusRecord::Ok();
}

StatusRecord StatementHandle::SetAttribute(int attribute, SQLULEN value) {
  StatusRecord status_record =
      ValidateStatementAttributeToSet(attribute, value);
  if (!status_record.ok()) {
    return status_record;
  }
  attributes_[attribute] = value;
  return StatusRecord::Ok();
}

StatusRecordOr<SQLULEN> StatementHandle::GetAttribute(int attribute) {
  if (!IsStatementAttributeValid(attribute)) {
    return StatusRecord{SQLStates::k_HY092(), "Invalid attribute"};
  }
  return attributes_[attribute];
}

void DeleteDescriptor(HandleWrapped* wrapper) {
  if (wrapper) {
    auto* desc = reinterpret_cast<DescriptorHandle*>(wrapper->handle_ref);
    delete desc;
    delete wrapper;
  }
}

void StatementHandle::DeleteDescriptors() const {
  DeleteDescriptor(descriptors_.ard_);
  DeleteDescriptor(descriptors_.ard_expl_);
  DeleteDescriptor(descriptors_.apd_);
  DeleteDescriptor(descriptors_.apd_expl_);
  DeleteDescriptor(descriptors_.ird_);
  DeleteDescriptor(descriptors_.ipd_);
}

}  // namespace google::cloud::odbc_bq_driver_internal
