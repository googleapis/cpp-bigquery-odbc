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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_HANDLE_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_HANDLE_H

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_attr.h"
#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include <map>

namespace google::cloud::odbc_bq_driver_internal {

class DescriptorHandle : public Handle {
 public:
  explicit DescriptorHandle(DescriptorType type = DescriptorType::kApplication,
                            SQLSMALLINT alloc_type = SQL_DESC_ALLOC_AUTO)
      : type_(type), header_record_(alloc_type){};

  ~DescriptorHandle() override = default;

  DescriptorHandle(DescriptorHandle const&) = default;
  DescriptorHandle& operator=(DescriptorHandle const&) = default;
  DescriptorHandle(DescriptorHandle&&) = default;
  DescriptorHandle& operator=(DescriptorHandle&&) = default;

  HandleType GetHandleType() override { return HandleType::kDescriptorHandle; }

  DescriptorType GetType() { return type_; }

  HeaderRecord& GetHeaderRecord() { return header_record_; }

  bool HasDescriptorRecord(int index) {
    return descriptor_records_.count(index) > 0;
  }

  // Should be called after HasDescriptorRecord function.
  // Because we can't return StatusRecordOr of a reference.
  DescriptorRecord& GetDescriptorRecord(int index) {
    return descriptor_records_[index];
  }

  void BindNewDescriptorRecord(SQLSMALLINT index,
                               DescriptorRecord descriptor_record);

  odbc_internal::StatusRecordOr<DescriptorRecord> UnbindDescriptorRecord(
      SQLSMALLINT index);

  odbc_internal::StatusRecord UnbindAllDescriptorRecordsFrom(SQLSMALLINT index);

  inline std::map<SQLSMALLINT, DescriptorRecord> GetDescriptorRecords() {
    return descriptor_records_;
  }

  odbc_internal::StatusRecord SetDescriptorRecords(
      std::map<SQLSMALLINT, DescriptorRecord> const& descriptor_records);

 private:
  DescriptorType type_;
  HeaderRecord header_record_;
  std::map<SQLSMALLINT, DescriptorRecord> descriptor_records_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_HANDLE_H
