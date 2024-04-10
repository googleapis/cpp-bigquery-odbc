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

#ifndef CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_ATTR_H
#define CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_ATTR_H

#include "google/cloud/odbc/bq_driver/internal/odbc_desc_handle.h"
#include "google/cloud/odbc/internal/odbc_includes.h"
#include <memory>

namespace google::cloud::odbc_bq_driver_internal {

struct Descriptors {
  Descriptors() = default;
  Descriptors(DescriptorHandle const& ard, DescriptorHandle const& apd,
              DescriptorHandle const& ird, DescriptorHandle const& ipd)
      : ard_(std::make_unique<DescriptorHandle>(ard)),
        apd_(std::make_unique<DescriptorHandle>(apd)),
        ird_(std::make_unique<DescriptorHandle>(ird)),
        ipd_(std::make_unique<DescriptorHandle>(ipd)){};

  std::unique_ptr<DescriptorHandle> ard_;
  DescriptorHandle* ard_expl_ = nullptr;
  std::unique_ptr<DescriptorHandle> apd_;
  DescriptorHandle* apd_expl_ = nullptr;
  std::unique_ptr<DescriptorHandle> ird_;
  std::unique_ptr<DescriptorHandle> ipd_;
};

struct StatementAttributes {
  SQLULEN async_enable = SQL_ASYNC_ENABLE_OFF;
  SQLULEN concurrency = SQL_CONCUR_READ_ONLY;
  SQLULEN cursor_scrollable = SQL_NONSCROLLABLE;
  SQLULEN cursor_sensitivity = SQL_UNSPECIFIED;
  SQLULEN cursor_type = SQL_CURSOR_FORWARD_ONLY;
  SQLULEN enable_auto_ipd = SQL_TRUE;
  SQLULEN max_length = 0;
  SQLULEN max_rows = 0;
  SQLULEN metadata_id = SQL_FALSE;
  SQLULEN noscan = SQL_NOSCAN_OFF;
  SQLULEN query_timeout = 0;
  SQLULEN retrieve_data = SQL_RD_ON;
  SQLULEN row_number = 0;
  SQLULEN use_bookmarks = SQL_UB_OFF;
};

}  // namespace google::cloud::odbc_bq_driver_internal

#endif  // CPP_BIGQUERY_ODBC_GOOGLE_CLOUD_ODBC_BQ_DRIVER_INTERNAL_ODBC_STMT_ATTR_H
