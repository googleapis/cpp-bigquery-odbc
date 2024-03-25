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

#include "google/cloud/odbc/bq_driver/internal/odbc_handle.h"
#include "google/cloud/odbc/internal/diagnostic_records.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include "google/cloud/odbc/internal/status_record_or.h"
#include "google/cloud/status_or.h"
#include <memory>

namespace google::cloud::odbc_bq_driver_internal {

enum class DescriptorType { kApplication, kIRD, kIPD };

class DescriptorHandle : public Handle {
 public:
  explicit DescriptorHandle(DescriptorType type = DescriptorType::kApplication)
      : type_(type){};
  ~DescriptorHandle() = default;

  DescriptorHandle(DescriptorHandle const&) = default;
  DescriptorHandle& operator=(DescriptorHandle const&) = default;
  DescriptorHandle(DescriptorHandle&&) = default;
  DescriptorHandle& operator=(DescriptorHandle&&) = default;

 private:
  DescriptorType type_;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_DESC_HANDLE_H
